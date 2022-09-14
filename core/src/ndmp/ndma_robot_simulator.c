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

#ifndef NDMOS_OPTION_NO_ROBOT_AGENT
#ifdef NDMOS_OPTION_ROBOT_SIMULATOR

#include "scsiconst.h"

#define ROBOT_CONTROLLER 0
#define ROBOT_ID 7
#define ROBOT_LUN 1

// interface

void ndmos_scsi_register_callbacks(
    struct ndm_session* sess,
    struct ndm_robot_simulator_callbacks* callbacks)
{
  // Only allow one register.
  if (!sess->nrsc) {
    sess->nrsc = NDMOS_API_MALLOC(sizeof(struct ndm_robot_simulator_callbacks));
    if (sess->nrsc) {
      memcpy(sess->nrsc, callbacks,
             sizeof(struct ndm_robot_simulator_callbacks));
    }
  }
}

void ndmos_scsi_unregister_callbacks(struct ndm_session* sess)
{
  if (sess->nrsc) {
    NDMOS_API_FREE(sess->nrsc);
    sess->nrsc = NULL;
  }
}

int ndmos_scsi_initialize(struct ndm_session* sess)
{
  struct ndm_robot_agent* ra = sess->robot_acb;

  NDMOS_MACRO_ZEROFILL(&ra->scsi_state);
  ra->scsi_state.error = NDMP9_DEV_NOT_OPEN_ERR;
  ra->scsi_state.target_controller = ROBOT_CONTROLLER;
  ra->scsi_state.target_id = ROBOT_ID;
  ra->scsi_state.target_lun = ROBOT_LUN;

  return 0;
}

void ndmos_scsi_sync_state(struct ndm_session* sess) {}

ndmp9_error ndmos_scsi_open(struct ndm_session* sess, char* name)
{
  ndmp9_error err;

  if (!name || strlen(name) > NDMOS_CONST_PATH_MAX - 1)
    return NDMP9_NO_DEVICE_ERR;

  if (sess->nrsc && sess->nrsc->scsi_open) {
    err = sess->nrsc->scsi_open(sess, name);
    if (err != NDMP9_NO_ERR) return err;
  }

  return NDMP9_NO_ERR;
}

ndmp9_error ndmos_scsi_close(struct ndm_session* sess)
{
  ndmp9_error err;

  if (sess->nrsc && sess->nrsc->scsi_close) {
    err = sess->nrsc->scsi_close(sess);
    if (err != NDMP9_NO_ERR) return err;
  }

  ndmos_scsi_initialize(sess);
  return NDMP9_NO_ERR;
}

/* deprecated */
ndmp9_error ndmos_scsi_set_target(struct ndm_session* sess)
{
  return NDMP9_NOT_SUPPORTED_ERR;
}


ndmp9_error ndmos_scsi_reset_device(struct ndm_session* sess)
{
  ndmp9_error err;
  struct ndm_robot_agent* ra = sess->robot_acb;

  if (sess->nrsc && sess->nrsc->scsi_reset) {
    err = sess->nrsc->scsi_reset(sess);
    if (err != NDMP9_NO_ERR) return err;
  }

  return ra->scsi_state.error;
}

/* deprecated */
ndmp9_error ndmos_scsi_reset_bus(struct ndm_session* sess)
{
  return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error ndmos_scsi_execute_cdb(struct ndm_session* sess,
                                   ndmp9_execute_cdb_request* request,
                                   ndmp9_execute_cdb_reply* reply)
{
  ndmp9_error err;
  struct ndm_robot_agent* ra = sess->robot_acb;

  if (ra->scsi_state.error != NDMP9_NO_ERR) return ra->scsi_state.error;

  if (request->cdb.cdb_len < 1) return NDMP9_ILLEGAL_ARGS_ERR;

  if (sess->nrsc && sess->nrsc->scsi_execute_cdb) {
    err = sess->nrsc->scsi_execute_cdb(sess, request, reply);
    if (err != NDMP9_NO_ERR) return err;
  }

  return NDMP9_NO_ERR;
}

#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */

#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */
