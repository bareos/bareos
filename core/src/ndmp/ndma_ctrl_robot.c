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


int ndmca_robot_issue_scsi_req(struct smc_ctrl_block* smc)
{
  struct ndmconn* conn = (struct ndmconn*)smc->app_data;
  struct smc_scsi_req* sr = &smc->scsi_req;
  int rc;

  rc = ndmscsi_execute(conn, (struct ndmscsi_request*)sr, 0);
  return rc;
}


int ndmca_robot_prep_target(struct ndm_session* sess)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  int rc;

  if (!smc) {
    ndmalogf(sess, 0, 0, "Allocating robot target");
    return -1;
  }

  NDMOS_MACRO_ZEROFILL(smc);

  smc->app_data = sess->plumb.robot;
  smc->issue_scsi_req = ndmca_robot_issue_scsi_req;

  /*
   * We are about to start using a Robot Target so allocate it.
   * Only do this when not allocated yet.
   */
  if (!sess->control_acb->job.robot_target) {
    sess->control_acb->job.robot_target =
        NDMOS_API_MALLOC(sizeof(struct ndmscsi_target));
    if (!sess->control_acb->job.robot_target) {
      ndmalogf(sess, 0, 0, "Failed allocating robot target");
      return -1;
    }
    NDMOS_MACRO_ZEROFILL(sess->control_acb->job.robot_target);
  }

  rc = ndmscsi_use(sess->plumb.robot, sess->control_acb->job.robot_target);
  if (rc) return rc;

  return 0;
}

int ndmca_robot_obtain_info(struct ndm_session* sess)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  int rc;

  rc = smc_inquire(smc);
  if (rc) return rc;

  rc = smc_get_elem_aa(smc);
  if (rc) return rc;

  rc = smc_read_elem_status(smc);
  if (rc) return rc;

  return 0;
}

int ndmca_robot_init_elem_status(struct ndm_session* sess)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  int rc;

  ndmalogf(sess, 0, 1,
           "Commanding robot to initialize element status (take inventory)");

  rc = smc_init_elem_status(smc);
  if (rc) {
    ndmalogf(sess, 0, 0, "init-elem-status failed");
    return rc;
  }

  return 0;
}

int ndmca_robot_startup(struct ndm_session* sess)
{
  int rc;

  if (!sess->control_acb->job.have_robot)
    return -1; /* Huh? why were we called */

  if (!sess->control_acb->smc_cb) {
    sess->control_acb->smc_cb = NDMOS_API_MALLOC(sizeof(struct smc_ctrl_block));
    NDMOS_MACRO_ZEROFILL(sess->control_acb->smc_cb);
  }

  rc = ndmca_connect_robot_agent(sess);
  if (rc) return rc;

  rc = ndmca_robot_prep_target(sess);
  if (rc) return rc;

  return 0;
}

int ndmca_robot_move(struct ndm_session* sess, int src_addr, int dst_addr)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct smc_ctrl_block* smc = ca->smc_cb;
  int rc;
  unsigned int t;

  ndmalogf(sess, 0, 2, "robot moving @%d to @%d", src_addr, dst_addr);

  rc = -1;
  for (t = 0; t <= ca->job.robot_timeout; t += 10) {
    if (t > 0) {
      ndmalogf(sess, 0, 2, "Pausing ten seconds before retry (%d/%d)", t,
               ca->job.robot_timeout);
      sleep(10);
    }
    rc = smc_move(smc, src_addr, dst_addr, 0, smc->elem_aa.mte_addr);
    if (rc == 0) break;
  }

  if (rc == 0) {
    ndmalogf(sess, 0, 2, "robot move OK @%d to @%d", src_addr, dst_addr);
  } else {
    ndmalogf(sess, 0, 2, "robot move BAD @%d to @%d", src_addr, dst_addr);
  }

  return rc;
}

