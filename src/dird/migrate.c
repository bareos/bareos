/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * BAREOS Director -- migrate.c -- responsible for doing migration and copy jobs.
 *
 * Also handles Copy jobs (March 2008)
 *
 * Kern Sibbald, September 2004
 * SD-SD Migration by Marco van Wieringen, November 2012
 *
 * Basic tasks done here:
 *    Open DB and create records for this job.
 *    Open Message Channel with Storage daemon to tell him a job will be starting.
 *    Open connection with Storage daemon and pass him commands to do the backup.
 *    When the Storage daemon finishes the job, update the DB.
 */

#include "bareos.h"
#include "dird.h"
#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

/* Commands sent to other storage daemon */
static char replicatecmd[]  =
   "replicate Job=%s address=%s port=%d ssl=%d Authorization=%s\n";

static const int dbglevel = 10;

static int getJob_to_migrate(JCR *jcr);
struct idpkt;
static bool regex_find_jobids(JCR *jcr, idpkt *ids, const char *query1,
                 const char *query2, const char *type);
static bool find_mediaid_then_jobids(JCR *jcr, idpkt *ids, const char *query1,
                 const char *type);
static bool find_jobids_from_mediaid_list(JCR *jcr, idpkt *ids, const char *type);
static bool find_jobids_of_pool_uncopied_jobs(JCR *jcr, idpkt *ids);
static void start_migration_job(JCR *jcr);
static int get_next_dbid_from_list(char **p, DBId_t *DBId);
static bool set_migration_next_pool(JCR *jcr, POOLRES **pool);

/*
 * See if two storage definitions point to the same Storage Daemon.
 *
 * We compare:
 *  - address
 *  - SDport
 *  - password
 */
static inline bool is_same_storage_daemon(STORERES *rstore, STORERES *wstore)
{
   return rstore->SDport == wstore->SDport &&
          bstrcasecmp(rstore->address, wstore->address) &&
          bstrcasecmp(rstore->password.value, wstore->password.value);
}

/*
 * Called here before the job is run to do the job
 * specific setup.  Note, one of the important things to
 * complete in this init code is to make the definitive
 * choice of input and output storage devices.  This is
 * because immediately after the init, the job is queued
 * in the jobq.c code, and it checks that all the resources
 * (storage resources in particular) are available, so these
 * must all be properly defined.
 *
 * - previous_jr refers to the job DB record of the Job that is
 *   going to be migrated.
 * - prev_job refers to the job resource of the Job that is
 *   going to be migrated.
 * - jcr is the jcr for the current "migration" job. It is a
 *   control job that is put in the DB as a migration job, which
 *   means that this job migrated a previous job to a new job.
 *   No Volume or File data is associated with this control
 *   job.
 * - mig_jcr refers to the newly migrated job that is run by
 *   the current jcr. It is a backup job that moves (migrates) the
 *   data written for the previous_jr into the new pool. This
 *   job (mig_jcr) becomes the new backup job that replaces
 *   the original backup job. Note, when this is a migration
 *   on a single storage daemon this jcr is not really run. It
 *   is simply attached to the current jcr. It will show up in
 *   the Director's status output, but not in the SD or FD, both of
 *   which deal only with the current migration job (i.e. jcr).
 *   When this is is a migration between two storage daemon this
 *   mig_jcr is used to control the second connection to the
 *   remote storage daemon.
 */
bool do_migration_init(JCR *jcr)
{
   POOLRES *pool = NULL;
   JOBRES *job, *prev_job;
   JCR *mig_jcr;                   /* newly migrated job */
   int count;

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
   Dmsg2(dbglevel, "Read pool=%s (From %s)\n", jcr->res.rpool->name(), jcr->res.rpool_source);

   /*
    * If we find a job or jobs to migrate it is previous_jr.JobId
    */
   count = getJob_to_migrate(jcr);
   if (count < 0) {
      return false;
   }

   if (count == 0) {
      set_migration_next_pool(jcr, &pool);
      return true;                    /* no work */
   }

   Dmsg1(dbglevel, "Back from getJob_to_migrate JobId=%d\n", (int)jcr->JobId);

   if (jcr->previous_jr.JobId == 0) {
      Dmsg1(dbglevel, "JobId=%d no previous JobId\n", (int)jcr->JobId);
      Jmsg(jcr, M_INFO, 0, _("No previous Job found to %s.\n"), jcr->get_ActionName());
      set_migration_next_pool(jcr, &pool);
      return true;                    /* no work */
   }

   if (create_restore_bootstrap_file(jcr) < 0) {
      Jmsg(jcr, M_FATAL, 0, _("Create bootstrap file failed.\n"));
      return false;
   }

   if (jcr->previous_jr.JobId == 0 || jcr->ExpectedFiles == 0) {
      jcr->setJobStatus(JS_Terminated);
      Dmsg1(dbglevel, "JobId=%d expected files == 0\n", (int)jcr->JobId);
      if (jcr->previous_jr.JobId == 0) {
         Jmsg(jcr, M_INFO, 0, _("No previous Job found to %s.\n"), jcr->get_ActionName());
      } else {
         Jmsg(jcr, M_INFO, 0, _("Previous Job has no data to %s.\n"), jcr->get_ActionName());
      }
      set_migration_next_pool(jcr, &pool);
      return true;                    /* no work */
   }

   Dmsg5(dbglevel, "JobId=%d: Current: Name=%s JobId=%d Type=%c Level=%c\n",
         (int)jcr->JobId, jcr->jr.Name, (int)jcr->jr.JobId, jcr->jr.JobType, jcr->jr.JobLevel);

   LockRes();
   job = (JOBRES *)GetResWithName(R_JOB, jcr->jr.Name);
   prev_job = (JOBRES *)GetResWithName(R_JOB, jcr->previous_jr.Name);
   UnlockRes();

   if (!job) {
      Jmsg(jcr, M_FATAL, 0, _("Job resource not found for \"%s\".\n"), jcr->jr.Name);
      return false;
   }

   if (!prev_job) {
      Jmsg(jcr, M_FATAL, 0, _("Previous Job resource not found for \"%s\".\n"),
           jcr->previous_jr.Name);
      return false;
   }

   /*
    * Copy the actual level setting of the previous Job to this Job.
    * This overrides the dummy backup level given to the migrate/copy Job and replaces it
    * with the actual level the backup run at.
    */
   jcr->setJobLevel(prev_job->JobLevel);

   /*
    * If the current Job has no explicit client set use the client setting of the previous Job.
    */
   if (!jcr->res.client && prev_job->client) {
      jcr->res.client = prev_job->client;
      if (!jcr->client_name) {
         jcr->client_name = get_pool_memory(PM_NAME);
      }
      pm_strcpy(jcr->client_name, jcr->res.client->hdr.name);
   }

   /*
    * If the current Job has no explicit fileset set use the client setting of the previous Job.
    */
   if (!jcr->res.fileset) {
      jcr->res.fileset = prev_job->fileset;
   }

   /*
    * See if spooling data is not enabled yet. If so turn on spooling if requested in job
    */
   if (!jcr->spool_data) {
      jcr->spool_data = job->spool_data;
   }

   /*
    * Create a migration jcr
    */
   mig_jcr = jcr->mig_jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   memcpy(&mig_jcr->previous_jr, &jcr->previous_jr, sizeof(mig_jcr->previous_jr));

   /*
    * Turn the mig_jcr into a "real" job that takes on the aspects of
    * the previous backup job "prev_job". We only don't want it to
    * ever send any messages to the database or mail messages when
    * we are doing a migrate or copy to a remote storage daemon. When
    * doing such operations the mig_jcr is used for tracking some of
    * the remote state and it might want to send some captured state
    * info on tear down of the mig_jcr so we call setup_job with the
    * suppress_output argument set to true (e.g. don't init messages
    * and set the jcr suppress_output boolean to true).
    */
   set_jcr_defaults(mig_jcr, prev_job);

   /*
    * Don't let WatchDog checks Max*Time value on this Job
    */
   mig_jcr->no_maxtime = true;

   /*
    * Don't check for duplicates on migration and copy jobs
    */
   mig_jcr->IgnoreDuplicateJobChecking = true;

   if (!setup_job(mig_jcr, true)) {
      Jmsg(jcr, M_FATAL, 0, _("setup job failed.\n"));
      return false;
   }

   /*
    * Keep track that the mig_jcr has a controling JCR.
    */
   mig_jcr->cjcr = jcr;

   /*
    * Now reset the job record from the previous job
    */
   memcpy(&mig_jcr->jr, &jcr->previous_jr, sizeof(mig_jcr->jr));

   /*
    * Update the jr to reflect the new values of PoolId and JobId.
    */
   mig_jcr->jr.PoolId = jcr->jr.PoolId;
   mig_jcr->jr.JobId = mig_jcr->JobId;

   Dmsg4(dbglevel, "mig_jcr: Name=%s JobId=%d Type=%c Level=%c\n",
         mig_jcr->jr.Name, (int)mig_jcr->jr.JobId, mig_jcr->jr.JobType,
         mig_jcr->jr.JobLevel);

   if (set_migration_next_pool(jcr, &pool)) {
      /*
       * If pool storage specified, use it as source
       */
      copy_rstorage(mig_jcr, pool->storage, _("Pool resource"));
      copy_rstorage(jcr, pool->storage, _("Pool resource"));

      mig_jcr->res.pool = jcr->res.pool;
      mig_jcr->res.next_pool = jcr->res.next_pool;
      mig_jcr->jr.PoolId = jcr->jr.PoolId;
   }

   /*
    * Get the storage that was used for the original Job.
    * This only happens when the original pool used doesn't have an explicit storage.
    */
   if (!jcr->rstorage) {
      copy_rstorage(jcr, prev_job->storage, _("previous Job"));
   }

   /*
    * See if the read and write storage is the same.
    * When they are we do the migrate/copy over one SD connection
    * otherwise we open a connection to the reading SD and a second
    * one to the writing SD.
    */
   jcr->remote_replicate = !is_same_storage_daemon(jcr->res.rstore, jcr->res.wstore);

   return true;
}

