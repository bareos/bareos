/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * Marco van Wieringen, May 2015
 */
/**
 * @file
 * Backup specific NDMP Data Management Application (DMA) routines
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/job.h"
#include "dird/msgchan.h"
#include "dird/quota.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "lib/edit.h"

#if HAVE_NDMP
#include "dird/ndmp_dma_backup_common.h"
#include "dird/ndmp_dma_generic.h"

#define NDMP_NEED_ENV_KEYWORDS 1

#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"
#endif /* HAVE_NDMP */

namespace directordaemon {

#if HAVE_NDMP

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Imported variables */

/* Forward referenced functions */


/**
 * Extract any post backup statistics.
 */
static inline bool extract_post_backup_stats(JobControlRecord* jcr,
                                             char* filesystem,
                                             struct ndm_session* sess)
{
  bool retval = true;
  struct ndmmedia* media;
  ndmp_backup_format_option* nbf_options;
  struct ndm_env_entry* ndm_ee;

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
   * See if there is any media error.
   */
  for (media = sess->control_acb->job.result_media_tab.head; media;
       media = media->next) {
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
   * Update the Job statistics from the NDMP statistics.
   */
  jcr->JobBytes += sess->control_acb->job.bytes_written;

  /*
   * After a successful backup we need to store all NDMP ENV variables
   * for doing a successful restore operation.
   */
  ndm_ee = sess->control_acb->job.result_env_tab.head;
  while (ndm_ee) {
    if (!jcr->db->CreateNdmpEnvironmentString(jcr, &jcr->jr, ndm_ee->pval.name,
                                              ndm_ee->pval.value)) {
      break;
    }
    ndm_ee = ndm_ee->next;
  }

  /*
   * If we are doing a backup type that uses dumplevels save the last used dump
   * level.
   */
  if (nbf_options && nbf_options->uses_level) {
    jcr->db->UpdateNdmpLevelMapping(jcr, &jcr->jr, filesystem,
                                    sess->control_acb->job.bu_level);
  }

  return retval;
}

/**
 * Setup a NDMP backup session.
 */
bool DoNdmpBackupInit(JobControlRecord* jcr)
{
  FreeRstorage(jcr); /* we don't read so release */

  if (!AllowDuplicateJob(jcr)) { return false; }

  jcr->jr.PoolId = GetOrCreatePoolRecord(jcr, jcr->res.pool->resource_name_);
  if (jcr->jr.PoolId == 0) { return false; }

  jcr->start_time = time(NULL);
  jcr->jr.StartTime = jcr->start_time;
  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
    return false;
  }

  /*
   * If pool storage specified, use it instead of job storage
   */
  CopyWstorage(jcr, jcr->res.pool->storage, _("Pool resource"));

  if (!jcr->res.write_storage_list) {
    Jmsg(jcr, M_FATAL, 0,
         _("No Storage specification found in Job or Pool.\n"));
    return false;
  }

  /*
   * Validate the Job to have a NDMP client and NDMP storage.
   */
  if (!NdmpValidateClient(jcr)) { return false; }

  if (!NdmpValidateStorage(jcr)) { return false; }

  /*
   * For now we only allow NDMP backups to bareos SD's
   * so we need a paired storage definition.
   */
  if (!HasPairedStorage(jcr)) {
    Jmsg(jcr, M_FATAL, 0,
         _("Write storage doesn't point to storage definition with paired "
           "storage option.\n"));
    return false;
  }

  return true;
}


/**
 * Run a NDMP backup session.
 */
bool DoNdmpBackup(JobControlRecord* jcr)
{
  unsigned int cnt;
  int i, status;
  char ed1[100];
  NIS* nis = NULL;
  FilesetResource* fileset;
  struct ndm_job_param ndmp_job;
  struct ndm_session ndmp_sess;
  bool session_initialized = false;
  bool retval = false;
  int NdmpLoglevel;

  NdmpLoglevel = std::max(jcr->res.client->ndmp_loglevel, me->ndmp_loglevel);

  /*
   * Print Job Start message
   */
  Jmsg(jcr, M_INFO, 0, _("Start NDMP Backup JobId %s, Job=%s\n"),
       edit_uint64(jcr->JobId, ed1), jcr->Job);

  jcr->setJobStatus(JS_Running);
  Dmsg2(100, "JobId=%d JobLevel=%c\n", jcr->jr.JobId, jcr->jr.JobLevel);
  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
    return false;
  }

  if (CheckHardquotas(jcr)) {
    Jmsg(jcr, M_FATAL, 0, "Quota Exceeded. Job terminated.");
    return false;
  }

