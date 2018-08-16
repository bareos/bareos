/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, October MM
 */
/**
 * @file
 * BAREOS Director Job processing routines
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/admin.h"
#include "dird/archive.h"
#include "dird/autoprune.h"
#include "dird/backup.h"
#include "dird/consolidate.h"
#include "dird/fd_cmds.h"
#include "dird/job.h"
#include "dird/migration.h"
#include "dird/restore.h"
#include "dird/sd_cmds.h"
#include "dird/stats.h"
#include "dird/storage.h"
#include "dird/ua_cmds.h"
#include "dird/ua_db.h"
#include "dird/ua_input.h"
#include "dird/ua_server.h"
#include "dird/ua_run.h"
#include "dird/vbackup.h"
#include "dird/verify.h"

#include "dird/ndmp_dma_backup_common.h"
#include "dird/ndmp_dma_backup.h"
#include "dird/ndmp_dma_backup_NATIVE_NDMP.h"
#include "dird/ndmp_dma_restore_common.h"
#include "dird/ndmp_dma_restore_NDMP_BAREOS.h"
#include "dird/ndmp_dma_restore_NDMP_NATIVE.h"

#include "cats/cats_backends.h"
#include "cats/sql_pooling.h"
#include "lib/edit.h"
#include "lib/parse_bsr.h"


/* Forward referenced subroutines */
static void *job_thread(void *arg);
static void JobMonitorWatchdog(watchdog_t *self);
static void JobMonitorDestructor(watchdog_t *self);
static bool JobCheckMaxwaittime(JobControlRecord *jcr);
static bool JobCheckMaxruntime(JobControlRecord *jcr);
static bool JobCheckMaxrunschedtime(JobControlRecord *jcr);

/* Imported subroutines */

/* Imported variables */

jobq_t job_queue;

void InitJobServer(int max_workers)
{
   int status;
   watchdog_t *wd;

   if ((status = JobqInit(&job_queue, max_workers, job_thread)) != 0) {
      BErrNo be;
      Emsg1(M_ABORT, 0, _("Could not init job queue: ERR=%s\n"), be.bstrerror(status));
   }
   wd = new_watchdog();
   wd->callback = JobMonitorWatchdog;
   wd->destructor = JobMonitorDestructor;
   wd->one_shot = false;
   wd->interval = 60;
   wd->data = new_control_jcr("*JobMonitor*", JT_SYSTEM);
   RegisterWatchdog(wd);
}

void TermJobServer()
{
   JobqDestroy(&job_queue);          /* ignore any errors */
}

/**
 * Run a job -- typically called by the scheduler, but may also
 *              be called by the UA (Console program).
 *
 *  Returns: 0 on failure
 *           JobId on success
 */
JobId_t RunJob(JobControlRecord *jcr)
{
   int status;

   if (SetupJob(jcr)) {
      Dmsg0(200, "Add jrc to work queue\n");
      /*
       * Queue the job to be run
       */
      if ((status = JobqAdd(&job_queue, jcr)) != 0) {
         BErrNo be;
         Jmsg(jcr, M_FATAL, 0, _("Could not add job queue: ERR=%s\n"), be.bstrerror(status));
         return 0;
      }
      return jcr->JobId;
   }

   return 0;
}

