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


#include "ndmos.h"
#include "ndmprotocol.h"


#ifndef NDMOS_OPTION_NO_NDMP3


#define xdr_ndmp3_connect_close_request xdr_void
#define xdr_ndmp3_connect_close_reply xdr_void
#define xdr_ndmp3_config_get_host_info_request xdr_void
#define xdr_ndmp3_config_get_connection_type_request xdr_void
#define xdr_ndmp3_config_get_butype_info_request xdr_void
#define xdr_ndmp3_config_get_fs_info_request xdr_void
#define xdr_ndmp3_config_get_tape_info_request xdr_void
#define xdr_ndmp3_config_get_scsi_info_request xdr_void
#define xdr_ndmp3_config_get_server_info_request xdr_void
#define xdr_ndmp3_scsi_close_request xdr_void
#define xdr_ndmp3_scsi_get_state_request xdr_void
#define xdr_ndmp3_scsi_reset_device_request xdr_void
#define xdr_ndmp3_scsi_reset_bus_request xdr_void
#define xdr_ndmp3_tape_close_request xdr_void
#define xdr_ndmp3_tape_get_state_request xdr_void
#define xdr_ndmp3_data_get_state_request xdr_void

#define xdr_ndmp3_data_abort_request xdr_void
#define xdr_ndmp3_data_get_env_request xdr_void
#define xdr_ndmp3_data_stop_request xdr_void
#define xdr_ndmp3_notify_data_halted_reply 0
#define xdr_ndmp3_notify_connected_reply 0
#define xdr_ndmp3_notify_mover_halted_reply 0
#define xdr_ndmp3_notify_mover_paused_reply 0
#define xdr_ndmp3_notify_data_read_reply 0

#define xdr_ndmp3_log_message_reply 0
#define xdr_ndmp3_log_file_reply 0
#define xdr_ndmp3_fh_add_file_reply 0
#define xdr_ndmp3_fh_add_dir_reply 0
#define xdr_ndmp3_fh_add_node_reply 0

#define xdr_ndmp3_mover_get_state_request xdr_void
#define xdr_ndmp3_mover_continue_request xdr_void
#define xdr_ndmp3_mover_abort_request xdr_void
#define xdr_ndmp3_mover_stop_request xdr_void
#define xdr_ndmp3_mover_close_request xdr_void


