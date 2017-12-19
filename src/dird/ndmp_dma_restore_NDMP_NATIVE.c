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
 * Restore specific NDMP Data Management Application (DMA) routines
 * for NDMP_NATIVE restores
 *
 * extracted and reorganized from ndmp_dma_restore.c
 * Philipp Storz, April 2017
 */

#include "bareos.h"
#include "dird.h"

#if HAVE_NDMP

#define NDMP_NEED_ENV_KEYWORDS 1

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"

/*
 * Fill the NDMP restore environment table with the data for the data agent to act on.
 */
static inline bool fill_restore_environment_ndmp_native(JCR *jcr,
                                            int32_t current_fi,
                                            struct ndm_job_param *job)
{
   int i;
   char *bp;
   ndmp9_pval pv;
   FILESETRES *fileset;
   char *restore_pathname,
        *ndmp_filesystem,
        *restore_prefix;
   POOL_MEM tape_device;
   POOL_MEM destination_path;
   ndmp_backup_format_option *nbf_options;

   ndmp_filesystem = NULL;

   /*
    * See if we know this backup format and get it options.
    */
   nbf_options = ndmp_lookup_backup_format_options(job->bu_type);


   /*
    * Selected JobIds are stored in jcr->JobIds, comma separated
    * We use the first jobid to get the environment string
    */

   JobId_t JobId = str_to_int32(jcr->JobIds);
   // TODO: Check if JobId is Zero as this indicates error

   if (jcr->db->get_ndmp_environment_string(jcr, JobId,
                           ndmp_env_handler, &job->env_tab)) {
      /*
       * extract ndmp_filesystem from environment
       */
      for (struct ndm_env_entry *entry = job->env_tab.head; entry; entry = entry->next) {
         if (bstrcmp(entry->pval.name,ndmp_env_keywords[NDMP_ENV_KW_FILESYSTEM])) {
            ndmp_filesystem = entry->pval.value;
            break;
         }
      }
   } else {
      /*
       * Fallback code try to build a environment stack that is good enough to
       * restore this NDMP backup. This is used when the data is not available in
       * the database when its either expired or when an old NDMP backup is restored
       * where the whole environment was not saved.
       */

      if (!nbf_options || nbf_options->uses_file_history) {
         /*
          * We asked during the NDMP backup to receive file history info.
          */
         pv.name = ndmp_env_keywords[NDMP_ENV_KW_HIST];
         pv.value = ndmp_env_values[NDMP_ENV_VALUE_YES];
         ndma_store_env_list(&job->env_tab, &pv);
      }

      /*
       * Tell the data agent what type of restore stream to expect.
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_TYPE];
      pv.value = job->bu_type;
      ndma_store_env_list(&job->env_tab, &pv);

      /*
       * Tell the data engine what was backuped.
       */
      pv.name = ndmp_env_keywords[NDMP_ENV_KW_FILESYSTEM];
      pv.value = ndmp_filesystem;
      ndma_store_env_list(&job->env_tab, &pv);
   }


   /*
    * See where to restore the data.
    */
   restore_prefix = NULL;
   if (jcr->where) {
      restore_prefix = jcr->where;
   } else {
      restore_prefix = jcr->res.job->RestoreWhere;
   }

   if (!restore_prefix) {
      return false;
   }

   /*
    * Tell the data engine where to restore.
    */
   if (nbf_options && nbf_options->restore_prefix_relative) {
      switch (*restore_prefix) {
      case '^':
         /*
          * Use the restore_prefix as an absolute restore prefix.
          * We skip the leading ^ that is the trigger for absolute restores.
          */
         pm_strcpy(destination_path, restore_prefix + 1);
         break;
      default:
         /*
          * Use the restore_prefix as an relative restore prefix.
          */
         if (strlen(restore_prefix) == 1 && *restore_prefix == '/') {
            pm_strcpy(destination_path, ndmp_filesystem);
         } else {
            pm_strcpy(destination_path, ndmp_filesystem);
            pm_strcat(destination_path, restore_prefix);
         }
      }
   } else {
      if (strlen(restore_prefix) == 1 && *restore_prefix == '/') {
         /*
          * Use the original pathname as restore prefix.
          */
         pm_strcpy(destination_path, ndmp_filesystem);
      } else {
         /*
          * Use the restore_prefix as an absolute restore prefix.
          */
         pm_strcpy(destination_path, restore_prefix);
      }
   }

