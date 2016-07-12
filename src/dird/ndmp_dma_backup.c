/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
   Copyright (C) 2013-2015 Bareos GmbH & Co. KG

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
 * Backup specific NDMP Data Management Application (DMA) routines
 *
 * Marco van Wieringen, May 2015
 */

#include "bareos.h"
#include "dird.h"

#if HAVE_NDMP

#define NDMP_NEED_ENV_KEYWORDS 1

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Imported variables */

/* Forward referenced functions */

/*
 * This glues the NDMP File Handle DB with internal code.
 */
static inline void register_callback_hooks(struct ndmlog *ixlog)
{
#ifdef HAVE_LMDB
   NIS *nis = (NIS *)ixlog->ctx;

   if (nis->jcr->res.client->ndmp_use_lmdb) {
      ndmp_fhdb_lmdb_register(ixlog);
   } else {
      ndmp_fhdb_mem_register(ixlog);
   }
#else
   ndmp_fhdb_mem_register(ixlog);
#endif
}

static inline void unregister_callback_hooks(struct ndmlog *ixlog)
{
#ifdef HAVE_LMDB
   NIS *nis = (NIS *)ixlog->ctx;

   if (nis->jcr->res.client->ndmp_use_lmdb) {
      ndmp_fhdb_lmdb_unregister(ixlog);
   } else {
      ndmp_fhdb_mem_unregister(ixlog);
   }
#else
   ndmp_fhdb_mem_unregister(ixlog);
#endif
}

static inline void process_fhdb(struct ndmlog *ixlog)
{
#ifdef HAVE_LMDB
   NIS *nis = (NIS *)ixlog->ctx;

   if (nis->jcr->res.client->ndmp_use_lmdb) {
      ndmp_fhdb_lmdb_process_db(ixlog);
   } else {
      ndmp_fhdb_mem_process_db(ixlog);
   }
#else
   ndmp_fhdb_mem_process_db(ixlog);
#endif
}

static inline int native_to_ndmp_level(JCR *jcr, char *filesystem)
{
   int level = -1;

   if (!db_create_ndmp_level_mapping(jcr, jcr->db, &jcr->jr, filesystem)) {
      return -1;
   }

   switch (jcr->getJobLevel()) {
   case L_FULL:
      level = 0;
      break;
   case L_DIFFERENTIAL:
      level = 1;
      break;
   case L_INCREMENTAL:
      level = db_get_ndmp_level_mapping(jcr, jcr->db, &jcr->jr, filesystem);
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Illegal Job Level %c for NDMP Job\n"), jcr->getJobLevel());
      break;
   }

   /*
    * Dump level can be from 0 - 9
    */
   if (level < 0 || level > 9) {
      Jmsg(jcr, M_FATAL, 0, _("NDMP dump format doesn't support more than 8 "
                              "incrementals, please run a Differential or a Full Backup\n"));
      level = -1;
   }

   return level;
}

/*
 * Fill the NDMP backup environment table with the data for the data agent to act on.
 */
static inline bool fill_backup_environment(JCR *jcr,
                                           INCEXE *ie,
                                           char *filesystem,
                                           struct ndm_job_param *job)
{
   int i, j, cnt;
   bool exclude;
   FOPTS *fo;
   ndmp9_pval pv;
   POOL_MEM pattern;
   POOL_MEM tape_device;
   ndmp_backup_format_option *nbf_options;

   /*
    * See if we know this backup format and get it options.
    */
   nbf_options = ndmp_lookup_backup_format_options(job->bu_type);

