/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * BAREOS Director -- vbackup.c -- responsible for doing virtual backup jobs or
 *                    in other words, consolidation or synthetic backups.
 *
 * Kern Sibbald, July MMVIII
 *
 * Basic tasks done here:
 *    Open DB and create records for this job.
 *    Figure out what Jobs to copy.
 *    Open Message Channel with Storage daemon to tell him a job will be starting.
 *    Open connection with File daemon and pass him commands to do the backup.
 *    When the File daemon finishes the job, update the DB.
 */

#include "bareos.h"
#include "dird.h"

static const int dbglevel = 10;

static bool create_bootstrap_file(JCR *jcr, char *jobids);

/*
 * Called here before the job is run to do the job specific setup.
 */
bool do_native_vbackup_init(JCR *jcr)
{
   const char *storage_source;

   if (!get_or_create_fileset_record(jcr)) {
      Dmsg1(dbglevel, "JobId=%d no FileSet\n", (int)jcr->JobId);
      return false;
   }

   apply_pool_overrides(jcr);

   if (!allow_duplicate_job(jcr)) {
      return false;
   }

   jcr->jr.PoolId = get_or_create_pool_record(jcr, jcr->res.pool->name());
   if (jcr->jr.PoolId == 0) {
      Dmsg1(dbglevel, "JobId=%d no PoolId\n", (int)jcr->JobId);
      Jmsg(jcr, M_FATAL, 0, _("Could not get or create a Pool record.\n"));
      return false;
   }

   /*
    * Note, at this point, pool is the pool for this job.
    * We transfer it to rpool (read pool), and a bit later,
    * pool will be changed to point to the write pool,
    * which comes from pool->NextPool.
    */
   jcr->res.rpool = jcr->res.pool;    /* save read pool */
   pm_strcpy(jcr->res.rpool_source, jcr->res.pool_source);

   /*
    * If pool storage specified, use it for restore
    */
   copy_rstorage(jcr, jcr->res.pool->storage, _("Pool resource"));

   Dmsg2(dbglevel, "Read pool=%s (From %s)\n", jcr->res.rpool->name(), jcr->res.rpool_source);

   jcr->start_time = time(NULL);
   jcr->jr.StartTime = jcr->start_time;
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
   }

   /*
    * See if there is a next pool override.
    */
   if (jcr->res.run_next_pool_override) {
      pm_strcpy(jcr->res.npool_source, _("Run NextPool override"));
      pm_strcpy(jcr->res.pool_source, _("Run NextPool override"));
      storage_source = _("Storage from Run NextPool override");
   } else {
      /*
       * See if there is a next pool override in the Job definition.
       */
      if (jcr->res.job->next_pool) {
         jcr->res.next_pool = jcr->res.job->next_pool;
         pm_strcpy(jcr->res.npool_source, _("Job's NextPool resource"));
         pm_strcpy(jcr->res.pool_source, _("Job's NextPool resource"));
         storage_source = _("Storage from Job's NextPool resource");
      } else {
         /*
          * Fall back to the pool's NextPool definition.
          */
         jcr->res.next_pool = jcr->res.pool->NextPool;
         pm_strcpy(jcr->res.npool_source, _("Job Pool's NextPool resource"));
         pm_strcpy(jcr->res.pool_source, _("Job Pool's NextPool resource"));
         storage_source = _("Storage from Pool's NextPool resource");
      }
   }

   /*
    * If the original backup pool has a NextPool, make sure a
    * record exists in the database. Note, in this case, we
    * will be migrating from pool to pool->NextPool.
    */
   if (jcr->res.next_pool) {
      jcr->jr.PoolId = get_or_create_pool_record(jcr, jcr->res.next_pool->name());
      if (jcr->jr.PoolId == 0) {
         return false;
      }
   }

   if (!set_migration_wstorage(jcr, jcr->res.pool, jcr->res.next_pool, storage_source)) {
      return false;
   }

   jcr->res.pool = jcr->res.next_pool;

   Dmsg2(dbglevel, "Write pool=%s read rpool=%s\n", jcr->res.pool->name(), jcr->res.rpool->name());

// create_clones(jcr);

   return true;
}

/*
 * Do a virtual backup, which consolidates all previous backups into a sort of synthetic Full.
 *
 * Returns:  false on failure
 *           true  on success
 */