/*
 * set_migration_next_pool() called by do_migration_init()
 * at differents stages.
 *
 * The idea here is to make a common subroutine for the
 * NextPool's search code and to permit do_migration_init()
 * to return with NextPool set in jcr struct.
 */
static bool set_migration_next_pool(JCR *jcr, POOLRES **retpool)
{
   POOL_DBR pr;
   char ed1[100];
   POOLRES *pool;
   const char *storage_source;

   /*
    * Get the PoolId used with the original job. Then
    * find the pool name from the database record.
    */
   memset(&pr, 0, sizeof(pr));
   pr.PoolId = jcr->jr.PoolId;
   if (!db_get_pool_record(jcr, jcr->db, &pr)) {
      Jmsg(jcr, M_FATAL, 0, _("Pool for JobId %s not in database. ERR=%s\n"),
           edit_int64(pr.PoolId, ed1), db_strerror(jcr->db));
         return false;
   }

   /*
    * Get the pool resource corresponding to the original job
    */
   pool = (POOLRES *)GetResWithName(R_POOL, pr.Name);
   *retpool = pool;
   if (!pool) {
      Jmsg(jcr, M_FATAL, 0, _("Pool resource \"%s\" not found.\n"), pr.Name);
      return false;
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
         jcr->res.next_pool = pool->NextPool;
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

   if (!set_migration_wstorage(jcr, pool, jcr->res.next_pool, storage_source)) {
      return false;
   }

   jcr->res.pool = jcr->res.next_pool;
   Dmsg2(dbglevel, "Write pool=%s read rpool=%s\n", jcr->res.pool->name(), jcr->res.rpool->name());

   return true;
}

/*
 * Do a Migration of a previous job
 *
 * Returns:  false on failure
 *           true  on success
 */
bool do_migration(JCR *jcr)
{
   char ed1[100];
   bool retval = false;
   JCR *mig_jcr = jcr->mig_jcr;    /* newly migrated job */

   /*
    * If mig_jcr is NULL, there is nothing to do for this job,
    * so set a normal status, cleanup and return OK.
    */
   if (!mig_jcr) {
      jcr->setJobStatus(JS_Terminated);
      migration_cleanup(jcr, jcr->JobStatus);
      return true;
   }

   if (!db_get_job_record(jcr, jcr->db, &jcr->previous_jr)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not get job record for JobId %s to %s. ERR=%s"),
           edit_int64(jcr->previous_jr.JobId, ed1),
           jcr->get_ActionName(),
           db_strerror(jcr->db));
      jcr->setJobStatus(JS_Terminated);
      migration_cleanup(jcr, jcr->JobStatus);
      return true;
   }

   /*
    * Make sure this job was not already migrated
    */
   if (jcr->previous_jr.JobType != JT_BACKUP &&
       jcr->previous_jr.JobType != JT_JOB_COPY) {
      Jmsg(jcr, M_INFO, 0, _("JobId %s already %s probably by another Job. %s stopped.\n"),
           edit_int64(jcr->previous_jr.JobId, ed1),
           jcr->get_ActionName(true),
           jcr->get_OperationName());
      jcr->setJobStatus(JS_Terminated);
      migration_cleanup(jcr, jcr->JobStatus);
      return true;
   }

   /*
    * Sanity check that we are not using the same storage for reading and writing.
    */
   if (bstrcmp(((STORERES *)jcr->rstorage->first())->name(),
                ((STORERES *)jcr->wstorage->first())->name())) {
      Jmsg(jcr, M_FATAL, 0, _("JobId %s cannot %s using the same read and write storage.\n"),
           edit_int64(jcr->previous_jr.JobId, ed1),
           jcr->get_OperationName());
      jcr->setJobStatus(JS_Terminated);
      migration_cleanup(jcr, jcr->JobStatus);
      return true;
   }

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start %s JobId %s, Job=%s\n"),
        jcr->get_OperationName(), edit_uint64(jcr->JobId, ed1), jcr->Job);

   Dmsg2(dbglevel, "Read store=%s, write store=%s\n",
         ((STORERES *)jcr->rstorage->first())->name(),
         ((STORERES *)jcr->wstorage->first())->name());

   if (!jcr->remote_replicate) {
      /*
       * Open a message channel connection with the Storage daemon.
       */
      Dmsg0(110, "Open connection with storage daemon\n");
      jcr->setJobStatus(JS_WaitSD);
      mig_jcr->setJobStatus(JS_WaitSD);

      /*
       * Start conversation with Storage daemon
       */
      if (!connect_to_storage_daemon(jcr, 10, me->SDConnectTimeout, true)) {
         return false;
      }

      /*
       * Now start a job with the Storage daemon
       */
      if (!start_storage_daemon_job(jcr, jcr->rstorage, jcr->wstorage, /* send_bsr */ true)) {
         return false;
      }

      Dmsg0(150, "Storage daemon connection OK\n");
   } else {
      alist *wstorage;

      /*
       * See if we need to apply any bandwidth limiting.
       * We search the bandwidth limiting in the following way:
       * - Job bandwith limiting
       * - Writing Storage Daemon bandwidth limiting
       * - Reading Storage Daemon bandwidth limiting
       */
      if (jcr->res.job->max_bandwidth > 0) {
         jcr->max_bandwidth = jcr->res.job->max_bandwidth;
      } else if (jcr->res.wstore->max_bandwidth > 0) {
         jcr->max_bandwidth = jcr->res.wstore->max_bandwidth;
      } else if (jcr->res.rstore->max_bandwidth > 0) {
         jcr->max_bandwidth = jcr->res.rstore->max_bandwidth;
      }

      /*
       * Open a message channel connection with the Reading Storage daemon.
       */
      Dmsg0(110, "Open connection with reading storage daemon\n");

      /*
       * Clear the wstore of the jcr and assign it to the mig_jcr so
       * the jcr is connected to the reading storage daemon and the
       * mig_jcr to the writing storage daemon.
       */
      mig_jcr->res.wstore = jcr->res.wstore;
      jcr->res.wstore = NULL;

      /*
       * Swap the wstorage between the jcr and the mig_jcr.
       */
      wstorage = mig_jcr->wstorage;
      mig_jcr->wstorage = jcr->wstorage;
      jcr->wstorage = wstorage;

      /*
       * Start conversation with Reading Storage daemon
       */
      jcr->setJobStatus(JS_WaitSD);
      if (!connect_to_storage_daemon(jcr, 10, me->SDConnectTimeout, true)) {
         goto bail_out;
      }

      /*
       * Open a message channel connection with the Writing Storage daemon.
       */
      Dmsg0(110, "Open connection with writing storage daemon\n");

      /*
       * Start conversation with Writing Storage daemon
       */
      mig_jcr->setJobStatus(JS_WaitSD);
      if (!connect_to_storage_daemon(mig_jcr, 10, me->SDConnectTimeout, true)) {
         goto bail_out;
      }

      /*
       * Now start a job with the Reading Storage daemon
       */
      if (!start_storage_daemon_job(jcr, jcr->rstorage, NULL, /* send_bsr */ true)) {
         goto bail_out;
      }

      Dmsg0(150, "Reading Storage daemon connection OK\n");

      /*
       * Now start a job with the Writing Storage daemon
       */
      if (!start_storage_daemon_job(mig_jcr, NULL, mig_jcr->wstorage, /* send_bsr */ false)) {
         goto bail_out;
      }

      Dmsg0(150, "Writing Storage daemon connection OK\n");
   }

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
    * Update job start record for this migration control job
    */
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      goto bail_out;
   }

   /*
    * Declare the job started to start the MaxRunTime check
    */
   jcr->setJobStarted();

   mig_jcr->start_time = time(NULL);
   mig_jcr->jr.StartTime = mig_jcr->start_time;
   mig_jcr->jr.JobTDate = mig_jcr->start_time;
   mig_jcr->setJobStatus(JS_Running);

   /*
    * Update job start record for the real migration backup job
    */
   if (!db_update_job_start_record(mig_jcr, mig_jcr->db, &mig_jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(mig_jcr->db));
      goto bail_out;
   }

   Dmsg4(dbglevel, "mig_jcr: Name=%s JobId=%d Type=%c Level=%c\n",
         mig_jcr->jr.Name, (int)mig_jcr->jr.JobId,
         mig_jcr->jr.JobType, mig_jcr->jr.JobLevel);

   /*
    * If we are connected to two different SDs tell the writing one
    * to be ready to receive the data and tell the reading one
    * to replicate to the other.
    */
   if (jcr->remote_replicate) {
      STORERES *store;
      POOL_MEM command(PM_MESSAGE);
      int tls_need = BNET_TLS_NONE;

      if (jcr->max_bandwidth > 0) {
         send_bwlimit_to_sd(jcr, jcr->Job);
      }

      /*
       * Start the job prior to starting the message thread below
       * to avoid two threads from using the BSOCK structure at
       * the same time.
       */
      if (!mig_jcr->store_bsock->fsend("listen")) {
         goto bail_out;
      }

      if (!start_storage_daemon_message_thread(mig_jcr)) {
         goto bail_out;
      }

      /*
       * Send Storage daemon address to the other Storage daemon
       */
      store = mig_jcr->res.wstore;
      if (store->SDDport == 0) {
         store->SDDport = store->SDport;
      }

      /*
       * TLS Requirement
       */
      if (store->tls_enable) {
         if (store->tls_require) {
            tls_need = BNET_TLS_REQUIRED;
         } else {
            tls_need = BNET_TLS_OK;
         }
      }

      Mmsg(command, replicatecmd, mig_jcr->Job, store->address,
           store->SDDport, tls_need, mig_jcr->sd_auth_key);

      if (!jcr->store_bsock->fsend(command.c_str())) {
         return false;
      }
   }

   /*
    * Start the job prior to starting the message thread below
    * to avoid two threads from using the BSOCK structure at
    * the same time.
    */
   if (!jcr->store_bsock->fsend("run")) {
      goto bail_out;
   }

   /*
    * Now start a Storage daemon message thread
    */
   if (!start_storage_daemon_message_thread(jcr)) {
      goto bail_out;
   }

   jcr->setJobStatus(JS_Running);
   mig_jcr->setJobStatus(JS_Running);

   /*
    * Pickup Job termination data
    * Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/JobErrors or
    * mig_jcr->JobFiles/ReadBytes/JobBytes/JobErrors when replicating to
    * a remote storage daemon.
    */
   if (jcr->remote_replicate) {
      wait_for_storage_daemon_termination(jcr);
      wait_for_storage_daemon_termination(mig_jcr);
      jcr->setJobStatus(jcr->SDJobStatus);
      db_write_batch_file_records(mig_jcr);
   } else {
      wait_for_storage_daemon_termination(jcr);
      jcr->setJobStatus(jcr->SDJobStatus);
      db_write_batch_file_records(jcr);
   }