   if (!nbf_options || nbf_options->uses_file_history) {
      /*
       * We want to receive file history info from the NDMP backup.
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_HIST];
      pv.value = ndmp_env_values[NDMP_ENV_VALUE_YES];
      ndma_store_env_list(&job->env_tab, &pv);
   } else {
      /*
       * We don't want to receive file history info from the NDMP backup.
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_HIST];
      pv.value = ndmp_env_values[NDMP_ENV_VALUE_NO];
      ndma_store_env_list(&job->env_tab, &pv);
   }

   /*
    * Tell the data agent what type of backup to make.
    */
   pv.name = ndmp_env_keywords[NDMP_ENV_KW_TYPE];
   pv.value = job->bu_type;
   ndma_store_env_list(&job->env_tab, &pv);

   /*
    * See if we are doing a backup type that uses dumplevels.
    */
   if (nbf_options && nbf_options->uses_level) {
      char text_level[50];

      /*
       * Set the dump level for the backup.
       */
      jcr->DumpLevel = native_to_ndmp_level(jcr, filesystem);
      job->bu_level = jcr->DumpLevel;
      if (job->bu_level == -1) {
         return false;
      }

      pv.name = ndmp_env_keywords[NDMP_ENV_KW_LEVEL];
      pv.value = edit_uint64(job->bu_level, text_level);
      ndma_store_env_list(&job->env_tab, &pv);

      /*
       * Update the dumpdates
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_UPDATE];
      pv.value = ndmp_env_values[NDMP_ENV_VALUE_YES];
      ndma_store_env_list(&job->env_tab, &pv);
   }

   /*
    * Tell the data engine what to backup.
    */
   pv.name = ndmp_env_keywords[NDMP_ENV_KW_FILESYSTEM];
   pv.value = filesystem;
   ndma_store_env_list(&job->env_tab, &pv);

   /*
    * Loop over each option block for this fileset and append any
    * INCLUDE/EXCLUDE and/or META tags to the env_tab of the NDMP backup.
    */
   for (i = 0; i < ie->num_opts; i++) {
      fo = ie->opts_list[i];

      /*
       * Pickup any interesting patterns.
       */
      cnt = 0;
      pm_strcpy(pattern, "");
      for (j = 0; j < fo->wild.size(); j++) {
         if (cnt != 0) {
            pm_strcat(pattern, ",");
         }
         pm_strcat(pattern, (char *)fo->wild.get(j));
         cnt++;
      }
      for (j = 0; j < fo->wildfile.size(); j++) {
         if (cnt != 0) {
            pm_strcat(pattern, ",");
         }
         pm_strcat(pattern, (char *)fo->wildfile.get(j));
         cnt++;
      }
      for (j = 0; j < fo->wilddir.size(); j++) {
         if (cnt != 0) {
            pm_strcat(pattern, ",");
         }
         pm_strcat(pattern, (char *)fo->wilddir.get(j));
         cnt++;
      }

      /*
       * See if this is a INCLUDE or EXCLUDE block.
       */
      if (cnt > 0) {
         exclude = false;
         for (j = 0; fo->opts[j] != '\0'; j++) {
            if (fo->opts[j] == 'e') {
               exclude = true;
               break;
            }
         }

         if (exclude) {
            pv.name = ndmp_env_keywords[NDMP_ENV_KW_EXCLUDE];
         } else {
            pv.name = ndmp_env_keywords[NDMP_ENV_KW_INCLUDE];
         }

         pv.value = pattern.c_str();
         ndma_store_env_list(&job->env_tab, &pv);
      }

      /*
       * Parse all specific META tags for this option block.
       */
      for (j = 0; j < fo->meta.size(); j++) {
         ndmp_parse_meta_tag(&job->env_tab, (char *)fo->meta.get(j));
      }
   }

   /*
    * If we have a paired storage definition we put the
    * - Storage Daemon Auth Key
    * - Filesystem
    * - Dumplevel
    * into the tape device name of the  NDMP session. This way the storage
    * daemon can link the NDMP data and the normal save session together.
    */
   if (jcr->store_bsock) {
      if (nbf_options && nbf_options->uses_level) {
         Mmsg(tape_device, "%s@%s%%%d", jcr->sd_auth_key, filesystem, jcr->DumpLevel);
      } else {
         Mmsg(tape_device, "%s@%s", jcr->sd_auth_key, filesystem);
      }
      job->tape_device = bstrdup(tape_device.c_str());
   }

   return true;
}

/*
 * Extract any post backup statistics.
 */
static inline bool extract_post_backup_stats(JCR *jcr,
                                             char *filesystem,
                                             struct ndm_session *sess)
{
   bool retval = true;
   struct ndmmedia *media;
   ndmp_backup_format_option *nbf_options;
   struct ndm_env_entry *ndm_ee;

   /*
    * See if we know this backup format and get it options.
    */
   nbf_options = ndmp_lookup_backup_format_options(sess->control_acb->job.bu_type);

   /*
    * See if an error was raised during the backup session.
    */
   if (sess->error_raised) {
      return false;
   }

   /*
    * See if there is any media error.
    */
   for (media = sess->control_acb->job.result_media_tab.head; media; media = media->next) {
      if (media->media_open_error ||
          media->media_io_error ||
          media->label_io_error ||
          media->label_mismatch ||
          media->fmark_error) {
         retval = false;
      }
   }

   /*
    * Process the FHDB.
    */
   process_fhdb(&sess->control_acb->job.index_log);

   /*
    * Update the Job statistics from the NDMP statistics.
    */
   jcr->JobBytes += sess->control_acb->job.bytes_written;

   /*
    * After a successfull backup we need to store all NDMP ENV variables
    * for doing a successfull restore operation.
    */
   ndm_ee = sess->control_acb->job.result_env_tab.head;
   while (ndm_ee) {
      if (!db_create_ndmp_environment_string(jcr, jcr->db, &jcr->jr,
                                             ndm_ee->pval.name, ndm_ee->pval.value)) {
         break;
      }
      ndm_ee = ndm_ee->next;
   }

   /*
    * If we are doing a backup type that uses dumplevels save the last used dump level.
    */
   if (nbf_options && nbf_options->uses_level) {
      db_update_ndmp_level_mapping(jcr, jcr->db, &jcr->jr,
                                   filesystem, sess->control_acb->job.bu_level);
   }

   return retval;
}

/*
 * Setup a NDMP backup session.
 */
bool do_ndmp_backup_init(JCR *jcr)
{
   free_rstorage(jcr);                   /* we don't read so release */

   if (!allow_duplicate_job(jcr)) {
      return false;
   }

   jcr->jr.PoolId = get_or_create_pool_record(jcr, jcr->res.pool->name());
   if (jcr->jr.PoolId == 0) {
      return false;
   }

   jcr->start_time = time(NULL);
   jcr->jr.StartTime = jcr->start_time;
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      return false;
   }

   /*
    * If pool storage specified, use it instead of job storage
    */
   copy_wstorage(jcr, jcr->res.pool->storage, _("Pool resource"));

   if (!jcr->wstorage) {
      Jmsg(jcr, M_FATAL, 0, _("No Storage specification found in Job or Pool.\n"));
      return false;
   }

   /*
    * Validate the Job to have a NDMP client and NDMP storage.
    */
   if (!ndmp_validate_client(jcr)) {
      return false;
   }

   if (!ndmp_validate_storage(jcr)) {
      return false;
   }

   /*
    * For now we only allow NDMP backups to bareos SD's
    * so we need a paired storage definition.
    */
   if (!has_paired_storage(jcr)) {
      Jmsg(jcr, M_FATAL, 0,
           _("Write storage doesn't point to storage definition with paired storage option.\n"));
      return false;
   }

   return true;
}

/*
 * Run a NDMP backup session.
 */
bool do_ndmp_backup(JCR *jcr)
{
   unsigned int cnt;
   int i, status;
   char ed1[100];
   NIS *nis = NULL;
   FILESETRES *fileset;
   struct ndm_job_param ndmp_job;
   struct ndm_session ndmp_sess;
   bool session_initialized = false;
   bool retval = false;
   int NdmpLoglevel;

   if (jcr->res.client->ndmp_loglevel > me->ndmp_loglevel) {
      NdmpLoglevel = jcr->res.client->ndmp_loglevel;
   } else {
      NdmpLoglevel = me->ndmp_loglevel;
   }

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start NDMP Backup JobId %s, Job=%s\n"),
        edit_uint64(jcr->JobId, ed1), jcr->Job);