  if (CheckSoftquotas(jcr)) {
    Dmsg0(10, "Quota exceeded\n");
    Jmsg(jcr, M_FATAL, 0,
         "Soft Quota Exceeded / Grace Time expired. Job terminated.");
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
  if (jcr->res.write_storage->paired_storage) {
    SetPairedStorage(jcr);

    jcr->setJobStatus(JS_WaitSD);
    if (!ConnectToStorageDaemon(jcr, 10, me->SDConnectTimeout, true)) {
      return false;
    }

    /*
     * Now start a job with the Storage daemon
     */
    if (!StartStorageDaemonJob(jcr, NULL, jcr->res.write_storage_list)) {
      return false;
    }

    /*
     * Start the job prior to starting the message thread below
     * to avoid two threads from using the BareosSocket structure at
     * the same time.
     */
    if (!jcr->store_bsock->fsend("run")) { return false; }

    /*
     * Now start a Storage daemon message thread.  Note,
     *   this thread is used to provide the catalog services
     *   for the backup job.
     */
    if (!StartStorageDaemonMessageThread(jcr)) { return false; }
    Dmsg0(150, "Storage daemon connection OK\n");
  }

  status = 0;

  /*
   * Initialize the ndmp backup job. We build the generic job only once
   * and reuse the job definition for each separate sub-backup we perform as
   * part of the whole job. We only free the env_table between every sub-backup.
   */
  if (!NdmpBuildClientJob(jcr, jcr->res.client,
                          jcr->res.paired_read_write_storage, NDM_JOB_OP_BACKUP,
                          &ndmp_job)) {
    goto bail_out;
  }

  nis = (NIS*)malloc(sizeof(NIS));
  memset(nis, 0, sizeof(NIS));

  /*
   * Loop over each include set of the fileset and fire off a NDMP backup of the
   * included fileset.
   */
  cnt = 0;
  fileset = jcr->res.fileset;


  for (i = 0; i < fileset->num_includes; i++) {
    int j;
    char* item;
    IncludeExcludeItem* ie = fileset->include_items[i];
    PoolMem virtual_filename(PM_FNAME);

    /*
     * Loop over each file = entry of the fileset.
     */
    for (j = 0; j < ie->name_list.size(); j++) {
      item = (char*)ie->name_list.get(j);

      /*
       * See if this is the first Backup run or not. For NDMP we can have
       * multiple Backup runs as part of the same Job. When we are saving data
       * to a Native Storage Daemon we let it know to expect a new backup
       * session. It will generate a new authorization key so we wait for the
       * nextrun_ready conditional variable to be raised by the msg_thread.
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

      ndmp_sess.param =
          (struct ndm_session_param*)malloc(sizeof(struct ndm_session_param));
      memset(ndmp_sess.param, 0, sizeof(struct ndm_session_param));
      ndmp_sess.param->log.deliver = NdmpLoghandler;
      ndmp_sess.param->log_level =
          NativeToNdmpLoglevel(NdmpLoglevel, debug_level, nis);
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
      if (ndma_session_initialize(&ndmp_sess)) { goto cleanup; }
      session_initialized = true;

      /*
       * Copy the actual job to perform.
       */
      memcpy(&ndmp_sess.control_acb->job, &ndmp_job,
             sizeof(struct ndm_job_param));

      /*
       * We can use the same private pointer used in the logging with the
       * JobControlRecord in the file index generation. We don't setup a
       * index_log.deliver function as we catch the index information via
       * callbacks.
       */
      ndmp_sess.control_acb->job.index_log.ctx = ndmp_sess.param->log.ctx;

      if (!FillBackupEnvironment(jcr, ie, nis->filesystem,
                                 &ndmp_sess.control_acb->job)) {
        goto cleanup;
      }

      /*
       * The full ndmp archive has a virtual filename, we need it to hardlink
       * the individual file records to it. So we allocate it here once so its
       * available during the whole NDMP session.
       */
      if (Bstrcasecmp(jcr->backup_format, "dump")) {
        Mmsg(virtual_filename, "/@NDMP%s%%%d", nis->filesystem, jcr->DumpLevel);
      } else {
        Mmsg(virtual_filename, "/@NDMP%s", nis->filesystem);
      }

      if (nis->virtual_filename) { free(nis->virtual_filename); }
      nis->virtual_filename = bstrdup(virtual_filename.c_str());

      ndma_job_auto_adjust(&ndmp_sess.control_acb->job);
      if (!NdmpValidateJob(jcr, &ndmp_sess.control_acb->job)) { goto cleanup; }

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

      /*
       * See if there were any errors during the backup.
       */
      jcr->jr.FileIndex = cnt + 1;
      if (!extract_post_backup_stats(jcr, item, &ndmp_sess)) { goto cleanup; }

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
    WaitForStorageDaemonTermination(jcr);
    jcr->db_batch->WriteBatchFileRecords(
        jcr); /* used by bulk batch file insert */
  }

  /*
   * If we do incremental backups it can happen that the backup is empty if
   * nothing changed but we always write a filestream. So we use the counter
   * which counts the number of actual NDMP backup sessions we run to
   * completion.
   */
  if (jcr->JobFiles < cnt) { jcr->JobFiles = cnt; }

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

    UnregisterCallbackHooks(&ndmp_sess.control_acb->job.index_log);

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
    CancelStorageDaemonJob(jcr);
    WaitForStorageDaemonTermination(jcr);
  }

ok_out:
  if (nis) {
    if (nis->virtual_filename) { free(nis->virtual_filename); }
    free(nis);
  }
  FreePairedStorage(jcr);

  if (status == JS_Terminated) { NdmpBackupCleanup(jcr, status); }

  return retval;
}

#else
bool DoNdmpBackupInit(JobControlRecord* jcr)
{
  Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return false;
}

bool DoNdmpBackup(JobControlRecord* jcr)
{
  Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
  return false;
}

#endif /* HAVE_NDMP */
} /* namespace directordaemon */
