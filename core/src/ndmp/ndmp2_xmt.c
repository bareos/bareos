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


#ifndef NDMOS_OPTION_NO_NDMP2


#define xdr_ndmp2_connect_close_request		xdr_void
#define xdr_ndmp2_connect_close_reply		xdr_void
#define xdr_ndmp2_config_get_host_info_request	xdr_void
#define xdr_ndmp2_config_get_mover_type_request	xdr_void
#define xdr_ndmp2_scsi_close_request		xdr_void
#define xdr_ndmp2_scsi_get_state_request	xdr_void
#define xdr_ndmp2_scsi_reset_device_request	xdr_void
#define xdr_ndmp2_scsi_reset_bus_request	xdr_void
#define xdr_ndmp2_tape_close_request		xdr_void
#define xdr_ndmp2_tape_get_state_request	xdr_void
#define xdr_ndmp2_data_get_state_request	xdr_void

#define xdr_ndmp2_data_abort_request		xdr_void
#define xdr_ndmp2_data_get_env_request		xdr_void
#define xdr_ndmp2_data_stop_request		xdr_void
#define xdr_ndmp2_notify_data_halted_reply	0
#define xdr_ndmp2_notify_connected_reply	0
#define xdr_ndmp2_notify_mover_halted_reply	0
#define xdr_ndmp2_notify_mover_paused_reply	0
#define xdr_ndmp2_notify_data_read_reply	0
#define xdr_ndmp2_log_log_reply			0
#define xdr_ndmp2_log_debug_reply		0
#define xdr_ndmp2_log_file_reply		0
#define xdr_ndmp2_fh_add_unix_path_reply	0
#define xdr_ndmp2_fh_add_unix_dir_reply		0
#define xdr_ndmp2_fh_add_unix_node_reply	0
#define xdr_ndmp2_mover_get_state_request	xdr_void
#define xdr_ndmp2_mover_continue_request	xdr_void
#define xdr_ndmp2_mover_abort_request		xdr_void
#define xdr_ndmp2_mover_stop_request		xdr_void
#define xdr_ndmp2_mover_close_request		xdr_void




