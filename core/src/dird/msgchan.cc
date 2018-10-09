/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, August MM
 */

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
#include "dird/msgchan.h"
#include "dird/quota.h"
#include "dird/sd_cmds.h"
#include "lib/bnet.h"
#include "lib/edit.h"

namespace directordaemon {

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Commands sent to Storage daemon */
static char jobcmd[] =
   "JobId=%s job=%s job_name=%s client_name=%s "
   "type=%d level=%d FileSet=%s NoAttr=%d SpoolAttr=%d FileSetMD5=%s "
   "SpoolData=%d PreferMountedVols=%d SpoolSize=%s "
   "rerunning=%d VolSessionId=%d VolSessionTime=%d Quota=%llu "
   "Protocol=%d BackupFormat=%s\n";
static char use_storage[] =
   "use storage=%s media_type=%s pool_name=%s "
   "pool_type=%s append=%d copy=%d stripe=%d\n";
static char use_device[] =
   "use device=%s\n";
//static char query_device[] =
//   "query device=%s";

/* Response from Storage daemon */
static char OKbootstrap[] =
   "3000 OK bootstrap\n";
static char OK_job[] =
   "3000 OK Job SDid=%d SDtime=%d Authorization=%100s\n";
static char OK_nextrun[] =
   "3000 OK Job Authorization=%100s\n";
static char OK_device[] =
   "3000 OK use device device=%s\n";

/* Storage Daemon requests */
static char Job_start[] =
   "3010 Job %127s start\n";
static char Job_end[] =
   "3099 Job %127s end JobStatus=%d JobFiles=%d JobBytes=%lld JobErrors=%u\n";

/* Forward referenced functions */
extern "C" void *msg_thread(void *arg);

/*
 * Here we ask the SD to send us the info for a
 *  particular device resource.
 */
#ifdef xxx
bool UpdateDeviceRes(JobControlRecord *jcr, DeviceResource *dev)
{
   PoolMem device_name;
   BareosSocket *sd;
   if (!ConnectToStorageDaemon(jcr, 5, 30, false)) {
      return false;
   }
   sd = jcr->store_bsock;
   PmStrcpy(device_name, dev->name());
   BashSpaces(device_name);
   sd->fsend(query_device, device_name.c_str());
   Dmsg1(100, ">stored: %s\n", sd->msg);
   /* The data is returned through Device_update */
   if (BgetDirmsg(sd) <= 0) {
      return false;
   }
   return true;
}
#endif

/** Send bootstrap file to Storage daemon.
 *  This is used for
 *    * restore
 *    * verify
 *    * VolumeToCatalog
 *    * migration and
 *    * copy Jobs
 */
static inline bool SendBootstrapFileToSd(JobControlRecord *jcr, BareosSocket *sd)
{
   FILE *bs;
   char buf[1000];
   const char *bootstrap = "bootstrap\n";

   Dmsg1(400, "SendBootstrapFileToSd: %s\n", jcr->RestoreBootstrap);
   if (!jcr->RestoreBootstrap) {
      return true;
   }
   bs = fopen(jcr->RestoreBootstrap, "rb");
   if (!bs) {
      BErrNo be;
      Jmsg(jcr, M_FATAL, 0, _("Could not open bootstrap file %s: ERR=%s\n"),
         jcr->RestoreBootstrap, be.bstrerror());
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }
   sd->fsend(bootstrap);
   while (fgets(buf, sizeof(buf), bs)) {
      sd->fsend("%s", buf);
   }
   sd->signal(BNET_EOD);
   fclose(bs);
   if (jcr->unlink_bsr) {
      SecureErase(jcr, jcr->RestoreBootstrap);
      jcr->unlink_bsr = false;
   }
   return true;
}

/** Start a job with the Storage daemon
 */
bool StartStorageDaemonJob(JobControlRecord *jcr, alist *rstore, alist *wstore, bool send_bsr)
{
   bool ok = true;
   StorageResource *storage = nullptr;
   char auth_key[100];
   const char *fileset_md5;
   PoolMem StoreName, device_name, pool_name, pool_type, media_type, backup_format;
   PoolMem job_name, client_name, fileset_name;
   int copy = 0;
   int stripe = 0;
   uint64_t remainingquota = 0;
   char ed1[30], ed2[30];
   BareosSocket *sd = jcr->store_bsock;

   /*
    * Before actually starting a new Job on the SD make sure we send any specific plugin options for this Job.
    */
   if (!SendStoragePluginOptions(jcr)) {
      Jmsg(jcr, M_FATAL, 0, _("Storage daemon rejected Plugin Options command: %s\n"), sd->msg);
      return false;
   }

   /*
    * Now send JobId and permissions, and get back the authorization key.
    */
   PmStrcpy(job_name, jcr->res.job->name());
   BashSpaces(job_name);

   if (jcr->res.client) {
      PmStrcpy(client_name, jcr->res.client->name());
   } else {
      PmStrcpy(client_name, "**None**");
   }
   BashSpaces(client_name);

   if (jcr->res.fileset) {
      PmStrcpy(fileset_name, jcr->res.fileset->name());
   } else {
      PmStrcpy(fileset_name, "**None**");
   }
   BashSpaces(fileset_name);

   PmStrcpy(backup_format, jcr->backup_format);
   BashSpaces(backup_format);

   if (jcr->res.fileset && jcr->res.fileset->MD5[0] == 0) {
      bstrncpy(jcr->res.fileset->MD5, "**Dummy**", sizeof(jcr->res.fileset->MD5));
      fileset_md5 = jcr->res.fileset->MD5;
   } else if (jcr->res.fileset) {
      fileset_md5 = jcr->res.fileset->MD5;
   } else {
      fileset_md5 = "**Dummy**";
   }

   /*
    * If rescheduling, cancel the previous incarnation of this job
    * with the SD, which might be waiting on the FD connection.
    * If we do not cancel it the SD will not accept a new connection
    * for the same jobid.
    */
   if (jcr->reschedule_count) {
      sd->fsend("cancel Job=%s\n", jcr->Job);
      while (sd->recv() >= 0) {
         continue;
      }
   }

   /*
    * Retrieve available quota 0 bytes means dont perform the check
    */
   remainingquota = FetchRemainingQuotas(jcr);
   Dmsg1(50,"Remainingquota: %llu\n", remainingquota);

   sd->fsend(jobcmd, edit_int64(jcr->JobId, ed1), jcr->Job,
             job_name.c_str(), client_name.c_str(),
             jcr->getJobType(), jcr->getJobLevel(),
             fileset_name.c_str(), !jcr->res.pool->catalog_files,
             jcr->res.job->SpoolAttributes, fileset_md5, jcr->spool_data,
             jcr->res.job->PreferMountedVolumes, edit_int64(jcr->spool_size, ed2),
             jcr->rerunning, jcr->VolSessionId, jcr->VolSessionTime, remainingquota,
             jcr->getJobProtocol(), backup_format.c_str());

   Dmsg1(100, ">stored: %s", sd->msg);
   if (BgetDirmsg(sd) > 0) {
      Dmsg1(100, "<stored: %s", sd->msg);
      if (sscanf(sd->msg, OK_job, &jcr->VolSessionId,
                 &jcr->VolSessionTime, &auth_key) != 3) {
         Dmsg1(100, "BadJob=%s\n", sd->msg);
         Jmsg(jcr, M_FATAL, 0, _("Storage daemon rejected Job command: %s\n"), sd->msg);
         return false;
      } else {
         BfreeAndNull(jcr->sd_auth_key);
         jcr->sd_auth_key = bstrdup(auth_key);
         Dmsg1(150, "sd_auth_key=%s\n", jcr->sd_auth_key);
      }
   } else {
      Jmsg(jcr, M_FATAL, 0, _("<stored: bad response to Job command: %s\n"),
         sd->bstrerror());
      return false;
   }

   if (send_bsr && (!SendBootstrapFileToSd(jcr, sd) ||
       !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR))) {
      return false;
   }

