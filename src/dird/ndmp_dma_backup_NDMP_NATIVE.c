/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
   Copyright (C) 2013-2017 Bareos GmbH & Co. KG

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
 * for NDMP_NATIVE Backups
 *
 * Philipp Storz, April 2017
 */

#include "bareos.h"
#include "dird.h"

#if HAVE_NDMP

#define NDMP_NEED_ENV_KEYWORDS 1

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"


/* Forward referenced functions */
static inline bool extract_post_backup_stats_ndmp_native(JCR *jcr,
                                             char *filesystem,
                                             struct ndm_session *sess);



/**
 * Load next medium, also used as "load first" callback
 * find the next volume for append and add it to the
 * media tab of the current ndmp job so that the next
 * medium can be loaded.
 *
 * \return 0 if OK, -1 if NOT OK
 */

int ndmp_load_next(struct ndm_session *sess) {

   NIS *nis = (NIS *)sess->param->log.ctx;
   JCR *jcr = nis->jcr;
   struct ndm_media_table	*media_tab;
   media_tab = &sess->control_acb->job.media_tab;
   MEDIA_DBR mr;
   char *unwanted_volumes = (char *)"";
   bool create = false;
   bool prune = false;
   struct ndmmedia *media;
   int index = 1;
   STORERES *store = jcr->res.wstore;

   /*
    * get the poolid for pool name
    */
   mr.PoolId = jcr->jr.PoolId;


   if ( find_next_volume_for_append(jcr, &mr, index, unwanted_volumes, create, prune) ) {

      Jmsg(jcr, M_INFO, 0, _("Found volume for append: %s\n"), mr.VolumeName);

      /*
       * reserve medium so that it cannot be used by other job while we use it
       */
      bstrncpy(mr.VolStatus, NT_("Used"), sizeof(mr.VolStatus));


      /*
       * set FirstWritten Timestamp
       */
      mr.FirstWritten = (utime_t)time(NULL);

      if (!jcr->db->update_media_record(jcr, &mr)) {
         Jmsg(jcr, M_FATAL, 0, _("Catalog error updating Media record. %s"),
               jcr->db->strerror());
         goto bail_out;
      }

      /*
       * insert volume info in ndmmedia list
       */
      media = ndma_store_media(media_tab, mr.Slot);

      bstrncpy(media->label, mr.VolumeName, NDMMEDIA_LABEL_MAX - 1);
      media->valid_label = NDMP9_VALIDITY_VALID;



      /*
       * we want to append, so we need to skip what is already written
       */
      media->file_mark_offset = mr.VolFiles + 1;
      media->valid_filemark = NDMP9_VALIDITY_VALID;

      media->slot_addr = mr.Slot;

      if ( !ndmp_update_storage_mappings(jcr, store) ){
         Jmsg(jcr, M_ERROR, 0, _("ERROR in ndmp_update_storage_mappings\n"));
         goto bail_out;
      }

      if ( media->slot_addr < 0 ) {
         Jmsg(jcr, M_FATAL, 0, _("lookup_storage_mapping failed\n"));
         goto bail_out;
      }

      slot_number_t slotnumber = lookup_storage_mapping(store, slot_type_normal, LOGICAL_TO_PHYSICAL, mr.Slot);
      /*
       * check if lookup_storage_mapping was successful
       */
      if ( slotnumber < 0 ) {
         Jmsg(jcr, M_FATAL, 0, _("lookup_storage_mapping failed\n"));
         goto bail_out;
      }

      media->slot_addr = slotnumber;
      media->valid_slot = NDMP9_VALIDITY_VALID;

      ndmca_media_calculate_offsets(sess);

      return 0;

   } else {

bail_out:
      Jmsg(jcr, M_INFO, 0, _("Error finding volume for append\n"));
      return -1;
   }
}

/*
 * Run a NDMP backup session for NDMP_NATIVE backup
 */
