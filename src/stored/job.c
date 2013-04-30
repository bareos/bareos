/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
 *   Job control and execution for Storage Daemon
 *
 *   Kern Sibbald, MM
 *
 */

#include "bacula.h"
#include "stored.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Imported variables */
extern uint32_t VolSessionTime;

/* Imported functions */
extern uint32_t newVolSessionId();
extern bool do_mac(JCR *jcr);

/* Requests from the Director daemon */
/* Added in 3.1.4 14Sep09 KES */
static char jobcmd[] = "JobId=%d job=%127s job_name=%127s client_name=%127s "
      "type=%d level=%d FileSet=%127s NoAttr=%d SpoolAttr=%d FileSetMD5=%127s "
      "SpoolData=%d WritePartAfterJob=%d PreferMountedVols=%d SpoolSize=%s "
      "rerunning=%d VolSessionId=%d VolSessionTime=%d\n";

/* Responses sent to Director daemon */
static char OKjob[]     = "3000 OK Job SDid=%u SDtime=%u Authorization=%s\n";
static char BAD_job[]   = "3915 Bad Job command. stat=%d CMD: %s\n";

/*
 * Director requests us to start a job
 * Basic tasks done here:
 *  - We pickup the JobId to be run from the Director.
 *  - We pickup the device, media, and pool from the Director
 *  - Wait for a connection from the File Daemon (FD)
 *  - Accept commands from the FD (i.e. run the job)
 *  - Return when the connection is terminated or
 *    there is an error.
 */
bool job_cmd(JCR *jcr)
{
   int32_t JobId;
   char auth_key[100];
   char spool_size[30];
   char seed[100];
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM job_name, client_name, job, fileset_name, fileset_md5;
   int32_t JobType, level, spool_attributes, no_attributes, spool_data;
   int32_t write_part_after_job, PreferMountedVols, rerunning;
   int stat;
   JCR *ojcr;

   /*
    * Get JobId and permissions from Director
    */
   Dmsg1(100, "<dird: %s", dir->msg);
   bstrncpy(spool_size, "0", sizeof(spool_size));
   stat = sscanf(dir->msg, jobcmd, &JobId, job.c_str(), job_name.c_str(),
              client_name.c_str(),
              &JobType, &level, fileset_name.c_str(), &no_attributes,
              &spool_attributes, fileset_md5.c_str(), &spool_data,
              &write_part_after_job, &PreferMountedVols, spool_size,
              &rerunning, &jcr->VolSessionId, &jcr->VolSessionTime);
   if (stat != 17) {
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(BAD_job, stat, jcr->errmsg);
      Dmsg1(100, ">dird: %s", dir->msg);
      jcr->setJobStatus(JS_ErrorTerminated);
      return false;
   }
   jcr->rerunning = (rerunning) ? true : false;
   Dmsg3(100, "rerunning=%d VolSesId=%d VolSesTime=%d\n", jcr->rerunning,
         jcr->VolSessionId, jcr->VolSessionTime);
   /*
    * Since this job could be rescheduled, we
    *  check to see if we have it already. If so
    *  free the old jcr and use the new one.
    */
   ojcr = get_jcr_by_full_name(job.c_str());
   if (ojcr && !ojcr->authenticated) {
      Dmsg2(100, "Found ojcr=0x%x Job %s\n", (unsigned)(intptr_t)ojcr, job.c_str());
      free_jcr(ojcr);
   }
   jcr->JobId = JobId;
   Dmsg2(800, "Start JobId=%d %p\n", JobId, jcr);
   set_jcr_in_tsd(jcr);

   /*
    * If job rescheduled because previous was incomplete,
    * the Resched flag is set and VolSessionId and VolSessionTime
    * are given to us (same as restarted job).
    */
   if (!jcr->rerunning) {
      jcr->VolSessionId = newVolSessionId();
      jcr->VolSessionTime = VolSessionTime;
   }
   bstrncpy(jcr->Job, job, sizeof(jcr->Job));
   unbash_spaces(job_name);
   jcr->job_name = get_pool_memory(PM_NAME);
   pm_strcpy(jcr->job_name, job_name);
   unbash_spaces(client_name);
   jcr->client_name = get_pool_memory(PM_NAME);
   pm_strcpy(jcr->client_name, client_name);
   unbash_spaces(fileset_name);
   jcr->fileset_name = get_pool_memory(PM_NAME);
   pm_strcpy(jcr->fileset_name, fileset_name);
   jcr->setJobType(JobType);
   jcr->setJobLevel(level);
   jcr->no_attributes = no_attributes;
   jcr->spool_attributes = spool_attributes;
   jcr->spool_data = spool_data;
   jcr->spool_size = str_to_int64(spool_size);
   jcr->write_part_after_job = write_part_after_job;
   jcr->fileset_md5 = get_pool_memory(PM_NAME);
   pm_strcpy(jcr->fileset_md5, fileset_md5);
   jcr->PreferMountedVols = PreferMountedVols;


   jcr->authenticated = false;

   /*
    * Pass back an authorization key for the File daemon
    */
   bsnprintf(seed, sizeof(seed), "%p%d", jcr, JobId);
   make_session_key(auth_key, seed, 1);
   dir->fsend(OKjob, jcr->VolSessionId, jcr->VolSessionTime, auth_key);
   Dmsg2(50, ">dird jid=%u: %s", (uint32_t)jcr->JobId, dir->msg);
   jcr->sd_auth_key = bstrdup(auth_key);
   memset(auth_key, 0, sizeof(auth_key));
   new_plugins(jcr);            /* instantiate the plugins */
   generate_daemon_event(jcr, "JobStart");
   generate_plugin_event(jcr, bsdEventJobStart, (void *)"JobStart");
   return true;
}

