/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, MM
/**
 * @file
 * Job control and execution for Storage Daemon
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/bsr.h"
#include "stored/acquire.h"
#include "stored/fd_cmds.h"
#include "stored/stored_jcr_impl.h"
#include "stored/ndmp_tape.h"
#include "stored/read_record.h"
#include "stored/stored_globals.h"
#include "lib/bsock.h"
#include "lib/edit.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/compression.h"
#include "include/jcr.h"
#include "include/protocol_types.h"

namespace storagedaemon {

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Requests from the Director daemon */
inline constexpr const char jobcmd[]
    = "JobId=%d job=%127s job_name=%127s client_name=%127s "
      "type=%d level=%d FileSet=%127s NoAttr=%d SpoolAttr=%d FileSetMD5=%127s "
      "SpoolData=%d PreferMountedVols=%d SpoolSize=%127s "
      "rerunning=%d VolSessionId=%d VolSessionTime=%d Quota=%llu "
      "Protocol=%d BackupFormat=%127s\n";

/* Responses sent to Director daemon */
inline constexpr const char OK_job[]
    = "3000 OK Job SDid=%u SDtime=%u Authorization=%s\n";
inline constexpr const char OK_nextrun[] = "3000 OK Job Authorization=%s\n";
inline constexpr const char BAD_job[]
    = "3915 Bad Job command. stat=%d CMD: %s\n";
inline constexpr const char Job_end[]
    = "3099 Job %s end JobStatus=%d JobFiles=%d JobBytes=%s JobErrors=%u\n";

/**
 * Director requests us to start a job
 * Basic tasks done here:
 *  - We pickup the JobId to be run from the Director.
 *  - We pickup the device, media, and pool from the Director
 *  - Wait for a connection from the File Daemon (FD)
 *  - Accept commands from the FD (i.e. run the job)
 *  - Return when the connection is terminated or
 *    there is an error.
 */
bool job_cmd(JobControlRecord* jcr)
{
  int32_t JobId;
  char auth_key[MAX_NAME_LENGTH];
  char spool_size[MAX_NAME_LENGTH];
  BareosSocket* dir = jcr->dir_bsock;
  PoolMem job_name, client_name, job, fileset_name, fileset_md5, backup_format;
  int32_t JobType, level, spool_attributes, no_attributes, spool_data;
  int32_t PreferMountedVols, rerunning, protocol;
  int status;
  uint64_t quota = 0;
  JobControlRecord* ojcr;

  // Get JobId and permissions from Director
  Dmsg1(100, "<dird: %s", dir->msg);
  bstrncpy(spool_size, "0", sizeof(spool_size));
  status = sscanf(dir->msg, jobcmd, &JobId, job.c_str(), job_name.c_str(),
                  client_name.c_str(), &JobType, &level, fileset_name.c_str(),
                  &no_attributes, &spool_attributes, fileset_md5.c_str(),
                  &spool_data, &PreferMountedVols, spool_size, &rerunning,
                  &jcr->VolSessionId, &jcr->VolSessionTime, &quota, &protocol,
                  backup_format.c_str());
  if (status != 19) {
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(BAD_job, status, jcr->errmsg);
    Dmsg1(100, ">dird: %s", dir->msg);
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    return false;
  }

  jcr->rerunning = (rerunning) ? true : false;
  jcr->setJobProtocol(protocol);

  Dmsg4(100, "rerunning=%d VolSesId=%d VolSesTime=%d Protocol=%d\n",
        jcr->rerunning, jcr->VolSessionId, jcr->VolSessionTime,
        jcr->getJobProtocol());
  /* Since this job could be rescheduled, we
   *  check to see if we have it already. If so
   *  free the old jcr and use the new one. */
  ojcr = get_jcr_by_full_name(job.c_str());
  if (ojcr && !ojcr->authenticated) {
    Dmsg2(100, "Found ojcr=0x%x Job %s\n", (unsigned)(intptr_t)ojcr,
          job.c_str());
    FreeJcr(ojcr);
  }
  jcr->JobId = JobId;
  Dmsg2(800, "Start JobId=%d %p\n", JobId, jcr);
  /* If job rescheduled because previous was incomplete,
   * the Resched flag is set and VolSessionId and VolSessionTime
   * are given to us (same as restarted job). */
  if (!jcr->rerunning) {
    jcr->VolSessionId = NewVolSessionId();
    jcr->VolSessionTime = vol_session_time;
  }
  bstrncpy(jcr->Job, job, sizeof(jcr->Job));
  UnbashSpaces(job_name);
  jcr->sd_impl->job_name = GetPoolMemory(PM_NAME);
  PmStrcpy(jcr->sd_impl->job_name, job_name);
  UnbashSpaces(client_name);
  jcr->client_name = GetPoolMemory(PM_NAME);
  PmStrcpy(jcr->client_name, client_name);
  UnbashSpaces(fileset_name);
  jcr->sd_impl->fileset_name = GetPoolMemory(PM_NAME);
  PmStrcpy(jcr->sd_impl->fileset_name, fileset_name);
  jcr->setJobType(JobType);
  jcr->setJobLevel(level);
  jcr->sd_impl->no_attributes = no_attributes;
  jcr->sd_impl->spool_attributes = spool_attributes;
  jcr->sd_impl->spool_data = spool_data;
  jcr->sd_impl->spool_size = str_to_int64(spool_size);
  jcr->sd_impl->fileset_md5 = GetPoolMemory(PM_NAME);
  PmStrcpy(jcr->sd_impl->fileset_md5, fileset_md5);
  jcr->sd_impl->PreferMountedVols = PreferMountedVols;
  jcr->sd_impl->RemainingQuota = quota;
  UnbashSpaces(backup_format);
  jcr->sd_impl->backup_format = GetPoolMemory(PM_NAME);
  PmStrcpy(jcr->sd_impl->backup_format, backup_format);
  jcr->authenticated = false;

  Dmsg1(50, "Quota set as %" PRIu64 "\n", quota);

  // Pass back an authorization key for the File daemon
  if (!MakeSessionKey(auth_key)) {
    Jmsg2(jcr, M_FATAL, 0, "Could not generate authentication key: %s.\n",
          auth_key);
    return false;
  }
  jcr->sd_auth_key = strdup(auth_key);
  dir->fsend(OK_job, jcr->VolSessionId, jcr->VolSessionTime, auth_key);
  memset(auth_key, 0, sizeof(auth_key));
  Dmsg2(50, ">dird jid=%u: %s", (uint32_t)jcr->JobId, dir->msg);

  DispatchNewPluginOptions(jcr);
  GeneratePluginEvent(jcr, bSdEventJobStart, (void*)"JobStart");

  return true;
}

