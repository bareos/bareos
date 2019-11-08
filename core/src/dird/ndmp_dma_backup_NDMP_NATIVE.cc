/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "dird.h"
#include "dird/jcr_private.h"
#include "dird/dird_globals.h"
#include "dird/job.h"
#include "dird/next_vol.h"
#include "dird/quota.h"
#include "dird/storage.h"

#include "lib/edit.h"

#if HAVE_NDMP
#include "dird/ndmp_dma_backup_common.h"
#include "dird/ndmp_dma_generic.h"
#include "dird/ndmp_dma_storage.h"

#define NDMP_NEED_ENV_KEYWORDS 1

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"
#endif /* HAVE_NDMP */

namespace directordaemon {

#if HAVE_NDMP
/* Forward referenced functions */
static inline bool extract_post_backup_stats_ndmp_native(
    JobControlRecord* jcr,
    char* filesystem,
    struct ndm_session* sess);


/**
 * callback to check if job is cancelled and NDMP should stop
 */
int is_job_canceled_cb(struct ndm_session* sess)
{
  NIS* nis = (NIS*)sess->param->log.ctx;
  JobControlRecord* jcr = nis->jcr;

  return jcr->IsCanceled();
}


/**
 * Load next medium, also used as "load first" callback
 * find the next volume for append and add it to the
 * media tab of the current ndmp job so that the next
 * medium can be loaded.
 *
 * \return 0 if OK, -1 if NOT OK
 */

int NdmpLoadNext(struct ndm_session* sess)
{
  NIS* nis = (NIS*)sess->param->log.ctx;
  JobControlRecord* jcr = nis->jcr;
  struct ndm_media_table* media_tab;
  media_tab = &sess->control_acb->job.media_tab;
  MediaDbRecord mr;
  char* unwanted_volumes = (char*)"";
  bool create = false;
  bool prune = false;
  struct ndmmedia* media;
  int index = 1;
  StorageResource* store = jcr->impl->res.write_storage;

  /*
   * get the poolid for pool name
   */
  mr.PoolId = jcr->impl->jr.PoolId;


  if (FindNextVolumeForAppend(jcr, &mr, index, unwanted_volumes, create,
                              prune)) {
    Jmsg(jcr, M_INFO, 0, _("Found volume for append: %s\n"), mr.VolumeName);

    /*
     * reserve medium so that it cannot be used by other job while we use it
     */
    bstrncpy(mr.VolStatus, NT_("Used"), sizeof(mr.VolStatus));


    /*
     * set FirstWritten Timestamp
     */
    mr.FirstWritten = (utime_t)time(NULL);

    if (!jcr->db->UpdateMediaRecord(jcr, &mr)) {
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

    if (!NdmpUpdateStorageMappings(jcr, store)) {
      Jmsg(jcr, M_ERROR, 0, _("ERROR in NdmpUpdateStorageMappings\n"));
      goto bail_out;
    }

    slot_number_t slotnumber = GetElementAddressByBareosSlotNumber(
        &store->runtime_storage_status->storage_mapping,
        slot_type_t::kSlotTypeStorage, mr.Slot);
    /* check for success */
    if (!IsSlotNumberValid(slotnumber)) {
      Jmsg(jcr, M_FATAL, 0, _("GetElementAddressByBareosSlotNumber failed\n"));
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
bool DoNdmpBackupNdmpNative(JobControlRecord* jcr)
{
  int status;
  char ed1[100];
  NIS* nis = NULL;
  FilesetResource* fileset;
  struct ndm_job_param ndmp_job;
  struct ndm_session ndmp_sess;
  bool session_initialized = false;
  bool retval = false;
  uint32_t ndmp_log_level;

  char* item;

  ndmp_log_level =
      std::max(jcr->impl->res.client->ndmp_loglevel, me->ndmp_loglevel);

  struct ndmca_media_callbacks media_callbacks;

  media_callbacks.load_first =
      NdmpLoadNext; /* we use the same callback for first and next*/
  media_callbacks.load_next = NdmpLoadNext;
  media_callbacks.unload_current = NULL;

  struct ndmca_jobcontrol_callbacks jobcontrol_callbacks;
  jobcontrol_callbacks.is_job_canceled = is_job_canceled_cb;


  /*
   * Print Job Start message
   */
  Jmsg(jcr, M_INFO, 0, _("Start NDMP Backup JobId %s, Job=%s\n"),
       edit_uint64(jcr->JobId, ed1), jcr->Job);

  jcr->setJobStatus(JS_Running);
  Dmsg2(100, "JobId=%d JobLevel=%c\n", jcr->impl->jr.JobId,
        jcr->impl->jr.JobLevel);
  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->impl->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
    return false;
  }

  status = 0;
  StorageResource* store = jcr->impl->res.write_storage;
  PoolMem virtual_filename(PM_FNAME);
  IncludeExcludeItem* ie;


  if (!NdmpBuildClientAndStorageJob(jcr, jcr->impl->res.write_storage,
                                    jcr->impl->res.client,
                                    true, /* init_tape */
                                    true, /* init_robot */
                                    NDM_JOB_OP_BACKUP, &ndmp_job)) {
    goto bail_out;
  }

  if (!ndmp_native_setup_robot_and_tape_for_native_backup_job(jcr, store,
                                                              ndmp_job)) {
    Jmsg(jcr, M_ERROR, 0,
         _("ndmp_native_setup_robot_and_tape_for_native_backup_job failed\n"));
    goto bail_out;
  }

  nis = (NIS*)malloc(sizeof(NIS));
  memset(nis, 0, sizeof(NIS));

  /*
   * Only one include set of the fileset  is allowed in NATIVE mode as
   * in NDMP also per job only one filesystem can be backed up
   */
  fileset = jcr->impl->res.fileset;

  if (fileset->include_items.size() > 1) {
    Jmsg(jcr, M_ERROR, 0,
         "Exactly one include set is supported in NDMP NATIVE mode\n");
    return retval;
  }

  ie = fileset->include_items[0];

  /*
   * only one file = entry is allowed
   * and it is the ndmp filesystem to be backed up
   */
  if (ie->name_list.size() != 1) {
    Jmsg(jcr, M_ERROR, 0,
         "Exactly one  File specification is supported in NDMP NATIVE mode\n");
    return retval;
  }

  item = (char*)ie->name_list.first();

  /*
   * Perform the actual NDMP job.
   * Initialize a new NDMP session
   */
  memset(&ndmp_sess, 0, sizeof(ndmp_sess));
  ndmp_sess.conn_snooping = (me->ndmp_snooping) ? 1 : 0;
  ndmp_sess.control_agent_enabled = 1;

  ndmp_sess.param =
      (struct ndm_session_param*)malloc(sizeof(struct ndm_session_param));
  memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
  ndmp_sess.param->log.deliver = NdmpLoghandler;
  ndmp_sess.param->log_level =
      NativeToNdmpLoglevel(ndmp_log_level, debug_level, nis);
  nis->filesystem = item;
  nis->FileIndex = 1;
  nis->jcr = jcr;
  nis->save_filehist = jcr->impl->res.job->SaveFileHist;
  nis->filehist_size = jcr->impl->res.job->FileHistSize;

  ndmp_sess.param->log.ctx = nis;
  ndmp_sess.param->log_tag = strdup("DIR-NDMP");
  ndmp_sess.dump_media_info = 1;

  /*
   * Initialize the session structure.
   */
  if (ndma_session_initialize(&ndmp_sess)) { goto cleanup; }
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

  if (!FillBackupEnvironment(jcr, ie, nis->filesystem,
                             &ndmp_sess.control_acb->job)) {
    goto cleanup;
  }
  /* register the callbacks */
  ndmca_media_register_callbacks(&ndmp_sess, &media_callbacks);
  ndmca_jobcontrol_register_callbacks(&ndmp_sess, &jobcontrol_callbacks);

  /*
   * The full ndmp archive has a virtual filename, we need it to hardlink the
   * individual file records to it. So we allocate it here once so its available
   * during the whole NDMP session.
   */
  if (Bstrcasecmp(jcr->impl->backup_format, "dump")) {
    Mmsg(virtual_filename, "/@NDMP%s%%%d", nis->filesystem,
         jcr->impl->DumpLevel);
  } else {
    Mmsg(virtual_filename, "/@NDMP%s", nis->filesystem);
  }

  if (nis->virtual_filename) { free(nis->virtual_filename); }
  nis->virtual_filename = strdup(virtual_filename.c_str());

  // FIXME: disabled because of "missing media entry" error
  // if (!ndmp_validate_job(jcr, &ndmp_sess.control_acb->job)) {
  //   goto cleanup;
  //}

  /*
   * Commission the session for a run.
   */
  if (ndma_session_commission(&ndmp_sess)) { goto cleanup; }

  /*
   * Setup the DMA.
   */
  if (ndmca_connect_control_agent(&ndmp_sess)) { goto cleanup; }

  ndmp_sess.conn_open = 1;
  ndmp_sess.conn_authorized = 1;

  RegisterCallbackHooks(&ndmp_sess.control_acb->job.index_log);

  /*
   * Let the DMA perform its magic.
   */
  if (ndmca_control_agent(&ndmp_sess) != 0) { goto cleanup; }

  if (!unreserve_ndmp_tapedevice_for_job(store, jcr)) {
    Jmsg(jcr, M_ERROR, 0, "could not free ndmp tape device %s from job %d",
         ndmp_job.tape_device, jcr->JobId);
  }

  /*
   * See if there were any errors during the backup.
   */
  jcr->impl->jr.FileIndex = 1;
  if (!extract_post_backup_stats_ndmp_native(jcr, item, &ndmp_sess)) {
    goto cleanup;
  }
  UnregisterCallbackHooks(&ndmp_sess.control_acb->job.index_log);

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


  status = JS_Terminated;
  retval = true;


  /*
   * If we do incremental backups it can happen that the backup is empty if
   * nothing changed but we always write a filestream. So we use the counter
   * which counts the number of actual NDMP backup sessions we run to
   * completion.
   */
  if (jcr->JobFiles == 0) { jcr->JobFiles = 1; }

  /*
   * Jump to the generic cleanup done for every Job.
   */
  goto ok_out;

cleanup:

  if (!unreserve_ndmp_tapedevice_for_job(store, jcr)) {
    Jmsg(jcr, M_ERROR, 0, "could not free ndmp tape device %s from job %d",
         ndmp_job.tape_device, jcr->JobId);
  }

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
    UnregisterCallbackHooks(&ndmp_sess.control_acb->job.index_log);

    /*
     * Destroy the session.
     */
    ndma_session_destroy(&ndmp_sess);
  }

  /*
   * Free the param block.
   */
  free(ndmp_sess.param->log_tag);
  free(ndmp_sess.param);
  ndmp_sess.param = NULL;

bail_out:
  /*
   * Error handling of failed Job.
   */
  status = JS_ErrorTerminated;
  jcr->setJobStatus(JS_ErrorTerminated);

ok_out:
  if (nis) {
    if (nis->virtual_filename) { free(nis->virtual_filename); }
    free(nis);
  }

  if (status == JS_Terminated) { NdmpBackupCleanup(jcr, status); }

  return retval;
}

/*
 * Setup a NDMP backup session.
 */
bool DoNdmpBackupInitNdmpNative(JobControlRecord* jcr)
{
  FreeRstorage(jcr); /* we don't read so release */

  if (!AllowDuplicateJob(jcr)) { return false; }

  jcr->impl->jr.PoolId =
      GetOrCreatePoolRecord(jcr, jcr->impl->res.pool->resource_name_);
  if (jcr->impl->jr.PoolId == 0) { return false; }

  jcr->start_time = time(NULL);
  jcr->impl->jr.StartTime = jcr->start_time;
  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->impl->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
    return false;
  }

  /*
   * If pool storage specified, use it instead of job storage
   */
  CopyWstorage(jcr, jcr->impl->res.pool->storage, _("Pool resource"));

  if (!jcr->impl->res.write_storage_list) {
    Jmsg(jcr, M_FATAL, 0,
         _("No Storage specification found in Job or Pool.\n"));
    return false;
  }

  /*
   * Validate the Job to have a NDMP client and NDMP storage.
   */
  if (!NdmpValidateClient(jcr)) { return false; }

  if (!NdmpValidateStorage(jcr)) { return false; }

  return true;
}

/*
 * Extract any post backup statistics for native NDMP
 */
static inline bool extract_post_backup_stats_ndmp_native(
    JobControlRecord* jcr,
    char* filesystem,
    struct ndm_session* sess)
{
  bool retval = true;
  struct ndmmedia* media;
  ndmp_backup_format_option* nbf_options;
  struct ndm_env_entry* ndm_ee;
  char mediabuf[100];
  /*
   * See if we know this backup format and get it options.
   */
  nbf_options =
      ndmp_lookup_backup_format_options(sess->control_acb->job.bu_type);

  /*
   * See if an error was raised during the backup session.
   */
  if (sess->error_raised) { return false; }

  /*
   * extract_post_backup_stats
   */
  for (media = sess->control_acb->job.media_tab.head; media;
       media = media->next) {
    /*
     * translate Physical to Logical Slot before storing into database
     */

    media->slot_addr = GetBareosSlotNumberByElementAddress(
        &jcr->impl->res.write_storage->runtime_storage_status->storage_mapping,
        slot_type_t::kSlotTypeStorage, media->slot_addr);
#if 0
      Jmsg(jcr, M_INFO, 0, _("Physical Slot is %d\n"), media->slot_addr);
      Jmsg(jcr, M_INFO, 0, _("Logical slot is : %d\n"), media->slot_addr);
      Jmsg(jcr, M_INFO, 0, _("label           : %s\n"), media->label);
      Jmsg(jcr, M_INFO, 0, _("index           : %d\n"), media->index);
      Jmsg(jcr, M_INFO, 0, _("n_bytes         : %lld\n"), media->n_bytes);
      Jmsg(jcr, M_INFO, 0, _("begin_offset    : %u\n"), media->begin_offset);
      Jmsg(jcr, M_INFO, 0, _("end_offset      : %u\n"), media->end_offset);
#endif

    StoreNdmmediaInfoInDatabase(media, jcr);

    ndmmedia_to_str(media, mediabuf);

    Jmsg(jcr, M_INFO, 0, _("Media: %s\n"), mediabuf);

    /*
     * See if there is any media error.
     */
    if (media->media_open_error || media->media_io_error ||
        media->label_io_error || media->label_mismatch || media->fmark_error) {
      retval = false;
    }
  }

  /*
   * Process the FHDB.
   */
  ProcessFhdb(&sess->control_acb->job.index_log);

  /*
   * insert batched files into database
   */

  jcr->db->WriteBatchFileRecords(jcr);
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
    if (!jcr->db->CreateNdmpEnvironmentString(
            jcr, &jcr->impl->jr, ndm_ee->pval.name, ndm_ee->pval.value)) {
      break;
    }
    ndm_ee = ndm_ee->next;
  }

  /*
   * If we are doing a backup type that uses dumplevels save the last used dump
   * level.
   */
  if (nbf_options && nbf_options->uses_level) {
    jcr->db->UpdateNdmpLevelMapping(jcr, &jcr->impl->jr, filesystem,
                                    sess->control_acb->job.bu_level);
  }

  return retval;
}

#else

bool DoNdmpBackupInitNdmpNative(JobControlRecord* jcr)
{
  Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return false;
}

bool DoNdmpBackupNdmpNative(JobControlRecord* jcr)
{
  Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return false;
}

#endif /* HAVE_NDMP */
} /* namespace directordaemon */
