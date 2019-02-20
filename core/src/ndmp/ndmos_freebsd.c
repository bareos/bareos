/*
 * Copyright (c) 1998,2001
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
 *      This contains the O/S (Operating System) specific
 *      portions of NDMJOBLIB for the FreeBSD platform.
 *
 *      This file is #include'd by ndmos.c when
 *      selected by #ifdef's of NDMOS_ID.
 *
 *      There are four major portions:
 *      1) Misc support routines: password check, local info, etc
 *      2) Non-blocking I/O support routines
 *      3) Tape interfacs ndmos_tape_xxx()
 *      4) OS Specific NDMP request dispatcher which intercepts
 *         requests implemented here, such as SCSI operations
 *         and system configuration queries.
 */


/*
 * #include "ndmagents.h" already done in ndmos.c
 * Additional #include's, not needed in ndmos_freebsd.h, yet needed here.
 */
#include <sys/utsname.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>


/*
 * Select common code fragments from ndmos_common.c
 */
#define NDMOS_COMMON_SYNC_CONFIG_INFO /* from config file (ndmjob.conf) */
#define NDMOS_COMMON_OK_NAME_PASSWORD
#define NDMOS_COMMON_MD5
#define NDMOS_COMMON_NONBLOCKING_IO_SUPPORT
#define NDMOS_COMMON_TAPE_INTERFACE   /* uses tape simulator */
#define NDMOS_COMMON_SCSI_INTERFACE   /* use scsi simulator */
#define NDMOS_COMMON_DISPATCH_REQUEST /* no-op */


#include "ndmos_common.c"

#ifdef NDMOS_COMMON_SCSI_INTERFACE
#ifndef NDMOS_OPTION_NO_ROBOT_AGENT /* Surrounds all SCSI intfs */
#ifndef NDMOS_OPTION_ROBOT_SIMULATOR
/*
 * SCSI INTERFACE
 ****************************************************************
 */

int ndmos_scsi_initialize(struct ndm_session* sess)
{
  sess->robot_acb->camdev = 0;
  return 0;
}

void ndmos_scsi_sync_state(struct ndm_session* sess)
{
  struct ndm_robot_agent* robot = sess->robot_acb;
  struct cam_device* camdev = robot->camdev;
  ndmp9_scsi_get_state_reply* state = &robot->scsi_state;

  if (robot->camdev) {
    state->error = NDMP9_NO_ERR;
    state->target_controller = camdev->path_id;
    state->target_id = camdev->target_id;
    state->target_lun = camdev->target_lun;
  } else {
    state->error = NDMP9_DEV_NOT_OPEN_ERR;
    state->target_controller = -1;
    state->target_id = -1;
    state->target_lun = -1;
  }
}

ndmp9_error ndmos_scsi_open(struct ndm_session* sess, char* name)
{
  struct ndm_robot_agent* robot = sess->robot_acb;
  struct cam_device* camdev = robot->camdev;

  /* redundant test */
  if (camdev) { return NDMP9_DEVICE_OPENED_ERR; }

  camdev = cam_open_pass(name, 2, 0);
  if (!camdev) {
    /* could sniff errno */
    switch (errno) {
      default:
      case ENOENT:
        return NDMP9_NO_DEVICE_ERR;

      case EACCES:
      case EPERM:
        return NDMP9_PERMISSION_ERR;

      case EBUSY:
        return NDMP9_DEVICE_BUSY_ERR;
    }
  }

  robot->camdev = camdev;
  ndmos_scsi_sync_state(sess);

  return NDMP9_NO_ERR;
}

ndmp9_error ndmos_scsi_close(struct ndm_session* sess)
{
  struct ndm_robot_agent* robot = sess->robot_acb;
  struct cam_device* camdev = robot->camdev;

  /* redundant test */
  if (!camdev) { return NDMP9_DEV_NOT_OPEN_ERR; }

  robot->camdev = 0;
  ndmos_scsi_sync_state(sess);

  cam_close_device(camdev);

  return NDMP9_NO_ERR;
}

