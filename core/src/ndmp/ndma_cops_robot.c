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


int ndmca_op_robot_remedy(struct ndm_session* sess)
{
  struct ndm_job_param* job = &sess->control_acb->job;
  int rc;

  if (!job->have_robot) return 0;

  rc = ndmca_connect_robot_agent(sess);
  if (rc) return rc; /* already tattled */

  rc = ndmca_robot_prep_target(sess);
  if (rc) return rc; /* already tattled */

  rc = ndmca_robot_check_ready(sess);
  if (rc) { /* already tattled */
    ndmalogf(sess, 0, 0, "Robot is not ready, trying to remedy");
    rc = ndmca_robot_remedy_ready(sess);
    if (rc) {
      ndmalogf(sess, 0, 0, "Robot remedy failed");
      return -1;
    }
  }

  return 0;
}

int ndmca_op_robot_startup(struct ndm_session* sess, int verify_media_flag)
{
  struct ndm_job_param* job = &sess->control_acb->job;
  int rc;

  if (!job->have_robot) return 0;

  if (!sess->control_acb->smc_cb) {
    sess->control_acb->smc_cb = NDMOS_API_MALLOC(sizeof(struct smc_ctrl_block));
    NDMOS_MACRO_ZEROFILL(sess->control_acb->smc_cb);
  }
  rc = ndmca_connect_robot_agent(sess);
  if (rc) return rc; /* already tattled */

  rc = ndmca_robot_prep_target(sess);
  if (rc) return rc; /* already tattled */

  rc = ndmca_robot_check_ready(sess);
  if (rc) { /* already tattled */
    if (!job->auto_remedy) {
      ndmalogf(sess, 0, 0, "Robot is not ready, failing");
      return -1;
    }
    ndmalogf(sess, 0, 0, "Robot is not ready, trying to remedy");
    rc = ndmca_robot_remedy_ready(sess);
    if (rc) {
      ndmalogf(sess, 0, 0, "Robot remedy failed");
      return -1;
    }
  }

  if (verify_media_flag) {
    rc = ndmca_media_verify(sess);
    if (rc) return rc; /* already tattled */
  }

  return 0;
}

/*
 * ndmca_op_rewind_tape() and ndmca_op_eject_tape() really
 * belong somewhere else. Here because they are close
 * to the other "tape handling" operations.
 */

int ndmca_op_rewind_tape(struct ndm_session* sess)
{
  return ndmca_op_mtio(sess, NDMP9_MTIO_REW);
}

int ndmca_op_eject_tape(struct ndm_session* sess)
{
  return ndmca_op_mtio(sess, NDMP9_MTIO_OFF);
}


int ndmca_op_mtio(struct ndm_session* sess, ndmp9_tape_mtio_op mtio_op)
{
  struct ndm_control_agent* ca = sess->control_acb;
  int rc;

  ca->tape_mode = NDMP9_TAPE_READ_MODE;
  ca->is_label_op = 1;

  rc = ndmca_connect_tape_agent(sess);
  if (rc) {
    ndmconn_destruct(sess->plumb.tape);
    return rc; /* already tattled */
  }

  rc = ndmca_media_open_tape(sess);
  if (rc) return rc; /* already tattled */

  if (mtio_op == NDMP9_MTIO_OFF) {
    /* best-effort rewind */
    ndmca_media_mtio_tape(sess, NDMP9_MTIO_REW, 1, 0);
  }

  rc = ndmca_media_mtio_tape(sess, mtio_op, 1, 0);
  if (rc) {
    /* best-effort close */
    ndmca_media_close_tape(sess);
    return rc; /* already tattled */
  }

  rc = ndmca_media_close_tape(sess);
  if (rc) return rc; /* already tattled */

  return 0;
}

int ndmca_op_move_tape(struct ndm_session* sess)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndm_job_param* job = &ca->job;
  int src_addr = job->from_addr;
  int dst_addr = job->to_addr;
  int rc;

  /* repeat audits */
  if (!job->to_addr_given || !job->from_addr_given) {
    ndmalogf(sess, 0, 0, "Missing to/from addr");
    return -1;
  }

  rc = ndmca_robot_startup(sess);
  if (rc) return rc; /* already tattled -- NOT */

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc; /* already tattled -- NOT */

  rc = ndmca_robot_move(sess, src_addr, dst_addr);
  if (rc) return rc; /* already tattled */

  return 0;
}

int ndmca_op_import_tape(struct ndm_session* sess)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndm_job_param* job = &ca->job;
  struct smc_ctrl_block* smc = ca->smc_cb;
  int src_addr;
  int dst_addr = job->to_addr;
  int rc;

  /* repeat audits */
  if (!job->to_addr_given) {
    ndmalogf(sess, 0, 0, "Missing to-addr");
    return -1;
  }

  rc = ndmca_robot_startup(sess);
  if (rc) return rc; /* already tattled -- NOT */

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc; /* already tattled -- NOT */

  if (smc->elem_aa.iee_count < 1) {
    ndmalogf(sess, 0, 0, "robot has no import/export; try move");
    return -1;
  }
  src_addr = smc->elem_aa.iee_addr;

  rc = ndmca_robot_move(sess, src_addr, dst_addr);
  if (rc) return rc; /* already tattled */

  return 0;
}

