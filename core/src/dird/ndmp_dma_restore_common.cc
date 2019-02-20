/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
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
 * Restore specific NDMP Data Management Application (DMA) routines
 * common for NDMP_BAREOS and NDMP_NATIVE restores
 *
 * extracted and reorganized from ndmp_dma_restore.c
 *
 * Philipp Storz, April 2017
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/job.h"
#include "dird/restore.h"

#if HAVE_NDMP
#include "ndmp/ndmagents.h"
#include "ndmp_dma_priv.h"
#endif /* HAVE_NDMP */

namespace directordaemon {

#if HAVE_NDMP
/*
 * Add a filename to the files we want to restore.
 *
 * The RFC says this:
 *
 * original_path - The original path name of the data to be recovered,
 *                 relative to the backup root. If original_path is the null
 *                 string, the server shall recover all data contained in the
 *                 backup image.
 *
 * destination_path, name, other_name
 *               - Together, these identify the absolute path name to which
 *                 data are to be recovered.
 *
 *               If name is the null string:
 *                 - destination_path identifies the name to which the data
 *                   identified by original_path are to be recovered.
 *                 - other_name must be the null string.
 *
 *               If name is not the null string:
 *                 - destination_path, when concatenated with the server-
 *                   specific path name delimiter and name, identifies the
 *                   name to which the data identified by original_path are
 *                   to be recovered.
 *
 *               If other_name is not the null string:
 *                 - destination_path, when concatenated with the server-
 *                   specific path name delimiter and other_name,
 *                   identifies the alternate name-space name of the data
 *                   to be recovered. The definition of such alternate
 *                   name-space is server-specific.
 *
 * Neither name nor other_name may contain a path name delimiter.
 *
 * Under no circumstance may destination_path be the null string.
 *
 * If intermediate directories that lead to the path name to
 * recover do not exist, the server should create them.
 */
void AddToNamelist(struct ndm_job_param* job,
                   char* filename,
                   const char* restore_prefix,
                   char* name,
                   char* other_name,
                   uint64_t node,
                   uint64_t fhinfo)
{
  ndmp9_name nl;
  PoolMem destination_path;

  memset(&nl, 0, sizeof(ndmp9_name));

  /*
   * See if the filename is an absolute pathname.
   */
  if (*filename == '\0') {
    PmStrcpy(destination_path, restore_prefix);
  } else if (*filename == '/') {
    Mmsg(destination_path, "%s%s", restore_prefix, filename);
  } else {
    Mmsg(destination_path, "%s/%s", restore_prefix, filename);
  }

  nl.original_path = filename;
  nl.destination_path = destination_path.c_str();
  nl.name = name;
  nl.other_name = other_name;


  /*
   * add fh_node and fh_info for DAR for native NDMP backup
   */
  nl.node = node;

  if (fhinfo) {
    nl.fh_info.value = fhinfo;
    nl.fh_info.valid = NDMP9_VALIDITY_VALID;
  }

  ndma_store_nlist(&job->nlist_tab, &nl);
}

/*
 * Database handler that handles the returned environment data for a given
 * JobId.
 */
int NdmpEnvHandler(void* ctx, int num_fields, char** row)
{
  struct ndm_env_table* envtab;
  ndmp9_pval pv;

  if (row[0] && row[1]) {
    envtab = (struct ndm_env_table*)ctx;
    pv.name = row[0];
    pv.value = row[1];

    ndma_store_env_list(envtab, &pv);
  }

  return 0;
}

/*
 * Extract any post backup statistics.
 */
bool ExtractPostRestoreStats(JobControlRecord* jcr, struct ndm_session* sess)
{
  bool retval = true;
  struct ndmmedia* media;

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
   * Update the Job statistics from the NDMP statistics.
   */
  jcr->JobBytes += sess->control_acb->job.bytes_read;
  jcr->JobFiles++;

  return retval;
}

/**
 * Cleanup a NDMP restore session.
 */
void NdmpRestoreCleanup(JobControlRecord* jcr, int TermCode)
{
  char term_code[100];
  const char* TermMsg;
  int msg_type = M_INFO;

  Dmsg0(20, "In NdmpRestoreCleanup\n");
  UpdateJobEnd(jcr, TermCode);

  if (jcr->unlink_bsr && jcr->RestoreBootstrap) {
    SecureErase(jcr, jcr->RestoreBootstrap);
    jcr->unlink_bsr = false;
  }

  if (JobCanceled(jcr)) { CancelStorageDaemonJob(jcr); }

  switch (TermCode) {
    case JS_Terminated:
      if (jcr->ExpectedFiles > jcr->jr.JobFiles) {
        TermMsg = _("Restore OK -- warning file count mismatch");
      } else {
        TermMsg = _("Restore OK");
      }
      break;
    case JS_Warnings:
      TermMsg = _("Restore OK -- with warnings");
      break;
    case JS_FatalError:
    case JS_ErrorTerminated:
      TermMsg = _("*** Restore Error ***");
      msg_type = M_ERROR; /* Generate error message */
      if (jcr->store_bsock) {
        jcr->store_bsock->signal(BNET_TERMINATE);
        if (jcr->SD_msg_chan_started) { pthread_cancel(jcr->SD_msg_chan); }
      }
      break;
    case JS_Canceled:
      TermMsg = _("Restore Canceled");
      if (jcr->store_bsock) {
        jcr->store_bsock->signal(BNET_TERMINATE);
        if (jcr->SD_msg_chan_started) { pthread_cancel(jcr->SD_msg_chan); }
      }
      break;
    default:
      TermMsg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), TermCode);
      break;
  }

  GenerateRestoreSummary(jcr, msg_type, TermMsg);

  Dmsg0(20, "Leaving NdmpRestoreCleanup\n");
}

#else /* HAVE_NDMP */

void NdmpRestoreCleanup(JobControlRecord* jcr, int TermCode)
{
  Jmsg(jcr, M_FATAL, 0, _("NDMP protocol not supported\n"));
}

#endif /* HAVE_NDMP */
} /* namespace directordaemon */
