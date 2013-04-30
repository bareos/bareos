/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Bacula Director -- msgchan.c -- handles the message channel
 *    to the Storage daemon and the File daemon.
 *
 *     Kern Sibbald, August MM
 *
 *    This routine runs as a thread and must be thread reentrant.
 *
 *  Basic tasks done here:
 *    Open a message channel with the Storage daemon
 *      to authenticate ourself and to pass the JobId.
 *    Create a thread to interact with the Storage daemon
 *      who returns a job status and requests Catalog services, etc.
 *
 */

#include "bacula.h"
#include "dird.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Commands sent to Storage daemon */
static char jobcmd[]     = "JobId=%s job=%s job_name=%s client_name=%s "
   "type=%d level=%d FileSet=%s NoAttr=%d SpoolAttr=%d FileSetMD5=%s "
   "SpoolData=%d WritePartAfterJob=%d PreferMountedVols=%d SpoolSize=%s "
   "rerunning=%d VolSessionId=%d VolSessionTime=%d\n";
static char use_storage[] = "use storage=%s media_type=%s pool_name=%s "
   "pool_type=%s append=%d copy=%d stripe=%d\n";
static char use_device[] = "use device=%s\n";
//static char query_device[] = _("query device=%s");

/* Response from Storage daemon */
static char OKjob[]      = "3000 OK Job SDid=%d SDtime=%d Authorization=%100s\n";
static char OK_device[]  = "3000 OK use device device=%s\n";

/* Storage Daemon requests */
static char Job_start[]  = "3010 Job %127s start\n";
static char Job_end[]    =
   "3099 Job %127s end JobStatus=%d JobFiles=%d JobBytes=%lld JobErrors=%u\n";

/* Forward referenced functions */
extern "C" void *msg_thread(void *arg);

/*
 * Establish a message channel connection with the Storage daemon
 * and perform authentication.
 */
bool connect_to_storage_daemon(JCR *jcr, int retry_interval,
                              int max_retry_time, int verbose)
{
   BSOCK *sd = new_bsock();
   STORE *store;
   utime_t heart_beat;    

   if (jcr->store_bsock) {
      return true;                    /* already connected */
   }

   /* If there is a write storage use it */
   if (jcr->wstore) {
      store = jcr->wstore;
   } else {
      store = jcr->rstore;
   }

   if (store->heartbeat_interval) {
      heart_beat = store->heartbeat_interval;
   } else {           
      heart_beat = director->heartbeat_interval;
   }

   /*
    *  Open message channel with the Storage daemon
    */
   Dmsg2(100, "bnet_connect to Storage daemon %s:%d\n", store->address,
      store->SDport);
   sd->set_source_address(director->DIRsrc_addr);
   if (!sd->connect(jcr, retry_interval, max_retry_time, heart_beat, _("Storage daemon"),
         store->address, NULL, store->SDport, verbose)) {
      sd->destroy();
      sd = NULL;
   }

   if (sd == NULL) {
      return false;
   }
   sd->res = (RES *)store;        /* save pointer to other end */
   jcr->store_bsock = sd;

   if (!authenticate_storage_daemon(jcr, store)) {
      sd->close();
      jcr->store_bsock = NULL;
      return false;
   }
   return true;
}

/*
 * Here we ask the SD to send us the info for a 
 *  particular device resource.
 */
#ifdef xxx
bool update_device_res(JCR *jcr, DEVICE *dev)
{
   POOL_MEM device_name; 
   BSOCK *sd;
   if (!connect_to_storage_daemon(jcr, 5, 30, 0)) {
      return false;
   }
   sd = jcr->store_bsock;
   pm_strcpy(device_name, dev->name());
   bash_spaces(device_name);
   sd->fsend(query_device, device_name.c_str());
   Dmsg1(100, ">stored: %s\n", sd->msg);
   /* The data is returned through Device_update */
   if (bget_dirmsg(sd) <= 0) {
      return false;
   }
   return true;
}
#endif

static char OKbootstrap[] = "3000 OK bootstrap\n";