bool do_ndmp_backup_ndmp_native(JCR *jcr)
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

   struct ndmca_media_callbacks  media_callbacks;

   media_callbacks.load_first = ndmp_load_next; /* we use the same callback for first and next*/
   media_callbacks.load_next = ndmp_load_next;
   media_callbacks.unload_current = NULL;


   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start NDMP Backup JobId %s, Job=%s\n"),
         edit_uint64(jcr->JobId, ed1), jcr->Job);

   jcr->setJobStatus(JS_Running);
   Dmsg2(100, "JobId=%d JobLevel=%c\n", jcr->jr.JobId, jcr->jr.JobLevel);
   if (!jcr->db->update_job_start_record(jcr, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
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

   status = 0;
   STORERES *store = jcr->res.wstore;
   int drive = 0;

   /*
    * Initialize the ndmp backup job. We build the generic job only once
    * and reuse the job definition for each separate sub-backup we perform as
    * part of the whole job. We only free the env_table between every sub-backup.
    */
   if (!ndmp_build_client_and_storage_job(jcr, jcr->res.wstore, jcr->res.client,
            true, /* init_tape */
            true, /* init_robot */
            NDM_JOB_OP_BACKUP, &ndmp_job)) {
      goto bail_out;
   }

   /*
    * Set the remote robotics name to use.
    * We use the ndmscsi_target_from_str() function which parses the NDMJOB format of a
    * device in the form NAME[,[CNUM,]SID[,LUN]
    */
   ndmp_job.robot_target = (struct ndmscsi_target *)actuallymalloc(sizeof(struct ndmscsi_target));
   if (ndmscsi_target_from_str(ndmp_job.robot_target, store->ndmp_changer_device) != 0) {
      actuallyfree(ndmp_job.robot_target);
      Dmsg0(100,"ndmp_send_label_request: no robot to use\n");
      return retval;
   }

   ndmp_job.have_robot = 1;
   /*
    * unload tape if tape is in drive
    */
   ndmp_job.auto_remedy = 1;

   /*
    * Set the remote tape drive to use.
    */
   ndmp_job.tape_device = lookup_ndmp_drive(store, drive);
   ndmp_job.record_size = jcr->res.client->ndmp_blocksize;

   Jmsg(jcr, M_INFO, 0, _("Using Data  host %s\n"), ndmp_job.data_agent.host);
   Jmsg(jcr, M_INFO, 0, _("Using Tape  host:device  %s:%s\n"), ndmp_job.tape_agent.host, ndmp_job.tape_device);
   Jmsg(jcr, M_INFO, 0, _("Using Robot host:device  %s:%s\n"), ndmp_job.robot_agent.host, ndmp_job.robot_target);
   Jmsg(jcr, M_INFO, 0, _("Using Tape record size %d\n"), ndmp_job.record_size);

   if (!ndmp_job.tape_device) {
      actuallyfree(ndmp_job.robot_target);
      Dmsg0(100,"ndmp: no tape drive to use\n");
      return retval;
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
         ndmp_sess.dump_media_info = 1;

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
         /* register the callbacks */
         ndmca_media_register_callbacks (&ndmp_sess, &media_callbacks);

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

         // FIXME: disabled because of "missing media entry" error
         //if (!ndmp_validate_job(jcr, &ndmp_sess.control_acb->job)) {
         //   goto cleanup;
         //}

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
         if (!extract_post_backup_stats_ndmp_native(jcr, item, &ndmp_sess)) {
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
      /*
         if (ndmp_sess.control_acb->job.tape_device) {
         free(ndmp_sess.control_acb->job.tape_device);
         }
         */
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

ok_out:
   if (nis) {
      if (nis->virtual_filename) {
         free(nis->virtual_filename);
      }
      free(nis);
   }

   if (status == JS_Terminated) {
      ndmp_backup_cleanup(jcr, status);
   }

   return retval;
}

/*
 * Setup a NDMP backup session.
 */
bool do_ndmp_backup_init_ndmp_native(JCR *jcr)
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
   if (!jcr->db->update_job_start_record(jcr, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
      return false;
   }

   /*
    * If pool storage specified, use it instead of job storage
    */
   copy_wstorage(jcr, jcr->res.pool->storage, _("Pool resource"));

   if (!jcr->res.wstorage) {
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

   return true;
}

/*
 * Extract any post backup statistics for native NDMP
 */
static inline bool extract_post_backup_stats_ndmp_native(JCR *jcr,
                                             char *filesystem,
                                             struct ndm_session *sess)
{
   bool retval = true;
   struct ndmmedia *media;
   ndmp_backup_format_option *nbf_options;
   struct ndm_env_entry *ndm_ee;
   char mediabuf[100];
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
    * extract_post_backup_stats
    */
   for (media = sess->control_acb->job.media_tab.head; media; media = media->next) {

      /*
       * translate Physical to Logical Slot before storing into database
       */

      media->slot_addr = lookup_storage_mapping(jcr->res.wstore, slot_type_normal,
                                                  PHYSICAL_TO_LOGICAL, media->slot_addr);
#if 0
      Jmsg(jcr, M_INFO, 0, _("Physical Slot is %d\n"), media->slot_addr);
      Jmsg(jcr, M_INFO, 0, _("Logical slot is : %d\n"), media->slot_addr);
      Jmsg(jcr, M_INFO, 0, _("label           : %s\n"), media->label);
      Jmsg(jcr, M_INFO, 0, _("index           : %d\n"), media->index);
      Jmsg(jcr, M_INFO, 0, _("n_bytes         : %lld\n"), media->n_bytes);
      Jmsg(jcr, M_INFO, 0, _("begin_offset    : %u\n"), media->begin_offset);
      Jmsg(jcr, M_INFO, 0, _("end_offset      : %u\n"), media->end_offset);
#endif

      store_ndmmedia_info_in_database(media, jcr);

      ndmmedia_to_str(media, mediabuf);

      Jmsg(jcr, M_INFO, 0, _("Media: %s\n"), mediabuf);

      /*
       * See if there is any media error.
       */
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
    * insert batched files into database
    */

   jcr->db->write_batch_file_records(jcr);
   /*
    * Update the Job statistics from the NDMP statistics.
    */
   jcr->JobBytes += sess->control_acb->job.bytes_written;

   /*
    * After a successful backup we need to store all NDMP ENV variables
    * for doing a successful restore operation.
    */
   ndm_ee = sess->control_acb->job.result_env_tab.head;
   while (ndm_ee) {
      if (!jcr->db->create_ndmp_environment_string(jcr, &jcr->jr,
                                             ndm_ee->pval.name, ndm_ee->pval.value)) {
         break;
      }
      ndm_ee = ndm_ee->next;
   }

   /*
    * If we are doing a backup type that uses dumplevels save the last used dump level.
    */
   if (nbf_options && nbf_options->uses_level) {
      jcr->db->update_ndmp_level_mapping(jcr, &jcr->jr,
                                   filesystem, sess->control_acb->job.bu_level);
   }

   return retval;
}

#else

bool do_ndmp_backup_init_ndmp_native(JCR *jcr)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

bool do_ndmp_backup_ndmp_native(JCR *jcr)
{
   Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
   return false;
}

#endif /* HAVE_NDMP */
