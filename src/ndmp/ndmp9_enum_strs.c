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





extern struct ndmp_enum_str_table ndmp9_error_table[];
extern char * ndmp9_error_to_str (ndmp9_error val);
extern int    ndmp9_error_from_str (ndmp9_error *valp, char * str);

char * ndmp9_error_to_str (ndmp9_error val) {
	return ndmp_enum_to_str ((int)val, ndmp9_error_table);
}

int ndmp9_error_from_str (ndmp9_error *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_error_table);
}

struct ndmp_enum_str_table ndmp9_error_table[] = {
	{ "NDMP9_NO_ERR",		NDMP9_NO_ERR },
	{ "NDMP9_NOT_SUPPORTED_ERR",	NDMP9_NOT_SUPPORTED_ERR },
	{ "NDMP9_DEVICE_BUSY_ERR",	NDMP9_DEVICE_BUSY_ERR },
	{ "NDMP9_DEVICE_OPENED_ERR",	NDMP9_DEVICE_OPENED_ERR },
	{ "NDMP9_NOT_AUTHORIZED_ERR",	NDMP9_NOT_AUTHORIZED_ERR },
	{ "NDMP9_PERMISSION_ERR",	NDMP9_PERMISSION_ERR },
	{ "NDMP9_DEV_NOT_OPEN_ERR",	NDMP9_DEV_NOT_OPEN_ERR },
	{ "NDMP9_IO_ERR",		NDMP9_IO_ERR },
	{ "NDMP9_TIMEOUT_ERR",		NDMP9_TIMEOUT_ERR },
	{ "NDMP9_ILLEGAL_ARGS_ERR",	NDMP9_ILLEGAL_ARGS_ERR },
	{ "NDMP9_NO_TAPE_LOADED_ERR",	NDMP9_NO_TAPE_LOADED_ERR },
	{ "NDMP9_WRITE_PROTECT_ERR",	NDMP9_WRITE_PROTECT_ERR },
	{ "NDMP9_EOF_ERR",		NDMP9_EOF_ERR },
	{ "NDMP9_EOM_ERR",		NDMP9_EOM_ERR },
	{ "NDMP9_FILE_NOT_FOUND_ERR",	NDMP9_FILE_NOT_FOUND_ERR },
	{ "NDMP9_BAD_FILE_ERR",		NDMP9_BAD_FILE_ERR },
	{ "NDMP9_NO_DEVICE_ERR",	NDMP9_NO_DEVICE_ERR },
	{ "NDMP9_NO_BUS_ERR",		NDMP9_NO_BUS_ERR },
	{ "NDMP9_XDR_DECODE_ERR",	NDMP9_XDR_DECODE_ERR },
	{ "NDMP9_ILLEGAL_STATE_ERR",	NDMP9_ILLEGAL_STATE_ERR },
	{ "NDMP9_UNDEFINED_ERR",	NDMP9_UNDEFINED_ERR },
	{ "NDMP9_XDR_ENCODE_ERR",	NDMP9_XDR_ENCODE_ERR },
	{ "NDMP9_NO_MEM_ERR",		NDMP9_NO_MEM_ERR },
	{ "NDMP9_CONNECT_ERR",		NDMP9_CONNECT_ERR, },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp9_auth_type_table[];
extern char * ndmp9_auth_type_to_str (ndmp9_auth_type val);
extern int    ndmp9_auth_type_from_str (ndmp9_auth_type *valp, char * str);

char * ndmp9_auth_type_to_str (ndmp9_auth_type val) {
	return ndmp_enum_to_str ((int)val, ndmp9_auth_type_table);
}

int ndmp9_auth_type_from_str (ndmp9_auth_type *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_auth_type_table);
}