bail_out:
   if (jcr->remote_replicate) {
      alist *wstorage;

      /*
       * Swap the wstorage between the jcr and the mig_jcr.
       */
      wstorage = mig_jcr->wstorage;
      mig_jcr->wstorage = jcr->wstorage;
      jcr->wstorage = wstorage;

      /*
       * Undo the clear of the wstore in the jcr and assign the mig_jcr wstore
       * back to the jcr. This is an undo of the clearing we did earlier
       * as we want the jcr connected to the reading storage daemon and the
       * mig_jcr to the writing jcr. By clearing the wstore of the jcr the
       * connect_to_storage_daemon function will do the right thing e.g. connect
       * the jcrs in the way we want them to.
       */
      jcr->res.wstore = mig_jcr->res.wstore;
      mig_jcr->res.wstore = NULL;
   }

   if (jcr->is_JobStatus(JS_Terminated)) {
      migration_cleanup(jcr, jcr->JobStatus);
      retval = true;
   }

   return retval;
}

struct idpkt {
   POOLMEM *list;
   uint32_t count;
};

/*
 * Add an item to the list if it is unique
 */
static void add_unique_id(idpkt *ids, char *item)
{
   const int maxlen = 30;
   char id[maxlen+1];
   char *q = ids->list;

   /*
    * Walk through current list to see if each item is the same as item
    */
   while (*q) {
       id[0] = 0;
       for (int i=0; i<maxlen; i++) {
          if (*q == 0) {
             break;
          } else if (*q == ',') {
             q++;
             break;
          }
          id[i] = *q++;
          id[i+1] = 0;
       }
       if (bstrcmp(item, id)) {
          return;
       }
   }
   /*
    * Did not find item, so add it to list
    */
   if (ids->count == 0) {
      ids->list[0] = 0;
   } else {
      pm_strcat(ids->list, ",");
   }
   pm_strcat(ids->list, item);
   ids->count++;
// Dmsg3(0, "add_uniq count=%d Ids=%p %s\n", ids->count, ids->list, ids->list);
   return;
}

/*
 * Callback handler make list of DB Ids
 */
static int unique_dbid_handler(void *ctx, int num_fields, char **row)
{
   idpkt *ids = (idpkt *)ctx;

   /*
    * Sanity check
    */
   if (!row || !row[0]) {
      Dmsg0(dbglevel, "dbid_hdlr error empty row\n");
      return 1;              /* stop calling us */
   }

   add_unique_id(ids, row[0]);
   Dmsg3(dbglevel, "dbid_hdlr count=%d Ids=%p %s\n", ids->count, ids->list, ids->list);
   return 0;
}


struct uitem {
   dlink link;
   char *item;
};

static int item_compare(void *item1, void *item2)
{
   uitem *i1 = (uitem *)item1;
   uitem *i2 = (uitem *)item2;
   return strcmp(i1->item, i2->item);
}

static int unique_name_handler(void *ctx, int num_fields, char **row)
{
   dlist *list = (dlist *)ctx;

   uitem *new_item = (uitem *)malloc(sizeof(uitem));
   uitem *item;

   memset(new_item, 0, sizeof(uitem));
   new_item->item = bstrdup(row[0]);
   Dmsg1(dbglevel, "Unique_name_hdlr Item=%s\n", row[0]);
   item = (uitem *)list->binary_insert((void *)new_item, item_compare);
   if (item != new_item) {            /* already in list */
      free(new_item->item);
      free((char *)new_item);
      return 0;
   }
   return 0;
}

