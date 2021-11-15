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
 *      CONTROL agent entry point.
 */


#include "ndmagents.h"


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT

/* Initialize -- Set data structure to know value, ignore current value */
int ndmca_initialize(struct ndm_session* sess)
{
  sess->control_acb = NDMOS_API_MALLOC(sizeof(struct ndm_control_agent));
  if (!sess->control_acb) { return -1; }
  NDMOS_MACRO_ZEROFILL(sess->control_acb);

  return 0;
}

/* Commission -- Get agent ready. Entire session has been initialize()d */
int ndmca_commission(struct ndm_session* sess) { return 0; }

/* Decommission -- Discard agent */
int ndmca_decommission(struct ndm_session* sess) { return 0; }

/* Decommission -- Discard agent */
int ndmca_destroy(struct ndm_session* sess)
{
  if (!sess->control_acb) { return 0; }

  ndmca_destroy_media_table(&sess->control_acb->job.media_tab);
  ndmca_destroy_media_table(&sess->control_acb->job.result_media_tab);

  if (sess->control_acb->job.tape_target) {
    NDMOS_API_FREE(sess->control_acb->job.tape_target);
  }

  if (sess->control_acb->job.robot_target) {
    NDMOS_API_FREE(sess->control_acb->job.robot_target);
  }

  if (sess->control_acb->smc_cb) {
    smc_cleanup_element_status_data(sess->control_acb->smc_cb);
    NDMOS_API_FREE(sess->control_acb->smc_cb);
  }

  NDMOS_API_FREE(sess->control_acb);
  sess->control_acb = NULL;

  return 0;
}

int ndmca_control_agent(struct ndm_session* sess)
{
  struct ndm_job_param* job = &sess->control_acb->job;
  int rc = -1;

  if (!sess->control_acb->smc_cb) {
    sess->control_acb->smc_cb = NDMOS_API_MALLOC(sizeof(struct smc_ctrl_block));
    NDMOS_MACRO_ZEROFILL(sess->control_acb->smc_cb);
  }


  switch (job->operation) {
    default:
      ndmalogf(sess, 0, 0, "Job operation invalid");
      break;

    case NDM_JOB_OP_INIT_LABELS:
      rc = ndmca_op_init_labels(sess);
      break;

    case NDM_JOB_OP_LIST_LABELS:
      rc = ndmca_op_list_labels(sess);
      break;

    case NDM_JOB_OP_BACKUP:
      rc = ndmca_op_create_backup(sess);
      break;

    case NDM_JOB_OP_EXTRACT:
      rc = ndmca_op_recover_files(sess);
      break;

    case NDM_JOB_OP_TOC:
      rc = ndmca_op_recover_fh(sess);
      break;

    case NDM_JOB_OP_REMEDY_ROBOT:
      rc = ndmca_op_robot_remedy(sess);
      break;

    case NDM_JOB_OP_QUERY_AGENTS:
      rc = ndmca_op_query(sess);
      break;

    case NDM_JOB_OP_TEST_TAPE:
#ifndef NDMOS_OPTION_NO_TEST_AGENTS
      rc = ndmca_op_test_tape(sess);
#endif
      break;

    case NDM_JOB_OP_TEST_MOVER:
#ifndef NDMOS_OPTION_NO_TEST_AGENTS
      rc = ndmca_op_test_mover(sess);
#endif
      break;

    case NDM_JOB_OP_TEST_DATA:
#ifndef NDMOS_OPTION_NO_TEST_AGENTS
      rc = ndmca_op_test_data(sess);
#endif
      break;

    case NDM_JOB_OP_REWIND_TAPE:
      rc = ndmca_op_rewind_tape(sess);
      break;

    case NDM_JOB_OP_EJECT_TAPE:
      rc = ndmca_op_eject_tape(sess);
      break;

    case NDM_JOB_OP_MOVE_TAPE:
      rc = ndmca_op_move_tape(sess);
      break;

    case NDM_JOB_OP_LOAD_TAPE:
      rc = ndmca_op_load_tape(sess);
      break;

    case NDM_JOB_OP_UNLOAD_TAPE:
      rc = ndmca_op_unload_tape(sess);
      break;

    case NDM_JOB_OP_IMPORT_TAPE:
      rc = ndmca_op_import_tape(sess);
      break;

    case NDM_JOB_OP_EXPORT_TAPE:
      rc = ndmca_op_export_tape(sess);
      break;

    case NDM_JOB_OP_INIT_ELEM_STATUS:
      rc = ndmca_op_init_elem_status(sess);
      break;
  }

  return rc;
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