struct ndmp_enum_str_table ndmp9_auth_type_table[] = {
	{ "NDMP9_AUTH_NONE",	NDMP9_AUTH_NONE },
	{ "NDMP9_AUTH_TEXT",	NDMP9_AUTH_TEXT },
	{ "NDMP9_AUTH_MD5",	NDMP9_AUTH_MD5 },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_addr_type_table[];
extern char * ndmp9_addr_type_to_str (ndmp9_addr_type val);
extern int    ndmp9_addr_type_from_str (ndmp9_addr_type *valp, char * str);

char * ndmp9_addr_type_to_str (ndmp9_addr_type val) {
	return ndmp_enum_to_str ((int)val, ndmp9_addr_type_table);
}

int ndmp9_addr_type_from_str (ndmp9_addr_type *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_addr_type_table);
}

struct ndmp_enum_str_table ndmp9_addr_type_table[] = {
	{ "NDMP9_ADDR_LOCAL",	NDMP9_ADDR_LOCAL },
	{ "NDMP9_ADDR_TCP",	NDMP9_ADDR_TCP },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_tape_open_mode_table[];
extern char * ndmp9_tape_open_mode_to_str (ndmp9_tape_open_mode val);
extern int    ndmp9_tape_open_mode_from_str (ndmp9_tape_open_mode *valp, char * str);

char * ndmp9_tape_open_mode_to_str (ndmp9_tape_open_mode val) {
	return ndmp_enum_to_str ((int)val, ndmp9_tape_open_mode_table);
}

int ndmp9_tape_open_mode_from_str (ndmp9_tape_open_mode *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_tape_open_mode_table);
}

struct ndmp_enum_str_table ndmp9_tape_open_mode_table[] = {
	{ "NDMP9_TAPE_READ_MODE",	NDMP9_TAPE_READ_MODE },
	{ "NDMP9_TAPE_RDWR_MODE",	NDMP9_TAPE_RDWR_MODE },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_tape_mtio_op_table[];
extern char * ndmp9_tape_mtio_op_to_str (ndmp9_tape_mtio_op val);
extern int    ndmp9_tape_mtio_op_from_str (ndmp9_tape_mtio_op *valp, char * str);

char * ndmp9_tape_mtio_op_to_str (ndmp9_tape_mtio_op val) {
	return ndmp_enum_to_str ((int)val, ndmp9_tape_mtio_op_table);
}

int ndmp9_tape_mtio_op_from_str (ndmp9_tape_mtio_op *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_tape_mtio_op_table);
}

struct ndmp_enum_str_table ndmp9_tape_mtio_op_table[] = {
	{ "NDMP9_MTIO_FSF",	NDMP9_MTIO_FSF },
	{ "NDMP9_MTIO_BSF",	NDMP9_MTIO_BSF },
	{ "NDMP9_MTIO_FSR",	NDMP9_MTIO_FSR },
	{ "NDMP9_MTIO_BSR",	NDMP9_MTIO_BSR },
	{ "NDMP9_MTIO_REW",	NDMP9_MTIO_REW },
	{ "NDMP9_MTIO_EOF",	NDMP9_MTIO_EOF },
	{ "NDMP9_MTIO_OFF",	NDMP9_MTIO_OFF },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_mover_state_table[];
extern char * ndmp9_mover_state_to_str (ndmp9_mover_state val);
extern int    ndmp9_mover_state_from_str (ndmp9_mover_state *valp, char * str);

char * ndmp9_mover_state_to_str (ndmp9_mover_state val) {
	return ndmp_enum_to_str ((int)val, ndmp9_mover_state_table);
}

int ndmp9_mover_state_from_str (ndmp9_mover_state *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_mover_state_table);
}

struct ndmp_enum_str_table ndmp9_mover_state_table[] = {
	{ "NDMP9_MOVER_STATE_IDLE",	NDMP9_MOVER_STATE_IDLE },
	{ "NDMP9_MOVER_STATE_LISTEN",	NDMP9_MOVER_STATE_LISTEN },
	{ "NDMP9_MOVER_STATE_ACTIVE",	NDMP9_MOVER_STATE_ACTIVE },
	{ "NDMP9_MOVER_STATE_PAUSED",	NDMP9_MOVER_STATE_PAUSED },
	{ "NDMP9_MOVER_STATE_HALTED",	NDMP9_MOVER_STATE_HALTED },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_mover_pause_reason_table[];
extern char * ndmp9_mover_pause_reason_to_str (ndmp9_mover_pause_reason val);
extern int    ndmp9_mover_pause_reason_from_str (ndmp9_mover_pause_reason *valp, char * str);

char * ndmp9_mover_pause_reason_to_str (ndmp9_mover_pause_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp9_mover_pause_reason_table);
}