bool do_native_vbackup(JCR *jcr)
{
   char *p;
   BSOCK *sd;
   char *jobids;
   char ed1[100];
   int JobLevel_of_first_job;

   if (!jcr->res.rstorage) {
      Jmsg(jcr, M_FATAL, 0, _("No storage for reading given.\n"));
      return false;
   }

   if (!jcr->res.wstorage) {
      Jmsg(jcr, M_FATAL, 0, _("No storage for writing given.\n"));
      return false;
   }

   Dmsg2(100, "rstorage=%p wstorage=%p\n", jcr->res.rstorage, jcr->res.wstorage);
   Dmsg2(100, "Read store=%s, write store=%s\n",
         ((STORERES *)jcr->res.rstorage->first())->name(),
         ((STORERES *)jcr->res.wstorage->first())->name());

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start Virtual Backup JobId %s, Job=%s\n"),
        edit_uint64(jcr->JobId, ed1), jcr->Job);

   /*
    * TODO: What do we do with this message?
    */
   if (!jcr->accurate) {
      Jmsg(jcr, M_WARNING, 0,
           _("This Job is not an Accurate backup so is not equivalent to a Full backup.\n"));
   }

   /*
    * See if we already got a list of jobids to use.
    */
   if (jcr->vf_jobids) {
      Dmsg1(10, "jobids=%s\n", jcr->vf_jobids);
      jobids = bstrdup(jcr->vf_jobids);
   } else {
      db_list_ctx jobids_ctx;

      /*
       * If we are doing always incremental, we need to limit the search to
       * only include incrementals that are older than (now - AlwaysIncrementalInterval)
       */
      if (jcr->res.job->AlwaysIncremental && jcr->res.job->AlwaysIncrementalInterval) {
         char sdt[50];
         time_t starttime = time(NULL) - jcr->res.job->AlwaysIncrementalInterval;

         bstrftimes(sdt, sizeof(sdt), starttime);
         jcr->jr.StartTime = starttime;
         Jmsg(jcr, M_INFO, 0, _("Always incremental consolidation: consolidating jobs older than %s.\n"), sdt);
      }

      db_accurate_get_jobids(jcr, jcr->db, &jcr->jr, &jobids_ctx);
      Dmsg1(10, "consolidate candidates:  %s.\n", jobids_ctx.list);

      if (jobids_ctx.count == 0) {
         if (jcr->res.job->AlwaysIncremental && jcr->res.job->AlwaysIncrementalInterval) {
            Jmsg(jcr, M_WARNING, 0, _("Always incremental consolidation: No jobs to consolidate found.\n"));
            return true;
         } else {
            Jmsg(jcr, M_FATAL, 0, _("No previous Jobs found.\n"));
            return false;
         }
      } else if (jobids_ctx.count == 1) {
         /*
          * Consolidation of one job does not make sense, we leave it like it is
          */
         Jmsg(jcr, M_WARNING, 0, _("Exactly one job selected for consolidation - skipping.\n"));
         return true;
      }

      if (jcr->res.job->AlwaysIncremental && jcr->res.job->AlwaysIncrementalNumber) {
         int32_t incrementals_total;
         int32_t incrementals_to_consolidate;

         /*
          * Calculate limit for query. we specify how many incrementals should be left.
          * the limit is number of consolidate candidates - number required - 1
          */
         incrementals_total = jobids_ctx.count - 1;
         incrementals_to_consolidate = incrementals_total - jcr->res.job->AlwaysIncrementalNumber;

         Dmsg2(10, "Incrementals found/required. (%d/%d).\n", incrementals_total, jcr->res.job->AlwaysIncrementalNumber);
         if ((incrementals_to_consolidate + 1 ) > 1) {
            jcr->jr.limit = incrementals_to_consolidate + 1;
            Dmsg3(10, "total: %d, to_consolidate: %d, limit: %d.\n", incrementals_total, incrementals_to_consolidate, jcr->jr.limit);
            jobids_ctx.reset();
            db_accurate_get_jobids(jcr, jcr->db, &jcr->jr, &jobids_ctx);
            Dmsg1(10, "consolidate ids after limit: %s.\n", jobids_ctx.list);
         } else {
            Jmsg(jcr, M_WARNING, 0, _("not enough incrementals, not consolidating\n"));
            return true;
         }
      }

      jobids = bstrdup(jobids_ctx.list);
   }

   Jmsg(jcr, M_INFO, 0, _("Consolidating JobIds %s\n"), jobids);

   /*
    * Find oldest Jobid, get the db record and find its level
    */
   p = strchr(jobids, ',');                /* find oldest jobid */
   if (p) {
      *p = '\0';
   }

   memset(&jcr->previous_jr, 0, sizeof(jcr->previous_jr));
   jcr->previous_jr.JobId = str_to_int64(jobids);
   Dmsg1(10, "Previous JobId=%s\n", jobids);

   /*
    * See if we need to restore the stripped ','
    */
   if (p) {
      *p = ',';
   }

   if (!db_get_job_record(jcr, jcr->db, &jcr->previous_jr)) {
      Jmsg(jcr, M_FATAL, 0, _("Error getting Job record for first Job: ERR=%s"), db_strerror(jcr->db));
      goto bail_out;
   }

   JobLevel_of_first_job = jcr->previous_jr.JobLevel;
   Dmsg2(10, "Level of first consolidated job %d: %s\n", jcr->previous_jr.JobId, job_level_to_str(JobLevel_of_first_job));

   /*
    * Now we find the newest job that ran and store its info in
    * the previous_jr record. We will set our times to the
    * values from that job so that anything changed after that
    * time will be picked up on the next backup.
    */
   p = strrchr(jobids, ',');                /* find newest jobid */
   if (p) {
      p++;
   } else {
      p = jobids;
   }

   memset(&jcr->previous_jr, 0, sizeof(jcr->previous_jr));
   jcr->previous_jr.JobId = str_to_int64(p);
   Dmsg1(10, "Previous JobId=%s\n", p);

   if (!db_get_job_record(jcr, jcr->db, &jcr->previous_jr)) {
      Jmsg(jcr, M_FATAL, 0, _("Error getting Job record for previous Job: ERR=%s"),
           db_strerror(jcr->db));
      goto bail_out;
   }

   if (!create_bootstrap_file(jcr, jobids)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not get or create the FileSet record.\n"));
      goto bail_out;
   }

   /*
    * Open a message channel connection with the Storage
    * daemon. This is to let him know that our client
    * will be contacting him for a backup  session.
    */
   Dmsg0(110, "Open connection with storage daemon\n");
   jcr->setJobStatus(JS_WaitSD);

   /*
    * Start conversation with Storage daemon
    */
   if (!connect_to_storage_daemon(jcr, 10, me->SDConnectTimeout, true)) {
      goto bail_out;
   }
   sd = jcr->store_bsock;

   /*
    * Now start a job with the Storage daemon
    */
   if (!start_storage_daemon_job(jcr, jcr->res.rstorage, jcr->res.wstorage, /* send_bsr */ true)) {
      goto bail_out;
   }
   Dmsg0(100, "Storage daemon connection OK\n");

   /*
    * We re-update the job start record so that the start
    * time is set after the run before job.  This avoids
    * that any files created by the run before job will
    * be saved twice.  They will be backed up in the current
    * job, but not in the next one unless they are changed.
    * Without this, they will be backed up in this job and
    * in the next job run because in that case, their date
    * is after the start of this run.
    */
   jcr->start_time = time(NULL);
   jcr->jr.StartTime = jcr->start_time;
   jcr->jr.JobTDate = jcr->start_time;
   jcr->setJobStatus(JS_Running);

   /*
    * Update job start record
    */
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      goto bail_out;
   }

   /*
    * Declare the job started to start the MaxRunTime check
    */
   jcr->setJobStarted();

   /*
    * Start the job prior to starting the message thread below
    * to avoid two threads from using the BSOCK structure at
    * the same time.
    */
   if (!sd->fsend("run")) {
      goto bail_out;
   }

   /*
    * Now start a Storage daemon message thread
    */
   if (!start_storage_daemon_message_thread(jcr)) {
      goto bail_out;
   }

   jcr->setJobStatus(JS_Running);

   /*
    * Pickup Job termination data
    * Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/JobErrors
    */
   wait_for_storage_daemon_termination(jcr);
   jcr->setJobStatus(jcr->SDJobStatus);
   db_write_batch_file_records(jcr);    /* used by bulk batch file insert */
   if (!jcr->is_JobStatus(JS_Terminated)) {
      goto bail_out;
   }

   native_vbackup_cleanup(jcr, jcr->JobStatus, JobLevel_of_first_job);

   /*
    * Remove the successfully consolidated jobids from the database
    */
    if (jcr->res.job->AlwaysIncremental && jcr->res.job->AlwaysIncrementalInterval) {
      UAContext *ua;
      ua = new_ua_context(jcr);
      purge_jobs_from_catalog(ua, jobids);
      Jmsg(jcr, M_INFO, 0, _("purged JobIds %s as they were consolidated into Job %s\n"), jobids,  edit_uint64(jcr->JobId, ed1) );
    }

   free(jobids);
   return true;

