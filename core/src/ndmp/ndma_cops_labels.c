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


int ndmca_op_init_labels(struct ndm_session* sess)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndm_job_param* job = &ca->job;
  struct ndm_media_table* mtab = &job->media_tab;
  int n_media = mtab->n_media;
  struct ndmmedia* me;
  int rc, errors;

  ca->tape_mode = NDMP9_TAPE_RDWR_MODE;
  ca->is_label_op = 1;

  if (n_media <= 0) {
    ndmalogf(sess, 0, 0, "No media entries in table");
    return -1;
  }

  errors = 0;
  for (me = mtab->head; me; me = me->next) {
    if (me->valid_label) continue;

    ndmalogf(sess, 0, 0, "media #%d missing a label", me->index);
    errors++;
  }
  if (errors) return -1;

  rc = ndmca_op_robot_startup(sess, 1);
  if (rc) return rc; /* already tattled */

  rc = ndmca_connect_tape_agent(sess);
  if (rc) {
    ndmconn_destruct(sess->plumb.tape);
    sess->plumb.tape = NULL;
    return rc; /* already tattled */
  }

  for (me = mtab->head; me; me = me->next) {
    ca->cur_media_ix = me->index;

    rc = ndmca_media_load_current(sess);
    if (rc) {
      /* already tattled */
      continue;
    }

    rc = ndmca_media_write_label(sess, 'm', me->label);
    if (rc) { ndmalogf(sess, 0, 0, "failed label write"); }

    ndmca_media_write_filemarks(sess);
    ndmca_media_unload_current(sess);
  }

  return rc;
}

int ndmca_op_list_labels(struct ndm_session* sess)
{
  struct ndm_control_agent* ca = sess->control_acb;
  struct ndm_job_param* job = &ca->job;
  struct ndm_media_table* mtab = &job->media_tab;
  int n_media;
  char labbuf[NDMMEDIA_LABEL_MAX];
  char buf[200];
  struct ndmmedia* me;
  int rc;

  ca->tape_mode = NDMP9_TAPE_READ_MODE;
  ca->is_label_op = 1;

  rc = ndmca_op_robot_startup(sess, 0);
  if (rc) return rc; /* already tattled */

  if (job->media_tab.n_media == 0) {
    if (job->have_robot) {
      rc = ndmca_robot_synthesize_media(sess);
      if (rc) return rc; /* already tattled */
    } else {
      /*
       * No fixup. Should be done by now.
       * See ndma_job_auto_adjust()
       */
    }
  }

  if ((rc = ndmca_connect_tape_agent(sess)) != 0) {
    ndmconn_destruct(sess->plumb.tape);
    sess->plumb.tape = NULL;
    return rc; /* already tattled */
  }

  n_media = mtab->n_media;

  for (me = mtab->head; me; me = me->next) {
    ca->cur_media_ix = me->index;

    rc = ndmca_media_load_current(sess);
    if (rc) {
      /* already tattled */
      continue;
    }

    rc = ndmca_media_read_label(sess, labbuf);
    if (rc == 'm' || rc == 'V') {
      strcpy(me->label, labbuf);
      me->valid_label = 1;
      ndmmedia_to_str(me, buf);
      ndmalogf(sess, "ME", 0, "%s", buf);
    } else {
      ndmalogf(sess, 0, 0, "failed label read");
    }
    ndmca_media_unload_current(sess);
  }

  return rc;
}

#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