   pv.name = ndmp_env_keywords[NDMP_ENV_KW_PREFIX];
   pv.value = ndmp_filesystem;
   ndma_store_env_list(&job->env_tab, &pv);

   if (ndmp_filesystem &&
         set_files_to_restore_ndmp_native(jcr, job, current_fi,
            destination_path.c_str(), ndmp_filesystem) == 0) {
      return true;
   }
   return false;
}

/*
 * See in the tree with selected files what files were selected to be restored.
 */
int set_files_to_restore_ndmp_native(JCR *jcr, struct ndm_job_param *job, int32_t FileIndex,
                                       const char *restore_prefix, const char *ndmp_filesystem)
{
   int len;
   int cnt = 0;
   TREE_NODE *node, *parent;
   POOL_MEM restore_pathname, tmp;

   node = first_tree_node(jcr->restore_tree_root);
   while (node) {
      /*
       * node->extract_dir  means that only the directory should be selected for extraction
       * itself, the subdirs and subfiles are not automaticaly marked for extraction ( i.e. set node->extract)
       *
       * We can use this to select a directory for DDAR.
       *
       * If "DIRECT = Y" AND "RECURSIVE = Y" are set, directories in the namelist will be
       * recursively extracted.
       *
       * Restoring a whole directory using this mechanism is much more efficient than creating
       * an namelist entry for every single file and directory below the selected one.
       */
      if (node->extract_dir || node->extract) {
         pm_strcpy(restore_pathname, node->fname);
         /*
          * Walk up the parent until we hit the head of the list.
          */
         for (parent = node->parent; parent; parent = parent->parent) {
            pm_strcpy(tmp, restore_pathname.c_str());
            Mmsg(restore_pathname, "%s/%s", parent->fname, tmp.c_str());
         }
         /*
          * only add nodes that have valid DAR info i.e. fhinfo is not NDMP9_INVALID_U_QUAD
          */
         if (node->fhinfo != NDMP9_INVALID_U_QUAD) {
            /*
             * See if we need to strip the prefix from the filename.
             */
            len = 0;
            if (ndmp_filesystem &&
                  bstrncmp(restore_pathname.c_str(), ndmp_filesystem, strlen(ndmp_filesystem))) {
               len = strlen(ndmp_filesystem);
            }

            Jmsg(jcr, M_INFO, 0, _("Namelist add: node:%llu, info:%llu, name:\"%s\" \n"),
                  node->fhnode, node->fhinfo, restore_pathname.c_str());

            add_to_namelist(job,  restore_pathname.c_str() + len, restore_prefix,
                  (char *)"", (char *)"", node->fhnode, node->fhinfo);

            cnt++;

         } else {
            Jmsg(jcr, M_INFO, 0, _("not added node \"%s\" to namelist because "
                                        "of missing fhinfo: node:%llu info:%llu\n"),
                  restore_pathname.c_str(), node->fhnode, node->fhinfo );
         }

      }
      node = next_tree_node(node);
   }
   return cnt;
}

/**
 * Execute native NDMP restore.
 */