static void WaitClient(JobControlRecord* jcr, utime_t wait_time)
{
  auto timeout
      = std::chrono::system_clock::now() + std::chrono::seconds(wait_time);
  auto locked = jcr->sd_impl->client_available.lock();

  locked.wait_until(jcr->sd_impl->job_start_wait, timeout, [jcr](bool started) {
    return started || jcr->IsJobCanceled();
  });
}

static void WaitFD(JobControlRecord* jcr)
{
  jcr->sendJobStatus(JS_WaitFD); /* wait for FD to connect */

  utime_t wait_time = [] {
    ResLocker _{my_config};
    return me->client_wait;
  }();

  if (wait_time == 0) {
    Dmsg3(100, "Client Connect Wait was set to 0; Setting to 1800s instead.\n");
    wait_time = 1800;
  }

  Dmsg3(50, "%s waiting %" PRId64 " sec for FD to contact SD key=%s\n",
        jcr->Job, wait_time, jcr->sd_auth_key);
  Dmsg2(800, "Wait FD for jid=%d %p\n", jcr->JobId, jcr);

  /* Wait for the File daemon to contact us to start the Job,
   * when he does, we will be released, unless the me->client_wait seconds
   * (default: 1800 seconds = 30 minutes) expires. */
  WaitClient(jcr, wait_time);
}

bool DoJobRun(JobControlRecord* jcr)
{
  WaitFD(jcr);

  Dmsg2(50, "Auth=%d canceled=%d\n", jcr->authenticated, jcr->IsJobCanceled());

  memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
  switch (jcr->getJobProtocol()) {
    case PT_NDMP_BAREOS:
      if (jcr->authenticated && !jcr->IsJobCanceled()) {
        Dmsg2(800, "Running jid=%d %p\n", jcr->JobId, jcr);

        /* Wait for the Job to finish. As we want exclusive access to
         * things like the connection to the director we suspend this
         * thread and let the actual NDMP connection wake us after it
         * has performed the backup. E.g. instead of doing a busy wait
         * we just hang on a conditional variable. */
        Dmsg2(800, "Wait for end job jid=%d %p\n", jcr->JobId, jcr);
        lock_mutex(mutex);
        pthread_cond_wait(&jcr->sd_impl->job_end_wait, &mutex);
        unlock_mutex(mutex);
      } else {
        Dmsg2(800, "Auth fail or cancel for jid=%d %p\n", jcr->JobId, jcr);
      }
      Dmsg2(800, "Done jid=%d %p\n", jcr->JobId, jcr);

      /* For a NDMP backup we expect the protocol to send us either a nextrun
       * cmd or a finish cmd to let us know they are finished. */
      return true;
    default:
      // Handle the file daemon session.
      if (jcr->authenticated && !jcr->IsJobCanceled()) {
        Dmsg2(800, "Running jid=%d %p\n", jcr->JobId, jcr);
        RunJob(jcr); /* Run the job */
      } else {
        Dmsg2(800, "Auth fail or cancel for jid=%d %p\n", jcr->JobId, jcr);
      }
      Dmsg2(800, "Done jid=%d %p\n", jcr->JobId, jcr);

      /* After a run cmd of a native backup we are done e.g.
       * return false. */
      return false;
  }
}