   jcr->setJobStatus(JS_Running);
   Dmsg2(100, "JobId=%d JobLevel=%c\n", jcr->jr.JobId, jcr->jr.JobLevel);
   if (!db_update_job_start_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", db_strerror(jcr->db));
      return false;
   }

   if (check_hardquotas(jcr)) {
      Jmsg(jcr, M_FATAL, 0, "Quota Exceeded. Job terminated.");
      return false;
   }

   if (check_softquotas(jcr)) {
      Dmsg0(10, "Quota exceeded\n");
      Jmsg(jcr, M_FATAL, 0, "Soft Quota Exceeded / Grace Time expired. Job terminated.");
      return false;
   }

   /*
    * If we have a paired storage definition create a native connection
    * to a Storage daemon and make it ready to receive a backup.
    * The setup is more or less the same as for a normal non NDMP backup
    * only the data doesn't come in from a FileDaemon but from a NDMP
    * data mover which moves the data from the NDMP DATA AGENT to the NDMP
    * TAPE AGENT.
    */
   if (jcr->res.wstore->paired_storage) {
      set_paired_storage(jcr);

      jcr->setJobStatus(JS_WaitSD);
      if (!connect_to_storage_daemon(jcr, 10, me->SDConnectTimeout, true)) {
         return false;
      }

      /*
       * Now start a job with the Storage daemon
       */
      if (!start_storage_daemon_job(jcr, NULL, jcr->wstorage)) {
         return false;
      }

      /*
       * Start the job prior to starting the message thread below
       * to avoid two threads from using the BSOCK structure at
       * the same time.
       */
      if (!jcr->store_bsock->fsend("run")) {
         return false;
      }

      /*
       * Now start a Storage daemon message thread.  Note,
       *   this thread is used to provide the catalog services
       *   for the backup job.
       */
      if (!start_storage_daemon_message_thread(jcr)) {
         return false;
      }
      Dmsg0(150, "Storage daemon connection OK\n");
   }