bail_out:
   free(jobids);
   return false;
}

/*
 * Release resources allocated during backup.
 */
void native_vbackup_cleanup(JCR *jcr, int TermCode, int JobLevel)
{
   char ec1[30], ec2[30];
   char term_code[100];
   const char *term_msg;
   int msg_type = M_INFO;
   CLIENT_DBR cr;
   POOL_MEM query(PM_MESSAGE);

   Dmsg2(100, "Enter backup_cleanup %d %c\n", TermCode, TermCode);
   memset(&cr, 0, sizeof(cr));

   switch (jcr->JobStatus) {
   case JS_Terminated:
   case JS_Warnings:
      jcr->jr.JobLevel = JobLevel;      /* We want this to appear as what the first consolidated job was */
      Jmsg(jcr, M_INFO, 0, _("Joblevel was set to joblevel of first consolidated job: %s\n"), job_level_to_str(JobLevel));
      break;
   default:
      break;
   }

   jcr->JobFiles = jcr->SDJobFiles;
   jcr->JobBytes = jcr->SDJobBytes;

   if (jcr->getJobStatus() == JS_Terminated &&
       (jcr->JobErrors || jcr->SDErrors)) {
      TermCode = JS_Warnings;
   }

   update_job_end(jcr, TermCode);

   /*
    * Update final items to set them to the previous job's values
    */
   Mmsg(query, "UPDATE Job SET StartTime='%s',EndTime='%s',"
               "JobTDate=%s WHERE JobId=%s",
        jcr->previous_jr.cStartTime, jcr->previous_jr.cEndTime,
        edit_uint64(jcr->previous_jr.JobTDate, ec1),
        edit_uint64(jcr->JobId, ec2));
   db_sql_query(jcr->db, query.c_str());

   /*
    * Get the fully updated job record
    */
   if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Job record for Job report: ERR=%s"),
           db_strerror(jcr->db));
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   bstrncpy(cr.Name, jcr->res.client->name(), sizeof(cr.Name));
   if (!db_get_client_record(jcr, jcr->db, &cr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Client record for Job report: ERR=%s"),
           db_strerror(jcr->db));
   }

   update_bootstrap_file(jcr);

   switch (jcr->JobStatus) {
   case JS_Terminated:
      term_msg = _("Backup OK");
      break;
   case JS_Warnings:
      term_msg = _("Backup OK -- with warnings");
      break;
   case JS_FatalError:
   case JS_ErrorTerminated:
      term_msg = _("*** Backup Error ***");
      msg_type = M_ERROR;          /* Generate error message */
      if (jcr->store_bsock) {
         jcr->store_bsock->signal(BNET_TERMINATE);
         if (jcr->SD_msg_chan_started) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   case JS_Canceled:
      term_msg = _("Backup Canceled");
      if (jcr->store_bsock) {
         jcr->store_bsock->signal(BNET_TERMINATE);
         if (jcr->SD_msg_chan_started) {
            pthread_cancel(jcr->SD_msg_chan);
         }
      }
      break;
   default:
      term_msg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
      break;
   }

   generate_backup_summary(jcr, &cr, msg_type, term_msg);

   Dmsg0(100, "Leave vbackup_cleanup()\n");
}