   /*
    * request sd to reply the secure erase cmd
    * or "*None*" if not set
    */
   if(!SendSecureEraseReqToSd(jcr)) {
      Dmsg1(400,"Unexpected %s Secure Erase Reply\n","SD");
   }

   /*
    * We have two loops here. The first comes from the
    *  Storage = associated with the Job, and we need
    *  to attach to each one.
    * The inner loop loops over all the alternative devices
    *  associated with each Storage. It selects the first
    *  available one.
    *
    */
   /* Do read side of storage daemon */
   if (ok && rstore) {
      /* For the moment, only migrate, copy and vbackup have rpool */
      if (jcr->is_JobType(JT_MIGRATE) || jcr->is_JobType(JT_COPY) ||
         (jcr->is_JobType(JT_BACKUP) && jcr->is_JobLevel(L_VIRTUAL_FULL))) {
         PmStrcpy(pool_type, jcr->res.rpool->pool_type);
         PmStrcpy(pool_name, jcr->res.rpool->name());
      } else {
         PmStrcpy(pool_type, jcr->res.pool->pool_type);
         PmStrcpy(pool_name, jcr->res.pool->name());
      }
      BashSpaces(pool_type);
      BashSpaces(pool_name);
      foreach_alist(storage, rstore) {
         Dmsg1(100, "Rstore=%s\n", storage->name());
         PmStrcpy(StoreName, storage->name());
         BashSpaces(StoreName);
         PmStrcpy(media_type, storage->media_type);
         BashSpaces(media_type);
         sd->fsend(use_storage, StoreName.c_str(), media_type.c_str(),
                   pool_name.c_str(), pool_type.c_str(), 0, copy, stripe);
         Dmsg1(100, "rstore >stored: %s", sd->msg);
         DeviceResource *dev = nullptr;
         /* Loop over alternative storage Devices until one is OK */
         foreach_alist(dev, storage->device) {
            PmStrcpy(device_name, dev->name());
            BashSpaces(device_name);
            sd->fsend(use_device, device_name.c_str());
            Dmsg1(100, ">stored: %s", sd->msg);
         }
         sd->signal(BNET_EOD);           /* end of Devices */
      }
      sd->signal(BNET_EOD);              /* end of Storages */
      if (BgetDirmsg(sd) > 0) {
         Dmsg1(100, "<stored: %s", sd->msg);
         /* ****FIXME**** save actual device name */
         ok = sscanf(sd->msg, OK_device, device_name.c_str()) == 1;
      } else {
         ok = false;
      }
      if (ok) {
         Jmsg(jcr, M_INFO, 0, _("Using Device \"%s\" to read.\n"), device_name.c_str());
      }
   }