/* Get Job names in Pool */
static const char *sql_job =
   "SELECT DISTINCT Job.Name from Job,Pool"
   " WHERE Pool.Name='%s' AND Job.PoolId=Pool.PoolId";

/* Get JobIds from regex'ed Job names */
static const char *sql_jobids_from_job =
   "SELECT DISTINCT Job.JobId,Job.StartTime FROM Job,Pool"
   " WHERE Job.Name='%s' AND Pool.Name='%s' AND Job.PoolId=Pool.PoolId"
   " ORDER by Job.StartTime";

/* Get Client names in Pool */
static const char *sql_client =
   "SELECT DISTINCT Client.Name from Client,Pool,Job"
   " WHERE Pool.Name='%s' AND Job.ClientId=Client.ClientId AND"
   " Job.PoolId=Pool.PoolId";

/* Get JobIds from regex'ed Client names */
static const char *sql_jobids_from_client =
   "SELECT DISTINCT Job.JobId,Job.StartTime FROM Job,Pool,Client"
   " WHERE Client.Name='%s' AND Pool.Name='%s' AND Job.PoolId=Pool.PoolId"
   " AND Job.ClientId=Client.ClientId AND Job.Type IN ('B','C')"
   " AND Job.JobStatus IN ('T','W')"
   " ORDER by Job.StartTime";

/* Get Volume names in Pool */
static const char *sql_vol =
   "SELECT DISTINCT VolumeName FROM Media,Pool WHERE"
   " VolStatus in ('Full','Used','Error') AND Media.Enabled=1 AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s'";

/* Get JobIds from regex'ed Volume names */
static const char *sql_jobids_from_vol =
   "SELECT DISTINCT Job.JobId,Job.StartTime FROM Media,JobMedia,Job"
   " WHERE Media.VolumeName='%s' AND Media.MediaId=JobMedia.MediaId"
   " AND JobMedia.JobId=Job.JobId AND Job.Type IN ('B','C')"
   " AND Job.JobStatus IN ('T','W') AND Media.Enabled=1"
   " ORDER by Job.StartTime";

static const char *sql_smallest_vol =
   "SELECT Media.MediaId FROM Media,Pool,JobMedia WHERE"
   " Media.MediaId in (SELECT DISTINCT MediaId from JobMedia) AND"
   " Media.VolStatus in ('Full','Used','Error') AND Media.Enabled=1 AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s'"
   " ORDER BY VolBytes ASC LIMIT 1";

static const char *sql_oldest_vol =
   "SELECT Media.MediaId FROM Media,Pool,JobMedia WHERE"
   " Media.MediaId in (SELECT DISTINCT MediaId from JobMedia) AND"
   " Media.VolStatus in ('Full','Used','Error') AND Media.Enabled=1 AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s'"
   " ORDER BY LastWritten ASC LIMIT 1";

/* Get JobIds when we have selected MediaId */
static const char *sql_jobids_from_mediaid =
   "SELECT DISTINCT Job.JobId,Job.StartTime FROM JobMedia,Job"
   " WHERE JobMedia.JobId=Job.JobId AND JobMedia.MediaId IN (%s)"
   " AND Job.Type IN ('B','C') AND Job.JobStatus IN ('T','W')"
   " ORDER by Job.StartTime";

/* Get the number of bytes in the pool */
static const char *sql_pool_bytes =
   "SELECT SUM(JobBytes) FROM Job WHERE JobId IN"
   " (SELECT DISTINCT Job.JobId from Pool,Job,Media,JobMedia WHERE"
   " Pool.Name='%s' AND Media.PoolId=Pool.PoolId AND"
   " VolStatus in ('Full','Used','Error','Append') AND Media.Enabled=1 AND"
   " Job.Type IN ('B','C') AND Job.JobStatus IN ('T','W') AND"
   " JobMedia.JobId=Job.JobId AND Job.PoolId=Media.PoolId)";

/* Get the number of bytes in the Jobs */
static const char *sql_job_bytes =
   "SELECT SUM(JobBytes) FROM Job WHERE JobId IN (%s)";

/* Get Media Ids in Pool */
static const char *sql_mediaids =
   "SELECT MediaId FROM Media,Pool WHERE"
   " VolStatus in ('Full','Used','Error') AND Media.Enabled=1 AND"
   " Media.PoolId=Pool.PoolId AND Pool.Name='%s' ORDER BY LastWritten ASC";

/* Get JobIds in Pool longer than specified time */
static const char *sql_pool_time =
   "SELECT DISTINCT Job.JobId FROM Pool,Job,Media,JobMedia WHERE"
   " Pool.Name='%s' AND Media.PoolId=Pool.PoolId AND"
   " VolStatus IN ('Full','Used','Error') AND Media.Enabled=1 AND"
   " Job.Type IN ('B','C') AND Job.JobStatus IN ('T','W') AND"
   " JobMedia.JobId=Job.JobId AND Job.PoolId=Media.PoolId"
   " AND Job.RealEndTime<='%s'";

/* Get JobIds from successfully completed backup jobs which have not been copied before */
static const char *sql_jobids_of_pool_uncopied_jobs =
   "SELECT DISTINCT Job.JobId,Job.StartTime FROM Job,Pool"
   " WHERE Pool.Name = '%s' AND Pool.PoolId = Job.PoolId"
   " AND Job.Type = 'B' AND Job.JobStatus IN ('T','W')"
   " AND Job.jobBytes > 0"
   " AND Job.JobId NOT IN"
   " (SELECT PriorJobId FROM Job WHERE"
   " Type IN ('B','C') AND Job.JobStatus IN ('T','W')"
   " AND PriorJobId != 0)"
   " ORDER by Job.StartTime";

/*
* const char *sql_ujobid =
*   "SELECT DISTINCT Job.Job from Client,Pool,Media,Job,JobMedia "
*   " WHERE Media.PoolId=Pool.PoolId AND Pool.Name='%s' AND"
*   " JobMedia.JobId=Job.JobId AND Job.PoolId=Media.PoolId";
*/

/*
 *
 * This is the central piece of code that finds a job or jobs
 *   actually JobIds to migrate.  It first looks to see if one
 *   has been "manually" specified in jcr->MigrateJobId, and if
 *   so, it returns that JobId to be run.  Otherwise, it
 *   examines the Selection Type to see what kind of migration
 *   we are doing (Volume, Job, Client, ...) and applies any
 *   Selection Pattern if appropriate to obtain a list of JobIds.
 *   Finally, it will loop over all the JobIds found, except the last
 *   one starting a new job with MigrationJobId set to that JobId, and
 *   finally, it returns the last JobId to the caller.
 *
 * Returns: -1  on error
 *           0  if no jobs to migrate
 *           1  if OK and jcr->previous_jr filled in
 */