int ndmca_op_export_tape(struct ndm_session* sess)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndm_job_param* job = &ca->job;
  struct smc_ctrl_block* smc = ca->smc_cb;
  int src_addr = job->from_addr;
  int dst_addr;
  int rc;

  /* repeat audits */
  if (!job->from_addr_given) {
    ndmalogf(sess, 0, 0, "Missing from-addr");
    return -1;
  }

  rc = ndmca_robot_startup(sess);
  if (rc) return rc; /* already tattled -- NOT */

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc; /* already tattled -- NOT */

  if (smc->elem_aa.iee_count < 1) {
    ndmalogf(sess, 0, 0, "robot has no import/export; try move");
    return -1;
  }
  dst_addr = smc->elem_aa.iee_addr;

  rc = ndmca_robot_move(sess, src_addr, dst_addr);
  if (rc) return rc; /* already tattled */

  return 0;
}

int ndmca_op_load_tape(struct ndm_session* sess)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndm_job_param* job = &ca->job;
  struct smc_ctrl_block* smc = ca->smc_cb;
  int src_addr = job->from_addr;
  int dst_addr;
  int rc;

  /* repeat audits */
  if (!job->from_addr_given) {
    ndmalogf(sess, 0, 0, "Missing from-addr");
    return -1;
  }

  rc = ndmca_robot_startup(sess);
  if (rc) return rc; /* already tattled -- NOT */

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc; /* already tattled -- NOT */

  if (job->drive_addr_given) {
    dst_addr = job->drive_addr;
  } else if (smc->elem_aa.dte_count > 0) {
    dst_addr = smc->elem_aa.dte_addr;
  } else {
    ndmalogf(sess, 0, 0, "robot has no tape drives? try move");
    return -1;
  }

  /*
   * Calculation for dst_addr repeated in ndmca_robot_load().
   * We just did it to be sure it would succeed
   */

  rc = ndmca_robot_load(sess, src_addr);
  if (rc) return rc; /* already tattled */

  return 0;
}

int ndmca_op_unload_tape(struct ndm_session* sess)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndm_job_param* job = &ca->job;
  struct smc_ctrl_block* smc = ca->smc_cb;
  int src_addr = job->from_addr;
  int dst_addr;
  int rc;

  /* repeat audits */
  rc = ndmca_robot_startup(sess);
  if (rc) return rc; /* already tattled -- NOT */

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc; /* already tattled -- NOT */

  if (job->drive_addr_given) {
    src_addr = job->drive_addr;
  } else if (smc->elem_aa.dte_count > 0) {
    src_addr = smc->elem_aa.dte_addr;
  } else {
    ndmalogf(sess, 0, 0, "robot has no tape drives? try move");
    return -1;
  }

  /*
   * Calculation for src_addr repeated in ndmca_robot_unload().
   * We just did it to be sure it would succeed
   */

  if (job->tape_device) {
    /* best effort */
    rc = ndmca_op_mtio(sess, job->use_eject ? NDMP9_MTIO_OFF : NDMP9_MTIO_REW);
    if (rc) return rc; /* already tattled -- NOT */
  }

  if (job->to_addr_given) {
    dst_addr = job->to_addr;
  } else {
    struct smc_element_descriptor* edp;
    struct smc_element_descriptor* edp2;
    char prefix[60];

    /*
     * Try to automatically determine where to
     * put the tape, if there is one in the drive.
     * This is pretty much a rip-off of remedy_robot().
     * The difference is here we believe the user
     * that something should happen. Otherwise,
     * the user would have used REMEDY_ROBOT.
     */

    edp = ndmca_robot_find_element(sess, src_addr);
    if (!edp) {
      ndmalogf(sess, 0, 1, "no such slot @%d, trying unload anyway", src_addr);
      dst_addr = 0; /* g'luck! */
      goto unload_anyway;
    }

    if (!edp->Full) {
      ndmalogf(sess, 0, 1, "drive @%d empty, trying unload anyway", src_addr);
      dst_addr = 0; /* g'luck! */
      goto unload_anyway;
    }

    snprintf(prefix, sizeof(prefix), "drive @%d full", edp->element_address);

    if (!edp->SValid) {
      ndmalogf(sess, 0, 1, "%s, no SValid info, you must specify to-addr",
               prefix);
      return -1;
    }

    dst_addr = edp->src_se_addr;

    sprintf(NDMOS_API_STREND(prefix), ", src @%d", edp->src_se_addr);

    edp2 = ndmca_robot_find_element(sess, dst_addr);
    if (!edp2) {
      ndmalogf(sess, 0, 1, "%s, no such addr, trying unload anyway", prefix);
      goto unload_anyway;
    }

    if (edp2->element_type_code != SMC_ELEM_TYPE_SE) {
      ndmalogf(sess, 0, 1, "%s, not slot, trying unload anyway", prefix);
      goto unload_anyway;
    }

    if (edp2->Full) {
      ndmalogf(sess, 0, 1, "%s, slot Full, trying unload anyway", prefix);
      goto unload_anyway;
    }
  }


unload_anyway:
  rc = ndmca_robot_unload(sess, dst_addr);
  if (rc) return rc; /* already tattled */

  return 0;
}

int ndmca_op_init_elem_status(struct ndm_session* sess)
{
  int rc;

  /* repeat audits */
  rc = ndmca_robot_startup(sess);
  if (rc) return rc; /* already tattled -- NOT */

  /* best-effort */
  rc = ndmca_robot_obtain_info(sess);

  rc = ndmca_robot_init_elem_status(sess);
  if (rc) return rc; /* already tattled */

  rc = ndmca_robot_query(sess);
  if (rc) return rc; /* already tattled -- WAY WAY tattled */

  return 0;
}

#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