bool run_cmd(JCR *jcr)
{
   struct timeval tv;
   struct timezone tz;
   struct timespec timeout;
   int errstat = 0;

   Dsm_check(200);
   Dmsg1(200, "Run_cmd: %s\n", jcr->dir_bsock->msg);

   /* If we do not need the FD, we are doing a migrate, copy, or virtual
    *   backup.
    */
   if (jcr->no_client_used()) {
      do_mac(jcr);
      return false;
   }

   jcr->sendJobStatus(JS_WaitFD);          /* wait for FD to connect */

   gettimeofday(&tv, &tz);
   timeout.tv_nsec = tv.tv_usec * 1000;
   timeout.tv_sec = tv.tv_sec + me->client_wait;

   Dmsg3(50, "%s waiting %d sec for FD to contact SD key=%s\n",
         jcr->Job, (int)(timeout.tv_sec-time(NULL)), jcr->sd_auth_key);
   Dmsg2(800, "Wait FD for jid=%d %p\n", jcr->JobId, jcr);

   /*
    * Wait for the File daemon to contact us to start the Job,
    *  when he does, we will be released, unless the 30 minutes
    *  expires.
    */
   P(mutex);
   while ( !jcr->authenticated && !job_canceled(jcr) ) {
      errstat = pthread_cond_timedwait(&jcr->job_start_wait, &mutex, &timeout);
      if (errstat == ETIMEDOUT || errstat == EINVAL || errstat == EPERM) {
         break;
      }
      Dmsg1(800, "=== Auth cond errstat=%d\n", errstat);
   }
   Dmsg3(50, "Auth=%d canceled=%d errstat=%d\n", jcr->authenticated,
      job_canceled(jcr), errstat);
   V(mutex);
   Dmsg2(800, "Auth fail or cancel for jid=%d %p\n", jcr->JobId, jcr);

   memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));

   if (jcr->authenticated && !job_canceled(jcr)) {
      Dmsg2(800, "Running jid=%d %p\n", jcr->JobId, jcr);
      run_job(jcr);                   /* Run the job */
   }
   Dmsg2(800, "Done jid=%d %p\n", jcr->JobId, jcr);
   return false;
}

/*
 * After receiving a connection (in dircmd.c) if it is
 *   from the File daemon, this routine is called.
 */
void handle_filed_connection(BSOCK *fd, char *job_name)
{
   JCR *jcr;

/*
 * With the following bmicrosleep on, running the 
 * SD under the debugger fails.   
 */ 
// bmicrosleep(0, 50000);             /* wait 50 millisecs */
   if (!(jcr=get_jcr_by_full_name(job_name))) {
      Jmsg1(NULL, M_FATAL, 0, _("FD connect failed: Job name not found: %s\n"), job_name);
      Dmsg1(3, "**** Job \"%s\" not found.\n", job_name);
      fd->close();
      return;
   }


   Dmsg1(50, "Found Job %s\n", job_name);

   if (jcr->authenticated) {
      Jmsg2(jcr, M_FATAL, 0, _("Hey!!!! JobId %u Job %s already authenticated.\n"),
         (uint32_t)jcr->JobId, jcr->Job);
      Dmsg2(50, "Hey!!!! JobId %u Job %s already authenticated.\n",
         (uint32_t)jcr->JobId, jcr->Job);
      fd->close();
      free_jcr(jcr);
      return;
   }

   jcr->file_bsock = fd;
   jcr->file_bsock->set_jcr(jcr);

   /*
    * Authenticate the File daemon
    */
   if (jcr->authenticated || !authenticate_filed(jcr)) {
      Dmsg1(50, "Authentication failed Job %s\n", jcr->Job);
      Jmsg(jcr, M_FATAL, 0, _("Unable to authenticate File daemon\n"));
   } else {
      jcr->authenticated = true;
      Dmsg2(50, "OK Authentication jid=%u Job %s\n", (uint32_t)jcr->JobId, jcr->Job);
   }

   if (!jcr->authenticated) {
      jcr->setJobStatus(JS_ErrorTerminated);
   }
   pthread_cond_signal(&jcr->job_start_wait); /* wake waiting job */
   free_jcr(jcr);
   return;
}