static int getJob_to_migrate(JCR *jcr)
{
   char ed1[30], ed2[30];
   POOL_MEM query(PM_MESSAGE);
   JobId_t JobId;
   DBId_t DBId = 0;
   int status;
   char *p;
   idpkt ids, mid, jids;
   db_int64_ctx ctx;
   int64_t pool_bytes;
   time_t ttime;
   struct tm tm;
   char dt[MAX_TIME_LENGTH];
   int count = 0;
   int limit = 99;           /* limit + 1 is max jobs to start */

   ids.list = get_pool_memory(PM_MESSAGE);
   ids.list[0] = 0;
   ids.count = 0;
   mid.list = get_pool_memory(PM_MESSAGE);
   mid.list[0] = 0;
   mid.count = 0;
   jids.list = get_pool_memory(PM_MESSAGE);
   jids.list[0] = 0;
   jids.count = 0;

   /*
    * If MigrateJobId is set, then we migrate only that Job,
    *  otherwise, we go through the full selection of jobs to
    *  migrate.
    */
   if (jcr->MigrateJobId != 0) {
      Dmsg1(dbglevel, "At Job start previous jobid=%u\n", jcr->MigrateJobId);
      JobId = jcr->MigrateJobId;
   } else {
      switch (jcr->res.job->selection_type) {
      case MT_JOB:
         if (!regex_find_jobids(jcr, &ids, sql_job, sql_jobids_from_job, "Job")) {
            goto bail_out;
         }
         break;
      case MT_CLIENT:
         if (!regex_find_jobids(jcr, &ids, sql_client, sql_jobids_from_client, "Client")) {
            goto bail_out;
         }
         break;
      case MT_VOLUME:
         if (!regex_find_jobids(jcr, &ids, sql_vol, sql_jobids_from_vol, "Volume")) {
            goto bail_out;
         }
         break;
      case MT_SQLQUERY:
         if (!jcr->res.job->selection_pattern) {
            Jmsg(jcr, M_FATAL, 0, _("No %s SQL selection pattern specified.\n"), jcr->get_OperationName());
            goto bail_out;
         }
         Dmsg1(dbglevel, "SQL=%s\n", jcr->res.job->selection_pattern);
         if (!db_sql_query(jcr->db, jcr->res.job->selection_pattern,
              unique_dbid_handler, (void *)&ids)) {
            Jmsg(jcr, M_FATAL, 0,
                 _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
            goto bail_out;
         }
         break;
      case MT_SMALLEST_VOL:
         if (!find_mediaid_then_jobids(jcr, &ids, sql_smallest_vol, "Smallest Volume")) {
            goto bail_out;
         }
         break;
      case MT_OLDEST_VOL:
         if (!find_mediaid_then_jobids(jcr, &ids, sql_oldest_vol, "Oldest Volume")) {
            goto bail_out;
         }
         break;
      case MT_POOL_OCCUPANCY:
         ctx.count = 0;
         /* Find count of bytes in pool */
         Mmsg(query, sql_pool_bytes, jcr->res.rpool->name());
         if (!db_sql_query(jcr->db, query.c_str(), db_int64_handler, (void *)&ctx)) {
            Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
            goto bail_out;
         }
         if (ctx.count == 0) {
            Jmsg(jcr, M_INFO, 0, _("No Volumes found to %s.\n"), jcr->get_ActionName());
            goto ok_out;
         }
         pool_bytes = ctx.value;
         Dmsg2(dbglevel, "highbytes=%lld pool=%lld\n", jcr->res.rpool->MigrationHighBytes,
               pool_bytes);
         if (pool_bytes < (int64_t)jcr->res.rpool->MigrationHighBytes) {
            Jmsg(jcr, M_INFO, 0, _("No Volumes found to %s.\n"), jcr->get_ActionName());
            goto ok_out;
         }
         Dmsg0(dbglevel, "We should do Occupation migration.\n");

         ids.count = 0;
         /* Find a list of MediaIds that could be migrated */
         Mmsg(query, sql_mediaids, jcr->res.rpool->name());
         Dmsg1(dbglevel, "query=%s\n", query.c_str());
         if (!db_sql_query(jcr->db, query.c_str(), unique_dbid_handler, (void *)&ids)) {
            Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
            goto bail_out;
         }
         if (ids.count == 0) {
            Jmsg(jcr, M_INFO, 0, _("No Volumes found to %s.\n"), jcr->get_ActionName());
            goto ok_out;
         }
         Dmsg2(dbglevel, "Pool Occupancy ids=%d MediaIds=%s\n", ids.count, ids.list);

         if (!find_jobids_from_mediaid_list(jcr, &ids, "Volume")) {
            goto bail_out;
         }
         /* ids == list of jobs  */
         p = ids.list;
         for (int i = 0; i < (int)ids.count; i++) {
            status = get_next_dbid_from_list(&p, &DBId);
            Dmsg2(dbglevel, "get_next_dbid status=%d JobId=%u\n", status, (uint32_t)DBId);
            if (status < 0) {
               Jmsg(jcr, M_FATAL, 0, _("Invalid JobId found.\n"));
               goto bail_out;
            } else if (status == 0) {
               break;
            }

            mid.count = 1;
            Mmsg(mid.list, "%s", edit_int64(DBId, ed1));
            if (jids.count > 0) {
               pm_strcat(jids.list, ",");
            }
            pm_strcat(jids.list, mid.list);
            jids.count += mid.count;

            /* Find count of bytes from Jobs */
            Mmsg(query, sql_job_bytes, mid.list);
            Dmsg1(dbglevel, "Jobbytes query: %s\n", query.c_str());
            if (!db_sql_query(jcr->db, query.c_str(), db_int64_handler, (void *)&ctx)) {
               Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
               goto bail_out;
            }
            pool_bytes -= ctx.value;
            Dmsg2(dbglevel, "Total %s Job bytes=%s\n", jcr->get_ActionName(),
                  edit_int64_with_commas(ctx.value, ed1));
            Dmsg2(dbglevel, "lowbytes=%s poolafter=%s\n",
                  edit_int64_with_commas(jcr->res.rpool->MigrationLowBytes, ed1),
                  edit_int64_with_commas(pool_bytes, ed2));
            if (pool_bytes <= (int64_t)jcr->res.rpool->MigrationLowBytes) {
               Dmsg0(dbglevel, "We should be done.\n");
               break;
            }
         }
         /* Transfer jids to ids, where the jobs list is expected */
         ids.count = jids.count;
         pm_strcpy(ids.list, jids.list);
         Dmsg2(dbglevel, "Pool Occupancy ids=%d JobIds=%s\n", ids.count, ids.list);
         break;
      case MT_POOL_TIME:
         ttime = time(NULL) - (time_t)jcr->res.rpool->MigrationTime;
         (void)localtime_r(&ttime, &tm);
         strftime(dt, sizeof(dt), "%Y-%m-%d %H:%M:%S", &tm);

         ids.count = 0;
         Mmsg(query, sql_pool_time, jcr->res.rpool->name(), dt);
         Dmsg1(dbglevel, "query=%s\n", query.c_str());
         if (!db_sql_query(jcr->db, query.c_str(), unique_dbid_handler, (void *)&ids)) {
            Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
            goto bail_out;
         }
         if (ids.count == 0) {
            Jmsg(jcr, M_INFO, 0, _("No Volumes found to %s.\n"), jcr->get_ActionName());
            goto ok_out;
         }
         Dmsg2(dbglevel, "PoolTime ids=%d JobIds=%s\n", ids.count, ids.list);
         break;
      case MT_POOL_UNCOPIED_JOBS:
         if (!find_jobids_of_pool_uncopied_jobs(jcr, &ids)) {
            goto bail_out;
         }
         break;
      default:
         Jmsg(jcr, M_FATAL, 0, _("Unknown %s Selection Type.\n"), jcr->get_OperationName());
         goto bail_out;
      }

      /*
       * Loop over all jobids except the last one, sending
       * them to start_migration_job(), which will start a job
       * for each of them.  For the last JobId, we handle it below.
       */
      p = ids.list;
      if (ids.count == 0) {
         Jmsg(jcr, M_INFO, 0, _("No JobIds found to %s.\n"), jcr->get_ActionName());
         goto ok_out;
      }

      Jmsg(jcr, M_INFO, 0, _("The following %u JobId%s chosen to be %s: %s\n"),
         ids.count, (ids.count < 2) ? _(" was") : _("s were"),
         jcr->get_ActionName(true), ids.list);

      Dmsg2(dbglevel, "Before loop count=%d ids=%s\n", ids.count, ids.list);
      /*
       * Note: to not over load the system, limit the number
       *  of new jobs started to 100 (see limit above)
       */
      for (int i = 1; i < (int)ids.count; i++) {
         JobId = 0;
         status = get_next_jobid_from_list(&p, &JobId);
         Dmsg3(dbglevel, "getJobid_no=%d status=%d JobId=%u\n", i, status, JobId);
         if (status < 0) {
            Jmsg(jcr, M_FATAL, 0, _("Invalid JobId found.\n"));
            goto bail_out;
         } else if (status == 0) {
            Jmsg(jcr, M_INFO, 0, _("No JobIds found to %s.\n"), jcr->get_ActionName());
            goto ok_out;
         }
         jcr->MigrateJobId = JobId;
         /* Don't start any more when limit reaches zero */
         limit--;
         if (limit > 0) {
            start_migration_job(jcr);
            Dmsg0(dbglevel, "Back from start_migration_job\n");
         }
      }

      /* Now get the last JobId and handle it in the current job */
      JobId = 0;
      status = get_next_jobid_from_list(&p, &JobId);
      Dmsg2(dbglevel, "Last get_next_jobid status=%d JobId=%u\n", status, (int)JobId);
      if (status < 0) {
         Jmsg(jcr, M_FATAL, 0, _("Invalid JobId found.\n"));
         goto bail_out;
      } else if (status == 0) {
         Jmsg(jcr, M_INFO, 0, _("No JobIds found to %s.\n"), jcr->get_ActionName());
         goto ok_out;
      }
   }

   jcr->previous_jr.JobId = JobId;
   Dmsg1(dbglevel, "Previous jobid=%d\n", (int)jcr->previous_jr.JobId);

   if (!db_get_job_record(jcr, jcr->db, &jcr->previous_jr)) {
      Jmsg(jcr, M_FATAL, 0, _("Could not get job record for JobId %s to %s. ERR=%s"),
           edit_int64(jcr->previous_jr.JobId, ed1),
           jcr->get_ActionName(), db_strerror(jcr->db));
      goto bail_out;
   }

   Jmsg(jcr, M_INFO, 0, _("%s using JobId=%s Job=%s\n"),
        jcr->get_OperationName(), edit_int64(jcr->previous_jr.JobId, ed1),
        jcr->previous_jr.Job);
   Dmsg4(dbglevel, "%s JobId=%d  using JobId=%s Job=%s\n",
        jcr->get_OperationName(), jcr->JobId,
        edit_int64(jcr->previous_jr.JobId, ed1), jcr->previous_jr.Job);
   count = 1;

ok_out:
   goto out;

bail_out:
   count = -1;

out:
   free_pool_memory(ids.list);
   free_pool_memory(mid.list);
   free_pool_memory(jids.list);
   return count;
}

static void start_migration_job(JCR *jcr)
{
   char ed1[50];
   JobId_t jobid;
   UAContext *ua;
   POOL_MEM cmd(PM_MESSAGE);

   ua = new_ua_context(jcr);
   ua->batch = true;
   Mmsg(ua->cmd, "run job=\"%s\" jobid=%s ignoreduplicatecheck=yes",
        jcr->res.job->name(), edit_uint64(jcr->MigrateJobId, ed1));

   /*
    * Make sure we have something to compare against.
    */
   if (jcr->res.pool) {
      /*
       * See if there was actually a pool override.
       */
      if (jcr->res.pool != jcr->res.job->pool) {
         Mmsg(cmd, " pool=\"%s\"", jcr->res.pool->name());
         pm_strcat(ua->cmd, cmd.c_str());
      }

      /*
       * See if there was actually a next pool override.
       */
      if (jcr->res.next_pool && jcr->res.next_pool != jcr->res.pool->NextPool) {
         Mmsg(cmd, " nextpool=\"%s\"", jcr->res.next_pool->name());
         pm_strcat(ua->cmd, cmd.c_str());
      }
   }

   Dmsg2(dbglevel, "=============== %s cmd=%s\n", jcr->get_OperationName(), ua->cmd);
   parse_ua_args(ua);                 /* parse command */

   jobid = run_cmd(ua, ua->cmd);
   if (jobid == 0) {
      Jmsg(jcr, M_ERROR, 0, _("Could not start migration job.\n"));
   } else {
      Jmsg(jcr, M_INFO, 0, _("%s JobId %d started.\n"), jcr->get_OperationName(), (int)jobid);
   }

   free_ua_context(ua);
}

static bool find_mediaid_then_jobids(JCR *jcr, idpkt *ids,
                                     const char *query1,
                                     const char *type)
{
   bool ok = false;
   POOL_MEM query(PM_MESSAGE);

   ids->count = 0;
   /* Basic query for MediaId */
   Mmsg(query, query1, jcr->res.rpool->name());
   if (!db_sql_query(jcr->db, query.c_str(), unique_dbid_handler, (void *)ids)) {
      Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
      goto bail_out;
   }
   if (ids->count == 0) {
      Jmsg(jcr, M_INFO, 0, _("No %s found to %s.\n"), type, jcr->get_ActionName());
      ok = true;         /* Not an error */
      goto bail_out;
   } else if (ids->count != 1) {
      Jmsg(jcr, M_FATAL, 0, _("SQL error. Expected 1 MediaId got %d\n"), ids->count);
      goto bail_out;
   }
   Dmsg2(dbglevel, "%s MediaIds=%s\n", type, ids->list);

   ok = find_jobids_from_mediaid_list(jcr, ids, type);

bail_out:
   return ok;
}

/*
 * This routine returns:
 *    false       if an error occurred
 *    true        otherwise
 *    ids.count   number of jobids found (may be zero)
 */
static bool find_jobids_from_mediaid_list(JCR *jcr, idpkt *ids, const char *type)
{
   bool ok = false;
   POOL_MEM query(PM_MESSAGE);

   Mmsg(query, sql_jobids_from_mediaid, ids->list);
   ids->count = 0;
   if (!db_sql_query(jcr->db, query.c_str(), unique_dbid_handler, (void *)ids)) {
      Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
      goto bail_out;
   }
   if (ids->count == 0) {
      Jmsg(jcr, M_INFO, 0, _("No %ss found to %s.\n"), type, jcr->get_ActionName());
   }
   ok = true;

bail_out:
   return ok;
}

/*
 * This routine returns:
 *    false       if an error occurred
 *    true        otherwise
 *    ids.count   number of jobids found (may be zero)
 */
static bool find_jobids_of_pool_uncopied_jobs(JCR *jcr, idpkt *ids)
{
   bool ok = false;
   POOL_MEM query(PM_MESSAGE);

   /*
    * Only a copy job is allowed
    */
   if (!jcr->is_JobType(JT_COPY)) {
      Jmsg(jcr, M_FATAL, 0,
           _("Selection Type 'pooluncopiedjobs' only applies to Copy Jobs"));
      goto bail_out;
   }

   Dmsg1(dbglevel, "copy selection pattern=%s\n", jcr->res.rpool->name());
   Mmsg(query, sql_jobids_of_pool_uncopied_jobs, jcr->res.rpool->name());
   Dmsg1(dbglevel, "get uncopied jobs query=%s\n", query.c_str());
   if (!db_sql_query(jcr->db, query.c_str(), unique_dbid_handler, (void *)ids)) {
      Jmsg(jcr, M_FATAL, 0,
           _("SQL to get uncopied jobs failed. ERR=%s\n"), db_strerror(jcr->db));
      goto bail_out;
   }
   ok = true;

bail_out:
   return ok;
}

static bool regex_find_jobids(JCR *jcr, idpkt *ids,
                              const char *query1,
                              const char *query2,
                              const char *type)
{
   dlist *item_chain;
   uitem *item = NULL;
   uitem *last_item = NULL;
   regex_t preg;
   char prbuf[500];
   int rc;
   bool ok = false;
   POOL_MEM query(PM_MESSAGE);

   item_chain = New(dlist(item, &item->link));
   if (!jcr->res.job->selection_pattern) {
      Jmsg(jcr, M_FATAL, 0, _("No %s %s selection pattern specified.\n"),
         jcr->get_OperationName(), type);
      goto bail_out;
   }
   Dmsg1(dbglevel, "regex-sel-pattern=%s\n", jcr->res.job->selection_pattern);
   /* Basic query for names */
   Mmsg(query, query1, jcr->res.rpool->name());
   Dmsg1(dbglevel, "get name query1=%s\n", query.c_str());
   if (!db_sql_query(jcr->db, query.c_str(), unique_name_handler,
        (void *)item_chain)) {
      Jmsg(jcr, M_FATAL, 0,
           _("SQL to get %s failed. ERR=%s\n"), type, db_strerror(jcr->db));
      goto bail_out;
   }
   Dmsg1(dbglevel, "query1 returned %d names\n", item_chain->size());
   if (item_chain->size() == 0) {
      Jmsg(jcr, M_INFO, 0, _("Query of Pool \"%s\" returned no Jobs to %s.\n"),
           jcr->res.rpool->name(), jcr->get_ActionName());
      ok = true;
      goto bail_out;               /* skip regex match */
   } else {
      /* Compile regex expression */
      rc = regcomp(&preg, jcr->res.job->selection_pattern, REG_EXTENDED);
      if (rc != 0) {
         regerror(rc, &preg, prbuf, sizeof(prbuf));
         Jmsg(jcr, M_FATAL, 0, _("Could not compile regex pattern \"%s\" ERR=%s\n"),
              jcr->res.job->selection_pattern, prbuf);
         goto bail_out;
      }
      /* Now apply the regex to the names and remove any item not matched */
      foreach_dlist(item, item_chain) {
         const int nmatch = 30;
         regmatch_t pmatch[nmatch];
         if (last_item) {
            Dmsg1(dbglevel, "Remove item %s\n", last_item->item);
            free(last_item->item);
            item_chain->remove(last_item);
         }
         Dmsg1(dbglevel, "get name Item=%s\n", item->item);
         rc = regexec(&preg, item->item, nmatch, pmatch,  0);
         if (rc == 0) {
            last_item = NULL;   /* keep this one */
         } else {
            last_item = item;
         }
      }
      if (last_item) {
         free(last_item->item);
         Dmsg1(dbglevel, "Remove item %s\n", last_item->item);
         item_chain->remove(last_item);
      }
      regfree(&preg);
   }
   if (item_chain->size() == 0) {
      Jmsg(jcr, M_INFO, 0, _("Regex pattern matched no Jobs to %s.\n"), jcr->get_ActionName());
      ok = true;
      goto bail_out;               /* skip regex match */
   }

   /*
    * At this point, we have a list of items in item_chain
    *  that have been matched by the regex, so now we need
    *  to look up their jobids.
    */
   ids->count = 0;
   foreach_dlist(item, item_chain) {
      Dmsg2(dbglevel, "Got %s: %s\n", type, item->item);
      Mmsg(query, query2, item->item, jcr->res.rpool->name());
      Dmsg1(dbglevel, "get id from name query2=%s\n", query.c_str());
      if (!db_sql_query(jcr->db, query.c_str(), unique_dbid_handler, (void *)ids)) {
         Jmsg(jcr, M_FATAL, 0,
              _("SQL failed. ERR=%s\n"), db_strerror(jcr->db));
         goto bail_out;
      }
   }
   if (ids->count == 0) {
      Jmsg(jcr, M_INFO, 0, _("No %ss found to %s.\n"), type, jcr->get_ActionName());
   }
   ok = true;

bail_out:
   Dmsg2(dbglevel, "Count=%d Jobids=%s\n", ids->count, ids->list);
   foreach_dlist(item, item_chain) {
      free(item->item);
   }
   delete item_chain;
   return ok;
}

/*
 * Release resources allocated during backup.
 */
void migration_cleanup(JCR *jcr, int TermCode)
{
   char sdt[MAX_TIME_LENGTH], edt[MAX_TIME_LENGTH];
   char ec1[30], ec2[30], ec3[30], ec4[30], ec5[30], elapsed[50];
   char ec6[50], ec7[50], ec8[50];
   char term_code[100], sd_term_msg[100];
   const char *term_msg;
   int msg_type = M_INFO;
   MEDIA_DBR mr;
   double kbps;
   utime_t RunTime;
   JCR *mig_jcr = jcr->mig_jcr;
   POOL_MEM query(PM_MESSAGE);

   Dmsg2(100, "Enter migrate_cleanup %d %c\n", TermCode, TermCode);
   update_job_end(jcr, TermCode);

   /*
    * Check if we actually did something.
    * mig_jcr is jcr of the newly migrated job.
    */
   if (mig_jcr) {
      char old_jobid[50], new_jobid[50];

      edit_uint64(jcr->previous_jr.JobId, old_jobid);
      edit_uint64(mig_jcr->jr.JobId, new_jobid);

      /*
       * See if we used a remote SD if so the mig_jcr contains
       * the jobfiles and jobbytes and the new volsessionid
       * and volsessiontime as the writing SD generates this info.
       */
      if (jcr->remote_replicate) {
         mig_jcr->JobFiles = jcr->JobFiles = mig_jcr->SDJobFiles;
         mig_jcr->JobBytes = jcr->JobBytes = mig_jcr->SDJobBytes;
      } else {
         mig_jcr->JobFiles = jcr->JobFiles = jcr->SDJobFiles;
         mig_jcr->JobBytes = jcr->JobBytes = jcr->SDJobBytes;
         mig_jcr->VolSessionId = jcr->VolSessionId;
         mig_jcr->VolSessionTime = jcr->VolSessionTime;
      }
      mig_jcr->jr.RealEndTime = 0;
      mig_jcr->jr.PriorJobId = jcr->previous_jr.JobId;

      if (jcr->is_JobStatus(JS_Terminated) &&
         (jcr->JobErrors || jcr->SDErrors)) {
         TermCode = JS_Warnings;
      }

      update_job_end(mig_jcr, TermCode);

      /*
       * Update final items to set them to the previous job's values
       */
      Mmsg(query, "UPDATE Job SET StartTime='%s',EndTime='%s',"
                  "JobTDate=%s WHERE JobId=%s",
           jcr->previous_jr.cStartTime, jcr->previous_jr.cEndTime,
           edit_uint64(jcr->previous_jr.JobTDate, ec1),
           new_jobid);
      db_sql_query(mig_jcr->db, query.c_str());

      /*
       * If we terminated a migration normally:
       * - mark the previous job as migrated
       * - move any Log records to the new JobId
       * - Purge the File records from the previous job
       */
      if (jcr->is_JobType(JT_MIGRATE) && jcr->is_terminated_ok()) {
         UAContext *ua;

         Mmsg(query, "UPDATE Job SET Type='%c' WHERE JobId=%s",
              (char)JT_MIGRATED_JOB, old_jobid);
         db_sql_query(mig_jcr->db, query.c_str());

         /*
          * Move JobLog to new JobId
          */
         ua = new_ua_context(jcr);
         Mmsg(query, "UPDATE Log SET JobId=%s WHERE JobId=%s",
              new_jobid, old_jobid);
         db_sql_query(mig_jcr->db, query.c_str());

         if (jcr->res.job->PurgeMigrateJob) {
            /*
             * Purge old Job record
             */
            purge_jobs_from_catalog(ua, old_jobid);
         } else {
            /*
             * Purge all old file records, but leave Job record
             */
            purge_files_from_jobs(ua, old_jobid);
         }

         free_ua_context(ua);
      }

      /*
       * If we terminated a Copy (rather than a Migration) normally:
       * - copy any Log records to the new JobId
       * - set type="Job Copy" for the new job
       */
      if (jcr->is_JobType(JT_COPY) && jcr->is_terminated_ok()) {
         /*
          * Copy JobLog to new JobId
          */
         Mmsg(query, "INSERT INTO Log (JobId, Time, LogText ) "
                     "SELECT %s, Time, LogText FROM Log WHERE JobId=%s",
              new_jobid, old_jobid);
         db_sql_query(mig_jcr->db, query.c_str());

         Mmsg(query, "UPDATE Job SET Type='%c' WHERE JobId=%s",
              (char)JT_JOB_COPY, new_jobid);
         db_sql_query(mig_jcr->db, query.c_str());
      }

      if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
         Jmsg(jcr, M_WARNING, 0, _("Error getting Job record for Job report: ERR=%s"),
              db_strerror(jcr->db));
         jcr->setJobStatus(JS_ErrorTerminated);
      }

      update_bootstrap_file(mig_jcr);

      if (!db_get_job_volume_names(mig_jcr, mig_jcr->db, mig_jcr->jr.JobId, &mig_jcr->VolumeName)) {
         /*
          * Note, if the job has failed, most likely it did not write any
          * tape, so suppress this "error" message since in that case
          * it is normal.  Or look at it the other way, only for a
          * normal exit should we complain about this error.
          */
         if (jcr->is_terminated_ok() && jcr->jr.JobBytes) {
            Jmsg(jcr, M_ERROR, 0, "%s", db_strerror(mig_jcr->db));
         }
         mig_jcr->VolumeName[0] = 0;         /* none */
      }

      if (mig_jcr->VolumeName[0]) {
         /*
          * Find last volume name. Multiple vols are separated by |
          */
         char *p = strrchr(mig_jcr->VolumeName, '|');
         if (p) {
            p++;                         /* skip | */
         } else {
            p = mig_jcr->VolumeName;     /* no |, take full name */
         }
         bstrncpy(mr.VolumeName, p, sizeof(mr.VolumeName));
         if (!db_get_media_record(jcr, jcr->db, &mr)) {
            Jmsg(jcr, M_WARNING, 0, _("Error getting Media record for Volume \"%s\": ERR=%s"),
               mr.VolumeName, db_strerror(jcr->db));
         }
      }

      switch (jcr->JobStatus) {
      case JS_Terminated:
         term_msg = _("%s OK");
         break;
      case JS_Warnings:
         term_msg = _("%s OK -- with warnings");
         break;
      case JS_FatalError:
      case JS_ErrorTerminated:
      case JS_Canceled:
         /*
          * We catch any error here as the close of the SD sessions is mandatory for each
          * failure path. The termination message and the message type can be different
          * so that is why we do a second switch inside the switch on the JobStatus.
          */
         switch (jcr->JobStatus) {
         case JS_Canceled:
            term_msg = _("%s Canceled");
            break;
         default:
            term_msg = _("*** %s Error ***");
            msg_type = M_ERROR;          /* Generate error message */
            break;
         }

         /*
          * Close connection to Reading SD.
          */
         if (jcr->store_bsock) {
            jcr->store_bsock->signal(BNET_TERMINATE);
            if (jcr->SD_msg_chan_started) {
               pthread_cancel(jcr->SD_msg_chan);
            }
         }

         /*
          * Close connection to Writing SD (if SD-SD replication)
          */
         if (mig_jcr->store_bsock) {
            mig_jcr->store_bsock->signal(BNET_TERMINATE);
            if (mig_jcr->SD_msg_chan_started) {
               pthread_cancel(mig_jcr->SD_msg_chan);
            }
         }
         break;
      default:
         term_msg = _("Inappropriate %s term code");
         break;
      }
   } else {
      if (jcr->is_JobType(JT_MIGRATE) && jcr->previous_jr.JobId != 0) {
         /*
          * Mark previous job as migrated
          */
         Mmsg(query, "UPDATE Job SET Type='%c' WHERE JobId=%s",
              (char)JT_MIGRATED_JOB, edit_uint64(jcr->previous_jr.JobId, ec1));
         db_sql_query(jcr->db, query.c_str());
      }
      term_msg = _("%s -- no files to %s");
   }

   bsnprintf(term_code, sizeof(term_code), term_msg, jcr->get_OperationName(), jcr->get_ActionName());
   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);
   RunTime = jcr->jr.EndTime - jcr->jr.StartTime;
   if (RunTime <= 0) {
      kbps = 0;
   } else {
      kbps = (double)jcr->SDJobBytes / (1000 * RunTime);
   }

   jobstatus_to_ascii(jcr->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));

   Jmsg(jcr, msg_type, 0, _("%s %s %s (%s):\n"
        "  Build OS:               %s %s %s\n"
        "  Prev Backup JobId:      %s\n"
        "  Prev Backup Job:        %s\n"
        "  New Backup JobId:       %s\n"
        "  Current JobId:          %s\n"
        "  Current Job:            %s\n"
        "  Backup Level:           %s\n"
        "  Client:                 %s\n"
        "  FileSet:                \"%s\"\n"
        "  Read Pool:              \"%s\" (From %s)\n"
        "  Read Storage:           \"%s\" (From %s)\n"
        "  Write Pool:             \"%s\" (From %s)\n"
        "  Write Storage:          \"%s\" (From %s)\n"
        "  Next Pool:              \"%s\" (From %s)\n"
        "  Catalog:                \"%s\" (From %s)\n"
        "  Start time:             %s\n"
        "  End time:               %s\n"
        "  Elapsed time:           %s\n"
        "  Priority:               %d\n"
        "  SD Files Written:       %s\n"
        "  SD Bytes Written:       %s (%sB)\n"
        "  Rate:                   %.1f KB/s\n"
        "  Volume name(s):         %s\n"
        "  Volume Session Id:      %d\n"
        "  Volume Session Time:    %d\n"
        "  Last Volume Bytes:      %s (%sB)\n"
        "  SD Errors:              %d\n"
        "  SD termination status:  %s\n"
        "  Termination:            %s\n\n"),
        BAREOS, my_name, VERSION, LSMDATE,
        HOST_OS, DISTNAME, DISTVER,
        edit_uint64(jcr->previous_jr.JobId, ec6),
        jcr->previous_jr.Job,
        mig_jcr ? edit_uint64(mig_jcr->jr.JobId, ec7) : "0",
        edit_uint64(jcr->jr.JobId, ec8),
        jcr->jr.Job,
        level_to_str(jcr->getJobLevel()),
        jcr->res.client ? jcr->res.client->name() :  _("*None*"),
        jcr->res.fileset ? jcr->res.fileset->name() :  _("*None*"),
        jcr->res.rpool->name(), jcr->res.rpool_source,
        jcr->res.rstore ? jcr->res.rstore->name() : _("*None*"),
        NPRT(jcr->res.rstore_source),
        jcr->res.pool->name(), jcr->res.pool_source,
        jcr->res.wstore ? jcr->res.wstore->name() : _("*None*"),
        NPRT(jcr->res.wstore_source),
        jcr->res.next_pool ? jcr->res.next_pool->name() : _("*None*"),
        NPRT(jcr->res.npool_source),
        jcr->res.catalog->name(), jcr->res.catalog_source,
        sdt,
        edt,
        edit_utime(RunTime, elapsed, sizeof(elapsed)),
        jcr->JobPriority,
        edit_uint64_with_commas(jcr->SDJobFiles, ec1),
        edit_uint64_with_commas(jcr->SDJobBytes, ec2),
        edit_uint64_with_suffix(jcr->SDJobBytes, ec3),
        (float)kbps,
        mig_jcr ? mig_jcr->VolumeName : "",
        jcr->VolSessionId,
        jcr->VolSessionTime,
        edit_uint64_with_commas(mr.VolBytes, ec4),
        edit_uint64_with_suffix(mr.VolBytes, ec5),
        jcr->SDErrors,
        sd_term_msg,
        term_code);

   Dmsg0(100, "Leave migrate_cleanup()\n");
}

