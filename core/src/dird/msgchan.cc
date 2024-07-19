/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
// Kern Sibbald, August MM

/**
 * @file
 *
 * handles the message channel to the Storage daemon and the File daemon.
 *
 * * Basic tasks done here:
 *    * Open a message channel with the Storage daemon
 *      to authenticate ourself and to pass the JobId.
 *    * Create a thread to interact with the Storage daemon
 *      who returns a job status and requests Catalog services, etc.
 */
#include "include/bareos.h"
#include "dird.h"
#include "dird/getmsg.h"
#include "dird/job.h"
#include "dird/director_jcr_impl.h"
#include "dird/msgchan.h"
#include "dird/quota.h"
#include "dird/sd_cmds.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/util.h"
#include "lib/thread_specific_data.h"
#include "lib/watchdog.h"

namespace directordaemon {

/* Commands sent to Storage daemon */
static char jobcmd[]
    = "JobId=%s job=%s job_name=%s client_name=%s "
      "type=%d level=%d FileSet=%s NoAttr=%d SpoolAttr=%d FileSetMD5=%s "
      "SpoolData=%d PreferMountedVols=%d SpoolSize=%s "
      "rerunning=%d VolSessionId=%d VolSessionTime=%d Quota=%llu "
      "Protocol=%d BackupFormat=%s\n";
static char use_storage[]
    = "use storage=%s media_type=%s pool_name=%s "
      "pool_type=%s append=%d copy=%d stripe=%d\n";
static char use_device[] = "use device=%s\n";
// static char query_device[] =
//   "query device=%s";

/* Response from Storage daemon */
static char OKbootstrap[] = "3000 OK bootstrap\n";
static char OK_job[] = "3000 OK Job SDid=%d SDtime=%d Authorization=%100s\n";
static char OK_nextrun[] = "3000 OK Job Authorization=%100s\n";
static char OK_device[] = "3000 OK use device device=%s\n";

/* Storage Daemon requests */
static char Job_start[] = "3010 Job %127s start\n";
static char Job_end[]
    = "3099 Job %127s end JobStatus=%d JobFiles=%d JobBytes=%lld "
      "JobErrors=%u\n";

/* Forward referenced functions */
extern "C" void* msg_thread(void* arg);

/** Send bootstrap file to Storage daemon.
 *  This is used for
 *    * restore
 *    * verify
 *    * VolumeToCatalog
 *    * migration and
 *    * copy Jobs
 */
static inline bool SendBootstrapFileToSd(JobControlRecord* jcr,
                                         BareosSocket* sd)
{
  FILE* bs;
  char buf[1000];
  const char* bootstrap = "bootstrap\n";

  Dmsg1(400, "SendBootstrapFileToSd: %s\n", jcr->RestoreBootstrap);
  if (!jcr->RestoreBootstrap) { return true; }
  bs = fopen(jcr->RestoreBootstrap, "rb");
  if (!bs) {
    BErrNo be;
    Jmsg(jcr, M_FATAL, 0, T_("Could not open bootstrap file %s: ERR=%s\n"),
         jcr->RestoreBootstrap, be.bstrerror());
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }
  sd->fsend(bootstrap);
  while (fgets(buf, sizeof(buf), bs)) { sd->fsend("%s", buf); }
  sd->signal(BNET_EOD);
  fclose(bs);
  if (jcr->dir_impl->unlink_bsr) {
    SecureErase(jcr, jcr->RestoreBootstrap);
    jcr->dir_impl->unlink_bsr = false;
  }
  return true;
}

bool ReserveReadDevice(JobControlRecord* jcr,
                       alist<StorageResource*>* read_storage)
{
  BareosSocket* sd_socket = jcr->store_bsock;
  PoolMem device_name;
  std::string StoreName{};
  std::string pool_name{};
  std::string pool_type{};
  std::string media_type{};
  bool ok = true;
  int copy = 0;
  int stripe = 0;
  /* We have two loops here. The first comes from the
   *  Storage = associated with the Job, and we need
   *  to attach to each one.
   * The inner loop loops over all the alternative devices
   *  associated with each Storage. It selects the first
   *  available one.
   * */
  // Do read side of storage daemon
  if (read_storage) {
    // For the moment, only migrate, copy and vbackup have rpool
    if (jcr->is_JobType(JT_MIGRATE) || jcr->is_JobType(JT_COPY)
        || (jcr->is_JobType(JT_BACKUP) && jcr->is_JobLevel(L_VIRTUAL_FULL))) {
      pool_type = jcr->dir_impl->res.rpool->pool_type;
      pool_name = jcr->dir_impl->res.rpool->resource_name_;
    } else {
      pool_type = jcr->dir_impl->res.pool->pool_type;
      pool_name = jcr->dir_impl->res.pool->resource_name_;
    }
    BashSpaces(pool_type);
    BashSpaces(pool_name);
    for (auto* storage : read_storage) {
      Dmsg1(100, "Rstore=%s\n", storage->resource_name_);
      StoreName = storage->resource_name_;
      BashSpaces(StoreName);
      media_type = storage->media_type;
      BashSpaces(media_type);
      sd_socket->fsend(use_storage, StoreName.c_str(), media_type.c_str(),
                       pool_name.c_str(), pool_type.c_str(), 0, copy, stripe);
      Dmsg1(100, "read_storage >stored: %s", sd_socket->msg);
      /* Loop over alternative storage Devices until one is OK */
      for (auto* dev : storage->device) {
        PmStrcpy(device_name, dev->resource_name_);
        BashSpaces(device_name);
        sd_socket->fsend(use_device, device_name.c_str());
        Dmsg1(100, ">stored: %s", sd_socket->msg);
      }
      sd_socket->signal(BNET_EOD);  // end of Device
    }
    sd_socket->signal(BNET_EOD);  // end of Storages
    if (BgetDirmsg(sd_socket) > 0) {
      Dmsg1(100, "<stored: %s", sd_socket->msg);
      // ****FIXME**** save actual device name
      ok = sscanf(sd_socket->msg, OK_device, device_name.c_str()) == 1;
    } else {
      ok = false;
    }
    if (ok) {
      Jmsg(jcr, M_INFO, 0, T_("Using Device \"%s\" to read.\n"),
           device_name.c_str());
    }
  }

  if (!ok) {
    if (jcr->store_bsock->msg[0]) {
      Jmsg(jcr, M_FATAL, 0,
           T_("\n"
              "     Storage daemon didn't accept Device \"%s\" because:\n     "
              "%s\n"),
           device_name.c_str(), jcr->store_bsock->msg);
    } else {
      Jmsg(jcr, M_FATAL, 0,
           T_("\n"
              "     Storage daemon didn't accept Device \"%s\" command.\n"),
           device_name.c_str());
    }
  }

  return ok;
}

bool ReserveWriteDevice(JobControlRecord* jcr,
                        alist<StorageResource*>* write_storage)
{
  PoolMem device_name;
  std::string StoreName{};
  std::string pool_name{};
  std::string pool_type{};
  std::string media_type{};
  bool ok = true;
  int copy = 0;
  int stripe = 0;
  /* We have two loops here. The first comes from the
   *  Storage = associated with the Job, and we need
   *  to attach to each one.
   * The inner loop loops over all the alternative devices
   *  associated with each Storage. It selects the first
   *  available one.
   * */
  // Do write side of storage daemon
  if (write_storage) {
    pool_type = jcr->dir_impl->res.pool->pool_type;
    pool_name = jcr->dir_impl->res.pool->resource_name_;
    BashSpaces(pool_type);
    BashSpaces(pool_name);
    for (auto* storage : write_storage) {
      StoreName = storage->resource_name_;
      BashSpaces(StoreName);
      media_type = storage->media_type;
      BashSpaces(media_type);
      jcr->store_bsock->fsend(use_storage, StoreName.c_str(),
                              media_type.c_str(), pool_name.c_str(),
                              pool_type.c_str(), 1, copy, stripe);

      Dmsg1(100, "write_storage >stored: %s", jcr->store_bsock->msg);
      // Loop over alternative storage Devices until one is OK
      for (auto* dev : storage->device) {
        PmStrcpy(device_name, dev->resource_name_);
        BashSpaces(device_name);
        jcr->store_bsock->fsend(use_device, device_name.c_str());
        Dmsg1(100, ">stored: %s", jcr->store_bsock->msg);
      }
      jcr->store_bsock->signal(BNET_EOD);  // end of Devices
    }
    jcr->store_bsock->signal(BNET_EOD);  // end of Storages
    if (BgetDirmsg(jcr->store_bsock) > 0) {
      Dmsg1(100, "<stored: %s", jcr->store_bsock->msg);
      // ****FIXME**** save actual device name
      ok = sscanf(jcr->store_bsock->msg, OK_device, device_name.c_str()) == 1;
    } else {
      ok = false;
    }
    if (ok) {
      Jmsg(jcr, M_INFO, 0, T_("Using Device \"%s\" to write.\n"),
           device_name.c_str());
    }
  }
  if (!ok) {
    if (jcr->store_bsock->msg[0]) {
      Jmsg(jcr, M_FATAL, 0,
           T_("\n"
              "     Storage daemon didn't accept Device \"%s\" because:\n     "
              "%s\n"),
           device_name.c_str(), jcr->store_bsock->msg);
    } else {
      Jmsg(jcr, M_FATAL, 0,
           T_("\n"
              "     Storage daemon didn't accept Device \"%s\" command.\n"),
           device_name.c_str());
    }
  }

  return ok;
}

bool ReserveReadAndWriteDevices(JobControlRecord* jcr,
                                alist<StorageResource*>* read_storage,
                                alist<StorageResource*>* write_storage)
{
  if (!ReserveReadDevice(jcr, read_storage)) { return false; }
  if (!ReserveWriteDevice(jcr, write_storage)) { return false; }

  return true;
}

/** Start a job with the Storage daemon
 */
bool StartStorageDaemonJob(JobControlRecord* jcr, bool send_bsr)
{
  BareosSocket* sd_socket = jcr->store_bsock;

  /* Before actually starting a new Job on the SD make sure we send any specific
   * plugin options for this Job. */
  if (!SendStoragePluginOptions(jcr)) {
    Jmsg(jcr, M_FATAL, 0,
         T_("Storage daemon rejected Plugin Options command: %s\n"),
         sd_socket->msg);
    return false;
  }

  // Now send JobId and permissions, and get back the authorization key.
  std::string job_name = jcr->dir_impl->res.job->resource_name_;
  BashSpaces(job_name);

  std::string client_name{};
  if (jcr->dir_impl->res.client) {
    client_name = jcr->dir_impl->res.client->resource_name_;
  } else {
    client_name = "**None**";
  }
  BashSpaces(client_name);
  std::string fileset_name{};
  if (jcr->dir_impl->res.fileset) {
    fileset_name = jcr->dir_impl->res.fileset->resource_name_;
  } else {
    fileset_name = "**None**";
  }
  BashSpaces(fileset_name);

  std::string backup_format{};
  if (jcr->dir_impl->backup_format) {
    backup_format = jcr->dir_impl->backup_format;
    BashSpaces(backup_format);
  } else {
    backup_format = "**None**";
  }

  const char* fileset_md5;
  if (jcr->dir_impl->res.fileset && jcr->dir_impl->res.fileset->MD5[0] == 0) {
    bstrncpy(jcr->dir_impl->res.fileset->MD5, "**Dummy**",
             sizeof(jcr->dir_impl->res.fileset->MD5));
    fileset_md5 = jcr->dir_impl->res.fileset->MD5;
  } else if (jcr->dir_impl->res.fileset) {
    fileset_md5 = jcr->dir_impl->res.fileset->MD5;
  } else {
    fileset_md5 = "**Dummy**";
  }

  /* If rescheduling, cancel the previous incarnation of this job
   * with the SD, which might be waiting on the FD connection.
   * If we do not cancel it the SD will not accept a new connection
   * for the same jobid. */
  if (jcr->dir_impl->reschedule_count) {
    sd_socket->fsend("cancel Job=%s\n", jcr->Job);
    while (sd_socket->recv() >= 0) { continue; }
  }

  // Retrieve available quota 0 bytes means dont perform the check

  uint64_t remainingquota = FetchRemainingQuotas(jcr);
  Dmsg1(50, "Remainingquota: %llu\n", remainingquota);

  char ed1[30];
  char ed2[30];
  sd_socket->fsend(
      jobcmd, edit_int64(jcr->JobId, ed1), jcr->Job, job_name.c_str(),
      client_name.c_str(), jcr->getJobType(), jcr->getJobLevel(),
      fileset_name.c_str(), !jcr->dir_impl->res.pool->catalog_files,
      jcr->dir_impl->res.job->SpoolAttributes, fileset_md5,
      jcr->dir_impl->spool_data, jcr->dir_impl->res.job->PreferMountedVolumes,
      edit_int64(jcr->dir_impl->spool_size, ed2), jcr->rerunning,
      jcr->VolSessionId, jcr->VolSessionTime, remainingquota,
      jcr->getJobProtocol(), backup_format.c_str());

  Dmsg1(100, ">stored: %s", sd_socket->msg);
  if (BgetDirmsg(sd_socket) > 0) {
    Dmsg1(100, "<stored: %s", sd_socket->msg);
    char auth_key[100];
    if (sscanf(sd_socket->msg, OK_job, &jcr->VolSessionId, &jcr->VolSessionTime,
               &auth_key)
        != 3) {
      Dmsg1(100, "BadJob=%s\n", sd_socket->msg);
      Jmsg(jcr, M_FATAL, 0, T_("Storage daemon rejected Job command: %s\n"),
           sd_socket->msg);
      return false;
    } else {
      BfreeAndNull(jcr->sd_auth_key);
      jcr->sd_auth_key = strdup(auth_key);
      Dmsg1(150, "sd_auth_key=%s\n", jcr->sd_auth_key);
    }
  } else {
    Jmsg(jcr, M_FATAL, 0, T_("<stored: bad response to Job command: %s\n"),
         sd_socket->bstrerror());
    return false;
  }

  if (send_bsr
      && (!SendBootstrapFileToSd(jcr, sd_socket)
          || !response(jcr, sd_socket, OKbootstrap, "Bootstrap",
                       DISPLAY_ERROR))) {
    return false;
  }

  /* request sd to reply the secure erase cmd
   * or "*None*" if not set */
  if (!SendSecureEraseReqToSd(jcr)) {
    Dmsg1(400, "Unexpected %s Secure Erase Reply\n", "SD");
  }
  return true;
}

/** Start a thread to handle Storage daemon messages and
 *  Catalog requests.
 */
bool StartStorageDaemonMessageThread(JobControlRecord* jcr)
{
  int status;
  pthread_t thid;

  jcr->IncUseCount(); /* mark in use by msg thread */
  jcr->dir_impl->sd_msg_thread_done = false;
  jcr->dir_impl->SD_msg_chan_started = false;
  Dmsg0(100, "Start SD msg_thread.\n");
  if ((status = pthread_create(&thid, NULL, msg_thread, (void*)jcr)) != 0) {
    BErrNo be;
    Jmsg1(jcr, M_ABORT, 0, T_("Cannot create message thread: %s\n"),
          be.bstrerror(status));
  }
  /* Wait for thread to start */
  while (!jcr->dir_impl->SD_msg_chan_started) {
    Bmicrosleep(0, 50);
    if (jcr->IsJobCanceled() || jcr->dir_impl->sd_msg_thread_done) {
      return false;
    }
  }
  Dmsg1(100, "SD msg_thread started. use=%d\n", jcr->UseCount());
  return true;
}

extern "C" void MsgThreadCleanup(void* arg)
{
  JobControlRecord* jcr = (JobControlRecord*)arg;

  jcr->db->EndTransaction(jcr); /* Terminate any open transaction */

  {
    std::unique_lock l(jcr->mutex_guard());

    jcr->dir_impl->sd_msg_thread_done = true;
    jcr->dir_impl->SD_msg_chan_started = false;
  }

  pthread_cond_broadcast(
      &jcr->dir_impl->nextrun_ready);    /* wakeup any waiting threads */
  jcr->dir_impl->term_wait.notify_all(); /* wakeup any waiting threads */
  Dmsg2(100, "=== End msg_thread. JobId=%d usecnt=%d\n", jcr->JobId,
        jcr->UseCount());
  jcr->db->ThreadCleanup(); /* remove thread specific data */
  FreeJcr(jcr);             /* release jcr */
}

/** Handle the message channel (i.e. requests from the
 *  Storage daemon).
 * Note, we are running in a separate thread.
 */
extern "C" void* msg_thread(void* arg)
{
  JobControlRecord* jcr = (JobControlRecord*)arg;
  BareosSocket* sd;
  int JobStatus;
  int n;
  char auth_key[100];
  char Job[MAX_NAME_LENGTH];
  uint32_t JobFiles, JobErrors;
  uint64_t JobBytes;

  pthread_detach(pthread_self());
  SetJcrInThreadSpecificData(jcr);
  jcr->dir_impl->SD_msg_chan = pthread_self();
  jcr->dir_impl->SD_msg_chan_started = true;
  pthread_cleanup_push(MsgThreadCleanup, arg);
  sd = jcr->store_bsock;

  // Read the Storage daemon's output.
  Dmsg0(100, "Start msg_thread loop\n");
  n = 0;
  while (!jcr->IsJobCanceled() && (n = BgetDirmsg(sd)) >= 0) {
    Dmsg1(400, "<stored: %s", sd->msg);
    /* Check for "3000 OK Job Authorization="
     * Returned by a rerun cmd. */
    if (sscanf(sd->msg, OK_nextrun, &auth_key) == 1) {
      if (jcr->sd_auth_key) { free(jcr->sd_auth_key); }
      jcr->sd_auth_key = strdup(auth_key);
      pthread_cond_broadcast(
          &jcr->dir_impl->nextrun_ready); /* wakeup any waiting threads */
      continue;
    }

    // Check for "3010 Job <jobid> start"
    if (sscanf(sd->msg, Job_start, Job) == 1) { continue; }

    /* Check for "3099 Job <JobId> end JobStatus= JobFiles= JobBytes=
     * JobErrors=" */
    if (sscanf(sd->msg, Job_end, Job, &JobStatus, &JobFiles, &JobBytes,
               &JobErrors)
        == 5) {
      jcr->dir_impl->SDJobStatus = JobStatus; /* termination status */
      jcr->dir_impl->SDJobFiles = JobFiles;
      jcr->dir_impl->SDJobBytes = JobBytes;
      jcr->dir_impl->SDErrors = JobErrors;
      break;
    }
    Dmsg1(400, "end loop use=%d\n", jcr->UseCount());
  }
  if (n == BNET_HARDEOF) {
    /* A lost connection to the storage daemon is FATAL.
     * This is required, as otherwise
     * the job could failed to write data
     * but still end as JS_Warnings (OK -- with warnings). */
    Qmsg(jcr, M_FATAL, 0, T_("Director's comm line to SD dropped.\n"));
  }
  if (IsBnetError(sd)) { jcr->dir_impl->SDJobStatus = JS_ErrorTerminated; }
  pthread_cleanup_pop(1); /* remove and execute the handler */
  return NULL;
}

static void WaitForCanceledStorageDaemonTermination(
    JobControlRecord* jcr,
    std::unique_lock<std::mutex> l)
{
  /* we expect jcr to be locked by l */

  /* Give SD 30 seconds to clean up after cancel */
  auto timeout = std::chrono::system_clock::now() + std::chrono::seconds(30);

  while (!jcr->dir_impl->sd_msg_thread_done) {
    if (jcr->dir_impl->SD_msg_chan_started) {
      jcr->store_bsock->SetTimedOut();
      jcr->store_bsock->SetTerminated();
      SdMsgThreadSendSignal(jcr, TIMEOUT_SIGNAL);
    }

    if (jcr->dir_impl->term_wait.wait_until(l, timeout)
        == std::cv_status::timeout) {
      // we waited for 30 seconds so we just give up now
      break;
    }
  }
}

void WaitForStorageDaemonTermination(JobControlRecord* jcr)
{
  /* Now wait for Storage daemon to Terminate our message thread */
  {
    std::unique_lock l(jcr->mutex_guard());
    if (jcr->dir_impl->sd_msg_thread_done) { goto bail_out; }

    for (;;) {
      Dmsg0(400, "I'm waiting for message thread termination.\n");
      jcr->dir_impl->term_wait.wait_for(l, std::chrono::seconds(6));
      if (jcr->dir_impl->sd_msg_thread_done) {
        break;
      } else if (jcr->IsJobCanceled()) {
        WaitForCanceledStorageDaemonTermination(jcr, std::move(l));
        break;
      }
    }
  }

bail_out:
  jcr->setJobStatusWithPriorityCheck(JS_Terminated);
}

} /* namespace directordaemon */