   /* Do write side of storage daemon */
   if (ok && wstore) {
      PmStrcpy(pool_type, jcr->res.pool->pool_type);
      PmStrcpy(pool_name, jcr->res.pool->name());
      BashSpaces(pool_type);
      BashSpaces(pool_name);
      foreach_alist(storage, wstore) {
         PmStrcpy(StoreName, storage->name());
         BashSpaces(StoreName);
         PmStrcpy(media_type, storage->media_type);
         BashSpaces(media_type);
         sd->fsend(use_storage, StoreName.c_str(), media_type.c_str(),
                   pool_name.c_str(), pool_type.c_str(), 1, copy, stripe);

         Dmsg1(100, "wstore >stored: %s", sd->msg);
         DeviceResource *dev = nullptr;
         /* Loop over alternative storage Devices until one is OK */
         foreach_alist(dev, storage->device) {
            PmStrcpy(device_name, dev->name());
            BashSpaces(device_name);
            sd->fsend(use_device, device_name.c_str());
            Dmsg1(100, ">stored: %s", sd->msg);
         }
         sd->signal(BNET_EOD);           /* end of Devices */
      }
      sd->signal(BNET_EOD);              /* end of Storages */
      if (BgetDirmsg(sd) > 0) {
         Dmsg1(100, "<stored: %s", sd->msg);
         /* ****FIXME**** save actual device name */
         ok = sscanf(sd->msg, OK_device, device_name.c_str()) == 1;
      } else {
         ok = false;
      }
      if (ok) {
         Jmsg(jcr, M_INFO, 0, _("Using Device \"%s\" to write.\n"), device_name.c_str());
      }
   }
   if (!ok) {
      PoolMem err_msg;
      if (sd->msg[0]) {
         PmStrcpy(err_msg, sd->msg); /* save message */
         Jmsg(jcr, M_FATAL, 0, _("\n"
              "     Storage daemon didn't accept Device \"%s\" because:\n     %s"),
              device_name.c_str(), err_msg.c_str()/* sd->msg */);
      } else {
         Jmsg(jcr, M_FATAL, 0, _("\n"
              "     Storage daemon didn't accept Device \"%s\" command.\n"),
              device_name.c_str());
      }
   }
   return ok;
}