int ndmca_robot_load(struct ndm_session* sess, int slot_addr)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  unsigned dte_addr = smc->elem_aa.dte_addr;
  int rc;

  if (sess->control_acb->job.drive_addr_given)
    dte_addr = sess->control_acb->job.drive_addr;

  ndmalogf(sess, 0, 1, "Commanding robot to load slot @%d into drive @%d",
           slot_addr, dte_addr);

  rc = ndmca_robot_move(sess, slot_addr, dte_addr);

  return rc;
}

int ndmca_robot_unload(struct ndm_session* sess, int slot_addr)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  int dte_addr = smc->elem_aa.dte_addr;
  int rc;

  if (sess->control_acb->job.drive_addr_given)
    dte_addr = sess->control_acb->job.drive_addr;

  /* tricky part -- some (most?) robots need the drive to eject */

  ndmalogf(sess, 0, 1, "Commanding robot to unload drive @%d to slot @%d",
           dte_addr, slot_addr);

  rc = ndmca_robot_move(sess, dte_addr, slot_addr);

  return rc;
}


struct smc_element_descriptor* ndmca_robot_find_element(
    struct ndm_session* sess,
    int element_address)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  struct smc_element_descriptor* edp;

  for (edp = smc->elem_desc; edp; edp = edp->next) {
    if (edp->element_address == element_address) return edp;
  }

  return 0;
}

int ndmca_robot_check_ready(struct ndm_session* sess)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  unsigned first_dte_addr;
  unsigned n_dte_addr;
  int rc;
  unsigned int i;
  int errcnt = 0;
  struct smc_element_descriptor* edp;

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc;

  if (sess->control_acb->job.remedy_all) {
    first_dte_addr = smc->elem_aa.dte_addr;
    n_dte_addr = smc->elem_aa.dte_count;
  } else {
    n_dte_addr = 1;
    if (sess->control_acb->job.drive_addr_given) {
      first_dte_addr = sess->control_acb->job.drive_addr;
    } else {
      first_dte_addr = smc->elem_aa.dte_addr;
    }
  }

  for (i = 0; i < n_dte_addr; i++) {
    edp = ndmca_robot_find_element(sess, first_dte_addr + i);

    if (!edp->Full) continue;

    ndmalogf(sess, 0, 1, "tape drive @%d not empty", edp->element_address);
    errcnt++;
  }

  return errcnt;
}

int ndmca_robot_remedy_ready(struct ndm_session* sess)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  int rc;
  unsigned int i;
  int errcnt;
  struct smc_element_descriptor* edp;
  struct smc_element_descriptor* edp2;
  unsigned first_dte_addr;
  unsigned n_dte_addr;
  char prefix[60];

  errcnt = 0;

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc;

  if (sess->control_acb->job.remedy_all) {
    first_dte_addr = smc->elem_aa.dte_addr;
    n_dte_addr = smc->elem_aa.dte_count;
  } else {
    n_dte_addr = 1;
    if (sess->control_acb->job.drive_addr_given) {
      first_dte_addr = sess->control_acb->job.drive_addr;
    } else {
      first_dte_addr = smc->elem_aa.dte_addr;
    }
  }

  for (i = 0; i < n_dte_addr; i++) {
    edp = ndmca_robot_find_element(sess, first_dte_addr + i);

    if (!edp->Full) continue;

    snprintf(prefix, sizeof(prefix), "drive @%d not empty",
             edp->element_address);

    if (!edp->SValid) {
      ndmalogf(sess, 0, 1, "%s, invalid source", prefix);
      errcnt++;
      continue;
    }

    sprintf(NDMOS_API_STREND(prefix), ", src @%d", edp->src_se_addr);

    edp2 = ndmca_robot_find_element(sess, edp->src_se_addr);

    if (edp2->element_type_code != SMC_ELEM_TYPE_SE) {
      ndmalogf(sess, 0, 1, "%s, not slot", prefix);
      errcnt++;
      continue;
    }

    if (edp2->Full) {
      ndmalogf(sess, 0, 1, "%s, but slot Full", prefix);
      errcnt++;
      continue;
    }

    rc = ndmca_robot_move(sess, edp->element_address, edp->src_se_addr);
    if (rc) {
      ndmalogf(sess, 0, 1, "%s, move failed", prefix);
      errcnt++;
      continue;
    }
  }

  return errcnt;
}