bool SetupJob(JobControlRecord *jcr, bool suppress_output)
{
   int errstat;

   jcr->lock();
   Dsm_check(100);

   /*
    * See if we should suppress all output.
    */
   if (!suppress_output) {
      InitMsg(jcr, jcr->res.messages, job_code_callback_director);
   } else {
      jcr->suppress_output = true;
   }

   /*
    * Initialize termination condition variable
    */
   if ((errstat = pthread_cond_init(&jcr->term_wait, NULL)) != 0) {
      BErrNo be;
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job cond variable: ERR=%s\n"), be.bstrerror(errstat));
      jcr->unlock();
      goto bail_out;
   }
   jcr->term_wait_inited = true;

   /*
    * Initialize nextrun ready condition variable
    */
   if ((errstat = pthread_cond_init(&jcr->nextrun_ready, NULL)) != 0) {
      BErrNo be;
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job nextrun cond variable: ERR=%s\n"), be.bstrerror(errstat));
      jcr->unlock();
      goto bail_out;
   }
   jcr->nextrun_ready_inited = true;

   CreateUniqueJobName(jcr, jcr->res.job->name());
   jcr->setJobStatus(JS_Created);
   jcr->unlock();

   /*
    * Open database
    */
   Dmsg0(100, "Open database\n");
   jcr->db = DbSqlGetPooledConnection(jcr,
                                          jcr->res.catalog->db_driver,
                                          jcr->res.catalog->db_name,
                                          jcr->res.catalog->db_user,
                                          jcr->res.catalog->db_password.value,
                                          jcr->res.catalog->db_address,
                                          jcr->res.catalog->db_port,
                                          jcr->res.catalog->db_socket,
                                          jcr->res.catalog->mult_db_connections,
                                          jcr->res.catalog->disable_batch_insert,
                                          jcr->res.catalog->try_reconnect,
                                          jcr->res.catalog->exit_on_fatal);
   if (jcr->db == NULL) {
      Jmsg(jcr, M_FATAL, 0, _("Could not open database \"%s\".\n"), jcr->res.catalog->db_name);
      goto bail_out;
   }
   Dmsg0(150, "DB opened\n");
   if (!jcr->fname) {
      jcr->fname = GetPoolMemory(PM_FNAME);
   }

   if (!jcr->res.pool_source) {
      jcr->res.pool_source = GetPoolMemory(PM_MESSAGE);
      PmStrcpy(jcr->res.pool_source, _("unknown source"));
   }

   if (!jcr->res.npool_source) {
      jcr->res.npool_source = GetPoolMemory(PM_MESSAGE);
      PmStrcpy(jcr->res.npool_source, _("unknown source"));
   }

   if (jcr->JobReads()) {
      if (!jcr->res.rpool_source) {
         jcr->res.rpool_source = GetPoolMemory(PM_MESSAGE);
         PmStrcpy(jcr->res.rpool_source, _("unknown source"));
      }
   }

   /*
    * Create Job record
    */
   InitJcrJobRecord(jcr);

   if (jcr->res.client) {
      if (!GetOrCreateClientRecord(jcr)) {
         goto bail_out;
      }
   }

   if (!jcr->db->CreateJobRecord(jcr, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
      goto bail_out;
   }

   jcr->JobId = jcr->jr.JobId;
   Dmsg4(100, "Created job record JobId=%d Name=%s Type=%c Level=%c\n",
         jcr->JobId, jcr->Job, jcr->jr.JobType, jcr->jr.JobLevel);

   NewPlugins(jcr);                  /* instantiate plugins for this jcr */
   DispatchNewPluginOptions(jcr);
   GeneratePluginEvent(jcr, bDirEventJobStart);

   if (JobCanceled(jcr)) {
      goto bail_out;
   }

   if (jcr->JobReads() && !jcr->res.rstorage) {
      if (jcr->res.job->storage) {
         CopyRwstorage(jcr, jcr->res.job->storage, _("Job resource"));
      } else {
         CopyRwstorage(jcr, jcr->res.job->pool->storage, _("Pool resource"));
      }
   }

   if (!jcr->JobReads()) {
      FreeRstorage(jcr);
   }

   /*
    * Now, do pre-run stuff, like setting job level (Inc/diff, ...)
    *  this allows us to setup a proper job start record for restarting
    *  in case of later errors.
    */
   switch (jcr->getJobType()) {
   case JT_BACKUP:
      if (!jcr->is_JobLevel(L_VIRTUAL_FULL)) {
         if (GetOrCreateFilesetRecord(jcr)) {
            /*
             * See if we need to upgrade the level. If GetLevelSinceTime returns true
             * it has updated the level of the backup and we run apply_pool_overrides
             * with the force flag so the correct pool (full, diff, incr) is selected.
             * For all others we respect any set ignore flags.
             */
            if (GetLevelSinceTime(jcr)) {
               ApplyPoolOverrides(jcr, true);
            } else {
               ApplyPoolOverrides(jcr, false);
            }
         } else {
            goto bail_out;
         }
      }

      switch (jcr->getJobProtocol()) {
      case PT_NDMP_BAREOS:
         if (!DoNdmpBackupInit(jcr)) {
            NdmpBackupCleanup(jcr, JS_ErrorTerminated);
            goto bail_out;
         }
         break;
      case PT_NDMP_NATIVE:
         if (!DoNdmpBackupInitNdmpNative(jcr)) {
            NdmpBackupCleanup(jcr, JS_ErrorTerminated);
            goto bail_out;
         }
         break;
      default:
         if (jcr->is_JobLevel(L_VIRTUAL_FULL)) {
            if (!DoNativeVbackupInit(jcr)) {
               NativeVbackupCleanup(jcr, JS_ErrorTerminated);
               goto bail_out;
            }
         } else {
            if (!DoNativeBackupInit(jcr)) {
               NativeBackupCleanup(jcr, JS_ErrorTerminated);
               goto bail_out;
            }
         }
         break;
      }
      break;
   case JT_VERIFY:
      if (!DoVerifyInit(jcr)) {
         VerifyCleanup(jcr, JS_ErrorTerminated);
         goto bail_out;
      }
      break;
   case JT_RESTORE:
      switch (jcr->getJobProtocol()) {
      case PT_NDMP_BAREOS:
      case PT_NDMP_NATIVE:
         if (!DoNdmpRestoreInit(jcr)) {
            NdmpRestoreCleanup(jcr, JS_ErrorTerminated);
            goto bail_out;
         }
         break;
      default:
         /*
          * Any non NDMP restore is not interested at the items
          * that were selected for restore so drop them now.
          */
         if (jcr->restore_tree_root) {
            FreeTree(jcr->restore_tree_root);
            jcr->restore_tree_root = NULL;
         }
         if (!DoNativeRestoreInit(jcr)) {
            NativeRestoreCleanup(jcr, JS_ErrorTerminated);
            goto bail_out;
         }
         break;
      }
      break;
   case JT_ADMIN:
      if (!DoAdminInit(jcr)) {
         AdminCleanup(jcr, JS_ErrorTerminated);
         goto bail_out;
      }
      break;
   case JT_ARCHIVE:
      if (!DoArchiveInit(jcr)) {
         ArchiveCleanup(jcr, JS_ErrorTerminated);
         goto bail_out;
      }
      break;
   case JT_COPY:
   case JT_MIGRATE:
      if (!DoMigrationInit(jcr)) {
         MigrationCleanup(jcr, JS_ErrorTerminated);
         goto bail_out;
      }

      /*
       * If there is nothing to do the DoMigrationInit() function will set
       * the termination status to JS_Terminated.
       */
      if (JobTerminatedSuccessfully(jcr)) {
         MigrationCleanup(jcr, jcr->getJobStatus());
         goto bail_out;
      }
      break;
   case JT_CONSOLIDATE:
      if (!DoConsolidateInit(jcr)) {
         ConsolidateCleanup(jcr, JS_ErrorTerminated);
         goto bail_out;
      }

      /*
       * If there is nothing to do the do_consolidation_init() function will set
       * the termination status to JS_Terminated.
       */
      if (JobTerminatedSuccessfully(jcr)) {
         ConsolidateCleanup(jcr, jcr->getJobStatus());
         goto bail_out;
      }
      break;
   default:
      Pmsg1(0, _("Unimplemented job type: %d\n"), jcr->getJobType());
      jcr->setJobStatus(JS_ErrorTerminated);
      goto bail_out;
   }

   GeneratePluginEvent(jcr, bDirEventJobInit);
   Dsm_check(100);
   return true;

bail_out:
   return false;
}

bool IsConnectingToClientAllowed(ClientResource *res)
{
   return res->conn_from_dir_to_fd;
}

bool IsConnectingToClientAllowed(JobControlRecord *jcr)
{
   return IsConnectingToClientAllowed(jcr->res.client);
}

bool IsConnectFromClientAllowed(ClientResource *res)
{
   return res->conn_from_fd_to_dir;
}

bool IsConnectFromClientAllowed(JobControlRecord *jcr)
{
   return IsConnectFromClientAllowed(jcr->res.client);
}

bool UseWaitingClient(JobControlRecord *jcr, int timeout)
{
   bool result = false;
   Connection *connection = NULL;
   ConnectionPool *connections = get_client_connections();

   if (!IsConnectFromClientAllowed(jcr)) {
      Dmsg1(120, "Client Initiated Connection from \"%s\" is not allowed.\n", jcr->res.client->name());
   } else {
      connection = connections->remove(jcr->res.client->name(), timeout);
      if (connection) {
         jcr->file_bsock = connection->bsock();
         jcr->FDVersion = connection->protocol_version();
         jcr->authenticated = connection->authenticated();
         delete(connection);
         Jmsg(jcr, M_INFO, 0, _("Using Client Initiated Connection (%s).\n"), jcr->res.client->name());
         result = true;
      }
   }

   return result;
}

void UpdateJobEnd(JobControlRecord *jcr, int TermCode)
{
   DequeueMessages(jcr);             /* display any queued messages */
   jcr->setJobStatus(TermCode);
   UpdateJobEndRecord(jcr);
}

/**
 * This is the engine called by jobq.c:JobqAdd() when we were pulled from the work queue.
 *
 * At this point, we are running in our own thread and all necessary resources are
 * allocated -- see jobq.c
 */
static void *job_thread(void *arg)
{
   JobControlRecord *jcr = (JobControlRecord *)arg;

   pthread_detach(pthread_self());
   Dsm_check(100);

   Dmsg0(200, "=====Start Job=========\n");
   jcr->setJobStatus(JS_Running);   /* this will be set only if no error */
   jcr->start_time = time(NULL);      /* set the real start time */
   jcr->jr.StartTime = jcr->start_time;

   /*
    * Let the statistics subsystem know a new Job was started.
    */
   stats_job_started();

   if (jcr->res.job->MaxStartDelay != 0 && jcr->res.job->MaxStartDelay <
       (utime_t)(jcr->start_time - jcr->sched_time)) {
      jcr->setJobStatus(JS_Canceled);
      Jmsg(jcr, M_FATAL, 0, _("Job canceled because max start delay time exceeded.\n"));
   }

   if (JobCheckMaxrunschedtime(jcr)) {
      jcr->setJobStatus(JS_Canceled);
      Jmsg(jcr, M_FATAL, 0, _("Job canceled because max run sched time exceeded.\n"));
   }

   /*
    * TODO : check if it is used somewhere
    */
   if (jcr->res.job->RunScripts == NULL) {
      Dmsg0(200, "Warning, job->RunScripts is empty\n");
      jcr->res.job->RunScripts = New(alist(10, not_owned_by_alist));
   }

   if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
   }

   /*
    * Run any script BeforeJob on dird
    */
   RunScripts(jcr, jcr->res.job->RunScripts, "BeforeJob");

   /*
    * We re-update the job start record so that the start time is set after the run before job.
    * This avoids that any files created by the run before job will be saved twice. They will
    * be backed up in the current job, but not in the next one unless they are changed.
    *
    * Without this, they will be backed up in this job and in the next job run because in that
    * case, their date is after the start of this run.
    */
   jcr->start_time = time(NULL);
   jcr->jr.StartTime = jcr->start_time;
   if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
   }

   GeneratePluginEvent(jcr, bDirEventJobRun);

   switch (jcr->getJobType()) {
   case JT_BACKUP:
      switch (jcr->getJobProtocol()) {
      case PT_NDMP_BAREOS:
         if (!JobCanceled(jcr)) {
            if (DoNdmpBackup(jcr)) {
               DoAutoprune(jcr);
            } else {
               NdmpBackupCleanup(jcr, JS_ErrorTerminated);
            }
         } else {
            NdmpBackupCleanup(jcr, JS_Canceled);
         }
         break;
      case PT_NDMP_NATIVE:
         if (!JobCanceled(jcr)) {
            if (DoNdmpBackupNdmpNative(jcr)) {
               DoAutoprune(jcr);
            } else {
               NdmpBackupCleanup(jcr, JS_ErrorTerminated);
            }
         } else {
            NdmpBackupCleanup(jcr, JS_Canceled);
         }
         break;
      default:
         if (!JobCanceled(jcr)) {
            if (jcr->is_JobLevel(L_VIRTUAL_FULL)) {
               if (DoNativeVbackup(jcr)) {
                  DoAutoprune(jcr);
               } else {
                  NativeVbackupCleanup(jcr, JS_ErrorTerminated);
               }
            } else {
               if (DoNativeBackup(jcr)) {
                  DoAutoprune(jcr);
               } else {
                  NativeBackupCleanup(jcr, JS_ErrorTerminated);
               }
            }
         } else {
            if (jcr->is_JobLevel(L_VIRTUAL_FULL)) {
               NativeVbackupCleanup(jcr, JS_Canceled);
            } else {
               NativeBackupCleanup(jcr, JS_Canceled);
            }
         }
         break;
      }
      break;
   case JT_VERIFY:
      if (!JobCanceled(jcr)) {
         if (DoVerify(jcr)) {
            DoAutoprune(jcr);
         } else {
            VerifyCleanup(jcr, JS_ErrorTerminated);
         }
      } else {
         VerifyCleanup(jcr, JS_Canceled);
      }
      break;
   case JT_RESTORE:
      switch (jcr->getJobProtocol()) {
      case PT_NDMP_BAREOS:
         if (!JobCanceled(jcr)) {
            if (DoNdmpRestore(jcr)) {
               DoAutoprune(jcr);
            } else {
               NdmpRestoreCleanup(jcr, JS_ErrorTerminated);
            }
         } else {
            NdmpRestoreCleanup(jcr, JS_Canceled);
         }
         break;
      case PT_NDMP_NATIVE:
         if (!JobCanceled(jcr)) {
            if (DoNdmpRestoreNdmpNative(jcr)) {
               DoAutoprune(jcr);
            } else {
               NdmpRestoreCleanup(jcr, JS_ErrorTerminated);
            }
         } else {
            NdmpRestoreCleanup(jcr, JS_Canceled);
         }
         break;
      default:
         if (!JobCanceled(jcr)) {
            if (DoNativeRestore(jcr)) {
               DoAutoprune(jcr);
            } else {
               NativeRestoreCleanup(jcr, JS_ErrorTerminated);
            }
         } else {
            NativeRestoreCleanup(jcr, JS_Canceled);
         }
         break;
      }
      break;
   case JT_ADMIN:
      if (!JobCanceled(jcr)) {
         if (do_admin(jcr)) {
            DoAutoprune(jcr);
         } else {
            AdminCleanup(jcr, JS_ErrorTerminated);
         }
      } else {
         AdminCleanup(jcr, JS_Canceled);
      }
      break;
   case JT_ARCHIVE:
      if (!JobCanceled(jcr)) {
         if (DoArchive(jcr)) {
            DoAutoprune(jcr);
         } else {
            ArchiveCleanup(jcr, JS_ErrorTerminated);
         }
      } else {
         ArchiveCleanup(jcr, JS_Canceled);
      }
      break;
   case JT_COPY:
   case JT_MIGRATE:
      if (!JobCanceled(jcr)) {
         if (DoMigration(jcr)) {
            DoAutoprune(jcr);
         } else {
            MigrationCleanup(jcr, JS_ErrorTerminated);
         }
      } else {
         MigrationCleanup(jcr, JS_Canceled);
      }
      break;
   case JT_CONSOLIDATE:
      if (!JobCanceled(jcr)) {
         if (DoConsolidate(jcr)) {
            DoAutoprune(jcr);
         } else {
            ConsolidateCleanup(jcr, JS_ErrorTerminated);
         }
      } else {
         ConsolidateCleanup(jcr, JS_Canceled);
      }
      break;
   default:
      Pmsg1(0, _("Unimplemented job type: %d\n"), jcr->getJobType());
      break;
   }

   /*
    * Check for subscriptions and issue a warning when exceeded.
    */
   if (me->subscriptions &&
       me->subscriptions < me->subscriptions_used) {
      Jmsg(jcr, M_WARNING, 0, _("Subscriptions exceeded: (used/total) (%d/%d)\n"),
           me->subscriptions_used, me->subscriptions);
   }

   RunScripts(jcr, jcr->res.job->RunScripts, "AfterJob");

   /*
    * Send off any queued messages
    */
   if (jcr->msg_queue && jcr->msg_queue->size() > 0) {
      DequeueMessages(jcr);
   }

   GeneratePluginEvent(jcr, bDirEventJobEnd);
   Dmsg1(50, "======== End Job stat=%c ==========\n", jcr->JobStatus);
   Dsm_check(100);

   return NULL;
}

void SdMsgThreadSendSignal(JobControlRecord *jcr, int sig)
{
   jcr->lock();
   if (!jcr->sd_msg_thread_done &&
       jcr->SD_msg_chan_started &&
       !pthread_equal(jcr->SD_msg_chan, pthread_self())) {
      Dmsg1(800, "Send kill to SD msg chan jid=%d\n", jcr->JobId);
      pthread_kill(jcr->SD_msg_chan, sig);
   }
   jcr->unlock();
}

/**
 * Cancel a job -- typically called by the UA (Console program), but may also
 *              be called by the job watchdog.
 *
 *  Returns: true  if cancel appears to be successful
 *           false on failure. Message sent to ua->jcr.
 */
bool CancelJob(UaContext *ua, JobControlRecord *jcr)
{
   char ed1[50];
   int32_t old_status = jcr->JobStatus;

   jcr->setJobStatus(JS_Canceled);

   switch (old_status) {
   case JS_Created:
   case JS_WaitJobRes:
   case JS_WaitClientRes:
   case JS_WaitStoreRes:
   case JS_WaitPriority:
   case JS_WaitMaxJobs:
   case JS_WaitStartTime:
      ua->InfoMsg(_("JobId %s, Job %s marked to be canceled.\n"),
              edit_uint64(jcr->JobId, ed1), jcr->Job);
      JobqRemove(&job_queue, jcr); /* attempt to remove it from queue */
      break;

   default:
      /*
       * Cancel File daemon
       */
      if (jcr->file_bsock) {
         if (!CancelFileDaemonJob(ua, jcr)) {
            return false;
         }
      }

      /*
       * Cancel Storage daemon
       */
      if (jcr->store_bsock) {
         if (!CancelStorageDaemonJob(ua, jcr)) {
            return false;
         }
      }

      /*
       * Cancel second Storage daemon for SD-SD replication.
       */
      if (jcr->mig_jcr && jcr->mig_jcr->store_bsock) {
         if (!CancelStorageDaemonJob(ua, jcr->mig_jcr)) {
            return false;
         }
      }

      break;
   }

   RunScripts(jcr, jcr->res.job->RunScripts, "AfterJob");

   return true;
}

static void JobMonitorDestructor(watchdog_t *self)
{
   JobControlRecord *control_jcr = (JobControlRecord *)self->data;

   FreeJcr(control_jcr);
}

static void JobMonitorWatchdog(watchdog_t *self)
{
   JobControlRecord *control_jcr, *jcr;

   control_jcr = (JobControlRecord *)self->data;

   Dsm_check(100);
   Dmsg1(800, "JobMonitorWatchdog %p called\n", self);

   foreach_jcr(jcr) {
      bool cancel = false;

      if (jcr->JobId == 0 || JobCanceled(jcr) || jcr->no_maxtime) {
         Dmsg2(800, "Skipping JobControlRecord=%p Job=%s\n", jcr, jcr->Job);
         continue;
      }

      /* check MaxWaitTime */
      if (JobCheckMaxwaittime(jcr)) {
         jcr->setJobStatus(JS_Canceled);
         Qmsg(jcr, M_FATAL, 0, _("Max wait time exceeded. Job canceled.\n"));
         cancel = true;
      /* check MaxRunTime */
      } else if (JobCheckMaxruntime(jcr)) {
         jcr->setJobStatus(JS_Canceled);
         Qmsg(jcr, M_FATAL, 0, _("Max run time exceeded. Job canceled.\n"));
         cancel = true;
      /* check MaxRunSchedTime */
      } else if (JobCheckMaxrunschedtime(jcr)) {
         jcr->setJobStatus(JS_Canceled);
         Qmsg(jcr, M_FATAL, 0, _("Max run sched time exceeded. Job canceled.\n"));
         cancel = true;
      }

      if (cancel) {
         Dmsg3(800, "Cancelling JobControlRecord %p jobid %d (%s)\n", jcr, jcr->JobId, jcr->Job);
         UaContext *ua = new_ua_context(jcr);
         ua->jcr = control_jcr;
         CancelJob(ua, jcr);
         FreeUaContext(ua);
         Dmsg2(800, "Have cancelled JobControlRecord %p Job=%d\n", jcr, jcr->JobId);
      }

   }
   /* Keep reference counts correct */
   endeach_jcr(jcr);
}

/**
 * Check if the maxwaittime has expired and it is possible
 *  to cancel the job.
 */
static bool JobCheckMaxwaittime(JobControlRecord *jcr)
{
   bool cancel = false;
   JobResource *job = jcr->res.job;
   utime_t current=0;

   if (!JobWaiting(jcr)) {
      return false;
   }

   if (jcr->wait_time) {
      current = watchdog_time - jcr->wait_time;
   }

   Dmsg2(200, "check maxwaittime %u >= %u\n",
         current + jcr->wait_time_sum, job->MaxWaitTime);
   if (job->MaxWaitTime != 0 &&
       (current + jcr->wait_time_sum) >= job->MaxWaitTime) {
      cancel = true;
   }

   return cancel;
}

/**
 * Check if maxruntime has expired and if the job can be
 *   canceled.
 */
static bool JobCheckMaxruntime(JobControlRecord *jcr)
{
   bool cancel = false;
   JobResource *job = jcr->res.job;
   utime_t run_time;

   if (JobCanceled(jcr) || !jcr->job_started) {
      return false;
   }
   if (job->MaxRunTime == 0 && job->FullMaxRunTime == 0 &&
       job->IncMaxRunTime == 0 && job->DiffMaxRunTime == 0) {
      return false;
   }
   run_time = watchdog_time - jcr->start_time;
   Dmsg7(200, "check_maxruntime %llu-%u=%llu >= %llu|%llu|%llu|%llu\n",
         watchdog_time, jcr->start_time, run_time, job->MaxRunTime, job->FullMaxRunTime,
         job->IncMaxRunTime, job->DiffMaxRunTime);

   if (jcr->getJobLevel() == L_FULL && job->FullMaxRunTime != 0 &&
         run_time >= job->FullMaxRunTime) {
      Dmsg0(200, "check_maxwaittime: FullMaxcancel\n");
      cancel = true;
   } else if (jcr->getJobLevel() == L_DIFFERENTIAL && job->DiffMaxRunTime != 0 &&
         run_time >= job->DiffMaxRunTime) {
      Dmsg0(200, "check_maxwaittime: DiffMaxcancel\n");
      cancel = true;
   } else if (jcr->getJobLevel() == L_INCREMENTAL && job->IncMaxRunTime != 0 &&
         run_time >= job->IncMaxRunTime) {
      Dmsg0(200, "check_maxwaittime: IncMaxcancel\n");
      cancel = true;
   } else if (job->MaxRunTime > 0 && run_time >= job->MaxRunTime) {
      Dmsg0(200, "check_maxwaittime: Maxcancel\n");
      cancel = true;
   }

   return cancel;
}

/**
 * Check if MaxRunSchedTime has expired and if the job can be
 *   canceled.
 */
static bool JobCheckMaxrunschedtime(JobControlRecord *jcr)
{
   if (jcr->MaxRunSchedTime == 0 || JobCanceled(jcr)) {
      return false;
   }
   if ((watchdog_time - jcr->initial_sched_time) < jcr->MaxRunSchedTime) {
      Dmsg3(200, "Job %p (%s) with MaxRunSchedTime %d not expired\n",
            jcr, jcr->Job, jcr->MaxRunSchedTime);
      return false;
   }

   return true;
}

/**
 * Get or create a Pool record with the given name.
 * Returns: 0 on error
 *          poolid if OK
 */
DBId_t GetOrCreatePoolRecord(JobControlRecord *jcr, char *pool_name)
{
   PoolDbRecord pr;

   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, pool_name, sizeof(pr.Name));
   Dmsg1(110, "get_or_create_pool=%s\n", pool_name);

   while (!jcr->db->GetPoolRecord(jcr, &pr)) { /* get by Name */
      /* Try to create the pool */
      if (CreatePool(jcr, jcr->db, jcr->res.pool, POOL_OP_CREATE) < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Pool \"%s\" not in database. ERR=%s"), pr.Name, jcr->db->strerror());
         return 0;
      } else {
         Jmsg(jcr, M_INFO, 0, _("Created database record for Pool \"%s\".\n"), pr.Name);
      }
   }
   return pr.PoolId;
}

/**
 * Check for duplicate jobs.
 *  Returns: true  if current job should continue
 *           false if current job should terminate
 */
bool AllowDuplicateJob(JobControlRecord *jcr)
{
   JobControlRecord *djcr;                /* possible duplicate job */
   JobResource *job = jcr->res.job;
   bool cancel_dup = false;
   bool cancel_me = false;

   /*
    * See if AllowDuplicateJobs is set or
    * if duplicate checking is disabled for this job.
    */
   if (job->AllowDuplicateJobs || jcr->IgnoreDuplicateJobChecking) {
      return true;
   }

   Dmsg0(800, "Enter AllowDuplicateJob\n");

   /*
    * After this point, we do not want to allow any duplicate
    * job to run.
    */

   foreach_jcr(djcr) {
      if (jcr == djcr || djcr->JobId == 0) {
         continue;                   /* do not cancel this job or consoles */
      }

      /*
       * See if this Job has the IgnoreDuplicateJobChecking flag set, ignore it
       * for any checking against other jobs.
       */
      if (djcr->IgnoreDuplicateJobChecking) {
         continue;
      }

      if (bstrcmp(job->name(), djcr->res.job->name())) {
         if (job->DuplicateJobProximity > 0) {
            utime_t now = (utime_t)time(NULL);
            if ((now - djcr->start_time) > job->DuplicateJobProximity) {
               continue;               /* not really a duplicate */
            }
         }
         if (job->CancelLowerLevelDuplicates &&
             djcr->is_JobType(JT_BACKUP) && jcr->is_JobType(JT_BACKUP)) {
            switch (jcr->getJobLevel()) {
            case L_FULL:
               if (djcr->getJobLevel() == L_DIFFERENTIAL ||
                   djcr->getJobLevel() == L_INCREMENTAL) {
                  cancel_dup = true;
               }
               break;
            case L_DIFFERENTIAL:
               if (djcr->getJobLevel() == L_INCREMENTAL) {
                  cancel_dup = true;
               }
               if (djcr->getJobLevel() == L_FULL) {
                  cancel_me = true;
               }
               break;
            case L_INCREMENTAL:
               if (djcr->getJobLevel() == L_FULL ||
                   djcr->getJobLevel() == L_DIFFERENTIAL) {
                  cancel_me = true;
               }
            }
            /*
             * cancel_dup will be done below
             */
            if (cancel_me) {
              /* Zap current job */
              jcr->setJobStatus(JS_Canceled);
              Jmsg(jcr, M_FATAL, 0, _("JobId %d already running. Duplicate job not allowed.\n"),
                 djcr->JobId);
              break;     /* get out of foreach_jcr */
            }
         }

         /*
          * Cancel one of the two jobs (me or dup)
          * If CancelQueuedDuplicates is set do so only if job is queued.
          */
         if (job->CancelQueuedDuplicates) {
             switch (djcr->JobStatus) {
             case JS_Created:
             case JS_WaitJobRes:
             case JS_WaitClientRes:
             case JS_WaitStoreRes:
             case JS_WaitPriority:
             case JS_WaitMaxJobs:
             case JS_WaitStartTime:
                cancel_dup = true;  /* cancel queued duplicate */
                break;
             default:
                break;
             }
         }

         if (cancel_dup || job->CancelRunningDuplicates) {
            /*
             * Zap the duplicated job djcr
             */
            UaContext *ua = new_ua_context(jcr);
            Jmsg(jcr, M_INFO, 0, _("Cancelling duplicate JobId=%d.\n"), djcr->JobId);
            CancelJob(ua, djcr);
            Bmicrosleep(0, 500000);
            djcr->setJobStatus(JS_Canceled);
            CancelJob(ua, djcr);
            FreeUaContext(ua);
            Dmsg2(800, "Cancel dup %p JobId=%d\n", djcr, djcr->JobId);
         } else {
            /*
             * Zap current job
             */
            jcr->setJobStatus(JS_Canceled);
            Jmsg(jcr, M_FATAL, 0, _("JobId %d already running. Duplicate job not allowed.\n"),
               djcr->JobId);
            Dmsg2(800, "Cancel me %p JobId=%d\n", jcr, jcr->JobId);
         }
         Dmsg4(800, "curJobId=%d use_cnt=%d dupJobId=%d use_cnt=%d\n",
               jcr->JobId, jcr->UseCount(), djcr->JobId, djcr->UseCount());
         break;                 /* did our work, get out of foreach loop */
      }
   }
   endeach_jcr(djcr);

   return true;
}

/**
 * This subroutine edits the last job start time into a
 * "since=date/time" buffer that is returned in the
 * variable since.  This is used for display purposes in
 * the job report.  The time in jcr->stime is later
 * passed to tell the File daemon what to do.
 */
bool GetLevelSinceTime(JobControlRecord *jcr)
{
   int JobLevel;
   bool have_full;
   bool do_full = false;
   bool do_vfull = false;
   bool do_diff = false;
   bool pool_updated = false;
   utime_t now;
   utime_t last_full_time = 0;
   utime_t last_diff_time;
   char prev_job[MAX_NAME_LENGTH];

   jcr->since[0] = 0;

   /*
    * If job cloned and a since time already given, use it
    */
   if (jcr->cloned && jcr->stime && jcr->stime[0]) {
      bstrncpy(jcr->since, _(", since="), sizeof(jcr->since));
      bstrncat(jcr->since, jcr->stime, sizeof(jcr->since));
      return pool_updated;
   }

   /*
    * Make sure stime buffer is allocated
    */
   if (!jcr->stime) {
      jcr->stime = GetPoolMemory(PM_MESSAGE);
   }
   jcr->PrevJob[0] = 0;
   jcr->stime[0] = 0;

   /*
    * Lookup the last FULL backup job to get the time/date for a
    * differential or incremental save.
    */
   JobLevel = jcr->getJobLevel();
   switch (JobLevel) {
   case L_DIFFERENTIAL:
   case L_INCREMENTAL:
      POOLMEM *stime = GetPoolMemory(PM_MESSAGE);

      /*
       * Look up start time of last Full job
       */
      now = (utime_t)time(NULL);
      jcr->jr.JobId = 0;     /* flag to return since time */

      /*
       * This is probably redundant, but some of the code below
       * uses jcr->stime, so don't remove unless you are sure.
       */
      if (!jcr->db->FindJobStartTime(jcr, &jcr->jr, jcr->stime, jcr->PrevJob)) {
         do_full = true;
      }

      have_full = jcr->db->FindLastJobStartTime(jcr, &jcr->jr, stime, prev_job, L_FULL);
      if (have_full) {
         last_full_time = StrToUtime(stime);
      } else {
         do_full = true;               /* No full, upgrade to one */
      }

      Dmsg4(50, "have_full=%d do_full=%d now=%lld full_time=%lld\n", have_full,
            do_full, now, last_full_time);

      /*
       * Make sure the last diff is recent enough
       */
      if (have_full && JobLevel == L_INCREMENTAL && jcr->res.job->MaxDiffInterval > 0) {
         /*
          * Lookup last diff job
          */
         if (jcr->db->FindLastJobStartTime(jcr, &jcr->jr, stime, prev_job, L_DIFFERENTIAL)) {
            last_diff_time = StrToUtime(stime);
            /*
             * If no Diff since Full, use Full time
             */
            if (last_diff_time < last_full_time) {
               last_diff_time = last_full_time;
            }
            Dmsg2(50, "last_diff_time=%lld last_full_time=%lld\n", last_diff_time,
                  last_full_time);
         } else {
            /*
             * No last differential, so use last full time
             */
            last_diff_time = last_full_time;
            Dmsg1(50, "No last_diff_time setting to full_time=%lld\n", last_full_time);
         }
         do_diff = ((now - last_diff_time) >= jcr->res.job->MaxDiffInterval);
         Dmsg2(50, "do_diff=%d diffInter=%lld\n", do_diff, jcr->res.job->MaxDiffInterval);
      }

      /*
       * Note, do_full takes precedence over do_vfull and do_diff
       */
      if (have_full && jcr->res.job->MaxFullInterval > 0) {
         do_full = ((now - last_full_time) >= jcr->res.job->MaxFullInterval);
      } else if (have_full && jcr->res.job->MaxVFullInterval > 0) {
         do_vfull = ((now - last_full_time) >= jcr->res.job->MaxVFullInterval);
      }
      FreePoolMemory(stime);

      if (do_full) {
         /*
          * No recent Full job found, so upgrade this one to Full
          */
         Jmsg(jcr, M_INFO, 0, "%s", jcr->db->strerror());
         Jmsg(jcr, M_INFO, 0, _("No prior or suitable Full backup found in catalog. Doing FULL backup.\n"));
         Bsnprintf(jcr->since, sizeof(jcr->since), _(" (upgraded from %s)"), level_to_str(JobLevel));
         jcr->setJobLevel(jcr->jr.JobLevel = L_FULL);
         pool_updated = true;
      } else if (do_vfull) {
         /*
          * No recent Full job found, and MaxVirtualFull is set so upgrade this one to Virtual Full
          */
         Jmsg(jcr, M_INFO, 0, "%s", jcr->db->strerror());
         Jmsg(jcr, M_INFO, 0, _("No prior or suitable Full backup found in catalog. Doing Virtual FULL backup.\n"));
         Bsnprintf(jcr->since, sizeof(jcr->since), _(" (upgraded from %s)"), level_to_str(jcr->getJobLevel()));
         jcr->setJobLevel(jcr->jr.JobLevel = L_VIRTUAL_FULL);
         pool_updated = true;

         /*
          * If we get upgraded to a Virtual Full we will be using a read pool so make sure we have a rpool_source.
          */
         if (!jcr->res.rpool_source) {
            jcr->res.rpool_source = GetPoolMemory(PM_MESSAGE);
            PmStrcpy(jcr->res.rpool_source, _("unknown source"));
         }
      } else if (do_diff) {
         /*
          * No recent diff job found, so upgrade this one to Diff
          */
         Jmsg(jcr, M_INFO, 0, _("No prior or suitable Differential backup found in catalog. Doing Differential backup.\n"));
         Bsnprintf(jcr->since, sizeof(jcr->since), _(" (upgraded from %s)"), level_to_str(JobLevel));
         jcr->setJobLevel(jcr->jr.JobLevel = L_DIFFERENTIAL);
         pool_updated = true;
      } else {
         if (jcr->res.job->rerun_failed_levels) {
            if (jcr->db->FindFailedJobSince(jcr, &jcr->jr, jcr->stime, JobLevel)) {
               Jmsg(jcr, M_INFO, 0, _("Prior failed job found in catalog. Upgrading to %s.\n"), level_to_str(JobLevel));
               Bsnprintf(jcr->since, sizeof(jcr->since), _(" (upgraded from %s)"), level_to_str(JobLevel));
               jcr->setJobLevel(jcr->jr.JobLevel = JobLevel);
               jcr->jr.JobId = jcr->JobId;
               pool_updated = true;
               break;
            }
         }

         bstrncpy(jcr->since, _(", since="), sizeof(jcr->since));
         bstrncat(jcr->since, jcr->stime, sizeof(jcr->since));
      }
      jcr->jr.JobId = jcr->JobId;

      /*
       * Lookup the Job record of the previous Job and store it in jcr->previous_jr.
       */
      if (jcr->PrevJob[0]) {
         bstrncpy(jcr->previous_jr.Job, jcr->PrevJob, sizeof(jcr->previous_jr.Job));
         if (!jcr->db->GetJobRecord(jcr, &jcr->previous_jr)) {
            Jmsg(jcr, M_FATAL, 0, _("Could not get job record for previous Job. ERR=%s"), jcr->db->strerror());
         }
      }

      break;
   }

   Dmsg3(100, "Level=%c last start time=%s job=%s\n", JobLevel, jcr->stime, jcr->PrevJob);

   return pool_updated;
}