/** Start a thread to handle Storage daemon messages and
 *  Catalog requests.
 */
bool StartStorageDaemonMessageThread(JobControlRecord *jcr)
{
   int status;
   pthread_t thid;

   jcr->IncUseCount();              /* mark in use by msg thread */
   jcr->sd_msg_thread_done = false;
   jcr->SD_msg_chan_started = false;
   Dmsg0(100, "Start SD msg_thread.\n");
   if ((status = pthread_create(&thid, NULL, msg_thread, (void *)jcr)) != 0) {
      BErrNo be;
      Jmsg1(jcr, M_ABORT, 0, _("Cannot create message thread: %s\n"), be.bstrerror(status));
   }
   /* Wait for thread to start */
   while (!jcr->SD_msg_chan_started) {
      Bmicrosleep(0, 50);
      if (JobCanceled(jcr) || jcr->sd_msg_thread_done) {
         return false;
      }
   }
   Dmsg1(100, "SD msg_thread started. use=%d\n", jcr->UseCount());
   return true;
}

extern "C" void MsgThreadCleanup(void *arg)
{
   JobControlRecord *jcr = (JobControlRecord *)arg;

   jcr->db->EndTransaction(jcr);           /* Terminate any open transaction */
   jcr->lock();
   jcr->sd_msg_thread_done = true;
   jcr->SD_msg_chan_started = false;
   jcr->unlock();
   pthread_cond_broadcast(&jcr->nextrun_ready); /* wakeup any waiting threads */
   pthread_cond_broadcast(&jcr->term_wait); /* wakeup any waiting threads */
   Dmsg2(100, "=== End msg_thread. JobId=%d usecnt=%d\n", jcr->JobId, jcr->UseCount());
   jcr->db->ThreadCleanup();               /* remove thread specific data */
   FreeJcr(jcr);                           /* release jcr */
}

/** Handle the message channel (i.e. requests from the
 *  Storage daemon).
 * Note, we are running in a separate thread.
 */
extern "C" void *msg_thread(void *arg)
{
   JobControlRecord *jcr = (JobControlRecord *)arg;
   BareosSocket *sd;
   int JobStatus;
   int n;
   char auth_key[100];
   char Job[MAX_NAME_LENGTH];
   uint32_t JobFiles, JobErrors;
   uint64_t JobBytes;

   pthread_detach(pthread_self());
   SetJcrInTsd(jcr);
   jcr->SD_msg_chan = pthread_self();
   jcr->SD_msg_chan_started = true;
   pthread_cleanup_push(MsgThreadCleanup, arg);
   sd = jcr->store_bsock;

   /*
    * Read the Storage daemon's output.
    */
   Dmsg0(100, "Start msg_thread loop\n");
   n = 0;
   while (!JobCanceled(jcr) && (n=BgetDirmsg(sd)) >= 0) {
      Dmsg1(400, "<stored: %s", sd->msg);
      /*
       * Check for "3000 OK Job Authorization="
       * Returned by a rerun cmd.
       */
      if (sscanf(sd->msg, OK_nextrun, &auth_key) == 1) {
         if (jcr->sd_auth_key) {
            free(jcr->sd_auth_key);
         }
         jcr->sd_auth_key = bstrdup(auth_key);
         pthread_cond_broadcast(&jcr->nextrun_ready); /* wakeup any waiting threads */
         continue;
      }

      /*
       * Check for "3010 Job <jobid> start"
       */
      if (sscanf(sd->msg, Job_start, Job) == 1) {
         continue;
      }

      /*
       * Check for "3099 Job <JobId> end JobStatus= JobFiles= JobBytes= JobErrors="
       */
      if (sscanf(sd->msg, Job_end, Job, &JobStatus, &JobFiles,
                 &JobBytes, &JobErrors) == 5) {
         jcr->SDJobStatus = JobStatus; /* termination status */
         jcr->SDJobFiles = JobFiles;
         jcr->SDJobBytes = JobBytes;
         jcr->SDErrors = JobErrors;
         break;
      }
      Dmsg1(400, "end loop use=%d\n", jcr->UseCount());
   }
   if (n == BNET_HARDEOF) {
      /*
       * This probably should be M_FATAL, but I am not 100% sure
       *  that this return *always* corresponds to a dropped line.
       */
      Qmsg(jcr, M_ERROR, 0, _("Director's comm line to SD dropped.\n"));
   }
   if (IsBnetError(sd)) {
      jcr->SDJobStatus = JS_ErrorTerminated;
   }
   pthread_cleanup_pop(1);            /* remove and execute the handler */
   return NULL;
}