/* deprecated */
ndmp9_error ndmos_scsi_set_target(struct ndm_session* sess)
{
  return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error ndmos_scsi_reset_device(struct ndm_session* sess)
{
  return NDMP9_NOT_SUPPORTED_ERR;
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
  struct ndm_robot_agent* robot = sess->robot_acb;
  struct cam_device* camdev = robot->camdev;
  union ccb* ccb;
  u_int32_t flags;
  u_int8_t* data_ptr = 0;
  u_int8_t* data_in_ptr = 0;
  u_int32_t data_len = 0;
  u_int32_t data_done;
  int rc;

  NDMOS_MACRO_ZEROFILL(reply);
  reply->error = NDMP9_IO_ERR; /* pessimistic */

  ccb = cam_getccb(camdev);

  if (!ccb) {
    reply->error = NDMP9_NO_MEM_ERR;
    return reply->error;
  }

  switch (request->data_dir) {
    case NDMP9_SCSI_DATA_DIR_NONE:
      flags = CAM_DIR_NONE;
      break;

    case NDMP9_SCSI_DATA_DIR_IN:
      if (data_len > 1024 * 1024) {
        reply->error = NDMP9_ILLEGAL_ARGS_ERR;
        goto out;
      }

      data_len = request->datain_len;
      data_in_ptr = (u_int8_t*)malloc(data_len);

      if (!data_in_ptr) {
        reply->error = NDMP9_NO_MEM_ERR;
        goto out;
      }
      data_ptr = data_in_ptr;
      flags = CAM_DIR_IN;
      break;

    case NDMP9_SCSI_DATA_DIR_OUT:
      data_len = request->dataout.dataout_len;
      data_ptr = (u_int8_t*)request->dataout.dataout_val;
      flags = CAM_DIR_OUT;
      break;

    default:
      return NDMP9_ILLEGAL_ARGS_ERR;
      break;
  }

  bcopy(request->cdb.cdb_val, &ccb->csio.cdb_io.cdb_bytes,
        request->cdb.cdb_len);

  cam_fill_csio(&ccb->csio,
                /*retries*/ 1,
                /*cbfcnp*/ NULL,
                /*flags*/ flags,
                /*tag_action*/ MSG_SIMPLE_Q_TAG,
                /*data_ptr*/ data_ptr,
                /*dxfer_len*/ data_len,
                /*sense_len*/ SSD_FULL_SIZE,
                /*cdb_len*/ request->cdb.cdb_len,
                /*timeout*/ request->timeout);

  rc = cam_send_ccb(camdev, ccb);
  if (rc != 0) {
    reply->error = NDMP9_IO_ERR;
    goto out;
  }

  switch (ccb->csio.ccb_h.status & CAM_STATUS_MASK) {
    case CAM_REQ_CMP: /* completed */
      reply->error = NDMP9_NO_ERR;
      break;

    case CAM_SEL_TIMEOUT:
    case CAM_CMD_TIMEOUT:
      reply->error = NDMP9_TIMEOUT_ERR;
      break;

    case CAM_SCSI_STATUS_ERROR:
      if (ccb->csio.ccb_h.status & CAM_AUTOSNS_VALID) {
        int n_sense;

        n_sense = ccb->csio.sense_len - ccb->csio.sense_resid;
        reply->ext_sense.ext_sense_val = (char*)malloc(n_sense);
        if (reply->ext_sense.ext_sense_val) {
          bcopy(&ccb->csio.sense_data, reply->ext_sense.ext_sense_val, n_sense);
          reply->ext_sense.ext_sense_len = n_sense;
        }
      }
      reply->error = NDMP9_NO_ERR;
      break;

    default:
      reply->error = NDMP9_IO_ERR;
      break;
  }

out:
  if (reply->error == NDMP9_NO_ERR) {
    reply->status = ccb->csio.scsi_status;
    data_done = data_len - ccb->csio.resid;

    switch (request->data_dir) {
      case NDMP9_SCSI_DATA_DIR_NONE:
        break;

      case NDMP9_SCSI_DATA_DIR_IN:
        reply->datain.datain_val = (char*)data_in_ptr;
        reply->datain.datain_len = data_len;
        break;

      case NDMP9_SCSI_DATA_DIR_OUT:
        reply->dataout_len = data_len;
        break;

      default:
        break;
    }
  } else {
    if (data_in_ptr) {
      free(data_in_ptr);
      data_in_ptr = 0;
    }
  }

  cam_freeccb(ccb);

  return reply->error;
}
#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */
#endif /* NDMOS_OPTION_NO_ROBOT_AGENT   Surrounds all SCSI intfs */
#endif /* NDMOS_COMMON_SCSI_INTERFACE */