void ApplyPoolOverrides(JobControlRecord *jcr, bool force)
{
   Dmsg0(100, "entering ApplyPoolOverrides()\n");
   bool pool_override = false;

   /*
    * If a cmdline pool override is given ignore any level pool overrides.
    * Unless a force is given then we always apply any overrides.
    */
   if (!force && jcr->IgnoreLevelPoolOverides) {
      return;
   }

   /*
    * If only a pool override and no level overrides are given in run entry choose this pool
    */
   if (jcr->res.run_pool_override &&
      !jcr->res.run_full_pool_override &&
      !jcr->res.run_vfull_pool_override &&
      !jcr->res.run_inc_pool_override &&
      !jcr->res.run_diff_pool_override) {
      PmStrcpy(jcr->res.pool_source, _("Run Pool override"));
      Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.pool->name(), _("Run Pool override\n"));
   } else {
      /*
       * Apply any level related Pool selections
       */
      switch (jcr->getJobLevel()) {
      case L_FULL:
         if (jcr->res.full_pool) {
            jcr->res.pool = jcr->res.full_pool;
            pool_override = true;
            if (jcr->res.run_full_pool_override) {
               PmStrcpy(jcr->res.pool_source, _("Run FullPool override"));
               Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.full_pool->name(), "Run FullPool override\n");
            } else {
               PmStrcpy(jcr->res.pool_source, _("Job FullPool override"));
               Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.full_pool->name(), "Job FullPool override\n");
            }
         }
         break;
      case L_VIRTUAL_FULL:
         if (jcr->res.vfull_pool) {
            jcr->res.pool = jcr->res.vfull_pool;
            pool_override = true;
            if (jcr->res.run_vfull_pool_override) {
               PmStrcpy(jcr->res.pool_source, _("Run VFullPool override"));
               Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.vfull_pool->name(), "Run VFullPool override\n");
            } else {
               PmStrcpy(jcr->res.pool_source, _("Job VFullPool override"));
               Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.vfull_pool->name(), "Job VFullPool override\n");
            }
         }
         break;
      case L_INCREMENTAL:
         if (jcr->res.inc_pool) {
            jcr->res.pool = jcr->res.inc_pool;
            pool_override = true;
            if (jcr->res.run_inc_pool_override) {
               PmStrcpy(jcr->res.pool_source, _("Run IncPool override"));
               Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.inc_pool->name(), "Run IncPool override\n");
            } else {
               PmStrcpy(jcr->res.pool_source, _("Job IncPool override"));
               Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.inc_pool->name(), "Job IncPool override\n");
            }
         }
         break;
      case L_DIFFERENTIAL:
         if (jcr->res.diff_pool) {
            jcr->res.pool = jcr->res.diff_pool;
            pool_override = true;
            if (jcr->res.run_diff_pool_override) {
               PmStrcpy(jcr->res.pool_source, _("Run DiffPool override"));
               Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.diff_pool->name(), "Run DiffPool override\n");
            } else {
               PmStrcpy(jcr->res.pool_source, _("Job DiffPool override"));
               Dmsg2(100, "Pool set to '%s' because of %s", jcr->res.diff_pool->name(), "Job DiffPool override\n");
            }
         }
         break;
      }
   }

   /*
    * Update catalog if pool overridden
    */
   if (pool_override && jcr->res.pool->catalog) {
      jcr->res.catalog = jcr->res.pool->catalog;
      PmStrcpy(jcr->res.catalog_source, _("Pool resource"));
   }
}

/**
 * Get or create a Client record for this Job
 */
bool GetOrCreateClientRecord(JobControlRecord *jcr)
{
   ClientDbRecord cr;

   memset(&cr, 0, sizeof(cr));
   bstrncpy(cr.Name, jcr->res.client->hdr.name, sizeof(cr.Name));
   cr.AutoPrune = jcr->res.client->AutoPrune;
   cr.FileRetention = jcr->res.client->FileRetention;
   cr.JobRetention = jcr->res.client->JobRetention;
   if (!jcr->client_name) {
      jcr->client_name = GetPoolMemory(PM_NAME);
   }
   PmStrcpy(jcr->client_name, jcr->res.client->hdr.name);
   if (!jcr->db->CreateClientRecord(jcr, &cr)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not create Client record. ERR=%s\n"), jcr->db->strerror());
      return false;
   }
   /*
    * Only initialize quota when a Soft or Hard Limit is set.
    */
   if (jcr->res.client->HardQuota != 0 ||
       jcr->res.client->SoftQuota != 0) {
      if (!jcr->db->GetQuotaRecord(jcr, &cr)) {
         if (!jcr->db->CreateQuotaRecord(jcr, &cr)) {
            Jmsg(jcr, M_FATAL, 0, _("Could not create Quota record. ERR=%s\n"), jcr->db->strerror());
         }
         jcr->res.client->QuotaLimit = 0;
         jcr->res.client->GraceTime = 0;
      }
   }
   jcr->jr.ClientId = cr.ClientId;
   jcr->res.client->QuotaLimit = cr.QuotaLimit;
   jcr->res.client->GraceTime = cr.GraceTime;
   if (cr.Uname[0]) {
      if (!jcr->client_uname) {
         jcr->client_uname = GetPoolMemory(PM_NAME);
      }
      PmStrcpy(jcr->client_uname, cr.Uname);
   }
   Dmsg2(100, "Created Client %s record %d\n", jcr->res.client->hdr.name,
      jcr->jr.ClientId);
   return true;
}

bool GetOrCreateFilesetRecord(JobControlRecord *jcr)
{
   FileSetDbRecord fsr;

   /*
    * Get or Create FileSet record
    */
   memset(&fsr, 0, sizeof(fsr));
   bstrncpy(fsr.FileSet, jcr->res.fileset->hdr.name, sizeof(fsr.FileSet));
   if (jcr->res.fileset->have_MD5) {
      MD5_CTX md5c;
      unsigned char digest[MD5HashSize];
      memcpy(&md5c, &jcr->res.fileset->md5c, sizeof(md5c));
      MD5_Final(digest, &md5c);
      /*
       * Keep the flag (last arg) set to false otherwise old FileSets will
       * get new MD5 sums and the user will get Full backups on everything
       */
      BinToBase64(fsr.MD5, sizeof(fsr.MD5), (char *)digest, MD5HashSize, false);
      bstrncpy(jcr->res.fileset->MD5, fsr.MD5, sizeof(jcr->res.fileset->MD5));
   } else {
      Jmsg(jcr, M_WARNING, 0, _("FileSet MD5 digest not found.\n"));
   }
   if (!jcr->res.fileset->ignore_fs_changes ||
       !jcr->db->GetFilesetRecord(jcr, &fsr)) {
      PoolMem FileSetText(PM_MESSAGE);

      jcr->res.fileset->PrintConfig(FileSetText, false, false);
      fsr.FileSetText = FileSetText.c_str();

      if (!jcr->db->CreateFilesetRecord(jcr, &fsr)) {
         Jmsg(jcr, M_ERROR, 0, _("Could not create FileSet \"%s\" record. ERR=%s\n"),
              fsr.FileSet, jcr->db->strerror());
         return false;
      }
   }

   jcr->jr.FileSetId = fsr.FileSetId;
   bstrncpy(jcr->FSCreateTime, fsr.cCreateTime, sizeof(jcr->FSCreateTime));

   Dmsg2(119, "Created FileSet %s record %u\n", jcr->res.fileset->hdr.name, jcr->jr.FileSetId);

   return true;
}