int ndmp9_mover_pause_reason_from_str (ndmp9_mover_pause_reason *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_mover_pause_reason_table);
}

struct ndmp_enum_str_table ndmp9_mover_pause_reason_table[] = {
	{ "NDMP9_MOVER_PAUSE_NA",	NDMP9_MOVER_PAUSE_NA },
	{ "NDMP9_MOVER_PAUSE_EOM",	NDMP9_MOVER_PAUSE_EOM },
	{ "NDMP9_MOVER_PAUSE_EOF",	NDMP9_MOVER_PAUSE_EOF },
	{ "NDMP9_MOVER_PAUSE_SEEK",	NDMP9_MOVER_PAUSE_SEEK },
	{ "NDMP9_MOVER_PAUSE_MEDIA_ERROR", NDMP9_MOVER_PAUSE_MEDIA_ERROR },
	{ "NDMP9_MOVER_PAUSE_EOW",      NDMP9_MOVER_PAUSE_EOW },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_mover_halt_reason_table[];
extern char * ndmp9_mover_halt_reason_to_str (ndmp9_mover_halt_reason val);
extern int    ndmp9_mover_halt_reason_from_str (ndmp9_mover_halt_reason *valp, char * str);

char * ndmp9_mover_halt_reason_to_str (ndmp9_mover_halt_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp9_mover_halt_reason_table);
}

int ndmp9_mover_halt_reason_from_str (ndmp9_mover_halt_reason *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_mover_halt_reason_table);
}

struct ndmp_enum_str_table ndmp9_mover_halt_reason_table[] = {
	{ "NDMP9_MOVER_HALT_NA",	NDMP9_MOVER_HALT_NA },
	{ "NDMP9_MOVER_HALT_CONNECT_CLOSED", NDMP9_MOVER_HALT_CONNECT_CLOSED },
	{ "NDMP9_MOVER_HALT_ABORTED",	NDMP9_MOVER_HALT_ABORTED },
	{ "NDMP9_MOVER_HALT_INTERNAL_ERROR", NDMP9_MOVER_HALT_INTERNAL_ERROR },
	{ "NDMP9_MOVER_HALT_CONNECT_ERROR", NDMP9_MOVER_HALT_CONNECT_ERROR },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_mover_mode_table[];
extern char * ndmp9_mover_mode_to_str (ndmp9_mover_mode val);
extern int    ndmp9_mover_mode_from_str (ndmp9_mover_mode *valp, char * str);

char * ndmp9_mover_mode_to_str (ndmp9_mover_mode val) {
	return ndmp_enum_to_str ((int)val, ndmp9_mover_mode_table);
}

int ndmp9_mover_mode_from_str (ndmp9_mover_mode *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_mover_mode_table);
}

struct ndmp_enum_str_table ndmp9_mover_mode_table[] = {
	{ "NDMP9_MOVER_MODE_READ",	NDMP9_MOVER_MODE_READ },
	{ "NDMP9_MOVER_MODE_WRITE",	NDMP9_MOVER_MODE_WRITE },
#if 0
	{ "NDMP9_MOVER_MODE_DATA",	NDMP9_MOVER_MODE_DATA },
#endif
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_data_operation_table[];
extern char * ndmp9_data_operation_to_str (ndmp9_data_operation val);
extern int    ndmp9_data_operation_from_str (ndmp9_data_operation *valp, char * str);

char * ndmp9_data_operation_to_str (ndmp9_data_operation val) {
	return ndmp_enum_to_str ((int)val, ndmp9_data_operation_table);
}

int ndmp9_data_operation_from_str (ndmp9_data_operation *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_data_operation_table);
}