   status = 0;

   /*
    * Initialize the ndmp backup job. We build the generic job only once
    * and reuse the job definition for each seperate sub-backup we perform as
    * part of the whole job. We only free the env_table between every sub-backup.
    */
   if (!ndmp_build_client_job(jcr, jcr->res.client, jcr->res.pstore, NDM_JOB_OP_BACKUP, &ndmp_job)) {
      goto bail_out;
   }

   nis = (NIS *)malloc(sizeof(NIS));
   memset(nis, 0, sizeof(NIS));

   /*
    * Loop over each include set of the fileset and fire off a NDMP backup of the included fileset.
    */
   cnt = 0;
   fileset = jcr->res.fileset;
   for (i = 0; i < fileset->num_includes; i++) {
      int j;
      char *item;
      INCEXE *ie = fileset->include_items[i];
      POOL_MEM virtual_filename(PM_FNAME);

      /*
       * Loop over each file = entry of the fileset.
       */
      for (j = 0; j < ie->name_list.size(); j++) {
         item = (char *)ie->name_list.get(j);

         /*
          * See if this is the first Backup run or not. For NDMP we can have multiple Backup
          * runs as part of the same Job. When we are saving data to a Native Storage Daemon
          * we let it know to expect a new backup session. It will generate a new authorization
          * key so we wait for the nextrun_ready conditional variable to be raised by the msg_thread.
          */
         if (jcr->store_bsock && cnt > 0) {
            jcr->store_bsock->fsend("nextrun");
            P(mutex);
            pthread_cond_wait(&jcr->nextrun_ready, &mutex);
            V(mutex);
         }

         /*
          * Perform the actual NDMP job.
          * Initialize a new NDMP session
          */
         memset(&ndmp_sess, 0, sizeof(ndmp_sess));
         ndmp_sess.conn_snooping = (me->ndmp_snooping) ? 1 : 0;
         ndmp_sess.control_agent_enabled = 1;

         ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
         memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
         ndmp_sess.param->log.deliver = ndmp_loghandler;
         ndmp_sess.param->log_level = native_to_ndmp_loglevel(NdmpLoglevel, debug_level, nis);
         nis->filesystem = item;
         nis->FileIndex = cnt + 1;
         nis->jcr = jcr;
         nis->save_filehist = jcr->res.job->SaveFileHist;
         nis->filehist_size = jcr->res.job->FileHistSize;

         ndmp_sess.param->log.ctx = nis;
         ndmp_sess.param->log_tag = bstrdup("DIR-NDMP");

         /*
          * Initialize the session structure.
          */
         if (ndma_session_initialize(&ndmp_sess)) {
            goto cleanup;
         }
         session_initialized = true;

         /*
          * Copy the actual job to perform.
          */
         memcpy(&ndmp_sess.control_acb->job, &ndmp_job, sizeof(struct ndm_job_param));

         /*
          * We can use the same private pointer used in the logging with the JCR in
          * the file index generation. We don't setup a index_log.deliver
          * function as we catch the index information via callbacks.
          */
         ndmp_sess.control_acb->job.index_log.ctx = ndmp_sess.param->log.ctx;

         if (!fill_backup_environment(jcr,
                                      ie,
                                      nis->filesystem,
                                      &ndmp_sess.control_acb->job)) {
            goto cleanup;
         }

         /*
          * The full ndmp archive has a virtual filename, we need it to hardlink the individual
          * file records to it. So we allocate it here once so its available during the whole
          * NDMP session.
          */
         if (bstrcasecmp(jcr->backup_format, "dump")) {
            Mmsg(virtual_filename, "/@NDMP%s%%%d", nis->filesystem, jcr->DumpLevel);
         } else {
            Mmsg(virtual_filename, "/@NDMP%s", nis->filesystem);
         }

         if (nis->virtual_filename) {
            free(nis->virtual_filename);
         }
         nis->virtual_filename = bstrdup(virtual_filename.c_str());

         ndma_job_auto_adjust(&ndmp_sess.control_acb->job);
         if (!ndmp_validate_job(jcr, &ndmp_sess.control_acb->job)) {
            goto cleanup;
         }

         /*
          * Commission the session for a run.
          */
         if (ndma_session_commission(&ndmp_sess)) {
            goto cleanup;
         }

         /*
          * Setup the DMA.
          */
         if (ndmca_connect_control_agent(&ndmp_sess)) {
            goto cleanup;
         }

         ndmp_sess.conn_open = 1;
         ndmp_sess.conn_authorized = 1;

         register_callback_hooks(&ndmp_sess.control_acb->job.index_log);

         /*
          * Let the DMA perform its magic.
          */
         if (ndmca_control_agent(&ndmp_sess) != 0) {
            goto cleanup;
         }

         /*
          * See if there were any errors during the backup.
          */
         jcr->jr.FileIndex = cnt + 1;
         if (!extract_post_backup_stats(jcr, item, &ndmp_sess)) {
            goto cleanup;
         }

         unregister_callback_hooks(&ndmp_sess.control_acb->job.index_log);

         /*
          * Reset the NDMP session states.
          */
         ndma_session_decommission(&ndmp_sess);

         /*
          * Cleanup the job after it has run.
          */
         ndma_destroy_env_list(&ndmp_sess.control_acb->job.env_tab);
         ndma_destroy_env_list(&ndmp_sess.control_acb->job.result_env_tab);
         ndma_destroy_nlist(&ndmp_sess.control_acb->job.nlist_tab);

         /*
          * Release any tape device name allocated.
          */
         if (ndmp_sess.control_acb->job.tape_device) {
            free(ndmp_sess.control_acb->job.tape_device);
            ndmp_sess.control_acb->job.tape_device = NULL;
         }

         /*
          * Destroy the session.
          */
         ndma_session_destroy(&ndmp_sess);

         /*
          * Free the param block.
          */
         free(ndmp_sess.param->log_tag);
         free(ndmp_sess.param);
         ndmp_sess.param = NULL;

         /*
          * Reset the initialized state so we don't try to cleanup again.
          */
         session_initialized = false;

         cnt++;
      }
   }

