/*
 * Copyright (c) 2000
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


#include "ndmos.h"		/* rpc/rpc.h */
#include "ndmprotocol.h"


#ifndef NDMOS_OPTION_NO_NDMP3


extern struct ndmp_enum_str_table ndmp3_error_table[];
extern char * ndmp3_error_to_str (ndmp3_error val);
extern int    ndmp3_error_from_str (ndmp3_error *valp, char *str);

char * ndmp3_error_to_str (ndmp3_error val) {
	return ndmp_enum_to_str ((int)val, ndmp3_error_table);
}

int ndmp3_error_from_str (ndmp3_error *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_error_table);
}

struct ndmp_enum_str_table ndmp3_error_table[] = {
	{ "NDMP3_NO_ERR",		NDMP3_NO_ERR, },
	{ "NDMP3_NOT_SUPPORTED_ERR",	NDMP3_NOT_SUPPORTED_ERR, },
	{ "NDMP3_DEVICE_BUSY_ERR",	NDMP3_DEVICE_BUSY_ERR, },
	{ "NDMP3_DEVICE_OPENED_ERR",	NDMP3_DEVICE_OPENED_ERR, },
	{ "NDMP3_NOT_AUTHORIZED_ERR",	NDMP3_NOT_AUTHORIZED_ERR, },
	{ "NDMP3_PERMISSION_ERR",	NDMP3_PERMISSION_ERR, },
	{ "NDMP3_DEV_NOT_OPEN_ERR",	NDMP3_DEV_NOT_OPEN_ERR, },
	{ "NDMP3_IO_ERR",		NDMP3_IO_ERR, },
	{ "NDMP3_TIMEOUT_ERR",		NDMP3_TIMEOUT_ERR, },
	{ "NDMP3_ILLEGAL_ARGS_ERR",	NDMP3_ILLEGAL_ARGS_ERR, },
	{ "NDMP3_NO_TAPE_LOADED_ERR",	NDMP3_NO_TAPE_LOADED_ERR, },
	{ "NDMP3_WRITE_PROTECT_ERR",	NDMP3_WRITE_PROTECT_ERR, },
	{ "NDMP3_EOF_ERR",		NDMP3_EOF_ERR, },
	{ "NDMP3_EOM_ERR",		NDMP3_EOM_ERR, },
	{ "NDMP3_FILE_NOT_FOUND_ERR",	NDMP3_FILE_NOT_FOUND_ERR, },
	{ "NDMP3_BAD_FILE_ERR",		NDMP3_BAD_FILE_ERR, },
	{ "NDMP3_NO_DEVICE_ERR",	NDMP3_NO_DEVICE_ERR, },
	{ "NDMP3_NO_BUS_ERR",		NDMP3_NO_BUS_ERR, },
	{ "NDMP3_XDR_DECODE_ERR",	NDMP3_XDR_DECODE_ERR, },
	{ "NDMP3_ILLEGAL_STATE_ERR",	NDMP3_ILLEGAL_STATE_ERR, },
	{ "NDMP3_UNDEFINED_ERR",	NDMP3_UNDEFINED_ERR, },
	{ "NDMP3_XDR_ENCODE_ERR",	NDMP3_XDR_ENCODE_ERR, },
	{ "NDMP3_NO_MEM_ERR",		NDMP3_NO_MEM_ERR, },
	{ "NDMP3_CONNECT_ERR",		NDMP3_CONNECT_ERR, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_header_message_type_table[];
extern char * ndmp3_header_message_type_to_str (ndmp3_header_message_type val);
extern int    ndmp3_header_message_type_from_str (ndmp3_header_message_type *valp, char *str);

char * ndmp3_header_message_type_to_str (ndmp3_header_message_type val) {
	return ndmp_enum_to_str ((int)val, ndmp3_header_message_type_table);
}

int ndmp3_header_message_type_from_str (ndmp3_header_message_type *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_header_message_type_table);
}

struct ndmp_enum_str_table ndmp3_header_message_type_table[] = {
	{ "NDMP3_MESSAGE_REQUEST",	NDMP3_MESSAGE_REQUEST, },
	{ "NDMP3_MESSAGE_REPLY",	NDMP3_MESSAGE_REPLY, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_message_table[];
extern char * ndmp3_message_to_str (ndmp3_message val);
extern int    ndmp3_message_from_str (ndmp3_message *valp, char *str);

char * ndmp3_message_to_str (ndmp3_message val) {
	return ndmp_enum_to_str ((int)val, ndmp3_message_table);
}

int ndmp3_message_from_str (ndmp3_message *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_message_table);
}

struct ndmp_enum_str_table ndmp3_message_table[] = {
	{ "NDMP3_CONNECT_OPEN",		NDMP3_CONNECT_OPEN, },
	{ "NDMP3_CONNECT_CLIENT_AUTH",	NDMP3_CONNECT_CLIENT_AUTH, },
	{ "NDMP3_CONNECT_CLOSE",	NDMP3_CONNECT_CLOSE, },
	{ "NDMP3_CONNECT_SERVER_AUTH",	NDMP3_CONNECT_SERVER_AUTH, },
	{ "NDMP3_CONFIG_GET_HOST_INFO",	NDMP3_CONFIG_GET_HOST_INFO, },
	{ "NDMP3_CONFIG_GET_CONNECTION_TYPE",
					NDMP3_CONFIG_GET_CONNECTION_TYPE, },
	{ "NDMP3_CONFIG_GET_AUTH_ATTR",	NDMP3_CONFIG_GET_AUTH_ATTR, },
	{ "NDMP3_CONFIG_GET_BUTYPE_INFO", NDMP3_CONFIG_GET_BUTYPE_INFO, },
	{ "NDMP3_CONFIG_GET_FS_INFO",	NDMP3_CONFIG_GET_FS_INFO, },
	{ "NDMP3_CONFIG_GET_TAPE_INFO",	NDMP3_CONFIG_GET_TAPE_INFO, },
	{ "NDMP3_CONFIG_GET_SCSI_INFO",	NDMP3_CONFIG_GET_SCSI_INFO, },
	{ "NDMP3_CONFIG_GET_SERVER_INFO",NDMP3_CONFIG_GET_SERVER_INFO, },
	{ "NDMP3_SCSI_OPEN",		NDMP3_SCSI_OPEN, },
	{ "NDMP3_SCSI_CLOSE",		NDMP3_SCSI_CLOSE, },
	{ "NDMP3_SCSI_GET_STATE",	NDMP3_SCSI_GET_STATE, },
	{ "NDMP3_SCSI_SET_TARGET",	NDMP3_SCSI_SET_TARGET, },
	{ "NDMP3_SCSI_RESET_DEVICE",	NDMP3_SCSI_RESET_DEVICE, },
	{ "NDMP3_SCSI_RESET_BUS",	NDMP3_SCSI_RESET_BUS, },
	{ "NDMP3_SCSI_EXECUTE_CDB",	NDMP3_SCSI_EXECUTE_CDB, },
	{ "NDMP3_TAPE_OPEN",		NDMP3_TAPE_OPEN, },
	{ "NDMP3_TAPE_CLOSE",		NDMP3_TAPE_CLOSE, },
	{ "NDMP3_TAPE_GET_STATE",	NDMP3_TAPE_GET_STATE, },
	{ "NDMP3_TAPE_MTIO",		NDMP3_TAPE_MTIO, },
	{ "NDMP3_TAPE_WRITE",		NDMP3_TAPE_WRITE, },
	{ "NDMP3_TAPE_READ",		NDMP3_TAPE_READ, },
	{ "NDMP3_TAPE_EXECUTE_CDB",	NDMP3_TAPE_EXECUTE_CDB, },
	{ "NDMP3_DATA_GET_STATE",	NDMP3_DATA_GET_STATE, },
	{ "NDMP3_DATA_START_BACKUP",	NDMP3_DATA_START_BACKUP, },
	{ "NDMP3_DATA_START_RECOVER",	NDMP3_DATA_START_RECOVER, },
	{ "NDMP3_DATA_START_RECOVER_FILEHIST",
					NDMP3_DATA_START_RECOVER_FILEHIST, },
	{ "NDMP3_DATA_ABORT",		NDMP3_DATA_ABORT, },
	{ "NDMP3_DATA_GET_ENV",		NDMP3_DATA_GET_ENV, },
	{ "NDMP3_DATA_STOP",		NDMP3_DATA_STOP, },
	{ "NDMP3_DATA_LISTEN",		NDMP3_DATA_LISTEN, },
	{ "NDMP3_DATA_CONNECT",		NDMP3_DATA_CONNECT, },
	{ "NDMP3_NOTIFY_DATA_HALTED",	NDMP3_NOTIFY_DATA_HALTED, },
	{ "NDMP3_NOTIFY_CONNECTED",	NDMP3_NOTIFY_CONNECTED, },
	{ "NDMP3_NOTIFY_MOVER_HALTED",	NDMP3_NOTIFY_MOVER_HALTED, },
	{ "NDMP3_NOTIFY_MOVER_PAUSED",	NDMP3_NOTIFY_MOVER_PAUSED, },
	{ "NDMP3_NOTIFY_DATA_READ",	NDMP3_NOTIFY_DATA_READ, },
	{ "NDMP3_LOG_FILE",		NDMP3_LOG_FILE, },
	{ "NDMP3_LOG_MESSAGE",		NDMP3_LOG_MESSAGE, },
	{ "NDMP3_FH_ADD_FILE",		NDMP3_FH_ADD_FILE, },
	{ "NDMP3_FH_ADD_DIR",		NDMP3_FH_ADD_DIR, },
	{ "NDMP3_FH_ADD_NODE",		NDMP3_FH_ADD_NODE, },
	{ "NDMP3_MOVER_GET_STATE",	NDMP3_MOVER_GET_STATE, },
	{ "NDMP3_MOVER_LISTEN",		NDMP3_MOVER_LISTEN, },
	{ "NDMP3_MOVER_CONTINUE",	NDMP3_MOVER_CONTINUE, },
	{ "NDMP3_MOVER_ABORT",		NDMP3_MOVER_ABORT, },
	{ "NDMP3_MOVER_STOP",		NDMP3_MOVER_STOP, },
	{ "NDMP3_MOVER_SET_WINDOW",	NDMP3_MOVER_SET_WINDOW, },
	{ "NDMP3_MOVER_READ",		NDMP3_MOVER_READ, },
	{ "NDMP3_MOVER_CLOSE",		NDMP3_MOVER_CLOSE, },
	{ "NDMP3_MOVER_SET_RECORD_SIZE", NDMP3_MOVER_SET_RECORD_SIZE, },
	{ "NDMP3_MOVER_CONNECT",	NDMP3_MOVER_CONNECT, },
	{ "NDMP3_VENDORS_BASE",		NDMP3_VENDORS_BASE, },
	{ "NDMP3_RESERVED_BASE",	NDMP3_RESERVED_BASE, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_auth_type_table[];
extern char * ndmp3_auth_type_to_str (ndmp3_auth_type val);
extern int    ndmp3_auth_type_from_str (ndmp3_auth_type *valp, char *str);

char * ndmp3_auth_type_to_str (ndmp3_auth_type val) {
	return ndmp_enum_to_str ((int)val, ndmp3_auth_type_table);
}

int ndmp3_auth_type_from_str (ndmp3_auth_type *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_auth_type_table);
}

struct ndmp_enum_str_table ndmp3_auth_type_table[] = {
	{ "NDMP3_AUTH_NONE",		NDMP3_AUTH_NONE, },
	{ "NDMP3_AUTH_TEXT",		NDMP3_AUTH_TEXT, },
	{ "NDMP3_AUTH_MD5",		NDMP3_AUTH_MD5, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_addr_type_table[];
extern char * ndmp3_addr_type_to_str (ndmp3_addr_type val);
extern int    ndmp3_addr_type_from_str (ndmp3_addr_type *valp, char *str);

char * ndmp3_addr_type_to_str (ndmp3_addr_type val) {
	return ndmp_enum_to_str ((int)val, ndmp3_addr_type_table);
}

int ndmp3_addr_type_from_str (ndmp3_addr_type *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_addr_type_table);
}

struct ndmp_enum_str_table ndmp3_addr_type_table[] = {
	{ "NDMP3_ADDR_LOCAL",		NDMP3_ADDR_LOCAL, },
	{ "NDMP3_ADDR_TCP",		NDMP3_ADDR_TCP, },
	{ "NDMP3_ADDR_FC",		NDMP3_ADDR_FC, },
	{ "NDMP3_ADDR_IPC",		NDMP3_ADDR_IPC, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_tape_open_mode_table[];
extern char * ndmp3_tape_open_mode_to_str (ndmp3_tape_open_mode val);
extern int    ndmp3_tape_open_mode_from_str (ndmp3_tape_open_mode *valp, char *str);

char * ndmp3_tape_open_mode_to_str (ndmp3_tape_open_mode val) {
	return ndmp_enum_to_str ((int)val, ndmp3_tape_open_mode_table);
}

int ndmp3_tape_open_mode_from_str (ndmp3_tape_open_mode *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_tape_open_mode_table);
}

struct ndmp_enum_str_table ndmp3_tape_open_mode_table[] = {
	{ "NDMP3_TAPE_READ_MODE",	NDMP3_TAPE_READ_MODE, },
	{ "NDMP3_TAPE_RDWR_MODE",	NDMP3_TAPE_RDWR_MODE, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_tape_mtio_op_table[];
extern char * ndmp3_tape_mtio_op_to_str (ndmp3_tape_mtio_op val);
extern int    ndmp3_tape_mtio_op_from_str (ndmp3_tape_mtio_op *valp, char *str);

char * ndmp3_tape_mtio_op_to_str (ndmp3_tape_mtio_op val) {
	return ndmp_enum_to_str ((int)val, ndmp3_tape_mtio_op_table);
}

int ndmp3_tape_mtio_op_from_str (ndmp3_tape_mtio_op *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_tape_mtio_op_table);
}

struct ndmp_enum_str_table ndmp3_tape_mtio_op_table[] = {
	{ "NDMP3_MTIO_FSF",		NDMP3_MTIO_FSF, },
	{ "NDMP3_MTIO_BSF",		NDMP3_MTIO_BSF, },
	{ "NDMP3_MTIO_FSR",		NDMP3_MTIO_FSR, },
	{ "NDMP3_MTIO_BSR",		NDMP3_MTIO_BSR, },
	{ "NDMP3_MTIO_REW",		NDMP3_MTIO_REW, },
	{ "NDMP3_MTIO_EOF",		NDMP3_MTIO_EOF, },
	{ "NDMP3_MTIO_OFF",		NDMP3_MTIO_OFF, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_mover_state_table[];
extern char * ndmp3_mover_state_to_str (ndmp3_mover_state val);
extern int    ndmp3_mover_state_from_str (ndmp3_mover_state *valp, char *str);

char * ndmp3_mover_state_to_str (ndmp3_mover_state val) {
	return ndmp_enum_to_str ((int)val, ndmp3_mover_state_table);
}

int ndmp3_mover_state_from_str (ndmp3_mover_state *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_mover_state_table);
}

struct ndmp_enum_str_table ndmp3_mover_state_table[] = {
	{ "NDMP3_MOVER_STATE_IDLE",	NDMP3_MOVER_STATE_IDLE, },
	{ "NDMP3_MOVER_STATE_LISTEN",	NDMP3_MOVER_STATE_LISTEN, },
	{ "NDMP3_MOVER_STATE_ACTIVE",	NDMP3_MOVER_STATE_ACTIVE, },
	{ "NDMP3_MOVER_STATE_PAUSED",	NDMP3_MOVER_STATE_PAUSED, },
	{ "NDMP3_MOVER_STATE_HALTED",	NDMP3_MOVER_STATE_HALTED, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_mover_pause_reason_table[];
extern char * ndmp3_mover_pause_reason_to_str (ndmp3_mover_pause_reason val);
extern int    ndmp3_mover_pause_reason_from_str (ndmp3_mover_pause_reason *valp, char *str);

char * ndmp3_mover_pause_reason_to_str (ndmp3_mover_pause_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp3_mover_pause_reason_table);
}

int ndmp3_mover_pause_reason_from_str (ndmp3_mover_pause_reason *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_mover_pause_reason_table);
}

struct ndmp_enum_str_table ndmp3_mover_pause_reason_table[] = {
	{ "NDMP3_MOVER_PAUSE_NA",	NDMP3_MOVER_PAUSE_NA, },
	{ "NDMP3_MOVER_PAUSE_EOM",	NDMP3_MOVER_PAUSE_EOM, },
	{ "NDMP3_MOVER_PAUSE_EOF",	NDMP3_MOVER_PAUSE_EOF, },
	{ "NDMP3_MOVER_PAUSE_SEEK",	NDMP3_MOVER_PAUSE_SEEK, },
	{ "NDMP3_MOVER_PAUSE_MEDIA_ERROR", NDMP3_MOVER_PAUSE_MEDIA_ERROR, },
	{ "NDMP3_MOVER_PAUSE_EOW",	NDMP3_MOVER_PAUSE_EOW, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_mover_halt_reason_table[];
extern char * ndmp3_mover_halt_reason_to_str (ndmp3_mover_halt_reason val);
extern int    ndmp3_mover_halt_reason_from_str (ndmp3_mover_halt_reason *valp, char *str);

char * ndmp3_mover_halt_reason_to_str (ndmp3_mover_halt_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp3_mover_halt_reason_table);
}

int ndmp3_mover_halt_reason_from_str (ndmp3_mover_halt_reason *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_mover_halt_reason_table);
}

struct ndmp_enum_str_table ndmp3_mover_halt_reason_table[] = {
	{ "NDMP3_MOVER_HALT_NA",	NDMP3_MOVER_HALT_NA, },
	{ "NDMP3_MOVER_HALT_CONNECT_CLOSED",
					NDMP3_MOVER_HALT_CONNECT_CLOSED, },
	{ "NDMP3_MOVER_HALT_ABORTED",	NDMP3_MOVER_HALT_ABORTED, },
	{ "NDMP3_MOVER_HALT_INTERNAL_ERROR",
					NDMP3_MOVER_HALT_INTERNAL_ERROR, },
	{ "NDMP3_MOVER_HALT_CONNECT_ERROR",
					NDMP3_MOVER_HALT_CONNECT_ERROR, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_mover_mode_table[];
extern char * ndmp3_mover_mode_to_str (ndmp3_mover_mode val);
extern int    ndmp3_mover_mode_from_str (ndmp3_mover_mode *valp, char *str);

char * ndmp3_mover_mode_to_str (ndmp3_mover_mode val) {
	return ndmp_enum_to_str ((int)val, ndmp3_mover_mode_table);
}

int ndmp3_mover_mode_from_str (ndmp3_mover_mode *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_mover_mode_table);
}

struct ndmp_enum_str_table ndmp3_mover_mode_table[] = {
	{ "NDMP3_MOVER_MODE_READ",	NDMP3_MOVER_MODE_READ, },
	{ "NDMP3_MOVER_MODE_WRITE",	NDMP3_MOVER_MODE_WRITE, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_data_operation_table[];
extern char * ndmp3_data_operation_to_str (ndmp3_data_operation val);
extern int    ndmp3_data_operation_from_str (ndmp3_data_operation *valp, char *str);

char * ndmp3_data_operation_to_str (ndmp3_data_operation val) {
	return ndmp_enum_to_str ((int)val, ndmp3_data_operation_table);
}

int ndmp3_data_operation_from_str (ndmp3_data_operation *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_data_operation_table);
}

struct ndmp_enum_str_table ndmp3_data_operation_table[] = {
	{ "NDMP3_DATA_OP_NOACTION",	NDMP3_DATA_OP_NOACTION, },
	{ "NDMP3_DATA_OP_BACKUP",	NDMP3_DATA_OP_BACKUP, },
	{ "NDMP3_DATA_OP_RESTORE",	NDMP3_DATA_OP_RESTORE, },
	{ "NDMP3_DATA_OP_RESTORE_FILEHIST",
					NDMP3_DATA_OP_RESTORE_FILEHIST, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_data_state_table[];
extern char * ndmp3_data_state_to_str (ndmp3_data_state val);
extern int    ndmp3_data_state_from_str (ndmp3_data_state *valp, char *str);

char * ndmp3_data_state_to_str (ndmp3_data_state val) {
	return ndmp_enum_to_str ((int)val, ndmp3_data_state_table);
}

int ndmp3_data_state_from_str (ndmp3_data_state *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_data_state_table);
}

struct ndmp_enum_str_table ndmp3_data_state_table[] = {
	{ "NDMP3_DATA_STATE_IDLE",	NDMP3_DATA_STATE_IDLE, },
	{ "NDMP3_DATA_STATE_ACTIVE",	NDMP3_DATA_STATE_ACTIVE, },
	{ "NDMP3_DATA_STATE_HALTED",	NDMP3_DATA_STATE_HALTED, },
	{ "NDMP3_DATA_STATE_LISTEN",	NDMP3_DATA_STATE_LISTEN, },
	{ "NDMP3_DATA_STATE_CONNECTED",	NDMP3_DATA_STATE_CONNECTED, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_data_halt_reason_table[];
extern char * ndmp3_data_halt_reason_to_str (ndmp3_data_halt_reason val);
extern int    ndmp3_data_halt_reason_from_str (ndmp3_data_halt_reason *valp, char *str);

char * ndmp3_data_halt_reason_to_str (ndmp3_data_halt_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp3_data_halt_reason_table);
}

int ndmp3_data_halt_reason_from_str (ndmp3_data_halt_reason *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_data_halt_reason_table);
}

struct ndmp_enum_str_table ndmp3_data_halt_reason_table[] = {
	{ "NDMP3_DATA_HALT_NA",		NDMP3_DATA_HALT_NA, },
	{ "NDMP3_DATA_HALT_SUCCESSFUL",	NDMP3_DATA_HALT_SUCCESSFUL, },
	{ "NDMP3_DATA_HALT_ABORTED",	NDMP3_DATA_HALT_ABORTED, },
	{ "NDMP3_DATA_HALT_INTERNAL_ERROR", NDMP3_DATA_HALT_INTERNAL_ERROR, },
	{ "NDMP3_DATA_HALT_CONNECT_ERROR", NDMP3_DATA_HALT_CONNECT_ERROR, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_connect_reason_table[];
extern char * ndmp3_connect_reason_to_str (ndmp3_connect_reason val);
extern int    ndmp3_connect_reason_from_str (ndmp3_connect_reason *valp, char *str);

char * ndmp3_connect_reason_to_str (ndmp3_connect_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp3_connect_reason_table);
}

int ndmp3_connect_reason_from_str (ndmp3_connect_reason *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_connect_reason_table);
}

struct ndmp_enum_str_table ndmp3_connect_reason_table[] = {
	{ "NDMP3_CONNECTED",		NDMP3_CONNECTED, },
	{ "NDMP3_SHUTDOWN",		NDMP3_SHUTDOWN, },
	{ "NDMP3_REFUSED",		NDMP3_REFUSED, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_log_type_table[];
extern char * ndmp3_log_type_to_str (ndmp3_log_type val);
extern int    ndmp3_log_type_from_str (ndmp3_log_type *valp, char *str);

char * ndmp3_log_type_to_str (ndmp3_log_type val) {
	return ndmp_enum_to_str ((int)val, ndmp3_log_type_table);
}

int ndmp3_log_type_from_str (ndmp3_log_type *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_log_type_table);
}

struct ndmp_enum_str_table ndmp3_log_type_table[] = {
	{ "NDMP3_LOG_NORMAL",		NDMP3_LOG_NORMAL, },
	{ "NDMP3_LOG_DEBUG",		NDMP3_LOG_DEBUG, },
	{ "NDMP3_LOG_ERROR",		NDMP3_LOG_ERROR, },
	{ "NDMP3_LOG_WARNING",		NDMP3_LOG_WARNING, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_fs_type_table[];
extern char * ndmp3_fs_type_to_str (ndmp3_fs_type val);
extern int    ndmp3_fs_type_from_str (ndmp3_fs_type *valp, char *str);

char * ndmp3_fs_type_to_str (ndmp3_fs_type val) {
	return ndmp_enum_to_str ((int)val, ndmp3_fs_type_table);
}

int ndmp3_fs_type_from_str (ndmp3_fs_type *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_fs_type_table);
}

struct ndmp_enum_str_table ndmp3_fs_type_table[] = {
	{ "NDMP3_FS_UNIX",		NDMP3_FS_UNIX, },
	{ "NDMP3_FS_NT",		NDMP3_FS_NT, },
	{ "NDMP3_FS_OTHER",		NDMP3_FS_OTHER, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp3_file_type_table[];
extern char * ndmp3_file_type_to_str (ndmp3_file_type val);
extern int    ndmp3_file_type_from_str (ndmp3_file_type *valp, char *str);

char * ndmp3_file_type_to_str (ndmp3_file_type val) {
	return ndmp_enum_to_str ((int)val, ndmp3_file_type_table);
}

int ndmp3_file_type_from_str (ndmp3_file_type *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp3_file_type_table);
}

struct ndmp_enum_str_table ndmp3_file_type_table[] = {
	{ "NDMP3_FILE_DIR",		NDMP3_FILE_DIR, },
	{ "NDMP3_FILE_FIFO",		NDMP3_FILE_FIFO, },
	{ "NDMP3_FILE_CSPEC",		NDMP3_FILE_CSPEC, },
	{ "NDMP3_FILE_BSPEC",		NDMP3_FILE_BSPEC, },
	{ "NDMP3_FILE_REG",		NDMP3_FILE_REG, },
	{ "NDMP3_FILE_SLINK",		NDMP3_FILE_SLINK, },
	{ "NDMP3_FILE_SOCK",		NDMP3_FILE_SOCK, },
	{ "NDMP3_FILE_REGISTRY",	NDMP3_FILE_REGISTRY, },
	{ "NDMP3_FILE_OTHER",		NDMP3_FILE_OTHER, },
	{ 0 }
};

#endif /* !NDMOS_OPTION_NO_NDMP3 */