struct ndmp_enum_str_table ndmp9_data_operation_table[] = {
	{ "NDMP9_DATA_OP_NOACTION",	NDMP9_DATA_OP_NOACTION },
	{ "NDMP9_DATA_OP_BACKUP",	NDMP9_DATA_OP_BACKUP },
	{ "NDMP9_DATA_OP_RECOVER",	NDMP9_DATA_OP_RECOVER },
	{ "NDMP9_DATA_OP_RECOVER_FILEHIST", NDMP9_DATA_OP_RECOVER_FILEHIST },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_data_state_table[];
extern char * ndmp9_data_state_to_str (ndmp9_data_state val);
extern int    ndmp9_data_state_from_str (ndmp9_data_state *valp, char * str);

char * ndmp9_data_state_to_str (ndmp9_data_state val) {
	return ndmp_enum_to_str ((int)val, ndmp9_data_state_table);
}

int ndmp9_data_state_from_str (ndmp9_data_state *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_data_state_table);
}

struct ndmp_enum_str_table ndmp9_data_state_table[] = {
	{ "NDMP9_DATA_STATE_IDLE",	NDMP9_DATA_STATE_IDLE },
	{ "NDMP9_DATA_STATE_ACTIVE",	NDMP9_DATA_STATE_ACTIVE },
	{ "NDMP9_DATA_STATE_HALTED",	NDMP9_DATA_STATE_HALTED },
	{ 0 }
};

extern struct ndmp_enum_str_table ndmp9_data_halt_reason_table[];
extern char * ndmp9_data_halt_reason_to_str (ndmp9_data_halt_reason val);
extern int    ndmp9_data_halt_reason_from_str (ndmp9_data_halt_reason *valp, char * str);

char * ndmp9_data_halt_reason_to_str (ndmp9_data_halt_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp9_data_halt_reason_table);
}

int ndmp9_data_halt_reason_from_str (ndmp9_data_halt_reason *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_data_halt_reason_table);
}

struct ndmp_enum_str_table ndmp9_data_halt_reason_table[] = {
	{ "NDMP9_DATA_HALT_NA",	NDMP9_DATA_HALT_NA },
	{ "NDMP9_DATA_HALT_SUCCESSFUL",	NDMP9_DATA_HALT_SUCCESSFUL },
	{ "NDMP9_DATA_HALT_ABORTED",	NDMP9_DATA_HALT_ABORTED },
	{ "NDMP9_DATA_HALT_INTERNAL_ERROR", NDMP9_DATA_HALT_INTERNAL_ERROR },
	{ "NDMP9_DATA_HALT_CONNECT_ERROR", NDMP9_DATA_HALT_CONNECT_ERROR },
	{ 0 }
};


extern struct ndmp_enum_str_table ndmp9_file_type_table[];
extern char * ndmp9_file_type_to_str (ndmp9_file_type val);
extern int    ndmp9_file_type_from_str (ndmp9_file_type *valp, char * str);

char * ndmp9_file_type_to_str (ndmp9_file_type val) {
	return ndmp_enum_to_str ((int)val, ndmp9_file_type_table);
}

int ndmp9_file_type_from_str (ndmp9_file_type *valp, char * str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp9_file_type_table);
}

struct ndmp_enum_str_table ndmp9_file_type_table[] = {
	{ "NDMP9_FILE_DIR",	NDMP9_FILE_DIR },
	{ "NDMP9_FILE_FIFO",	NDMP9_FILE_FIFO },
	{ "NDMP9_FILE_CSPEC",	NDMP9_FILE_CSPEC },
	{ "NDMP9_FILE_BSPEC",	NDMP9_FILE_BSPEC },
	{ "NDMP9_FILE_REG",	NDMP9_FILE_REG },
	{ "NDMP9_FILE_SLINK",	NDMP9_FILE_SLINK },
	{ "NDMP9_FILE_SOCK",	NDMP9_FILE_SOCK },
	{ "NDMP9_FILE_REGISTRY", NDMP9_FILE_REGISTRY },
	{ "NDMP9_FILE_OTHER",	NDMP9_FILE_OTHER },
	{ 0 }
};
