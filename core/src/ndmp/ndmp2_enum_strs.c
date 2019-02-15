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


#include "ndmos.h"		/* rpc/rpc.h */
#include "ndmprotocol.h"


#ifndef NDMOS_OPTION_NO_NDMP2


extern struct ndmp_enum_str_table ndmp2_error_table[];
extern char * ndmp2_error_to_str (ndmp2_error val);
extern int    ndmp2_error_from_str (ndmp2_error *valp, char * str);

char * ndmp2_error_to_str (ndmp2_error val) {
	return ndmp_enum_to_str ((int)val, ndmp2_error_table);
}

int ndmp2_error_from_str (ndmp2_error *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_error_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_error_table[] = {
	{ "NDMP2_NO_ERR",		NDMP2_NO_ERR },
	{ "NDMP2_NOT_SUPPORTED_ERR",	NDMP2_NOT_SUPPORTED_ERR },
	{ "NDMP2_DEVICE_BUSY_ERR",	NDMP2_DEVICE_BUSY_ERR },
	{ "NDMP2_DEVICE_OPENED_ERR",	NDMP2_DEVICE_OPENED_ERR },
	{ "NDMP2_NOT_AUTHORIZED_ERR",	NDMP2_NOT_AUTHORIZED_ERR },
	{ "NDMP2_PERMISSION_ERR",	NDMP2_PERMISSION_ERR },
	{ "NDMP2_DEV_NOT_OPEN_ERR",	NDMP2_DEV_NOT_OPEN_ERR },
	{ "NDMP2_IO_ERR",		NDMP2_IO_ERR },
	{ "NDMP2_TIMEOUT_ERR",		NDMP2_TIMEOUT_ERR },
	{ "NDMP2_ILLEGAL_ARGS_ERR",	NDMP2_ILLEGAL_ARGS_ERR },
	{ "NDMP2_NO_TAPE_LOADED_ERR",	NDMP2_NO_TAPE_LOADED_ERR },
	{ "NDMP2_WRITE_PROTECT_ERR",	NDMP2_WRITE_PROTECT_ERR },
	{ "NDMP2_EOF_ERR",		NDMP2_EOF_ERR },
	{ "NDMP2_EOM_ERR",		NDMP2_EOM_ERR },
	{ "NDMP2_FILE_NOT_FOUND_ERR",	NDMP2_FILE_NOT_FOUND_ERR },
	{ "NDMP2_BAD_FILE_ERR",		NDMP2_BAD_FILE_ERR },
	{ "NDMP2_NO_DEVICE_ERR",	NDMP2_NO_DEVICE_ERR },
	{ "NDMP2_NO_BUS_ERR",		NDMP2_NO_BUS_ERR },
	{ "NDMP2_XDR_DECODE_ERR",	NDMP2_XDR_DECODE_ERR },
	{ "NDMP2_ILLEGAL_STATE_ERR",	NDMP2_ILLEGAL_STATE_ERR },
	{ "NDMP2_UNDEFINED_ERR",	NDMP2_UNDEFINED_ERR },
	{ "NDMP2_XDR_ENCODE_ERR",	NDMP2_XDR_ENCODE_ERR },
	{ "NDMP2_NO_MEM_ERR",		NDMP2_NO_MEM_ERR },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_header_message_type_table[];
extern char * ndmp2_header_message_type_to_str (ndmp2_header_message_type val);
extern int    ndmp2_header_message_type_from_str (ndmp2_header_message_type *valp, char * str);

char * ndmp2_header_message_type_to_str (ndmp2_header_message_type val) {
	return ndmp_enum_to_str ((int)val, ndmp2_header_message_type_table);
}

int ndmp2_header_message_type_from_str (ndmp2_header_message_type *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_header_message_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_header_message_type_table[] = {
	{ "NDMP2_MESSAGE_REQUEST",	NDMP2_MESSAGE_REQUEST },
	{ "NDMP2_MESSAGE_REPLY",	NDMP2_MESSAGE_REPLY },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_message_table[];
extern char * ndmp2_message_to_str (ndmp2_message val);
extern int    ndmp2_message_from_str (ndmp2_message *valp, char * str);

char * ndmp2_message_to_str (ndmp2_message val) {
	return ndmp_enum_to_str ((int)val, ndmp2_message_table);
}

int ndmp2_message_from_str (ndmp2_message *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_message_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_message_table[] = {
	{ "NDMP2_CONNECT_OPEN",		NDMP2_CONNECT_OPEN },
	{ "NDMP2_CONNECT_CLIENT_AUTH",	NDMP2_CONNECT_CLIENT_AUTH },
	{ "NDMP2_CONNECT_CLOSE",	NDMP2_CONNECT_CLOSE },
	{ "NDMP2_CONNECT_SERVER_AUTH",	NDMP2_CONNECT_SERVER_AUTH },
	{ "NDMP2_CONFIG_GET_HOST_INFO",	NDMP2_CONFIG_GET_HOST_INFO },
	{ "NDMP2_CONFIG_GET_BUTYPE_ATTR", NDMP2_CONFIG_GET_BUTYPE_ATTR },
	{ "NDMP2_CONFIG_GET_MOVER_TYPE", NDMP2_CONFIG_GET_MOVER_TYPE },
	{ "NDMP2_CONFIG_GET_AUTH_ATTR",	NDMP2_CONFIG_GET_AUTH_ATTR },
	{ "NDMP2_SCSI_OPEN",		NDMP2_SCSI_OPEN },
	{ "NDMP2_SCSI_CLOSE",		NDMP2_SCSI_CLOSE },
	{ "NDMP2_SCSI_GET_STATE",	NDMP2_SCSI_GET_STATE },
	{ "NDMP2_SCSI_SET_TARGET",	NDMP2_SCSI_SET_TARGET },
	{ "NDMP2_SCSI_RESET_DEVICE",	NDMP2_SCSI_RESET_DEVICE },
	{ "NDMP2_SCSI_RESET_BUS",	NDMP2_SCSI_RESET_BUS },
	{ "NDMP2_SCSI_EXECUTE_CDB",	NDMP2_SCSI_EXECUTE_CDB },
	{ "NDMP2_TAPE_OPEN",		NDMP2_TAPE_OPEN },
	{ "NDMP2_TAPE_CLOSE",		NDMP2_TAPE_CLOSE },
	{ "NDMP2_TAPE_GET_STATE",	NDMP2_TAPE_GET_STATE },
	{ "NDMP2_TAPE_MTIO",		NDMP2_TAPE_MTIO },
	{ "NDMP2_TAPE_WRITE",		NDMP2_TAPE_WRITE },
	{ "NDMP2_TAPE_READ",		NDMP2_TAPE_READ },
	{ "NDMP2_TAPE_RESVD1",		NDMP2_TAPE_RESVD1 },
	{ "NDMP2_TAPE_EXECUTE_CDB",	NDMP2_TAPE_EXECUTE_CDB },
	{ "NDMP2_DATA_GET_STATE",	NDMP2_DATA_GET_STATE },
	{ "NDMP2_DATA_START_BACKUP",	NDMP2_DATA_START_BACKUP },
	{ "NDMP2_DATA_START_RECOVER",	NDMP2_DATA_START_RECOVER },
	{ "NDMP2_DATA_ABORT",		NDMP2_DATA_ABORT },
	{ "NDMP2_DATA_GET_ENV",		NDMP2_DATA_GET_ENV },
	{ "NDMP2_DATA_RESVD1",		NDMP2_DATA_RESVD1 },
	{ "NDMP2_DATA_RESVD2",		NDMP2_DATA_RESVD2 },
	{ "NDMP2_DATA_STOP",		NDMP2_DATA_STOP },
	{ "NDMP2_DATA_START_RECOVER_FILEHIST",
					NDMP2_DATA_START_RECOVER_FILEHIST },
	{ "NDMP2_NOTIFY_RESVD1",	NDMP2_NOTIFY_RESVD1 },
	{ "NDMP2_NOTIFY_DATA_HALTED",	NDMP2_NOTIFY_DATA_HALTED },
	{ "NDMP2_NOTIFY_CONNECTED",	NDMP2_NOTIFY_CONNECTED },
	{ "NDMP2_NOTIFY_MOVER_HALTED",	NDMP2_NOTIFY_MOVER_HALTED },
	{ "NDMP2_NOTIFY_MOVER_PAUSED",	NDMP2_NOTIFY_MOVER_PAUSED },
	{ "NDMP2_NOTIFY_DATA_READ",	NDMP2_NOTIFY_DATA_READ },
	{ "NDMP2_LOG_LOG",		NDMP2_LOG_LOG },
	{ "NDMP2_LOG_DEBUG",		NDMP2_LOG_DEBUG },
	{ "NDMP2_LOG_FILE",		NDMP2_LOG_FILE },
	{ "NDMP2_FH_ADD_UNIX_PATH",	NDMP2_FH_ADD_UNIX_PATH },
	{ "NDMP2_FH_ADD_UNIX_DIR",	NDMP2_FH_ADD_UNIX_DIR },
	{ "NDMP2_FH_ADD_UNIX_NODE",	NDMP2_FH_ADD_UNIX_NODE },
	{ "NDMP2_MOVER_GET_STATE",	NDMP2_MOVER_GET_STATE },
	{ "NDMP2_MOVER_LISTEN",		NDMP2_MOVER_LISTEN },
	{ "NDMP2_MOVER_CONTINUE",	NDMP2_MOVER_CONTINUE },
	{ "NDMP2_MOVER_ABORT",		NDMP2_MOVER_ABORT },
	{ "NDMP2_MOVER_STOP",		NDMP2_MOVER_STOP },
	{ "NDMP2_MOVER_SET_WINDOW",	NDMP2_MOVER_SET_WINDOW },
	{ "NDMP2_MOVER_READ",		NDMP2_MOVER_READ },
	{ "NDMP2_MOVER_CLOSE",		NDMP2_MOVER_CLOSE },
	{ "NDMP2_MOVER_SET_RECORD_SIZE", NDMP2_MOVER_SET_RECORD_SIZE },
	{ "NDMP2_RESERVED",		NDMP2_RESERVED },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_auth_type_table[];
extern char * ndmp2_auth_type_to_str (ndmp2_auth_type val);
extern int    ndmp2_auth_type_from_str (ndmp2_auth_type *valp, char * str);

char * ndmp2_auth_type_to_str (ndmp2_auth_type val) {
	return ndmp_enum_to_str ((int)val, ndmp2_auth_type_table);
}

int ndmp2_auth_type_from_str (ndmp2_auth_type *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_auth_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_auth_type_table[] = {
	{ "NDMP2_AUTH_NONE",	NDMP2_AUTH_NONE },
	{ "NDMP2_AUTH_TEXT",	NDMP2_AUTH_TEXT },
	{ "NDMP2_AUTH_MD5",	NDMP2_AUTH_MD5 },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_mover_addr_type_table[];
extern char * ndmp2_mover_addr_type_to_str (ndmp2_mover_addr_type val);
extern int    ndmp2_mover_addr_type_from_str (ndmp2_mover_addr_type *valp, char * str);

char * ndmp2_mover_addr_type_to_str (ndmp2_mover_addr_type val) {
	return ndmp_enum_to_str ((int)val, ndmp2_mover_addr_type_table);
}

int ndmp2_mover_addr_type_from_str (ndmp2_mover_addr_type *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_mover_addr_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_mover_addr_type_table[] = {
	{ "NDMP2_ADDR_LOCAL",	NDMP2_ADDR_LOCAL },
	{ "NDMP2_ADDR_TCP",	NDMP2_ADDR_TCP },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_tape_open_mode_table[];
extern char * ndmp2_tape_open_mode_to_str (ndmp2_tape_open_mode val);
extern int    ndmp2_tape_open_mode_from_str (ndmp2_tape_open_mode *valp, char * str);

char * ndmp2_tape_open_mode_to_str (ndmp2_tape_open_mode val) {
	return ndmp_enum_to_str ((int)val, ndmp2_tape_open_mode_table);
}

int ndmp2_tape_open_mode_from_str (ndmp2_tape_open_mode *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_tape_open_mode_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_tape_open_mode_table[] = {
	{ "NDMP2_TAPE_READ_MODE",	NDMP2_TAPE_READ_MODE },
	{ "NDMP2_TAPE_WRITE_MODE",	NDMP2_TAPE_WRITE_MODE },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_tape_mtio_op_table[];
extern char * ndmp2_tape_mtio_op_to_str (ndmp2_tape_mtio_op val);
extern int    ndmp2_tape_mtio_op_from_str (ndmp2_tape_mtio_op *valp, char * str);

char * ndmp2_tape_mtio_op_to_str (ndmp2_tape_mtio_op val) {
	return ndmp_enum_to_str ((int)val, ndmp2_tape_mtio_op_table);
}

int ndmp2_tape_mtio_op_from_str (ndmp2_tape_mtio_op *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_tape_mtio_op_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_tape_mtio_op_table[] = {
	{ "NDMP2_MTIO_FSF",	NDMP2_MTIO_FSF },
	{ "NDMP2_MTIO_BSF",	NDMP2_MTIO_BSF },
	{ "NDMP2_MTIO_FSR",	NDMP2_MTIO_FSR },
	{ "NDMP2_MTIO_BSR",	NDMP2_MTIO_BSR },
	{ "NDMP2_MTIO_REW",	NDMP2_MTIO_REW },
	{ "NDMP2_MTIO_EOF",	NDMP2_MTIO_EOF },
	{ "NDMP2_MTIO_OFF",	NDMP2_MTIO_OFF },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_mover_state_table[];
extern char * ndmp2_mover_state_to_str (ndmp2_mover_state val);
extern int    ndmp2_mover_state_from_str (ndmp2_mover_state *valp, char * str);

char * ndmp2_mover_state_to_str (ndmp2_mover_state val) {
	return ndmp_enum_to_str ((int)val, ndmp2_mover_state_table);
}

int ndmp2_mover_state_from_str (ndmp2_mover_state *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_mover_state_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_mover_state_table[] = {
	{ "NDMP2_MOVER_STATE_IDLE",	NDMP2_MOVER_STATE_IDLE },
	{ "NDMP2_MOVER_STATE_LISTEN",	NDMP2_MOVER_STATE_LISTEN },
	{ "NDMP2_MOVER_STATE_ACTIVE",	NDMP2_MOVER_STATE_ACTIVE },
	{ "NDMP2_MOVER_STATE_PAUSED",	NDMP2_MOVER_STATE_PAUSED },
	{ "NDMP2_MOVER_STATE_HALTED",	NDMP2_MOVER_STATE_HALTED },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_mover_pause_reason_table[];
extern char * ndmp2_mover_pause_reason_to_str (ndmp2_mover_pause_reason val);
extern int    ndmp2_mover_pause_reason_from_str (ndmp2_mover_pause_reason *valp, char * str);

char * ndmp2_mover_pause_reason_to_str (ndmp2_mover_pause_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp2_mover_pause_reason_table);
}

int ndmp2_mover_pause_reason_from_str (ndmp2_mover_pause_reason *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_mover_pause_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_mover_pause_reason_table[] = {
	{ "NDMP2_MOVER_PAUSE_NA",	NDMP2_MOVER_PAUSE_NA },
	{ "NDMP2_MOVER_PAUSE_EOM",	NDMP2_MOVER_PAUSE_EOM },
	{ "NDMP2_MOVER_PAUSE_EOF",	NDMP2_MOVER_PAUSE_EOF },
	{ "NDMP2_MOVER_PAUSE_SEEK",	NDMP2_MOVER_PAUSE_SEEK },
	{ "NDMP2_MOVER_PAUSE_MEDIA_ERROR", NDMP2_MOVER_PAUSE_MEDIA_ERROR },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_mover_halt_reason_table[];
extern char * ndmp2_mover_halt_reason_to_str (ndmp2_mover_halt_reason val);
extern int    ndmp2_mover_halt_reason_from_str (ndmp2_mover_halt_reason *valp, char * str);

char * ndmp2_mover_halt_reason_to_str (ndmp2_mover_halt_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp2_mover_halt_reason_table);
}

int ndmp2_mover_halt_reason_from_str (ndmp2_mover_halt_reason *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_mover_halt_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_mover_halt_reason_table[] = {
	{ "NDMP2_MOVER_HALT_NA",	NDMP2_MOVER_HALT_NA },
	{ "NDMP2_MOVER_HALT_CONNECT_CLOSED", NDMP2_MOVER_HALT_CONNECT_CLOSED },
	{ "NDMP2_MOVER_HALT_ABORTED",	NDMP2_MOVER_HALT_ABORTED },
	{ "NDMP2_MOVER_HALT_INTERNAL_ERROR", NDMP2_MOVER_HALT_INTERNAL_ERROR },
	{ "NDMP2_MOVER_HALT_CONNECT_ERROR", NDMP2_MOVER_HALT_CONNECT_ERROR },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_mover_mode_table[];
extern char * ndmp2_mover_mode_to_str (ndmp2_mover_mode val);
extern int    ndmp2_mover_mode_from_str (ndmp2_mover_mode *valp, char * str);

char * ndmp2_mover_mode_to_str (ndmp2_mover_mode val) {
	return ndmp_enum_to_str ((int)val, ndmp2_mover_mode_table);
}

int ndmp2_mover_mode_from_str (ndmp2_mover_mode *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_mover_mode_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_mover_mode_table[] = {
	{ "NDMP2_MOVER_MODE_READ",	NDMP2_MOVER_MODE_READ },
	{ "NDMP2_MOVER_MODE_WRITE",	NDMP2_MOVER_MODE_WRITE },
	{ "NDMP2_MOVER_MODE_DATA",	NDMP2_MOVER_MODE_DATA },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_data_operation_table[];
extern char * ndmp2_data_operation_to_str (ndmp2_data_operation val);
extern int    ndmp2_data_operation_from_str (ndmp2_data_operation *valp, char * str);

char * ndmp2_data_operation_to_str (ndmp2_data_operation val) {
	return ndmp_enum_to_str ((int)val, ndmp2_data_operation_table);
}

int ndmp2_data_operation_from_str (ndmp2_data_operation *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_data_operation_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_data_operation_table[] = {
	{ "NDMP2_DATA_OP_NOACTION",	NDMP2_DATA_OP_NOACTION },
	{ "NDMP2_DATA_OP_BACKUP",	NDMP2_DATA_OP_BACKUP },
	{ "NDMP2_DATA_OP_RESTORE",	NDMP2_DATA_OP_RESTORE },
	{ "NDMP2_DATA_OP_RESTORE_FILEHIST", NDMP2_DATA_OP_RESTORE_FILEHIST },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_data_state_table[];
extern char * ndmp2_data_state_to_str (ndmp2_data_state val);
extern int    ndmp2_data_state_from_str (ndmp2_data_state *valp, char * str);

char * ndmp2_data_state_to_str (ndmp2_data_state val) {
	return ndmp_enum_to_str ((int)val, ndmp2_data_state_table);
}

int ndmp2_data_state_from_str (ndmp2_data_state *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_data_state_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_data_state_table[] = {
	{ "NDMP2_DATA_STATE_IDLE",	NDMP2_DATA_STATE_IDLE },
	{ "NDMP2_DATA_STATE_ACTIVE",	NDMP2_DATA_STATE_ACTIVE },
	{ "NDMP2_DATA_STATE_HALTED",	NDMP2_DATA_STATE_HALTED },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_data_halt_reason_table[];
extern char * ndmp2_data_halt_reason_to_str (ndmp2_data_halt_reason val);
extern int    ndmp2_data_halt_reason_from_str (ndmp2_data_halt_reason *valp, char * str);

char * ndmp2_data_halt_reason_to_str (ndmp2_data_halt_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp2_data_halt_reason_table);
}

int ndmp2_data_halt_reason_from_str (ndmp2_data_halt_reason *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_data_halt_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_data_halt_reason_table[] = {
	{ "NDMP2_DATA_HALT_NA",	NDMP2_DATA_HALT_NA },
	{ "NDMP2_DATA_HALT_SUCCESSFUL",	NDMP2_DATA_HALT_SUCCESSFUL },
	{ "NDMP2_DATA_HALT_ABORTED",	NDMP2_DATA_HALT_ABORTED },
	{ "NDMP2_DATA_HALT_INTERNAL_ERROR", NDMP2_DATA_HALT_INTERNAL_ERROR },
	{ "NDMP2_DATA_HALT_CONNECT_ERROR", NDMP2_DATA_HALT_CONNECT_ERROR },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_connect_reason_table[];
extern char * ndmp2_connect_reason_to_str (ndmp2_connect_reason val);
extern int    ndmp2_connect_reason_from_str (ndmp2_connect_reason *valp, char * str);

char * ndmp2_connect_reason_to_str (ndmp2_connect_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp2_connect_reason_table);
}

int ndmp2_connect_reason_from_str (ndmp2_connect_reason *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_connect_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_connect_reason_table[] = {
	{ "NDMP2_CONNECTED",	NDMP2_CONNECTED },
	{ "NDMP2_SHUTDOWN",	NDMP2_SHUTDOWN },
	{ "NDMP2_REFUSED",	NDMP2_REFUSED },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_debug_level_table[];
extern char * ndmp2_debug_level_to_str (ndmp2_debug_level val);
extern int    ndmp2_debug_level_from_str (ndmp2_debug_level *valp, char * str);

char * ndmp2_debug_level_to_str (ndmp2_debug_level val) {
	return ndmp_enum_to_str ((int)val, ndmp2_debug_level_table);
}

int ndmp2_debug_level_from_str (ndmp2_debug_level *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_debug_level_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_debug_level_table[] = {
	{ "NDMP2_DBG_USER_INFO",	NDMP2_DBG_USER_INFO },
	{ "NDMP2_DBG_USER_SUMMARY",	NDMP2_DBG_USER_SUMMARY },
	{ "NDMP2_DBG_USER_DETAIL",	NDMP2_DBG_USER_DETAIL },
	{ "NDMP2_DBG_DIAG_INFO",	NDMP2_DBG_DIAG_INFO },
	{ "NDMP2_DBG_DIAG_SUMMARY",	NDMP2_DBG_DIAG_SUMMARY },
	{ "NDMP2_DBG_DIAG_DETAIL",	NDMP2_DBG_DIAG_DETAIL },
	{ "NDMP2_DBG_PROG_INFO",	NDMP2_DBG_PROG_INFO },
	{ "NDMP2_DBG_PROG_SUMMARY",	NDMP2_DBG_PROG_SUMMARY },
	{ "NDMP2_DBG_PROG_DETAIL",	NDMP2_DBG_PROG_DETAIL },
	{ 0 }
};
/* clang-format on */

extern struct ndmp_enum_str_table ndmp2_unix_file_type_table[];
extern char * ndmp2_unix_file_type_to_str (ndmp2_unix_file_type val);
extern int    ndmp2_unix_file_type_from_str (ndmp2_unix_file_type *valp, char * str);

char * ndmp2_unix_file_type_to_str (ndmp2_unix_file_type val) {
	return ndmp_enum_to_str ((int)val, ndmp2_unix_file_type_table);
}

int ndmp2_unix_file_type_from_str (ndmp2_unix_file_type *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp2_unix_file_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp2_unix_file_type_table[] = {
	{ "NDMP2_FILE_DIR",	NDMP2_FILE_DIR },
	{ "NDMP2_FILE_FIFO",	NDMP2_FILE_FIFO },
	{ "NDMP2_FILE_CSPEC",	NDMP2_FILE_CSPEC },
	{ "NDMP2_FILE_BSPEC",	NDMP2_FILE_BSPEC },
	{ "NDMP2_FILE_REG",	NDMP2_FILE_REG },
	{ "NDMP2_FILE_SLINK",	NDMP2_FILE_SLINK },
	{ "NDMP2_FILE_SOCK",	NDMP2_FILE_SOCK },
	{ 0 }
};
/* clang-format on */

#endif /* !NDMOS_OPTION_NO_NDMP2 */
