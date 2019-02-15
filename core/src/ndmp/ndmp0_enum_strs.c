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





extern struct ndmp_enum_str_table ndmp0_error_table[];
extern char * ndmp0_error_to_str (ndmp0_error val);
extern int    ndmp0_error_from_str (ndmp0_error *valp, char *str);

char * ndmp0_error_to_str (ndmp0_error val) {
	return ndmp_enum_to_str ((int)val, ndmp0_error_table);
}

int ndmp0_error_from_str (ndmp0_error *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp0_error_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp0_error_table[] = {
	{ "NDMP0_NO_ERR",		NDMP0_NO_ERR, },
	{ "NDMP0_NOT_SUPPORTED_ERR",	NDMP0_NOT_SUPPORTED_ERR, },
	{ "NDMP0_DEVICE_BUSY_ERR",	NDMP0_DEVICE_BUSY_ERR, },
	{ "NDMP0_DEVICE_OPENED_ERR",	NDMP0_DEVICE_OPENED_ERR, },
	{ "NDMP0_NOT_AUTHORIZED_ERR",	NDMP0_NOT_AUTHORIZED_ERR, },
	{ "NDMP0_PERMISSION_ERR",	NDMP0_PERMISSION_ERR, },
	{ "NDMP0_DEV_NOT_OPEN_ERR",	NDMP0_DEV_NOT_OPEN_ERR, },
	{ "NDMP0_IO_ERR",		NDMP0_IO_ERR, },
	{ "NDMP0_TIMEOUT_ERR",		NDMP0_TIMEOUT_ERR, },
	{ "NDMP0_ILLEGAL_ARGS_ERR",	NDMP0_ILLEGAL_ARGS_ERR, },
	{ "NDMP0_NO_TAPE_LOADED_ERR",	NDMP0_NO_TAPE_LOADED_ERR, },
	{ "NDMP0_WRITE_PROTECT_ERR",	NDMP0_WRITE_PROTECT_ERR, },
	{ "NDMP0_EOF_ERR",		NDMP0_EOF_ERR, },
	{ "NDMP0_EOM_ERR",		NDMP0_EOM_ERR, },
	{ "NDMP0_FILE_NOT_FOUND_ERR",	NDMP0_FILE_NOT_FOUND_ERR, },
	{ "NDMP0_BAD_FILE_ERR",		NDMP0_BAD_FILE_ERR, },
	{ "NDMP0_NO_DEVICE_ERR",	NDMP0_NO_DEVICE_ERR, },
	{ "NDMP0_NO_BUS_ERR",		NDMP0_NO_BUS_ERR, },
	{ "NDMP0_XDR_DECODE_ERR",	NDMP0_XDR_DECODE_ERR, },
	{ "NDMP0_ILLEGAL_STATE_ERR",	NDMP0_ILLEGAL_STATE_ERR, },
	{ "NDMP0_UNDEFINED_ERR",	NDMP0_UNDEFINED_ERR, },
	{ "NDMP0_XDR_ENCODE_ERR",	NDMP0_XDR_ENCODE_ERR, },
	{ "NDMP0_NO_MEM_ERR",		NDMP0_NO_MEM_ERR, },
	{ 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp0_header_message_type_table[];
extern char * ndmp0_header_message_type_to_str (ndmp0_header_message_type val);
extern int    ndmp0_header_message_type_from_str (ndmp0_header_message_type *valp, char *str);

char * ndmp0_header_message_type_to_str (ndmp0_header_message_type val) {
	return ndmp_enum_to_str ((int)val, ndmp0_header_message_type_table);
}

int ndmp0_header_message_type_from_str (ndmp0_header_message_type *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp0_header_message_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp0_header_message_type_table[] = {
	{ "NDMP0_MESSAGE_REQUEST",	NDMP0_MESSAGE_REQUEST, },
	{ "NDMP0_MESSAGE_REPLY",	NDMP0_MESSAGE_REPLY, },
	{ 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp0_message_table[];
extern char * ndmp0_message_to_str (ndmp0_message val);
extern int    ndmp0_message_from_str (ndmp0_message *valp, char *str);

char * ndmp0_message_to_str (ndmp0_message val) {
	return ndmp_enum_to_str ((int)val, ndmp0_message_table);
}

int ndmp0_message_from_str (ndmp0_message *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp0_message_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp0_message_table[] = {
	{ "NDMP0_CONNECT_OPEN",		NDMP0_CONNECT_OPEN, },
	{ "NDMP0_CONNECT_CLOSE",	NDMP0_CONNECT_CLOSE, },
	{ "NDMP0_NOTIFY_CONNECTED",	NDMP0_NOTIFY_CONNECTED, },
	{ 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp0_connect_reason_table[];
extern char * ndmp0_connect_reason_to_str (ndmp0_connect_reason val);
extern int    ndmp0_connect_reason_from_str (ndmp0_connect_reason *valp, char *str);

char * ndmp0_connect_reason_to_str (ndmp0_connect_reason val) {
	return ndmp_enum_to_str ((int)val, ndmp0_connect_reason_table);
}

int ndmp0_connect_reason_from_str (ndmp0_connect_reason *valp, char *str) {
	return ndmp_enum_from_str ((int*)valp, str, ndmp0_connect_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp0_connect_reason_table[] = {
	{ "NDMP0_CONNECTED",		NDMP0_CONNECTED, },
	{ "NDMP0_SHUTDOWN",		NDMP0_SHUTDOWN, },
	{ "NDMP0_REFUSED",		NDMP0_REFUSED, },
	{ 0 }
};
/* clang-format on */