   status = JS_Terminated;
   retval = true;

   /*
    * Tell the storage daemon we are done.
    */
   if (jcr->store_bsock) {
      jcr->store_bsock->fsend("finish");
      wait_for_storage_daemon_termination(jcr);
      db_write_batch_file_records(jcr);    /* used by bulk batch file insert */
   }

   /*
    * If we do incremental backups it can happen that the backup is empty if
    * nothing changed but we always write a filestream. So we use the counter
    * which counts the number of actual NDMP backup sessions we run to completion.
    */
   if (jcr->JobFiles < cnt) {
      jcr->JobFiles = cnt;
   }

   /*
    * Jump to the generic cleanup done for every Job.
    */
   goto ok_out;

cleanup:
   /*
    * Only need to cleanup when things are initialized.
    */
   if (session_initialized) {
      ndma_destroy_env_list(&ndmp_sess.control_acb->job.env_tab);
      ndma_destroy_env_list(&ndmp_sess.control_acb->job.result_env_tab);
      ndma_destroy_nlist(&ndmp_sess.control_acb->job.nlist_tab);

      if (ndmp_sess.control_acb->job.tape_device) {
         free(ndmp_sess.control_acb->job.tape_device);
      }

      unregister_callback_hooks(&ndmp_sess.control_acb->job.index_log);

      /*
       * Destroy the session.
       */
      ndma_session_destroy(&ndmp_sess);
   }