struct ndmp_xdr_message_table ndmp3_xdr_message_table[] = {
    {
        NDMP3_CONNECT_OPEN,
        xdr_ndmp3_connect_open_request,
        xdr_ndmp3_connect_open_reply,
    },
    {
        NDMP3_CONNECT_CLIENT_AUTH,
        xdr_ndmp3_connect_client_auth_request,
        xdr_ndmp3_connect_client_auth_reply,
    },
    {
        NDMP3_CONNECT_CLOSE,
        xdr_ndmp3_connect_close_request,
        xdr_ndmp3_connect_close_reply,
    },
    {
        NDMP3_CONNECT_SERVER_AUTH,
        xdr_ndmp3_connect_server_auth_request,
        xdr_ndmp3_connect_server_auth_reply,
    },
    {
        NDMP3_CONFIG_GET_HOST_INFO,
        xdr_ndmp3_config_get_host_info_request,
        xdr_ndmp3_config_get_host_info_reply,
    },
    {
        NDMP3_CONFIG_GET_CONNECTION_TYPE,
        xdr_ndmp3_config_get_connection_type_request,
        xdr_ndmp3_config_get_connection_type_reply,
    },
    {
        NDMP3_CONFIG_GET_AUTH_ATTR,
        xdr_ndmp3_config_get_auth_attr_request,
        xdr_ndmp3_config_get_auth_attr_reply,
    },
    {
        NDMP3_CONFIG_GET_BUTYPE_INFO,
        xdr_ndmp3_config_get_butype_info_request,
        xdr_ndmp3_config_get_butype_info_reply,
    },
    {
        NDMP3_CONFIG_GET_FS_INFO,
        xdr_ndmp3_config_get_fs_info_request,
        xdr_ndmp3_config_get_fs_info_reply,
    },
    {
        NDMP3_CONFIG_GET_TAPE_INFO,
        xdr_ndmp3_config_get_tape_info_request,
        xdr_ndmp3_config_get_tape_info_reply,
    },
    {
        NDMP3_CONFIG_GET_SCSI_INFO,
        xdr_ndmp3_config_get_scsi_info_request,
        xdr_ndmp3_config_get_scsi_info_reply,
    },
    {
        NDMP3_CONFIG_GET_SERVER_INFO,
        xdr_ndmp3_config_get_server_info_request,
        xdr_ndmp3_config_get_server_info_reply,
    },
    {
        NDMP3_SCSI_OPEN,
        xdr_ndmp3_scsi_open_request,
        xdr_ndmp3_scsi_open_reply,
    },
    {
        NDMP3_SCSI_CLOSE,
        xdr_ndmp3_scsi_close_request,
        xdr_ndmp3_scsi_close_reply,
    },
    {
        NDMP3_SCSI_GET_STATE,
        xdr_ndmp3_scsi_get_state_request,
        xdr_ndmp3_scsi_get_state_reply,
    },
    {
        NDMP3_SCSI_SET_TARGET,
        xdr_ndmp3_scsi_set_target_request,
        xdr_ndmp3_scsi_set_target_reply,
    },
    {
        NDMP3_SCSI_RESET_DEVICE,
        xdr_ndmp3_scsi_reset_device_request,
        xdr_ndmp3_scsi_reset_device_reply,
    },
    {
        NDMP3_SCSI_RESET_BUS,
        xdr_ndmp3_scsi_reset_bus_request,
        xdr_ndmp3_scsi_reset_bus_reply,
    },
    {
        NDMP3_SCSI_EXECUTE_CDB,
        xdr_ndmp3_scsi_execute_cdb_request,
        xdr_ndmp3_scsi_execute_cdb_reply,
    },
    {
        NDMP3_TAPE_OPEN,
        xdr_ndmp3_tape_open_request,
        xdr_ndmp3_tape_open_reply,
    },
    {
        NDMP3_TAPE_CLOSE,
        xdr_ndmp3_tape_close_request,
        xdr_ndmp3_tape_close_reply,
    },
    {
        NDMP3_TAPE_GET_STATE,
        xdr_ndmp3_tape_get_state_request,
        xdr_ndmp3_tape_get_state_reply,
    },
    {
        NDMP3_TAPE_MTIO,
        xdr_ndmp3_tape_mtio_request,
        xdr_ndmp3_tape_mtio_reply,
    },
    {
        NDMP3_TAPE_WRITE,
        xdr_ndmp3_tape_write_request,
        xdr_ndmp3_tape_write_reply,
    },
    {
        NDMP3_TAPE_READ,
        xdr_ndmp3_tape_read_request,
        xdr_ndmp3_tape_read_reply,
    },
    {
        NDMP3_TAPE_EXECUTE_CDB,
        xdr_ndmp3_tape_execute_cdb_request,
        xdr_ndmp3_tape_execute_cdb_reply,
    },
    {
        NDMP3_DATA_GET_STATE,
        xdr_ndmp3_data_get_state_request,
        xdr_ndmp3_data_get_state_reply,
    },
    {
        NDMP3_DATA_START_BACKUP,
        xdr_ndmp3_data_start_backup_request,
        xdr_ndmp3_data_start_backup_reply,
    },
    {
        NDMP3_DATA_START_RECOVER,
        xdr_ndmp3_data_start_recover_request,
        xdr_ndmp3_data_start_recover_reply,
    },
    {
        NDMP3_DATA_START_RECOVER_FILEHIST,
        xdr_ndmp3_data_start_recover_filehist_request,
        xdr_ndmp3_data_start_recover_filehist_reply,
    },
    {
        NDMP3_DATA_ABORT,
        xdr_ndmp3_data_abort_request,
        xdr_ndmp3_data_abort_reply,
    },
    {
        NDMP3_DATA_GET_ENV,
        xdr_ndmp3_data_get_env_request,
        xdr_ndmp3_data_get_env_reply,
    },
    {
        NDMP3_DATA_STOP,
        xdr_ndmp3_data_stop_request,
        xdr_ndmp3_data_stop_reply,
    },
    {
        NDMP3_DATA_LISTEN,
        xdr_ndmp3_data_listen_request,
        xdr_ndmp3_data_listen_reply,
    },
    {
        NDMP3_DATA_CONNECT,
        xdr_ndmp3_data_connect_request,
        xdr_ndmp3_data_connect_reply,
    },
    {
        NDMP3_NOTIFY_DATA_HALTED,
        xdr_ndmp3_notify_data_halted_request,
        xdr_ndmp3_notify_data_halted_reply,
    },
    {
        NDMP3_NOTIFY_CONNECTED,
        xdr_ndmp3_notify_connected_request,
        xdr_ndmp3_notify_connected_reply,
    },
    {
        NDMP3_NOTIFY_MOVER_HALTED,
        xdr_ndmp3_notify_mover_halted_request,
        xdr_ndmp3_notify_mover_halted_reply,
    },
    {
        NDMP3_NOTIFY_MOVER_PAUSED,
        xdr_ndmp3_notify_mover_paused_request,
        xdr_ndmp3_notify_mover_paused_reply,
    },
    {
        NDMP3_NOTIFY_DATA_READ,
        xdr_ndmp3_notify_data_read_request,
        xdr_ndmp3_notify_data_read_reply,
    },
    {
        NDMP3_LOG_FILE,
        xdr_ndmp3_log_file_request,
        xdr_ndmp3_log_file_reply,
    },
    {
        NDMP3_LOG_MESSAGE,
        xdr_ndmp3_log_message_request,
        xdr_ndmp3_log_message_reply,
    },
    {
        NDMP3_FH_ADD_FILE,
        xdr_ndmp3_fh_add_file_request,
        xdr_ndmp3_fh_add_file_reply,
    },
    {
        NDMP3_FH_ADD_DIR,
        xdr_ndmp3_fh_add_dir_request,
        xdr_ndmp3_fh_add_dir_reply,
    },
    {
        NDMP3_FH_ADD_NODE,
        xdr_ndmp3_fh_add_node_request,
        xdr_ndmp3_fh_add_node_reply,
    },
    {
        NDMP3_MOVER_GET_STATE,
        xdr_ndmp3_mover_get_state_request,
        xdr_ndmp3_mover_get_state_reply,
    },
    {
        NDMP3_MOVER_LISTEN,
        xdr_ndmp3_mover_listen_request,
        xdr_ndmp3_mover_listen_reply,
    },
    {
        NDMP3_MOVER_CONTINUE,
        xdr_ndmp3_mover_continue_request,
        xdr_ndmp3_mover_continue_reply,
    },
    {
        NDMP3_MOVER_ABORT,
        xdr_ndmp3_mover_abort_request,
        xdr_ndmp3_mover_abort_reply,
    },
    {
        NDMP3_MOVER_STOP,
        xdr_ndmp3_mover_stop_request,
        xdr_ndmp3_mover_stop_reply,
    },
    {
        NDMP3_MOVER_SET_WINDOW,
        xdr_ndmp3_mover_set_window_request,
        xdr_ndmp3_mover_set_window_reply,
    },
    {
        NDMP3_MOVER_READ,
        xdr_ndmp3_mover_read_request,
        xdr_ndmp3_mover_read_reply,
    },
    {
        NDMP3_MOVER_CLOSE,
        xdr_ndmp3_mover_close_request,
        xdr_ndmp3_mover_close_reply,
    },
    {
        NDMP3_MOVER_SET_RECORD_SIZE,
        xdr_ndmp3_mover_set_record_size_request,
        xdr_ndmp3_mover_set_record_size_reply,
    },
    {
        NDMP3_MOVER_CONNECT,
        xdr_ndmp3_mover_connect_request,
        xdr_ndmp3_mover_connect_reply,
    },
    {0}};