/*
 * ndmca_robot_query() incrementally obtains info so that we
 * can print progress.
 */

int ndmca_robot_query(struct ndm_session* sess)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  int rc;
  unsigned int i = 0;
  char buf[111];
  char lnbuf[30];
  int lineno, nline = 1;
  struct smc_element_descriptor* edp;

  ndmalogqr(sess, "  Type");

  rc = smc_inquire(smc);
  if (rc) {
    ndmalogqr(sess, "    ERROR smc_inquire(): %s", smc->errmsg);
  } else {
    ndmalogqr(sess, "    '%s'", smc->ident);
  }


  ndmalogqr(sess, "  Elements");
  rc = smc_get_elem_aa(smc);
  if (rc) {
    ndmalogqr(sess, "    ERROR smc_get_elem_aa(): %s", smc->errmsg);
  } else {
    strcpy(lnbuf, "    ");
    for (lineno = 0, nline = 1; lineno < nline; lineno++) {
      rc = smc_pp_element_address_assignments(&smc->elem_aa, lineno, buf);
      if (rc < 0) { strcpy(buf, "PP-ERROR"); }
      nline = rc;
      ndmalogqr(sess, "%s %s", lnbuf, buf);
    }
  }

  ndmalogqr(sess, "  Status");
  rc = smc_read_elem_status(smc);
  if (rc) {
    ndmalogqr(sess, "    ERROR smc_read_elem_status(): %s", smc->errmsg);
  } else {
    ndmalogqr(sess, "    E#  Addr Type Status");
    ndmalogqr(sess, "    --  ---- ---- ---------------------");
    for (edp = smc->elem_desc; edp; edp = edp->next) {
      for (lineno = 0, nline = 1; lineno < nline; lineno++) {
        rc = smc_pp_element_descriptor(edp, lineno, buf);

        if (lineno == 0)
          snprintf(lnbuf, sizeof(lnbuf), "    %2d ", i + 1);
        else
          snprintf(lnbuf, sizeof(lnbuf), "       ");

        if (rc < 0) { strcpy(buf, "PP-ERROR"); }
        nline = rc;
        ndmalogqr(sess, "%s %s", lnbuf, buf);
      }
      i++;
    }
  }

  return 0;
}


int ndmca_robot_verify_media(struct ndm_session* sess)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  struct ndm_media_table* mtab = &sess->control_acb->job.media_tab;
  int rc;
  struct ndmmedia* me;
  struct smc_element_descriptor* edp;
  int errcnt = 0;

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc;

  for (me = mtab->head; me; me = me->next) {
    if (!me->valid_slot) {
      me->slot_missing = 1;
      errcnt++;
      continue; /* what now */
    }

    for (edp = smc->elem_desc; edp; edp = edp->next) {
      if (edp->element_type_code != SMC_ELEM_TYPE_SE) continue;

      if (edp->element_address != me->slot_addr) continue;

      if (!edp->Full) {
        me->slot_empty = 1;
        errcnt++;
      } else {
        me->slot_empty = 0;
      }
      break;
    }

    if (!edp) {
      me->slot_bad = 1;
      errcnt++;
    }
  }

  return errcnt;
}

/*
 * For NDM_JOB_OP_LIST_LABELS, fill in media_tab based on non-empty slots.
 * Note: this might REALLY nerf on a cleaning cartridge.
 */

int ndmca_robot_synthesize_media(struct ndm_session* sess)
{
  struct smc_ctrl_block* smc = sess->control_acb->smc_cb;
  struct ndm_media_table* mtab = &sess->control_acb->job.media_tab;
  int rc;
  struct smc_element_descriptor* edp;

  rc = ndmca_robot_obtain_info(sess);
  if (rc) return rc;

  for (edp = smc->elem_desc; edp; edp = edp->next) {
    if (edp->element_type_code != SMC_ELEM_TYPE_SE) continue;

    if (!edp->Full) continue;

    ndma_store_media(mtab, edp->element_address);
  }

  return 0;
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