/*
 * Return next DBId from comma separated list
 *
 * Returns:
 *   1 if next DBId returned
 *   0 if no more DBIds are in list
 *  -1 there is an error
 */
static int get_next_dbid_from_list(char **p, DBId_t *DBId)
{
   int i;
   const int maxlen = 30;
   char id[maxlen+1];
   char *q = *p;

   id[0] = 0;
   for (i = 0; i < maxlen; i++) {
      if (*q == 0) {
         break;
      } else if (*q == ',') {
         q++;
         break;
      }
      id[i] = *q++;
      id[i+1] = 0;
   }
   if (id[0] == 0) {
      return 0;
   } else if (!is_a_number(id)) {
      return -1;                      /* error */
   }
   *p = q;
   *DBId = str_to_int64(id);
   return 1;
}

bool set_migration_wstorage(JCR *jcr, POOLRES *pool, POOLRES *next_pool, const char *where)
{
   if (!next_pool) {
      Jmsg(jcr, M_FATAL, 0, _("No Next Pool specification found in Pool \"%s\".\n"),
           pool->hdr.name);
      return false;
   }

   if (!next_pool->storage || next_pool->storage->size() == 0) {
      Jmsg(jcr, M_FATAL, 0, _("No Storage specification found in Next Pool \"%s\".\n"),
           next_pool->name());
      return false;
   }

   copy_wstorage(jcr, next_pool->storage, where);

   return true;
}