bool nextRunCmd(JobControlRecord* jcr)
{
  char auth_key[MAX_NAME_LENGTH];
  BareosSocket* dir = jcr->dir_bsock;

  switch (jcr->getJobProtocol()) {
    case PT_NDMP_BAREOS:
      /* We expect a next NDMP backup stream so clear the authenticated flag
       * and start waiting for the Next backup to Start. */
      jcr->authenticated = false;

      // Pass back a new authorization key for the File daemon
      if (!MakeSessionKey(auth_key)) {
        Jmsg1(jcr, M_FATAL, 0, "Could not generate authentication key: %s.\n",
              auth_key);
        return false;
      }
      if (jcr->sd_auth_key) { free(jcr->sd_auth_key); }
      jcr->sd_auth_key = strdup(auth_key);
      dir->fsend(OK_nextrun, auth_key);
      memset(auth_key, 0, sizeof(auth_key));
      Dmsg2(50, ">dird jid=%u: %s", (uint32_t)jcr->JobId, dir->msg);

      WaitFD(jcr);

      if (jcr->authenticated && !jcr->IsJobCanceled()) {
        Dmsg2(800, "Running jid=%d %p\n", jcr->JobId, jcr);

        /* Wait for the Job to finish. As we want exclusive access to
         * things like the connection to the director we suspend this
         * thread and let the actual NDMP connection wake us after it
         * has performed the backup. E.g. instead of doing a busy wait
         * we just hang on a conditional variable. */
        Dmsg2(800, "Wait for end job jid=%d %p\n", jcr->JobId, jcr);
        lock_mutex(mutex);
        pthread_cond_wait(&jcr->sd_impl->job_end_wait, &mutex);
        unlock_mutex(mutex);
      } else {
        Dmsg2(800, "Auth fail or cancel for jid=%d %p\n", jcr->JobId, jcr);
      }
      Dmsg2(800, "Done jid=%d %p\n", jcr->JobId, jcr);

      /* For a NDMP backup we expect the protocol to send us either a nextrun
       * cmd or a finish cmd to let us know they are finished. */
      return true;
    default:
      Dmsg1(200, "NextRunCmd: %s", jcr->dir_bsock->msg);
      Jmsg2(
          jcr, M_FATAL, 0,
          T_("Hey!!!! JobId %u Job %s tries to use nextrun cmd while not part "
             "of protocol.\n"),
          (uint32_t)jcr->JobId, jcr->Job);
      return false;
  }
}

bool FinishCmd(JobControlRecord* jcr)
{
  BareosSocket* dir = jcr->dir_bsock;
  char ec1[30];

  /* See if the Job has a certain protocol. Some protocols allow the
   * finish cmd some do not (Native backup for example does NOT) */
  switch (jcr->getJobProtocol()) {
    case PT_NDMP_BAREOS:
      Dmsg1(200, "Finish_cmd: %s", jcr->dir_bsock->msg);

      jcr->end_time = time(NULL);
      DequeueMessages(jcr); /* send any queued messages */
      jcr->setJobStatusWithPriorityCheck(JS_Terminated);

      switch (jcr->getJobType()) {
        case JT_BACKUP:
          EndOfNdmpBackup(jcr);
          break;
        case JT_RESTORE:
          EndOfNdmpRestore(jcr);
          break;
        default:
          break;
      }

      GeneratePluginEvent(jcr, bSdEventJobEnd);

      dir->fsend(Job_end, jcr->Job, jcr->getJobStatus(), jcr->JobFiles,
                 edit_uint64(jcr->JobBytes, ec1), jcr->JobErrors);
      dir->signal(BNET_EOD); /* send EOD to Director daemon */

      FreePlugins(jcr); /* release instantiated plugins */

      Dmsg2(800, "Done jid=%d %p\n", jcr->JobId, jcr);

      return false; /* Continue DIR session ? */
    default:
      Dmsg1(200, "Finish_cmd: %s", jcr->dir_bsock->msg);
      Jmsg2(jcr, M_FATAL, 0,
            T_("Hey!!!! JobId %u Job %s tries to use finish cmd while not part "
               "of protocol.\n"),
            (uint32_t)jcr->JobId, jcr->Job);
      return false; /* Continue DIR session ? */
  }
}