void InitJcrJobRecord(JobControlRecord *jcr)
{
   jcr->jr.SchedTime = jcr->sched_time;
   jcr->jr.StartTime = jcr->start_time;
   jcr->jr.EndTime = 0;               /* perhaps rescheduled, clear it */
   jcr->jr.JobType = jcr->getJobType();
   jcr->jr.JobLevel = jcr->getJobLevel();
   jcr->jr.JobStatus = jcr->JobStatus;
   jcr->jr.JobId = jcr->JobId;
   jcr->jr.JobSumTotalBytes = 18446744073709551615LLU;
   bstrncpy(jcr->jr.Name, jcr->res.job->name(), sizeof(jcr->jr.Name));
   bstrncpy(jcr->jr.Job, jcr->Job, sizeof(jcr->jr.Job));
}

/**
 * Write status and such in DB
 */
void UpdateJobEndRecord(JobControlRecord *jcr)
{
   jcr->jr.EndTime = time(NULL);
   jcr->end_time = jcr->jr.EndTime;
   jcr->jr.JobId = jcr->JobId;
   jcr->jr.JobStatus = jcr->JobStatus;
   jcr->jr.JobFiles = jcr->JobFiles;
   jcr->jr.JobBytes = jcr->JobBytes;
   jcr->jr.ReadBytes = jcr->ReadBytes;
   jcr->jr.VolSessionId = jcr->VolSessionId;
   jcr->jr.VolSessionTime = jcr->VolSessionTime;
   jcr->jr.JobErrors = jcr->JobErrors;
   jcr->jr.HasBase = jcr->HasBase;
   if (!jcr->db->UpdateJobEndRecord(jcr, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error updating job record. %s"), jcr->db->strerror());
   }
}

/**
 * Takes base_name and appends (unique) current
 *   date and time to form unique job name.
 *
 *  Note, the seconds are actually a sequence number. This
 *   permits us to start a maximum fo 59 unique jobs a second, which
 *   should be sufficient.
 *
 *  Returns: unique job name in jcr->Job
 *    date/time in jcr->start_time
 */
void CreateUniqueJobName(JobControlRecord *jcr, const char *base_name)
{
   /* Job start mutex */
   static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   static time_t last_start_time = 0;
   static int seq = 0;
   time_t now = time(NULL);
   char dt[MAX_TIME_LENGTH];
   char name[MAX_NAME_LENGTH];
   char *p;
   int len;

   /* Guarantee unique start time -- maximum one per second, and
    * thus unique Job Name
    */
   P(mutex);                          /* lock creation of jobs */
   seq++;
   if (seq > 59) {                    /* wrap as if it is seconds */
      seq = 0;
      while (now == last_start_time) {
         Bmicrosleep(0, 500000);
         now = time(NULL);
      }
   }
   last_start_time = now;
   V(mutex);                          /* allow creation of jobs */
   jcr->start_time = now;

   /*
    * Form Unique JobName
    * Use only characters that are permitted in Windows filenames
    */
   bstrftime(dt, sizeof(dt), jcr->start_time, "%Y-%m-%d_%H.%M.%S");

   len = strlen(dt) + 5;   /* dt + .%02d EOS */
   bstrncpy(name, base_name, sizeof(name));
   name[sizeof(name)-len] = 0;          /* truncate if too long */
   Bsnprintf(jcr->Job, sizeof(jcr->Job), "%s.%s_%02d", name, dt, seq); /* add date & time */
   /* Convert spaces into underscores */
   for (p=jcr->Job; *p; p++) {
      if (*p == ' ') {
         *p = '_';
      }
   }
   Dmsg2(100, "JobId=%u created Job=%s\n", jcr->JobId, jcr->Job);
}

/**
 * Called directly from job rescheduling
 */
void DirdFreeJcrPointers(JobControlRecord *jcr)
{
   if (jcr->file_bsock) {
      Dmsg0(200, "Close File bsock\n");
      jcr->file_bsock->close();
      delete jcr->file_bsock;
      jcr->file_bsock = NULL;
   }

   if (jcr->store_bsock) {
      Dmsg0(200, "Close Store bsock\n");
      jcr->store_bsock->close();
      delete jcr->store_bsock;
      jcr->store_bsock = NULL;
   }

   BfreeAndNull(jcr->sd_auth_key);
   BfreeAndNull(jcr->where);
   BfreeAndNull(jcr->backup_format);
   BfreeAndNull(jcr->RestoreBootstrap);
   BfreeAndNull(jcr->ar);

   FreeAndNullPoolMemory(jcr->JobIds);
   FreeAndNullPoolMemory(jcr->client_uname);
   FreeAndNullPoolMemory(jcr->attr);
   FreeAndNullPoolMemory(jcr->fname);
}

/**
 * Free the Job Control Record if no one is still using it.
 *  Called from main FreeJcr() routine in src/lib/jcr.c so
 *  that we can do our Director specific cleanup of the jcr.
 */
void DirdFreeJcr(JobControlRecord *jcr)
{
   Dmsg0(200, "Start dird FreeJcr\n");

   if (jcr->mig_jcr) {
      FreeJcr(jcr->mig_jcr);
      jcr->mig_jcr = NULL;
   }

   DirdFreeJcrPointers(jcr);

   if (jcr->term_wait_inited) {
      pthread_cond_destroy(&jcr->term_wait);
      jcr->term_wait_inited = false;
   }

   if (jcr->nextrun_ready_inited) {
      pthread_cond_destroy(&jcr->nextrun_ready);
      jcr->nextrun_ready_inited = false;
   }

   if (jcr->db_batch) {
      DbSqlClosePooledConnection(jcr, jcr->db_batch);
      jcr->db_batch = NULL;
      jcr->batch_started = false;
   }

   if (jcr->db) {
      DbSqlClosePooledConnection(jcr, jcr->db);
      jcr->db = NULL;
   }

   if (jcr->restore_tree_root) {
      FreeTree(jcr->restore_tree_root);
   }

   if (jcr->bsr) {
      libbareos::FreeBsr(jcr->bsr);
      jcr->bsr = NULL;
   }

   FreeAndNullPoolMemory(jcr->stime);
   FreeAndNullPoolMemory(jcr->fname);
   FreeAndNullPoolMemory(jcr->res.pool_source);
   FreeAndNullPoolMemory(jcr->res.npool_source);
   FreeAndNullPoolMemory(jcr->res.rpool_source);
   FreeAndNullPoolMemory(jcr->res.wstore_source);
   FreeAndNullPoolMemory(jcr->res.rstore_source);
   FreeAndNullPoolMemory(jcr->res.catalog_source);
   FreeAndNullPoolMemory(jcr->FDSecureEraseCmd);
   FreeAndNullPoolMemory(jcr->SDSecureEraseCmd);
   FreeAndNullPoolMemory(jcr->vf_jobids);

   /*
    * Delete lists setup to hold storage pointers
    */
   FreeRwstorage(jcr);

   jcr->job_end_callbacks.destroy();

   if (jcr->JobId != 0) {
      WriteStateFile(me->working_directory, "bareos-dir", GetFirstPortHostOrder(me->DIRaddrs));
   }

   FreePlugins(jcr);                 /* release instantiated plugins */

   Dmsg0(200, "End dird FreeJcr\n");
}

/**
 * The Job storage definition must be either in the Job record
 * or in the Pool record.  The Pool record overrides the Job record.
 */
void GetJobStorage(UnifiedStorageResource *store, JobResource *job, RunResource *run)
{
   if (run && run->pool && run->pool->storage) {
      store->store = (StorageResource *)run->pool->storage->first();
      PmStrcpy(store->store_source, _("Run pool override"));
      return;
   }
   if (run && run->storage) {
      store->store = run->storage;
      PmStrcpy(store->store_source, _("Run storage override"));
      return;
   }
   if (job->pool->storage) {
      store->store = (StorageResource *)job->pool->storage->first();
      PmStrcpy(store->store_source, _("Pool resource"));
   } else {
      if (job->storage) {
         store->store = (StorageResource *)job->storage->first();
         PmStrcpy(store->store_source, _("Job resource"));
      }
   }
}

/**
 * Set some defaults in the JobControlRecord necessary to
 * run. These items are pulled from the job
 * definition as defaults, but can be overridden
 * later either by the Run record in the Schedule resource,
 * or by the Console program.
 */
void SetJcrDefaults(JobControlRecord *jcr, JobResource *job)
{
   jcr->res.job = job;
   jcr->setJobType(job->JobType);
   jcr->setJobProtocol(job->Protocol);
   jcr->JobStatus = JS_Created;

   switch (jcr->getJobType()) {
   case JT_ADMIN:
      jcr->setJobLevel(L_NONE);
      break;
   case JT_ARCHIVE:
      jcr->setJobLevel(L_NONE);
      break;
   default:
      jcr->setJobLevel(job->JobLevel);
      break;
   }

   if (!jcr->fname) {
      jcr->fname = GetPoolMemory(PM_FNAME);
   }
   if (!jcr->res.pool_source) {
      jcr->res.pool_source = GetPoolMemory(PM_MESSAGE);
      PmStrcpy(jcr->res.pool_source, _("unknown source"));
   }
   if (!jcr->res.npool_source) {
      jcr->res.npool_source = GetPoolMemory(PM_MESSAGE);
      PmStrcpy(jcr->res.npool_source, _("unknown source"));
   }
   if (!jcr->res.catalog_source) {
      jcr->res.catalog_source = GetPoolMemory(PM_MESSAGE);
      PmStrcpy(jcr->res.catalog_source, _("unknown source"));
   }

   jcr->JobPriority = job->Priority;

   /*
    * Copy storage definitions -- deleted in dir_free_jcr above
    */
   if (job->storage) {
      CopyRwstorage(jcr, job->storage, _("Job resource"));
   } else {
      CopyRwstorage(jcr, job->pool->storage, _("Pool resource"));
   }
   jcr->res.client = job->client;

   if (jcr->res.client) {
      if (!jcr->client_name) {
         jcr->client_name = GetPoolMemory(PM_NAME);
      }
      PmStrcpy(jcr->client_name, jcr->res.client->hdr.name);
   }

   PmStrcpy(jcr->res.pool_source, _("Job resource"));
   jcr->res.pool = job->pool;
   jcr->res.full_pool = job->full_pool;
   jcr->res.inc_pool = job->inc_pool;
   jcr->res.diff_pool = job->diff_pool;

   if (job->pool->catalog) {
      jcr->res.catalog = job->pool->catalog;
      PmStrcpy(jcr->res.catalog_source, _("Pool resource"));
   } else {
      if (job->catalog) {
         jcr->res.catalog = job->catalog;
         PmStrcpy(jcr->res.catalog_source, _("Job resource"));
      } else {
         if (job->client) {
            jcr->res.catalog = job->client->catalog;
            PmStrcpy(jcr->res.catalog_source, _("Client resource"));
         } else {
            jcr->res.catalog = (CatalogResource *)GetNextRes(R_CATALOG, NULL);
            PmStrcpy(jcr->res.catalog_source, _("Default catalog"));
         }
      }
   }

   jcr->res.fileset = job->fileset;
   jcr->accurate = job->accurate;
   jcr->res.messages = job->messages;
   jcr->spool_data = job->spool_data;
   jcr->spool_size = job->spool_size;
   jcr->IgnoreDuplicateJobChecking = job->IgnoreDuplicateJobChecking;
   jcr->MaxRunSchedTime = job->MaxRunSchedTime;
   jcr->backup_format = bstrdup(job->backup_format);

   if (jcr->RestoreBootstrap) {
      free(jcr->RestoreBootstrap);
      jcr->RestoreBootstrap = NULL;
   }

   /*
    * This can be overridden by Console program
    */
   if (job->RestoreBootstrap) {
      jcr->RestoreBootstrap = bstrdup(job->RestoreBootstrap);
   }

   /*
    * This can be overridden by Console program
    */
   jcr->res.verify_job = job->verify_job;

   /*
    * If no default level given, set one
    */
   if (jcr->getJobLevel() == 0) {
      switch (jcr->getJobType()) {
      case JT_VERIFY:
         jcr->setJobLevel(L_VERIFY_CATALOG);
         break;
      case JT_BACKUP:
         jcr->setJobLevel(L_INCREMENTAL);
         break;
      case JT_RESTORE:
      case JT_ADMIN:
         jcr->setJobLevel(L_NONE);
         break;
      default:
         jcr->setJobLevel(L_FULL);
         break;
      }
   }
}

void CreateClones(JobControlRecord *jcr)
{
   /*
    * Fire off any clone jobs (run directives)
    */
   Dmsg2(900, "cloned=%d run_cmds=%p\n", jcr->cloned, jcr->res.job->run_cmds);
   if (!jcr->cloned && jcr->res.job->run_cmds) {
      char *runcmd;
      JobId_t jobid;
      JobResource *job = jcr->res.job;
      POOLMEM *cmd = GetPoolMemory(PM_FNAME);

      UaContext *ua = new_ua_context(jcr);
      ua->batch = true;
      foreach_alist(runcmd, job->run_cmds) {
         cmd = edit_job_codes(jcr, cmd, runcmd, "", job_code_callback_director);
         Mmsg(ua->cmd, "run %s cloned=yes", cmd);
         Dmsg1(900, "=============== Clone cmd=%s\n", ua->cmd);
         ParseUaArgs(ua);                 /* parse command */

         jobid = DoRunCmd(ua, ua->cmd);
         if (!jobid) {
            Jmsg(jcr, M_ERROR, 0, _("Could not start clone job: \"%s\".\n"), ua->cmd);
         } else {
            Jmsg(jcr, M_INFO, 0, _("Clone JobId %d started.\n"), jobid);
         }
      }
      FreeUaContext(ua);
      FreePoolMemory(cmd);
   }
}

/**
 * Given: a JobId in jcr->previous_jr.JobId,
 *  this subroutine writes a bsr file to restore that job.
 * Returns: -1 on error
 *           number of files if OK
 */
int CreateRestoreBootstrapFile(JobControlRecord *jcr)
{
   RestoreContext rx;
   UaContext *ua;
   int files;

   memset(&rx, 0, sizeof(rx));
   rx.bsr = new_bsr();
   rx.JobIds = (char *)"";
   rx.bsr->JobId = jcr->previous_jr.JobId;
   ua = new_ua_context(jcr);
   if (!CompleteBsr(ua, rx.bsr)) {
      files = -1;
      goto bail_out;
   }
   rx.bsr->fi = new_findex();
   rx.bsr->fi->findex = 1;
   rx.bsr->fi->findex2 = jcr->previous_jr.JobFiles;
   jcr->ExpectedFiles = WriteBsrFile(ua, rx);
   if (jcr->ExpectedFiles == 0) {
      files = 0;
      goto bail_out;
   }
   FreeUaContext(ua);
   directordaemon::FreeBsr(rx.bsr);
   jcr->needs_sd = true;
   return jcr->ExpectedFiles;

bail_out:
   FreeUaContext(ua);
   directordaemon::FreeBsr(rx.bsr);
   return files;
}

/* TODO: redirect command ouput to job log */
bool RunConsoleCommand(JobControlRecord *jcr, const char *cmd)
{
   UaContext *ua;
   bool ok;
   JobControlRecord *ljcr = new_control_jcr("-RunScript-", JT_CONSOLE);
   ua = new_ua_context(ljcr);
   /* run from runscript and check if commands are authorized */
   ua->runscript = true;
   Mmsg(ua->cmd, "%s", cmd);
   Dmsg1(100, "Console command: %s\n", ua->cmd);
   ParseUaArgs(ua);
   ok = Do_a_command(ua);
   FreeUaContext(ua);
   FreeJcr(ljcr);
   return ok;
}
