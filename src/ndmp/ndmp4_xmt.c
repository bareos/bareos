/*
 * Copyright (c) 1998,1999,2000
 *	Traakan, Inc., Los Altos, CA
 *	All rights reserved.
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


#ifndef NDMOS_OPTION_NO_NDMP4


#define xdr_ndmp4_connect_close_request		xdr_void
#define xdr_ndmp4_connect_close_reply		xdr_void
#define xdr_ndmp4_config_get_host_info_request	xdr_void
#define xdr_ndmp4_config_get_connection_type_request xdr_void
#define xdr_ndmp4_config_get_butype_info_request xdr_void
#define xdr_ndmp4_config_get_fs_info_request	xdr_void
#define xdr_ndmp4_config_get_tape_info_request	xdr_void
#define xdr_ndmp4_config_get_scsi_info_request	xdr_void
#define xdr_ndmp4_config_get_server_info_request xdr_void
#define xdr_ndmp4_scsi_close_request		xdr_void
#define xdr_ndmp4_scsi_get_state_request	xdr_void
#define xdr_ndmp4_scsi_reset_device_request	xdr_void
#define xdr_ndmp4_scsi_reset_bus_request	xdr_void
#define xdr_ndmp4_tape_close_request		xdr_void
#define xdr_ndmp4_tape_get_state_request	xdr_void
#define xdr_ndmp4_data_get_state_request	xdr_void

#define xdr_ndmp4_data_abort_request		xdr_void
#define xdr_ndmp4_data_get_env_request		xdr_void
#define xdr_ndmp4_data_stop_request		xdr_void

#define xdr_ndmp4_mover_get_state_request	xdr_void
#define xdr_ndmp4_mover_continue_request	xdr_void
#define xdr_ndmp4_mover_abort_request		xdr_void
#define xdr_ndmp4_mover_stop_request		xdr_void
#define xdr_ndmp4_mover_close_request		xdr_void




struct ndmp_xdr_message_table	ndmp4_xdr_message_table[] = {
   { NDMP4_CONNECT_OPEN,
     xdr_ndmp4_connect_open_request,
     xdr_ndmp4_connect_open_reply,
   },
   { NDMP4_CONNECT_CLIENT_AUTH,
     xdr_ndmp4_connect_client_auth_request,
     xdr_ndmp4_connect_client_auth_reply,
   },
   { NDMP4_CONNECT_CLOSE,
     xdr_ndmp4_connect_close_request,
     xdr_ndmp4_connect_close_reply,
   },
   { NDMP4_CONNECT_SERVER_AUTH,
     xdr_ndmp4_connect_server_auth_request,
     xdr_ndmp4_connect_server_auth_reply,
   },
   { NDMP4_CONFIG_GET_HOST_INFO,
     xdr_ndmp4_config_get_host_info_request,
     xdr_ndmp4_config_get_host_info_reply,
   },
   { NDMP4_CONFIG_GET_CONNECTION_TYPE,
     xdr_ndmp4_config_get_connection_type_request,
     xdr_ndmp4_config_get_connection_type_reply,
   },
   { NDMP4_CONFIG_GET_AUTH_ATTR,
     xdr_ndmp4_config_get_auth_attr_request,
     xdr_ndmp4_config_get_auth_attr_reply,
   },
   { NDMP4_CONFIG_GET_BUTYPE_INFO,
     xdr_ndmp4_config_get_butype_info_request,
     xdr_ndmp4_config_get_butype_info_reply,
   },
   { NDMP4_CONFIG_GET_FS_INFO,
     xdr_ndmp4_config_get_fs_info_request,
     xdr_ndmp4_config_get_fs_info_reply,
   },
   { NDMP4_CONFIG_GET_TAPE_INFO,
     xdr_ndmp4_config_get_tape_info_request,
     xdr_ndmp4_config_get_tape_info_reply,
   },
   { NDMP4_CONFIG_GET_SCSI_INFO,
     xdr_ndmp4_config_get_scsi_info_request,
     xdr_ndmp4_config_get_scsi_info_reply,
   },
   { NDMP4_CONFIG_GET_SERVER_INFO,
     xdr_ndmp4_config_get_server_info_request,
     xdr_ndmp4_config_get_server_info_reply,
   },
   { NDMP4_SCSI_OPEN,
     xdr_ndmp4_scsi_open_request,
     xdr_ndmp4_scsi_open_reply,
   },
   { NDMP4_SCSI_CLOSE,
     xdr_ndmp4_scsi_close_request,
     xdr_ndmp4_scsi_close_reply,
   },
   { NDMP4_SCSI_GET_STATE,
     xdr_ndmp4_scsi_get_state_request,
     xdr_ndmp4_scsi_get_state_reply,
   },
   { NDMP4_SCSI_RESET_DEVICE,
     xdr_ndmp4_scsi_reset_device_request,
     xdr_ndmp4_scsi_reset_device_reply,
   },
   { NDMP4_SCSI_EXECUTE_CDB,
     xdr_ndmp4_scsi_execute_cdb_request,
     xdr_ndmp4_scsi_execute_cdb_reply,
   },
   { NDMP4_TAPE_OPEN,
     xdr_ndmp4_tape_open_request,
     xdr_ndmp4_tape_open_reply,
   },
   { NDMP4_TAPE_CLOSE,
     xdr_ndmp4_tape_close_request,
     xdr_ndmp4_tape_close_reply,
   },
   { NDMP4_TAPE_GET_STATE,
     xdr_ndmp4_tape_get_state_request,
     xdr_ndmp4_tape_get_state_reply,
   },
   { NDMP4_TAPE_MTIO,
     xdr_ndmp4_tape_mtio_request,
     xdr_ndmp4_tape_mtio_reply,
   },
   { NDMP4_TAPE_WRITE,
     xdr_ndmp4_tape_write_request,
     xdr_ndmp4_tape_write_reply,
   },
   { NDMP4_TAPE_READ,
     xdr_ndmp4_tape_read_request,
     xdr_ndmp4_tape_read_reply,
   },
   { NDMP4_TAPE_EXECUTE_CDB,
     xdr_ndmp4_tape_execute_cdb_request,
     xdr_ndmp4_tape_execute_cdb_reply,
   },
   { NDMP4_DATA_GET_STATE,
     xdr_ndmp4_data_get_state_request,
     xdr_ndmp4_data_get_state_reply,
   },
   { NDMP4_DATA_START_BACKUP,
     xdr_ndmp4_data_start_backup_request,
     xdr_ndmp4_data_start_backup_reply,
   },
   { NDMP4_DATA_START_RECOVER,
     xdr_ndmp4_data_start_recover_request,
     xdr_ndmp4_data_start_recover_reply,
   },
   { NDMP4_DATA_START_RECOVER_FILEHIST,
     xdr_ndmp4_data_start_recover_filehist_request,
     xdr_ndmp4_data_start_recover_filehist_reply,
   },
   { NDMP4_DATA_ABORT,
     xdr_ndmp4_data_abort_request,
     xdr_ndmp4_data_abort_reply,
   },
   { NDMP4_DATA_GET_ENV,
     xdr_ndmp4_data_get_env_request,
     xdr_ndmp4_data_get_env_reply,
   },
   { NDMP4_DATA_STOP,
     xdr_ndmp4_data_stop_request,
     xdr_ndmp4_data_stop_reply,
   },
   { NDMP4_DATA_LISTEN,
     xdr_ndmp4_data_listen_request,
     xdr_ndmp4_data_listen_reply,
   },
   { NDMP4_DATA_CONNECT,
     xdr_ndmp4_data_connect_request,
     xdr_ndmp4_data_connect_reply,
   },
   { NDMP4_NOTIFY_DATA_HALTED,
     xdr_ndmp4_notify_data_halted_post,
     0
   },
   { NDMP4_NOTIFY_CONNECTION_STATUS,
     xdr_ndmp4_notify_connection_status_post,
     0
   },
   { NDMP4_NOTIFY_MOVER_HALTED,
     xdr_ndmp4_notify_mover_halted_post,
     0
   },
   { NDMP4_NOTIFY_MOVER_PAUSED,
     xdr_ndmp4_notify_mover_paused_post,
     0
   },
   { NDMP4_NOTIFY_DATA_READ,
     xdr_ndmp4_notify_data_read_post,
     0
   },
   { NDMP4_LOG_FILE,
     xdr_ndmp4_log_file_post,
     0
   },
   { NDMP4_LOG_MESSAGE,
     xdr_ndmp4_log_message_post,
     0
   },
   { NDMP4_FH_ADD_FILE,
     xdr_ndmp4_fh_add_file_post,
     0
   },
   { NDMP4_FH_ADD_DIR,
     xdr_ndmp4_fh_add_dir_post,
     0
   },
   { NDMP4_FH_ADD_NODE,
     xdr_ndmp4_fh_add_node_post,
     0
   },
   { NDMP4_MOVER_GET_STATE,
     xdr_ndmp4_mover_get_state_request,
     xdr_ndmp4_mover_get_state_reply,
   },
   { NDMP4_MOVER_LISTEN,
     xdr_ndmp4_mover_listen_request,
     xdr_ndmp4_mover_listen_reply,
   },
   { NDMP4_MOVER_CONTINUE,
     xdr_ndmp4_mover_continue_request,
     xdr_ndmp4_mover_continue_reply,
   },
   { NDMP4_MOVER_ABORT,
     xdr_ndmp4_mover_abort_request,
     xdr_ndmp4_mover_abort_reply,
   },
   { NDMP4_MOVER_STOP,
     xdr_ndmp4_mover_stop_request,
     xdr_ndmp4_mover_stop_reply,
   },
   { NDMP4_MOVER_SET_WINDOW,
     xdr_ndmp4_mover_set_window_request,
     xdr_ndmp4_mover_set_window_reply,
   },
   { NDMP4_MOVER_READ,
     xdr_ndmp4_mover_read_request,
     xdr_ndmp4_mover_read_reply,
   },
   { NDMP4_MOVER_CLOSE,
     xdr_ndmp4_mover_close_request,
     xdr_ndmp4_mover_close_reply,
   },
   { NDMP4_MOVER_SET_RECORD_SIZE,
     xdr_ndmp4_mover_set_record_size_request,
     xdr_ndmp4_mover_set_record_size_reply,
   },
   { NDMP4_MOVER_CONNECT,
     xdr_ndmp4_mover_connect_request,
     xdr_ndmp4_mover_connect_reply,
   },
   {0}
};



/*
 * XDR unsigned long integers
 * same as xdr_long - open coded to save a proc call!
 */
bool_t
xdr_ndmp4_u_quad(register XDR *xdrs, ndmp4_u_quad *objp)
{
	uint32_t	hi, lo;

	switch (xdrs->x_op) {
	case XDR_DECODE:
#if defined(_LP64)
		if (XDR_GETINT32(xdrs, (int32_t *)&hi)
		 && XDR_GETINT32(xdrs, (int32_t *)&lo)) {
#else
		if (XDR_GETLONG(xdrs, (long *)&hi)
		 && XDR_GETLONG(xdrs, (long *)&lo)) {
#endif
			*objp = ((uint64_t)hi << 32) | (lo & 0xffffffff);
			return TRUE;
		}
		break;

	case XDR_ENCODE:
		hi = *objp >> 32;
		lo = *objp & 0xffffffff;
#if defined(_LP64)
		return XDR_PUTINT32(xdrs, (int32_t *)&hi)
		    && XDR_PUTINT32(xdrs, (int32_t *)&lo);
#else
		return XDR_PUTLONG(xdrs, (long *)&hi)
		    && XDR_PUTLONG(xdrs, (long *)&lo);
#endif

	case XDR_FREE:
		return (TRUE);
	}

	return (FALSE);
}

#endif /* !NDMOS_OPTION_NO_NDMP4 */