/*
 * Start a job with the Storage daemon
 */
bool start_storage_daemon_job(JCR *jcr, alist *rstore, alist *wstore, bool send_bsr)
{
   bool ok = true;
   STORE *storage;
   BSOCK *sd;
   char auth_key[100];
   POOL_MEM store_name, device_name, pool_name, pool_type, media_type;
   POOL_MEM job_name, client_name, fileset_name;
   int copy = 0;
   int stripe = 0;
   char ed1[30], ed2[30];

   sd = jcr->store_bsock;
   /*
    * Now send JobId and permissions, and get back the authorization key.
    */
   pm_strcpy(job_name, jcr->job->name());
   bash_spaces(job_name);
   pm_strcpy(client_name, jcr->client->name());
   bash_spaces(client_name);
   pm_strcpy(fileset_name, jcr->fileset->name());
   bash_spaces(fileset_name);
   if (jcr->fileset->MD5[0] == 0) {
      bstrncpy(jcr->fileset->MD5, "**Dummy**", sizeof(jcr->fileset->MD5));
   }
   /* If rescheduling, cancel the previous incarnation of this job
    *  with the SD, which might be waiting on the FD connection.
    *  If we do not cancel it the SD will not accept a new connection
    *  for the same jobid.
    */
   if (jcr->reschedule_count) {
      sd->fsend("cancel Job=%s\n", jcr->Job);
      while (sd->recv() >= 0)
         { }
   } 
   sd->fsend(jobcmd, edit_int64(jcr->JobId, ed1), jcr->Job, 
             job_name.c_str(), client_name.c_str(), 
             jcr->getJobType(), jcr->getJobLevel(),
             fileset_name.c_str(), !jcr->pool->catalog_files,
             jcr->job->SpoolAttributes, jcr->fileset->MD5, jcr->spool_data, 
             jcr->write_part_after_job, jcr->job->PreferMountedVolumes,
             edit_int64(jcr->spool_size, ed2), jcr->rerunning,
             jcr->VolSessionId, jcr->VolSessionTime);
   Dmsg1(100, ">stored: %s", sd->msg);
   if (bget_dirmsg(sd) > 0) {
       Dmsg1(100, "<stored: %s", sd->msg);
       if (sscanf(sd->msg, OKjob, &jcr->VolSessionId,
                  &jcr->VolSessionTime, &auth_key) != 3) {
          Dmsg1(100, "BadJob=%s\n", sd->msg);
          Jmsg(jcr, M_FATAL, 0, _("Storage daemon rejected Job command: %s\n"), sd->msg);
          return false;
       } else {
          bfree_and_null(jcr->sd_auth_key);
          jcr->sd_auth_key = bstrdup(auth_key);
          Dmsg1(150, "sd_auth_key=%s\n", jcr->sd_auth_key);
       }
   } else {
      Jmsg(jcr, M_FATAL, 0, _("<stored: bad response to Job command: %s\n"),
         sd->bstrerror());
      return false;
   }

   if (send_bsr && (!send_bootstrap_file(jcr, sd) ||
       !response(jcr, sd, OKbootstrap, "Bootstrap", DISPLAY_ERROR))) {
      return false;
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
         pm_strcpy(pool_type, jcr->rpool->pool_type);
         pm_strcpy(pool_name, jcr->rpool->name());
      } else {
         pm_strcpy(pool_type, jcr->pool->pool_type);
         pm_strcpy(pool_name, jcr->pool->name());
      }
      bash_spaces(pool_type);
      bash_spaces(pool_name);
      foreach_alist(storage, rstore) {
         Dmsg1(100, "Rstore=%s\n", storage->name());
         pm_strcpy(store_name, storage->name());
         bash_spaces(store_name);
         pm_strcpy(media_type, storage->media_type);
         bash_spaces(media_type);
         sd->fsend(use_storage, store_name.c_str(), media_type.c_str(), 
                   pool_name.c_str(), pool_type.c_str(), 0, copy, stripe);
         Dmsg1(100, "rstore >stored: %s", sd->msg);
         DEVICE *dev;
         /* Loop over alternative storage Devices until one is OK */
         foreach_alist(dev, storage->device) {
            pm_strcpy(device_name, dev->name());
            bash_spaces(device_name);
            sd->fsend(use_device, device_name.c_str());
            Dmsg1(100, ">stored: %s", sd->msg);
         }
         sd->signal(BNET_EOD);           /* end of Devices */
      }
      sd->signal(BNET_EOD);              /* end of Storages */
      if (bget_dirmsg(sd) > 0) {
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
      pm_strcpy(pool_type, jcr->pool->pool_type);
      pm_strcpy(pool_name, jcr->pool->name());
      bash_spaces(pool_type);
      bash_spaces(pool_name);
      foreach_alist(storage, wstore) {
         pm_strcpy(store_name, storage->name());
         bash_spaces(store_name);
         pm_strcpy(media_type, storage->media_type);
         bash_spaces(media_type);
         sd->fsend(use_storage, store_name.c_str(), media_type.c_str(), 
                   pool_name.c_str(), pool_type.c_str(), 1, copy, stripe);

         Dmsg1(100, "wstore >stored: %s", sd->msg);
         DEVICE *dev;
         /* Loop over alternative storage Devices until one is OK */
         foreach_alist(dev, storage->device) {
            pm_strcpy(device_name, dev->name());
            bash_spaces(device_name);
            sd->fsend(use_device, device_name.c_str());
            Dmsg1(100, ">stored: %s", sd->msg);
         }
         sd->signal(BNET_EOD);           /* end of Devices */
      }
      sd->signal(BNET_EOD);              /* end of Storages */
      if (bget_dirmsg(sd) > 0) {
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
      POOL_MEM err_msg;
      if (sd->msg[0]) {
         pm_strcpy(err_msg, sd->msg); /* save message */
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

/*
 * Start a thread to handle Storage daemon messages and
 *  Catalog requests.
 */
bool start_storage_daemon_message_thread(JCR *jcr)
{
   int status;
   pthread_t thid;

   jcr->inc_use_count();              /* mark in use by msg thread */
   jcr->sd_msg_thread_done = false;
   jcr->SD_msg_chan = 0;
   Dmsg0(100, "Start SD msg_thread.\n");
   if ((status=pthread_create(&thid, NULL, msg_thread, (void *)jcr)) != 0) {
      berrno be;
      Jmsg1(jcr, M_ABORT, 0, _("Cannot create message thread: %s\n"), be.bstrerror(status));
   }
   /* Wait for thread to start */
   while (jcr->SD_msg_chan == 0) {
      bmicrosleep(0, 50);
      if (job_canceled(jcr) || jcr->sd_msg_thread_done) {
         return false;
      }
   }
   Dmsg1(100, "SD msg_thread started. use=%d\n", jcr->use_count());
   return true;
}

extern "C" void msg_thread_cleanup(void *arg)
{
   JCR *jcr = (JCR *)arg;
   db_end_transaction(jcr, jcr->db);        /* terminate any open transaction */
   jcr->lock();
   jcr->sd_msg_thread_done = true;
   jcr->SD_msg_chan = 0;
   jcr->unlock();
   pthread_cond_broadcast(&jcr->term_wait); /* wakeup any waiting threads */
   Dmsg2(100, "=== End msg_thread. JobId=%d usecnt=%d\n", jcr->JobId, jcr->use_count());
   db_thread_cleanup(jcr->db);              /* remove thread specific data */
   free_jcr(jcr);                           /* release jcr */
}

/*
 * Handle the message channel (i.e. requests from the
 *  Storage daemon).
 * Note, we are running in a separate thread.
 */
extern "C" void *msg_thread(void *arg)
{
   JCR *jcr = (JCR *)arg;
   BSOCK *sd;
   int JobStatus;
   int n;
   char Job[MAX_NAME_LENGTH];
   uint32_t JobFiles, JobErrors;
   uint64_t JobBytes;

   pthread_detach(pthread_self());
   set_jcr_in_tsd(jcr);
   jcr->SD_msg_chan = pthread_self();
   pthread_cleanup_push(msg_thread_cleanup, arg);
   sd = jcr->store_bsock;

   /* Read the Storage daemon's output.
    */
   Dmsg0(100, "Start msg_thread loop\n");
   n = 0;
   while (!job_canceled(jcr) && (n=bget_dirmsg(sd)) >= 0) {
      Dmsg1(400, "<stored: %s", sd->msg);
      if (sscanf(sd->msg, Job_start, Job) == 1) {
         continue;
      }
      if (sscanf(sd->msg, Job_end, Job, &JobStatus, &JobFiles,
                 &JobBytes, &JobErrors) == 5) {
         jcr->SDJobStatus = JobStatus; /* termination status */
         jcr->SDJobFiles = JobFiles;
         jcr->SDJobBytes = JobBytes;
         jcr->SDErrors = JobErrors;
         break;
      }
      Dmsg1(400, "end loop use=%d\n", jcr->use_count());
   }
   if (n == BNET_HARDEOF) {
      /*
       * This probably should be M_FATAL, but I am not 100% sure
       *  that this return *always* corresponds to a dropped line.
       */
      Qmsg(jcr, M_ERROR, 0, _("Director's comm line to SD dropped.\n"));
   }
   if (is_bnet_error(sd)) {
      jcr->SDJobStatus = JS_ErrorTerminated;
   }
   pthread_cleanup_pop(1);            /* remove and execute the handler */
   return NULL;
}

void wait_for_storage_daemon_termination(JCR *jcr)
{
   int cancel_count = 0;
   /* Now wait for Storage daemon to terminate our message thread */
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
      if (jcr->is_canceled()) {
         if (jcr->SD_msg_chan) {
            jcr->store_bsock->set_timed_out();
            jcr->store_bsock->set_terminated();
            sd_msg_thread_send_signal(jcr, TIMEOUT_SIGNAL);
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

/*
 * Send bootstrap file to Storage daemon.
 *  This is used for restore, verify VolumeToCatalog, migration,
 *    and copy Jobs.
 */
bool send_bootstrap_file(JCR *jcr, BSOCK *sd)
{
   FILE *bs;
   char buf[1000];
   const char *bootstrap = "bootstrap\n";

   Dmsg1(400, "send_bootstrap_file: %s\n", jcr->RestoreBootstrap);
   if (!jcr->RestoreBootstrap) {
      return true;
   }
   bs = fopen(jcr->RestoreBootstrap, "rb");
   if (!bs) {
      berrno be;
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
      unlink(jcr->RestoreBootstrap);
      jcr->unlink_bsr = false;
   }                         
   return true;
}


#ifdef needed
#define MAX_TRIES 30
#define WAIT_TIME 2
extern "C" void *device_thread(void *arg)
{
   int i;
   JCR *jcr;
   DEVICE *dev;


   pthread_detach(pthread_self());
   jcr = new_control_jcr("*DeviceInit*", JT_SYSTEM);
   for (i=0; i < MAX_TRIES; i++) {
      if (!connect_to_storage_daemon(jcr, 10, 30, 1)) {
         Dmsg0(900, "Failed connecting to SD.\n");
         continue;
      }
      LockRes();
      foreach_res(dev, R_DEVICE) {
         if (!update_device_res(jcr, dev)) {
            Dmsg1(900, "Error updating device=%s\n", dev->name());
         } else {
            Dmsg1(900, "Updated Device=%s\n", dev->name());
         }
      }
      UnlockRes();
      bnet_close(jcr->store_bsock);
      jcr->store_bsock = NULL;
      break;

   }
   free_jcr(jcr);
   return NULL;
}

/*
 * Start a thread to handle getting Device resource information
 *  from SD. This is called once at startup of the Director.
 */
void init_device_resources()
{
   int status;
   pthread_t thid;

   Dmsg0(100, "Start Device thread.\n");
   if ((status=pthread_create(&thid, NULL, device_thread, NULL)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("Cannot create message thread: %s\n"), be.bstrerror(status));
   }
}
#endif