static bool do_ndmp_native_restore(JCR *jcr)
{
   int cnt;
   BSOCK *sd;
   BSR *bsr;
   NIS *nis = NULL;
   int32_t current_fi = 0;
   struct ndm_session ndmp_sess;
   struct ndm_job_param ndmp_job;
   bool session_initialized = false;
   bool retval = false;
   int NdmpLoglevel;
   char mediabuf[100];
   ndmmedia *media;
   slot_number_t ndmp_slot;

   if (jcr->res.client->ndmp_loglevel > me->ndmp_loglevel) {
      NdmpLoglevel = jcr->res.client->ndmp_loglevel;
   } else {
      NdmpLoglevel = me->ndmp_loglevel;
   }


   nis = (NIS *)malloc(sizeof(NIS));
   memset(nis, 0, sizeof(NIS));



   memset(&ndmp_sess, 0, sizeof(ndmp_sess));
   ndmp_sess.conn_snooping = (me->ndmp_snooping) ? 1 : 0;
   ndmp_sess.control_agent_enabled = 1;

   ndmp_sess.dump_media_info = 1; // for debugging

   /*
    * session initialize
    */
   ndmp_sess.param = (struct ndm_session_param *)malloc(sizeof(struct ndm_session_param));
   memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
   ndmp_sess.param->log.deliver = ndmp_loghandler;
   ndmp_sess.param->log_level = native_to_ndmp_loglevel(NdmpLoglevel, debug_level, nis);
   nis->jcr = jcr;
   ndmp_sess.param->log.ctx = nis;
   ndmp_sess.param->log_tag = bstrdup("DIR-NDMP");


   int drive=0;
   ndmp_job.tape_device = lookup_ndmp_drive(jcr->res.rstore, drive);
   if (!ndmp_build_client_and_storage_job(jcr, jcr->res.rstore, jcr->res.client,
            true, /* init_tape */
            true, /* init_robot */
            NDM_JOB_OP_EXTRACT, &ndmp_job)) {
      goto cleanup;
   }


   jcr->setJobStatus(JS_Running);


   /*
    * Initialize the session structure.
    */
   if (ndma_session_initialize(&ndmp_sess)) {
      goto cleanup_ndmp;
   }
   session_initialized = true;

   memcpy(&ndmp_sess.control_acb->job, &ndmp_job, sizeof(struct ndm_job_param));

   if (!fill_restore_environment_ndmp_native(jcr,
            current_fi,
            &ndmp_sess.control_acb->job)) {
      Jmsg(jcr, M_ERROR, 0, _("ERROR in fill_restore_environment\n"));
      goto cleanup_ndmp;
   }

   ndmp_job.tape_device = lookup_ndmp_drive(jcr->res.rstore, drive);
   ndmp_job.record_size = jcr->res.client->ndmp_blocksize;
   Jmsg(jcr, M_INFO, 0, _("Record size is %d\n"), ndmp_job.record_size);

   ndmp_sess.control_acb->job.tape_device = ndmp_job.tape_device;
   ndmp_sess.control_acb->job.record_size = ndmp_job.record_size;


   get_ndmmedia_info_from_database(&ndmp_sess.control_acb->job.media_tab, jcr);

   for (ndmmedia *media = ndmp_sess.control_acb->job.media_tab.head; media; media = media->next) {
      ndmmedia_to_str(media, mediabuf);
      Jmsg(jcr, M_INFO, 0, _("Media: %s\n"), mediabuf);
   }

   for (ndmmedia *media = ndmp_sess.control_acb->job.media_tab.head; media; media = media->next) {

      if (!ndmp_update_storage_mappings(jcr, jcr->res.rstore )){
         Jmsg(jcr, M_ERROR, 0, _("ERROR in ndmp_update_storage_mappings\n"));
      }
      /*
       * convert slot from database to ndmp slot
       */
      Jmsg(jcr, M_INFO, 0, _("Logical slot for volume %s is %d\n"), media->label, media->slot_addr);

      ndmp_slot = lookup_storage_mapping(jcr->res.rstore, slot_type_normal, LOGICAL_TO_PHYSICAL, media->slot_addr);
      media->slot_addr = ndmp_slot;

      Jmsg(jcr, M_INFO, 0, _("Physical(NDMP) slot for volume %s is %d\n"), media->label, media->slot_addr);
      Jmsg(jcr, M_INFO, 0, _("Media Index of volume %s is %d\n"), media->label, media->index);

   }

   ndmca_media_calculate_offsets(&ndmp_sess);

   /*
    * Set the robot to use
    */
   ndmp_sess.control_acb->job.robot_target = (struct ndmscsi_target *)actuallymalloc(sizeof(struct ndmscsi_target));
   if (ndmscsi_target_from_str(ndmp_sess.control_acb->job.robot_target, jcr->res.rstore->ndmp_changer_device) != 0) {
      actuallyfree(ndmp_sess.control_acb->job.robot_target);
      Dmsg0(100,"no robot to use\n");
      goto bail_out;
   }
   ndmp_sess.control_acb->job.have_robot = 1;

   if (!ndmp_validate_job(jcr, &ndmp_sess.control_acb->job)) {
      Jmsg(jcr, M_ERROR, 0, _("ERROR in ndmp_validate_job\n"));
      goto cleanup_ndmp;
   }

   /*
    * Commission the session for a run.
    */
   if (ndma_session_commission(&ndmp_sess)) {
      Jmsg(jcr, M_ERROR, 0, _("ERROR in ndma_session_commission\n"));
      goto cleanup_ndmp;
   }

   /*
    * Setup the DMA.
    */
   if (ndmca_connect_control_agent(&ndmp_sess)) {
      Jmsg(jcr, M_ERROR, 0, _("ERROR in ndmca_connect_control_agent\n"));
      goto cleanup_ndmp;
   }

   ndmp_sess.conn_open = 1;
   ndmp_sess.conn_authorized = 1;

   /*
    * Let the DMA perform its magic.
    */
   if (ndmca_control_agent(&ndmp_sess) != 0) {
      Jmsg(jcr, M_ERROR, 0, _("ERROR in ndmca_control_agent\n"));
      goto cleanup_ndmp;
   }

   /*
    * See if there were any errors during the restore.
    */
   if (!extract_post_restore_stats(jcr, &ndmp_sess)) {
      Jmsg(jcr, M_ERROR, 0, _("ERROR in extract_post_restore_stats\n"));
      goto cleanup_ndmp;
   }

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

   /*
    * Jump to the generic cleanup done for every Job.
    */
   retval = true;
   goto cleanup;

cleanup_ndmp:
   /*
    * Only need to cleanup when things are initialized.
    */
   if (session_initialized) {
      ndma_destroy_env_list(&ndmp_sess.control_acb->job.env_tab);
      ndma_destroy_env_list(&ndmp_sess.control_acb->job.result_env_tab);
      ndma_destroy_nlist(&ndmp_sess.control_acb->job.nlist_tab);

      /*
       * Destroy the session.
       */
      ndma_session_destroy(&ndmp_sess);
   }

   if (ndmp_sess.param) {
      free(ndmp_sess.param->log_tag);
      free(ndmp_sess.param);
   }

cleanup:
   free(nis);

bail_out:
   free_tree(jcr->restore_tree_root);
   jcr->restore_tree_root = NULL;
   return retval;
}

/*
 * Run a NDMP restore session.
 */
bool do_ndmp_restore_ndmp_native(JCR *jcr)
{
   JOB_DBR rjr;                       /* restore job record */
   int status;

   memset(&rjr, 0, sizeof(rjr));
   jcr->jr.JobLevel = L_FULL;         /* Full restore */
   if (!jcr->db->update_job_start_record(jcr, &jcr->jr)) {
      Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
      goto bail_out;
   }
   Dmsg0(20, "Updated job start record\n");

   Dmsg1(20, "RestoreJobId=%d\n", jcr->res.job->RestoreJobId);

   /*
    * Validate the Job to have a NDMP client.
    */
   if (!ndmp_validate_client(jcr)) {
      return false;
   }

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start Restore Job %s\n"), jcr->Job);

   if (!do_ndmp_native_restore(jcr)) {
      goto bail_out;
   }

   status = JS_Terminated;
   ndmp_restore_cleanup(jcr, status);
   return true;

bail_out:
   return false;
}

#endif /* HAVE_NDMP */