void WaitForStorageDaemonTermination(JobControlRecord *jcr)
{
   int cancel_count = 0;
   /* Now wait for Storage daemon to Terminate our message thread */
   while (!jcr->sd_msg_thread_done) {
      struct timeval tv;
      struct timezone tz;
      struct timespec timeout;

      gettimeofday(&tv, &tz);
      timeout.tv_nsec = 0;
      timeout.tv_sec = tv.tv_sec + 5; /* wait 5 seconds */
      Dmsg0(400, "I'm waiting for message thread termination.\n");
      P(mutex);
      pthread_cond_timedwait(&jcr->term_wait, &mutex, &timeout);
      V(mutex);
      if (jcr->IsCanceled()) {
         if (jcr->SD_msg_chan_started) {
            jcr->store_bsock->SetTimedOut();
            jcr->store_bsock->SetTerminated();
            SdMsgThreadSendSignal(jcr, TIMEOUT_SIGNAL);
         }
         cancel_count++;
      }
      /* Give SD 30 seconds to clean up after cancel */
      if (cancel_count == 6) {
         break;
      }
   }
   jcr->setJobStatus(JS_Terminated);
}

#ifdef needed
#define MAX_TRIES 30
#define WAIT_TIME 2
extern "C" void *device_thread(void *arg)
{
   int i;
   JobControlRecord *jcr;
   DeviceResource *dev;

   pthread_detach(pthread_self());
   jcr = new_control_jcr("*DeviceInit*", JT_SYSTEM);
   for (i=0; i < MAX_TRIES; i++) {
      if (!ConnectToStorageDaemon(jcr, 10, 30, true)) {
         Dmsg0(900, "Failed connecting to SD.\n");
         continue;
      }
      LockRes(this);
      foreach_res(dev, R_DEVICE) {
         if (!UpdateDeviceRes(jcr, dev)) {
            Dmsg1(900, "Error updating device=%s\n", dev->name());
         } else {
            Dmsg1(900, "Updated Device=%s\n", dev->name());
         }
      }
      UnlockRes(this);
      jcr->store_bsock->close();
      delete jcr->store_bsock;
      jcr->store_bsock = NULL;
      break;

   }
   FreeJcr(jcr);
   return NULL;
}

/** Start a thread to handle getting Device resource information
 *  from SD. This is called once at startup of the Director.
 */
void InitDeviceResources()
{
   int status;
   pthread_t thid;

   Dmsg0(100, "Start Device thread.\n");
   if ((status = pthread_create(&thid, NULL, device_thread, NULL)) != 0) {
      BErrNo be;
      Jmsg1(NULL, M_ABORT, 0, _("Cannot create message thread: %s\n"), be.bstrerror(status));
   }
}
#endif

} /* namespace directordaemon */