struct ndmp_xdr_message_table	ndmp2_xdr_message_table[] = {
   { NDMP2_CONNECT_OPEN,
     xdr_ndmp2_connect_open_request,
     xdr_ndmp2_connect_open_reply,
   },
   { NDMP2_CONNECT_CLIENT_AUTH,
     xdr_ndmp2_connect_client_auth_request,
     xdr_ndmp2_connect_client_auth_reply,
   },
   { NDMP2_CONNECT_CLOSE,
     xdr_ndmp2_connect_close_request,
     xdr_ndmp2_connect_close_reply,
   },
   { NDMP2_CONNECT_SERVER_AUTH,
     xdr_ndmp2_connect_server_auth_request,
     xdr_ndmp2_connect_server_auth_reply,
   },
   { NDMP2_CONFIG_GET_HOST_INFO,
     xdr_ndmp2_config_get_host_info_request,
     xdr_ndmp2_config_get_host_info_reply,
   },
   { NDMP2_CONFIG_GET_BUTYPE_ATTR,
     xdr_ndmp2_config_get_butype_attr_request,
     xdr_ndmp2_config_get_butype_attr_reply,
   },
   { NDMP2_CONFIG_GET_MOVER_TYPE,
     xdr_ndmp2_config_get_mover_type_request,
     xdr_ndmp2_config_get_mover_type_reply,
   },
   { NDMP2_CONFIG_GET_AUTH_ATTR,
     xdr_ndmp2_config_get_auth_attr_request,
     xdr_ndmp2_config_get_auth_attr_reply,
   },
   { NDMP2_SCSI_OPEN,
     xdr_ndmp2_scsi_open_request,
     xdr_ndmp2_scsi_open_reply,
   },
   { NDMP2_SCSI_CLOSE,
     xdr_ndmp2_scsi_close_request,
     xdr_ndmp2_scsi_close_reply,
   },
   { NDMP2_SCSI_GET_STATE,
     xdr_ndmp2_scsi_get_state_request,
     xdr_ndmp2_scsi_get_state_reply,
   },
   { NDMP2_SCSI_SET_TARGET,
     xdr_ndmp2_scsi_set_target_request,
     xdr_ndmp2_scsi_set_target_reply,
   },
   { NDMP2_SCSI_RESET_DEVICE,
     xdr_ndmp2_scsi_reset_device_request,
     xdr_ndmp2_scsi_reset_device_reply,
   },
   { NDMP2_SCSI_RESET_BUS,
     xdr_ndmp2_scsi_reset_bus_request,
     xdr_ndmp2_scsi_reset_bus_reply,
   },
   { NDMP2_SCSI_EXECUTE_CDB,
     xdr_ndmp2_scsi_execute_cdb_request,
     xdr_ndmp2_scsi_execute_cdb_reply,
   },
   { NDMP2_TAPE_OPEN,
     xdr_ndmp2_tape_open_request,
     xdr_ndmp2_tape_open_reply,
   },
   { NDMP2_TAPE_CLOSE,
     xdr_ndmp2_tape_close_request,
     xdr_ndmp2_tape_close_reply,
   },
   { NDMP2_TAPE_GET_STATE,
     xdr_ndmp2_tape_get_state_request,
     xdr_ndmp2_tape_get_state_reply,
   },
   { NDMP2_TAPE_MTIO,
     xdr_ndmp2_tape_mtio_request,
     xdr_ndmp2_tape_mtio_reply,
   },
   { NDMP2_TAPE_WRITE,
     xdr_ndmp2_tape_write_request,
     xdr_ndmp2_tape_write_reply,
   },
   { NDMP2_TAPE_READ,
     xdr_ndmp2_tape_read_request,
     xdr_ndmp2_tape_read_reply,
   },
   { NDMP2_TAPE_EXECUTE_CDB,
     xdr_ndmp2_tape_execute_cdb_request,
     xdr_ndmp2_tape_execute_cdb_reply,
   },
   { NDMP2_DATA_GET_STATE,
     xdr_ndmp2_data_get_state_request,
     xdr_ndmp2_data_get_state_reply,
   },
   { NDMP2_DATA_START_BACKUP,
     xdr_ndmp2_data_start_backup_request,
     xdr_ndmp2_data_start_backup_reply,
   },
   { NDMP2_DATA_START_RECOVER,
     xdr_ndmp2_data_start_recover_request,
     xdr_ndmp2_data_start_recover_reply,
   },
   { NDMP2_DATA_ABORT,
     xdr_ndmp2_data_abort_request,
     xdr_ndmp2_data_abort_reply,
   },
   { NDMP2_DATA_GET_ENV,
     xdr_ndmp2_data_get_env_request,
     xdr_ndmp2_data_get_env_reply,
   },
   { NDMP2_DATA_STOP,
     xdr_ndmp2_data_stop_request,
     xdr_ndmp2_data_stop_reply,
   },
   { NDMP2_DATA_START_RECOVER_FILEHIST,
     xdr_ndmp2_data_start_recover_filehist_request,
     xdr_ndmp2_data_start_recover_filehist_reply,
   },
   { NDMP2_NOTIFY_DATA_HALTED,
     xdr_ndmp2_notify_data_halted_request,
     xdr_ndmp2_notify_data_halted_reply,
   },
   { NDMP2_NOTIFY_CONNECTED,
     xdr_ndmp2_notify_connected_request,
     xdr_ndmp2_notify_connected_reply,
   },
   { NDMP2_NOTIFY_MOVER_HALTED,
     xdr_ndmp2_notify_mover_halted_request,
     xdr_ndmp2_notify_mover_halted_reply,
   },
   { NDMP2_NOTIFY_MOVER_PAUSED,
     xdr_ndmp2_notify_mover_paused_request,
     xdr_ndmp2_notify_mover_paused_reply,
   },
   { NDMP2_NOTIFY_DATA_READ,
     xdr_ndmp2_notify_data_read_request,
     xdr_ndmp2_notify_data_read_reply,
   },
   { NDMP2_LOG_LOG,
     xdr_ndmp2_log_log_request,
     xdr_ndmp2_log_log_reply,
   },
   { NDMP2_LOG_DEBUG,
     xdr_ndmp2_log_debug_request,
     xdr_ndmp2_log_debug_reply,
   },
   { NDMP2_LOG_FILE,
     xdr_ndmp2_log_file_request,
     xdr_ndmp2_log_file_reply,
   },
   { NDMP2_FH_ADD_UNIX_PATH,
     xdr_ndmp2_fh_add_unix_path_request,
     xdr_ndmp2_fh_add_unix_path_reply,
   },
   { NDMP2_FH_ADD_UNIX_DIR,
     xdr_ndmp2_fh_add_unix_dir_request,
     xdr_ndmp2_fh_add_unix_dir_reply,
   },
   { NDMP2_FH_ADD_UNIX_NODE,
     xdr_ndmp2_fh_add_unix_node_request,
     xdr_ndmp2_fh_add_unix_node_reply,
   },
   { NDMP2_MOVER_GET_STATE,
     xdr_ndmp2_mover_get_state_request,
     xdr_ndmp2_mover_get_state_reply,
   },
   { NDMP2_MOVER_LISTEN,
     xdr_ndmp2_mover_listen_request,
     xdr_ndmp2_mover_listen_reply,
   },
   { NDMP2_MOVER_CONTINUE,
     xdr_ndmp2_mover_continue_request,
     xdr_ndmp2_mover_continue_reply,
   },
   { NDMP2_MOVER_ABORT,
     xdr_ndmp2_mover_abort_request,
     xdr_ndmp2_mover_abort_reply,
   },
   { NDMP2_MOVER_STOP,
     xdr_ndmp2_mover_stop_request,
     xdr_ndmp2_mover_stop_reply,
   },
   { NDMP2_MOVER_SET_WINDOW,
     xdr_ndmp2_mover_set_window_request,
     xdr_ndmp2_mover_set_window_reply,
   },
   { NDMP2_MOVER_READ,
     xdr_ndmp2_mover_read_request,
     xdr_ndmp2_mover_read_reply,
   },
   { NDMP2_MOVER_CLOSE,
     xdr_ndmp2_mover_close_request,
     xdr_ndmp2_mover_close_reply,
   },
   { NDMP2_MOVER_SET_RECORD_SIZE,
     xdr_ndmp2_mover_set_record_size_request,
     xdr_ndmp2_mover_set_record_size_reply,
   },
   {0}
};



/*
 * XDR unsigned long integers
 * same as xdr_long - open coded to save a proc call!
 */
bool_t
xdr_ndmp2_u_quad(register XDR *xdrs, ndmp2_u_quad *objp)
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

#endif /* !NDMOS_OPTION_NO_NDMP2 */