/*
 * XDR unsigned long integers
 * same as xdr_long - open coded to save a proc call!
 */
bool_t xdr_ndmp3_u_quad(register XDR* xdrs, ndmp3_u_quad* objp)
{
  uint32_t hi, lo;

  switch (xdrs->x_op) {
    case XDR_DECODE:
#if defined(_LP64)
      if (XDR_GETINT32(xdrs, (int32_t*)&hi) &&
          XDR_GETINT32(xdrs, (int32_t*)&lo)) {
#else
      if (XDR_GETLONG(xdrs, (long*)&hi) && XDR_GETLONG(xdrs, (long*)&lo)) {
#endif
        *objp = ((uint64_t)hi << 32) | (lo & 0xffffffff);
        return TRUE;
      }
      break;

    case XDR_ENCODE:
      hi = *objp >> 32;
      lo = *objp & 0xffffffff;
#if defined(_LP64)
      return XDR_PUTINT32(xdrs, (int32_t*)&hi) &&
             XDR_PUTINT32(xdrs, (int32_t*)&lo);
#else
      return XDR_PUTLONG(xdrs, (long*)&hi) && XDR_PUTLONG(xdrs, (long*)&lo);
#endif

    case XDR_FREE:
      return (TRUE);
  }

  return (FALSE);
}

#endif /* !NDMOS_OPTION_NO_NDMP3 */