#ifdef needed
/*
 *   Query Device command from Director
 *   Sends Storage Daemon's information on the device to the
 *    caller (presumably the Director).
 *   This command always returns "true" so that the line is
 *    not closed on an error.
 *
 */
bool query_cmd(JCR *jcr)
{
   POOL_MEM dev_name, VolumeName, MediaType, ChangerName;
   BSOCK *dir = jcr->dir_bsock;
   DEVRES *device;
   AUTOCHANGER *changer;
   bool ok;

   Dmsg1(100, "Query_cmd: %s", dir->msg);
   ok = sscanf(dir->msg, query_device, dev_name.c_str()) == 1;
   Dmsg1(100, "<dird: %s\n", dir->msg);
   if (ok) {
      unbash_spaces(dev_name);
      foreach_res(device, R_DEVICE) {
         /* Find resource, and make sure we were able to open it */
         if (strcmp(dev_name.c_str(), device->hdr.name) == 0) {
            if (!device->dev) {
               device->dev = init_dev(jcr, device);
            }
            if (!device->dev) {
               break;
            }  
            ok = dir_update_device(jcr, device->dev);
            if (ok) {
               ok = dir->fsend(OK_query);
            } else {
               dir->fsend(NO_query);
            }
            return ok;
         }
      }
      foreach_res(changer, R_AUTOCHANGER) {
         /* Find resource, and make sure we were able to open it */
         if (strcmp(dev_name.c_str(), changer->hdr.name) == 0) {
            if (!changer->device || changer->device->size() == 0) {
               continue;              /* no devices */
            }
            ok = dir_update_changer(jcr, changer);
            if (ok) {
               ok = dir->fsend(OK_query);
            } else {
               dir->fsend(NO_query);
            }
            return ok;
         }
      }
      /* If we get here, the device/autochanger was not found */
      unbash_spaces(dir->msg);
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(NO_device, dev_name.c_str());
      Dmsg1(100, ">dird: %s\n", dir->msg);
   } else {
      unbash_spaces(dir->msg);
      pm_strcpy(jcr->errmsg, dir->msg);
      dir->fsend(BAD_query, jcr->errmsg);
      Dmsg1(100, ">dird: %s\n", dir->msg);
   }

   return true;
}

#endif


/*
 * Destroy the Job Control Record and associated
 * resources (sockets).
 */
void stored_free_jcr(JCR *jcr)
{
   Dmsg2(800, "End Job JobId=%u %p\n", jcr->JobId, jcr);
   if (jcr->dir_bsock) {
      Dmsg2(800, "Send terminate jid=%d %p\n", jcr->JobId, jcr);
      jcr->dir_bsock->signal(BNET_EOD);
      jcr->dir_bsock->signal(BNET_TERMINATE);
   }
   if (jcr->file_bsock) {
      jcr->file_bsock->close();
      jcr->file_bsock = NULL;
   }
   if (jcr->job_name) {
      free_pool_memory(jcr->job_name);
   }
   if (jcr->client_name) {
      free_memory(jcr->client_name);
      jcr->client_name = NULL;
   }
   if (jcr->fileset_name) {
      free_memory(jcr->fileset_name);
   }
   if (jcr->fileset_md5) {
      free_memory(jcr->fileset_md5);
   }
   if (jcr->bsr) {
      free_bsr(jcr->bsr);
      jcr->bsr = NULL;
   }
   /* Free any restore volume list created */
   free_restore_volume_list(jcr);
   if (jcr->RestoreBootstrap) {
      unlink(jcr->RestoreBootstrap);
      free_pool_memory(jcr->RestoreBootstrap);
      jcr->RestoreBootstrap = NULL;
   }
   if (jcr->next_dev || jcr->prev_dev) {
      Emsg0(M_FATAL, 0, _("In free_jcr(), but still attached to device!!!!\n"));
   }
   pthread_cond_destroy(&jcr->job_start_wait);
   if (jcr->dcrs) {
      delete jcr->dcrs;
   }
   jcr->dcrs = NULL;

   /* Avoid a double free */
   if (jcr->dcr == jcr->read_dcr) {
      jcr->read_dcr = NULL;
   }
   if (jcr->dcr) {
      free_dcr(jcr->dcr);
      jcr->dcr = NULL;
   }
   if (jcr->read_dcr) {
      free_dcr(jcr->read_dcr);
      jcr->read_dcr = NULL;
   }

   if (jcr->read_store) {
      DIRSTORE *store;
      foreach_alist(store, jcr->read_store) {
         delete store->device;
         delete store;
      }
      delete jcr->read_store;
      jcr->read_store = NULL;
   }
   if (jcr->write_store) {
      DIRSTORE *store;
      foreach_alist(store, jcr->write_store) {
         delete store->device;
         delete store;
      }
      delete jcr->write_store;
      jcr->write_store = NULL;
   }
   Dsm_check(200);

   if (jcr->JobId != 0)
      write_state_file(me->working_directory, "bacula-sd", get_first_port_host_order(me->sdaddrs));

   return;
}