   if (ndmp_sess.param) {
      free(ndmp_sess.param->log_tag);
      free(ndmp_sess.param);
   }

bail_out:
   /*
    * Error handling of failed Job.
    */
   status = JS_ErrorTerminated;
   jcr->setJobStatus(JS_ErrorTerminated);

   if (jcr->store_bsock) {
      cancel_storage_daemon_job(jcr);
      wait_for_storage_daemon_termination(jcr);
   }

ok_out:
   if (nis) {
      if (nis->virtual_filename) {
         free(nis->virtual_filename);
      }
      free(nis);
   }
   free_paired_storage(jcr);

   if (status == JS_Terminated) {
      ndmp_backup_cleanup(jcr, status);
   }

   return retval;
}

/*
 * Cleanup a NDMP backup session.
 */
void ndmp_backup_cleanup(JCR *jcr, int TermCode)
{
   const char *term_msg;
   char term_code[100];
   int msg_type = M_INFO;
   CLIENT_DBR cr;

   Dmsg2(100, "Enter ndmp_backup_cleanup %d %c\n", TermCode, TermCode);
   memset(&cr, 0, sizeof(cr));

   if (jcr->is_JobStatus(JS_Terminated) &&
      (jcr->JobErrors || jcr->SDErrors || jcr->JobWarnings)) {
      TermCode = JS_Warnings;
   }

   update_job_end(jcr, TermCode);

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

   Dmsg0(100, "Leave ndmp_backup_cleanup\n");
}
#else
bool do_ndmp_backup_init(JCR *jcr)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

bool do_ndmp_backup(JCR *jcr)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

void ndmp_backup_cleanup(JCR *jcr, int TermCode)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}
#endif /* HAVE_NDMP */