/**
 * Destroy the Job Control Record and associated
 * resources (sockets).
 */
void StoredFreeJcr(JobControlRecord* jcr)
{
  Dmsg0(200, "Start stored FreeJcr\n");
  Dmsg2(800, "End Job JobId=%u %p\n", jcr->JobId, jcr);

  if (jcr->dir_bsock) {
    Dmsg2(800, "Send Terminate jid=%d %p\n", jcr->JobId, jcr);
    jcr->dir_bsock->signal(BNET_EOD);
    jcr->dir_bsock->signal(BNET_TERMINATE);
  }

  if (jcr->store_bsock) {
    jcr->store_bsock->close();
    delete jcr->store_bsock;
    jcr->store_bsock = NULL;
  }

  if (jcr->file_bsock) {
    jcr->file_bsock->close();
    delete jcr->file_bsock;
    jcr->file_bsock = NULL;
  }

  if (jcr->sd_impl->job_name) { FreePoolMemory(jcr->sd_impl->job_name); }

  if (jcr->client_name) {
    FreeMemory(jcr->client_name);
    jcr->client_name = NULL;
  }

  if (jcr->sd_impl->fileset_name) { FreeMemory(jcr->sd_impl->fileset_name); }

  if (jcr->sd_impl->fileset_md5) { FreeMemory(jcr->sd_impl->fileset_md5); }

  if (jcr->sd_impl->backup_format) { FreeMemory(jcr->sd_impl->backup_format); }

  if (jcr->sd_impl->read_session.bsr) {
    libbareos::FreeBsr(jcr->sd_impl->read_session.bsr);
    jcr->sd_impl->read_session.bsr = NULL;
  }

  if (jcr->sd_impl->read_session.rctx) {
    FreeReadContext(jcr->sd_impl->read_session.rctx);
    jcr->sd_impl->read_session.rctx = NULL;
  }

  if (jcr->compress.deflate_buffer || jcr->compress.inflate_buffer) {
    CleanupCompression(jcr);
  }

  // Free any restore volume list created
  FreeRestoreVolumeList(jcr);
  if (jcr->RestoreBootstrap) {
    SecureErase(jcr, jcr->RestoreBootstrap);
    FreePoolMemory(jcr->RestoreBootstrap);
    jcr->RestoreBootstrap = NULL;
  }

  if (jcr->sd_impl->next_dev || jcr->sd_impl->prev_dev) {
    Emsg0(M_FATAL, 0, T_("In FreeJcr(), but still attached to device!!!!\n"));
  }

  pthread_cond_destroy(&jcr->sd_impl->job_end_wait);

  // Avoid a double free
  if (jcr->sd_impl->dcr == jcr->sd_impl->read_dcr) {
    jcr->sd_impl->read_dcr = NULL;
  }

  if (jcr->sd_impl->dcr) {
    FreeDeviceControlRecord(jcr->sd_impl->dcr);
    jcr->sd_impl->dcr = NULL;
  }

  if (jcr->sd_impl->read_dcr) {
    FreeDeviceControlRecord(jcr->sd_impl->read_dcr);
    jcr->sd_impl->read_dcr = NULL;
  }

  if (jcr->sd_impl->plugin_options) { delete jcr->sd_impl->plugin_options; }

  FreePlugins(jcr); /* release instantiated plugins */

  if (jcr->JobId != 0) {
    WriteStateFile(me->working_directory, "bareos-sd",
                   GetFirstPortHostOrder(me->SDaddrs));
  }

  if (jcr->sd_impl) {
    delete jcr->sd_impl;
    jcr->sd_impl = nullptr;
  }

  Dmsg0(200, "End stored FreeJcr\n");

  return;
}

JobControlRecord* NewStoredJcr()
{
  JobControlRecord* jcr = new_jcr(StoredFreeJcr);
  jcr->sd_impl = new StoredJcrImpl;
  register_jcr(jcr);
  return jcr;
}

} /* namespace storagedaemon */
