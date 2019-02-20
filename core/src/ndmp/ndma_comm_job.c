/*
 * Copyright (c) 1998,1999,2000
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *
 */


#include "ndmagents.h"


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT


#define RETERR \
  if (errcnt++ >= errskip) return errcnt;
#define ERROR(S)                     \
  {                                  \
    if (errbuf) strcpy(errbuf, (S)); \
    RETERR                           \
  }


/*
 * To just check a job:
 *      rc = ndma_job_audit (job, 0, 0);
 *      if (rc) { "error" }
 *
 * To display everything wrong with a job:
 *      i = n_err = 0;
 *      do {
 *              n_err = ndma_job_audit (job, errbuf, i);
 *              if (n_err) display (errbuf);
 *              i++;
 *      } while (i < n_err);
 *
 *      if (n_err) { "error" }
 */

int ndma_job_audit(struct ndm_job_param* job, char* errbuf, int errskip)
{
  int errcnt = 0;
  char* audit_what;

  switch (job->operation) {
    default:
      ERROR("invalid operation")
      return -1;

    case NDM_JOB_OP_BACKUP:
      audit_what = "DfbBmM";
      break;
    case NDM_JOB_OP_EXTRACT:
      audit_what = "DfbBmM";
      break;
    case NDM_JOB_OP_TOC:
      audit_what = "DfbBmM";
      break;
    case NDM_JOB_OP_QUERY_AGENTS:
      audit_what = "";
      break;
    case NDM_JOB_OP_INIT_LABELS:
      audit_what = "TfmM";
      break;
    case NDM_JOB_OP_LIST_LABELS:
      audit_what = "TfM";
      break;
    case NDM_JOB_OP_REMEDY_ROBOT:
      audit_what = "";
      break;
    case NDM_JOB_OP_TEST_TAPE:
      audit_what = "TfM";
      break;
    case NDM_JOB_OP_TEST_MOVER:
      audit_what = "TfbM";
      break;
    case NDM_JOB_OP_TEST_DATA:
      audit_what = "DB";
      break;
    case NDM_JOB_OP_REWIND_TAPE:
      audit_what = "Tf";
      break;
    case NDM_JOB_OP_EJECT_TAPE:
      audit_what = "Tf";
      break;
    case NDM_JOB_OP_MOVE_TAPE:
      audit_what = "Rr@";
      break;
    case NDM_JOB_OP_IMPORT_TAPE:
      audit_what = "Rr@";
      break;
    case NDM_JOB_OP_EXPORT_TAPE:
      audit_what = "Rr@";
      break;
    case NDM_JOB_OP_LOAD_TAPE:
      audit_what = "Rr@";
      break;
    case NDM_JOB_OP_UNLOAD_TAPE:
      audit_what = "Rr";
      break;
    case NDM_JOB_OP_INIT_ELEM_STATUS:
      audit_what = "Rr";
      break;
  }

  while (*audit_what) switch (*audit_what++) {
      case 'D': /* DATA agent provided */
        if (job->data_agent.conn_type == NDMCONN_TYPE_NONE)
          ERROR("missing DATA agent")
        break;

      case 'T': /* TAPE agent provided (use DATA if given) */
        if (job->data_agent.conn_type == NDMCONN_TYPE_NONE &&
            job->tape_agent.conn_type == NDMCONN_TYPE_NONE)
          ERROR("missing TAPE or DATA agent")
        break;

      case 'R': /* ROBOT agent provided (use TAPE or DATA if given) */
        if (job->data_agent.conn_type == NDMCONN_TYPE_NONE &&
            job->tape_agent.conn_type == NDMCONN_TYPE_NONE &&
            job->robot_agent.conn_type == NDMCONN_TYPE_NONE)
          ERROR("missing ROBOT, TAPE or DATA agent")
        break;

      case 'B': /* Backup type */
        if (!job->bu_type) ERROR("missing bu_type")
        break;

      case 'b': /* block (record) size */
        if (!job->record_size) ERROR("missing record size")
        break;

      case 'f': /* tape file */
        if (!job->tape_device) ERROR("missing tape device")
        break;

      case 'm': /* media entry/ies */
        if (job->media_tab.n_media < 1) ERROR("missing media entry")
        break;

      case 'M': /* ? */
        errcnt += ndma_job_media_audit(job, errbuf, errskip - errcnt);
        break;

      case 'r': /* robot file/device name */
        if (!job->have_robot) ERROR("missing robot SCSI address");
        break;

      case '@': /* from and/or to address */
        if (job->operation == NDM_JOB_OP_MOVE_TAPE ||
            job->operation == NDM_JOB_OP_EXPORT_TAPE ||
            job->operation == NDM_JOB_OP_LOAD_TAPE) {
          if (!job->from_addr_given) ERROR("missing 'from' slot address");
        }
        if (job->operation == NDM_JOB_OP_MOVE_TAPE ||
            job->operation == NDM_JOB_OP_IMPORT_TAPE) {
          if (!job->to_addr_given) ERROR("missing 'to' slot address");
        }
        break;

      default:
        ERROR("INTERNAL BOTCH")
        return -2;
    }

  if (job->robot_agent.conn_type != NDMCONN_TYPE_NONE && !job->have_robot &&
      job->operation != NDM_JOB_OP_QUERY_AGENTS) {
    ERROR("robot agent, but no robot")
  }

  return errcnt;
}

int ndma_job_media_audit(struct ndm_job_param* job, char* errbuf, int errskip)
{
  struct ndm_media_table* mtab = &job->media_tab;
  int n_media = mtab->n_media;
  struct ndmmedia* me;
  struct ndmmedia* me2;
  int errcnt = 0;

  if (job->have_robot) {
    for (me = mtab->head; me; me = me->next) {
      if (!me->valid_slot) {
        if (errbuf) {
          sprintf(errbuf, "media #%d missing slot address", me->index);
        }
        RETERR
        continue;
      }
      for (me2 = me->next; me2; me2 = me2->next) {
        if (!me2->valid_slot) continue;
        if (me->slot_addr == me2->slot_addr) {
          if (errbuf) {
            sprintf(errbuf, "media #%d dup slot addr w/ #%d", me->index,
                    me2->index);
          }
          RETERR
        }
      }
    }
  } else {
    if (n_media > 1) { ERROR("no robot, too many media") }

    for (me = mtab->head; me; me = me->next) {
      if (me->valid_slot) {
        if (errbuf) {
          sprintf(errbuf, "media #%d slot address, but no robot", me->index);
        }
        RETERR
      }
    }
  }

  if (job->operation == NDM_JOB_OP_INIT_LABELS) {
    for (me = mtab->head; me; me = me->next) {
      if (!me->valid_label) {
        if (errbuf) { sprintf(errbuf, "media #%d missing label", me->index); }
        RETERR
      }
    }
  }

  return 0;
}

void ndma_job_auto_adjust(struct ndm_job_param* job)
{
  struct ndmmedia* me;

  if (job->media_tab.n_media == 0 && !job->have_robot &&
      job->operation != NDM_JOB_OP_INIT_LABELS) {
    /* synthesize one media table entry */

    me = ndma_store_media(&job->media_tab, 0);
    if (me) {
      /* As this is a fake media entry set valid_slot to 0 */
      me->valid_slot = 0;
    }
  }
}

#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