/*
 * This callback routine is responsible for inserting the
 *  items it gets into the bootstrap structure. For each JobId selected
 *  this routine is called once for each file. We do not allow
 *  duplicate filenames, but instead keep the info from the most
 *  recent file entered (i.e. the JobIds are assumed to be sorted)
 *
 *   See uar_sel_files in sql_cmds.c for query that calls us.
 *      row[0]=Path, row[1]=Filename, row[2]=FileIndex
 *      row[3]=JobId row[4]=LStat
 */
static int insert_bootstrap_handler(void *ctx, int num_fields, char **row)
{
   JobId_t JobId;
   int FileIndex;
   RBSR *bsr = (RBSR *)ctx;

   JobId = str_to_int64(row[3]);
   FileIndex = str_to_int64(row[2]);
   add_findex(bsr, JobId, FileIndex);
   return 0;
}

static bool create_bootstrap_file(JCR *jcr, char *jobids)
{
   RESTORE_CTX rx;
   UAContext *ua;

   memset(&rx, 0, sizeof(rx));
   rx.bsr = new_bsr();
   ua = new_ua_context(jcr);
   rx.JobIds = jobids;

   if (!db_open_batch_connection(jcr, jcr->db)) {
      Jmsg0(jcr, M_FATAL, 0, "Can't get batch sql connexion");
      return false;
   }

   if (!db_get_file_list(jcr, jcr->db_batch, jobids, false /* don't use md5 */,
                         true /* use delta */,
                         insert_bootstrap_handler, (void *)rx.bsr)) {
      Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(jcr->db_batch));
   }

   complete_bsr(ua, rx.bsr);
   jcr->ExpectedFiles = write_bsr_file(ua, rx);
   if (debug_level >= 10) {
      Dmsg1(000,  "Found %d files to consolidate.\n", jcr->ExpectedFiles);
   }
   if (jcr->ExpectedFiles == 0) {
      free_ua_context(ua);
      free_bsr(rx.bsr);
      return false;
   }
   free_ua_context(ua);
   free_bsr(rx.bsr);
   return true;
}
