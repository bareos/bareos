/*
 * Copyright (c) 2000
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


#include "ndmos.h" /* rpc/rpc.h */
#include "ndmprotocol.h"


#ifndef NDMOS_OPTION_NO_NDMP4


extern struct ndmp_enum_str_table ndmp4_error_table[];
extern char* ndmp4_error_to_str(ndmp4_error val);
extern int ndmp4_error_from_str(ndmp4_error* valp, char* str);

char* ndmp4_error_to_str(ndmp4_error val)
{
  return ndmp_enum_to_str((int)val, ndmp4_error_table);
}

int ndmp4_error_from_str(ndmp4_error* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_error_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_error_table[] = {
        { "NDMP4_NO_ERR",               NDMP4_NO_ERR, },
        { "NDMP4_NOT_SUPPORTED_ERR",    NDMP4_NOT_SUPPORTED_ERR, },
        { "NDMP4_DEVICE_BUSY_ERR",      NDMP4_DEVICE_BUSY_ERR, },
        { "NDMP4_DEVICE_OPENED_ERR",    NDMP4_DEVICE_OPENED_ERR, },
        { "NDMP4_NOT_AUTHORIZED_ERR",   NDMP4_NOT_AUTHORIZED_ERR, },
        { "NDMP4_PERMISSION_ERR",       NDMP4_PERMISSION_ERR, },
        { "NDMP4_DEV_NOT_OPEN_ERR",     NDMP4_DEV_NOT_OPEN_ERR, },
        { "NDMP4_IO_ERR",               NDMP4_IO_ERR, },
        { "NDMP4_TIMEOUT_ERR",          NDMP4_TIMEOUT_ERR, },
        { "NDMP4_ILLEGAL_ARGS_ERR",     NDMP4_ILLEGAL_ARGS_ERR, },
        { "NDMP4_NO_TAPE_LOADED_ERR",   NDMP4_NO_TAPE_LOADED_ERR, },
        { "NDMP4_WRITE_PROTECT_ERR",    NDMP4_WRITE_PROTECT_ERR, },
        { "NDMP4_EOF_ERR",              NDMP4_EOF_ERR, },
        { "NDMP4_EOM_ERR",              NDMP4_EOM_ERR, },
        { "NDMP4_FILE_NOT_FOUND_ERR",   NDMP4_FILE_NOT_FOUND_ERR, },
        { "NDMP4_BAD_FILE_ERR",         NDMP4_BAD_FILE_ERR, },
        { "NDMP4_NO_DEVICE_ERR",        NDMP4_NO_DEVICE_ERR, },
        { "NDMP4_NO_BUS_ERR",           NDMP4_NO_BUS_ERR, },
        { "NDMP4_XDR_DECODE_ERR",       NDMP4_XDR_DECODE_ERR, },
        { "NDMP4_ILLEGAL_STATE_ERR",    NDMP4_ILLEGAL_STATE_ERR, },
        { "NDMP4_UNDEFINED_ERR",        NDMP4_UNDEFINED_ERR, },
        { "NDMP4_XDR_ENCODE_ERR",       NDMP4_XDR_ENCODE_ERR, },
        { "NDMP4_NO_MEM_ERR",           NDMP4_NO_MEM_ERR, },
        { "NDMP4_CONNECT_ERR",          NDMP4_CONNECT_ERR, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_header_message_type_table[];
extern char* ndmp4_header_message_type_to_str(ndmp4_header_message_type val);
extern int ndmp4_header_message_type_from_str(ndmp4_header_message_type* valp,
                                              char* str);

char* ndmp4_header_message_type_to_str(ndmp4_header_message_type val)
{
  return ndmp_enum_to_str((int)val, ndmp4_header_message_type_table);
}

int ndmp4_header_message_type_from_str(ndmp4_header_message_type* valp,
                                       char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_header_message_type_table);
}

struct ndmp_enum_str_table ndmp4_header_message_type_table[] = {
    {
        "NDMP4_MESSAGE_REQUEST",
        NDMP4_MESSAGE_REQUEST,
    },
    {
        "NDMP4_MESSAGE_REPLY",
        NDMP4_MESSAGE_REPLY,
    },
    {0}};


extern struct ndmp_enum_str_table ndmp4_message_table[];
extern char* ndmp4_message_to_str(ndmp4_message val);
extern int ndmp4_message_from_str(ndmp4_message* valp, char* str);

char* ndmp4_message_to_str(ndmp4_message val)
{
  return ndmp_enum_to_str((int)val, ndmp4_message_table);
}

int ndmp4_message_from_str(ndmp4_message* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_message_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_message_table[] = {
        { "NDMP4_CONNECT_OPEN",         NDMP4_CONNECT_OPEN, },
        { "NDMP4_CONNECT_CLIENT_AUTH",  NDMP4_CONNECT_CLIENT_AUTH, },
        { "NDMP4_CONNECT_CLOSE",        NDMP4_CONNECT_CLOSE, },
        { "NDMP4_CONNECT_SERVER_AUTH",  NDMP4_CONNECT_SERVER_AUTH, },
        { "NDMP4_CONFIG_GET_HOST_INFO", NDMP4_CONFIG_GET_HOST_INFO, },
        { "NDMP4_CONFIG_GET_CONNECTION_TYPE",
                                        NDMP4_CONFIG_GET_CONNECTION_TYPE, },
        { "NDMP4_CONFIG_GET_AUTH_ATTR", NDMP4_CONFIG_GET_AUTH_ATTR, },
        { "NDMP4_CONFIG_GET_BUTYPE_INFO", NDMP4_CONFIG_GET_BUTYPE_INFO, },
        { "NDMP4_CONFIG_GET_FS_INFO",   NDMP4_CONFIG_GET_FS_INFO, },
        { "NDMP4_CONFIG_GET_TAPE_INFO", NDMP4_CONFIG_GET_TAPE_INFO, },
        { "NDMP4_CONFIG_GET_SCSI_INFO", NDMP4_CONFIG_GET_SCSI_INFO, },
        { "NDMP4_CONFIG_GET_SERVER_INFO",NDMP4_CONFIG_GET_SERVER_INFO, },
        { "NDMP4_SCSI_OPEN",            NDMP4_SCSI_OPEN, },
        { "NDMP4_SCSI_CLOSE",           NDMP4_SCSI_CLOSE, },
        { "NDMP4_SCSI_GET_STATE",       NDMP4_SCSI_GET_STATE, },
/*      { "NDMP4_SCSI_SET_TARGET",      NDMP4_SCSI_SET_TARGET, }, */
        { "NDMP4_SCSI_RESET_DEVICE",    NDMP4_SCSI_RESET_DEVICE, },
/*      { "NDMP4_SCSI_RESET_BUS",       NDMP4_SCSI_RESET_BUS, }, */
        { "NDMP4_SCSI_EXECUTE_CDB",     NDMP4_SCSI_EXECUTE_CDB, },
        { "NDMP4_TAPE_OPEN",            NDMP4_TAPE_OPEN, },
        { "NDMP4_TAPE_CLOSE",           NDMP4_TAPE_CLOSE, },
        { "NDMP4_TAPE_GET_STATE",       NDMP4_TAPE_GET_STATE, },
        { "NDMP4_TAPE_MTIO",            NDMP4_TAPE_MTIO, },
        { "NDMP4_TAPE_WRITE",           NDMP4_TAPE_WRITE, },
        { "NDMP4_TAPE_READ",            NDMP4_TAPE_READ, },
        { "NDMP4_TAPE_EXECUTE_CDB",     NDMP4_TAPE_EXECUTE_CDB, },
        { "NDMP4_DATA_GET_STATE",       NDMP4_DATA_GET_STATE, },
        { "NDMP4_DATA_START_BACKUP",    NDMP4_DATA_START_BACKUP, },
        { "NDMP4_DATA_START_RECOVER",   NDMP4_DATA_START_RECOVER, },
        { "NDMP4_DATA_START_RECOVER_FILEHIST",
                                        NDMP4_DATA_START_RECOVER_FILEHIST, },
        { "NDMP4_DATA_ABORT",           NDMP4_DATA_ABORT, },
        { "NDMP4_DATA_GET_ENV",         NDMP4_DATA_GET_ENV, },
        { "NDMP4_DATA_STOP",            NDMP4_DATA_STOP, },
        { "NDMP4_DATA_LISTEN",          NDMP4_DATA_LISTEN, },
        { "NDMP4_DATA_CONNECT",         NDMP4_DATA_CONNECT, },
        { "NDMP4_NOTIFY_DATA_HALTED",   NDMP4_NOTIFY_DATA_HALTED, },
        { "NDMP4_NOTIFY_CONNECTION_STATUS",
                                        NDMP4_NOTIFY_CONNECTION_STATUS, },
        { "NDMP4_NOTIFY_MOVER_HALTED",  NDMP4_NOTIFY_MOVER_HALTED, },
        { "NDMP4_NOTIFY_MOVER_PAUSED",  NDMP4_NOTIFY_MOVER_PAUSED, },
        { "NDMP4_NOTIFY_DATA_READ",     NDMP4_NOTIFY_DATA_READ, },
        { "NDMP4_LOG_FILE",             NDMP4_LOG_FILE, },
        { "NDMP4_LOG_MESSAGE",          NDMP4_LOG_MESSAGE, },
        { "NDMP4_FH_ADD_FILE",          NDMP4_FH_ADD_FILE, },
        { "NDMP4_FH_ADD_DIR",           NDMP4_FH_ADD_DIR, },
        { "NDMP4_FH_ADD_NODE",          NDMP4_FH_ADD_NODE, },
        { "NDMP4_MOVER_GET_STATE",      NDMP4_MOVER_GET_STATE, },
        { "NDMP4_MOVER_LISTEN",         NDMP4_MOVER_LISTEN, },
        { "NDMP4_MOVER_CONTINUE",       NDMP4_MOVER_CONTINUE, },
        { "NDMP4_MOVER_ABORT",          NDMP4_MOVER_ABORT, },
        { "NDMP4_MOVER_STOP",           NDMP4_MOVER_STOP, },
        { "NDMP4_MOVER_SET_WINDOW",     NDMP4_MOVER_SET_WINDOW, },
        { "NDMP4_MOVER_READ",           NDMP4_MOVER_READ, },
        { "NDMP4_MOVER_CLOSE",          NDMP4_MOVER_CLOSE, },
        { "NDMP4_MOVER_SET_RECORD_SIZE", NDMP4_MOVER_SET_RECORD_SIZE, },
        { "NDMP4_MOVER_CONNECT",        NDMP4_MOVER_CONNECT, },
/*      { "NDMP4_VENDORS_BASE",         NDMP4_VENDORS_BASE, }, */
/*      { "NDMP4_RESERVED_BASE",        NDMP4_RESERVED_BASE, }, */
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_auth_type_table[];
extern char* ndmp4_auth_type_to_str(ndmp4_auth_type val);
extern int ndmp4_auth_type_from_str(ndmp4_auth_type* valp, char* str);

char* ndmp4_auth_type_to_str(ndmp4_auth_type val)
{
  return ndmp_enum_to_str((int)val, ndmp4_auth_type_table);
}

int ndmp4_auth_type_from_str(ndmp4_auth_type* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_auth_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_auth_type_table[] = {
        { "NDMP4_AUTH_NONE",            NDMP4_AUTH_NONE, },
        { "NDMP4_AUTH_TEXT",            NDMP4_AUTH_TEXT, },
        { "NDMP4_AUTH_MD5",             NDMP4_AUTH_MD5, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_addr_type_table[];
extern char* ndmp4_addr_type_to_str(ndmp4_addr_type val);
extern int ndmp4_addr_type_from_str(ndmp4_addr_type* valp, char* str);

char* ndmp4_addr_type_to_str(ndmp4_addr_type val)
{
  return ndmp_enum_to_str((int)val, ndmp4_addr_type_table);
}

int ndmp4_addr_type_from_str(ndmp4_addr_type* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_addr_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_addr_type_table[] = {
        { "NDMP4_ADDR_LOCAL",           NDMP4_ADDR_LOCAL, },
        { "NDMP4_ADDR_TCP",             NDMP4_ADDR_TCP, },
        { "NDMP4_ADDR_RESERVED",        NDMP4_ADDR_RESERVED, },
        { "NDMP4_ADDR_IPC",             NDMP4_ADDR_IPC, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_tape_open_mode_table[];
extern char* ndmp4_tape_open_mode_to_str(ndmp4_tape_open_mode val);
extern int ndmp4_tape_open_mode_from_str(ndmp4_tape_open_mode* valp, char* str);

char* ndmp4_tape_open_mode_to_str(ndmp4_tape_open_mode val)
{
  return ndmp_enum_to_str((int)val, ndmp4_tape_open_mode_table);
}

int ndmp4_tape_open_mode_from_str(ndmp4_tape_open_mode* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_tape_open_mode_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_tape_open_mode_table[] = {
        { "NDMP4_TAPE_READ_MODE",       NDMP4_TAPE_READ_MODE, },
        { "NDMP4_TAPE_RDWR_MODE",       NDMP4_TAPE_RDWR_MODE, },
        { "NDMP4_TAPE_RAW_MODE",        NDMP4_TAPE_RAW_MODE, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_tape_mtio_op_table[];
extern char* ndmp4_tape_mtio_op_to_str(ndmp4_tape_mtio_op val);
extern int ndmp4_tape_mtio_op_from_str(ndmp4_tape_mtio_op* valp, char* str);

char* ndmp4_tape_mtio_op_to_str(ndmp4_tape_mtio_op val)
{
  return ndmp_enum_to_str((int)val, ndmp4_tape_mtio_op_table);
}

int ndmp4_tape_mtio_op_from_str(ndmp4_tape_mtio_op* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_tape_mtio_op_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_tape_mtio_op_table[] = {
        { "NDMP4_MTIO_FSF",             NDMP4_MTIO_FSF, },
        { "NDMP4_MTIO_BSF",             NDMP4_MTIO_BSF, },
        { "NDMP4_MTIO_FSR",             NDMP4_MTIO_FSR, },
        { "NDMP4_MTIO_BSR",             NDMP4_MTIO_BSR, },
        { "NDMP4_MTIO_REW",             NDMP4_MTIO_REW, },
        { "NDMP4_MTIO_EOF",             NDMP4_MTIO_EOF, },
        { "NDMP4_MTIO_OFF",             NDMP4_MTIO_OFF, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_mover_state_table[];
extern char* ndmp4_mover_state_to_str(ndmp4_mover_state val);
extern int ndmp4_mover_state_from_str(ndmp4_mover_state* valp, char* str);

char* ndmp4_mover_state_to_str(ndmp4_mover_state val)
{
  return ndmp_enum_to_str((int)val, ndmp4_mover_state_table);
}

int ndmp4_mover_state_from_str(ndmp4_mover_state* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_mover_state_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_mover_state_table[] = {
        { "NDMP4_MOVER_STATE_IDLE",     NDMP4_MOVER_STATE_IDLE, },
        { "NDMP4_MOVER_STATE_LISTEN",   NDMP4_MOVER_STATE_LISTEN, },
        { "NDMP4_MOVER_STATE_ACTIVE",   NDMP4_MOVER_STATE_ACTIVE, },
        { "NDMP4_MOVER_STATE_PAUSED",   NDMP4_MOVER_STATE_PAUSED, },
        { "NDMP4_MOVER_STATE_HALTED",   NDMP4_MOVER_STATE_HALTED, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_mover_pause_reason_table[];
extern char* ndmp4_mover_pause_reason_to_str(ndmp4_mover_pause_reason val);
extern int ndmp4_mover_pause_reason_from_str(ndmp4_mover_pause_reason* valp,
                                             char* str);

char* ndmp4_mover_pause_reason_to_str(ndmp4_mover_pause_reason val)
{
  return ndmp_enum_to_str((int)val, ndmp4_mover_pause_reason_table);
}

int ndmp4_mover_pause_reason_from_str(ndmp4_mover_pause_reason* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_mover_pause_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_mover_pause_reason_table[] = {
        { "NDMP4_MOVER_PAUSE_NA",       NDMP4_MOVER_PAUSE_NA, },
        { "NDMP4_MOVER_PAUSE_EOM",      NDMP4_MOVER_PAUSE_EOM, },
        { "NDMP4_MOVER_PAUSE_EOF",      NDMP4_MOVER_PAUSE_EOF, },
        { "NDMP4_MOVER_PAUSE_SEEK",     NDMP4_MOVER_PAUSE_SEEK, },
        { "NDMP4_MOVER_PAUSE_EOW",      NDMP4_MOVER_PAUSE_EOW, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_mover_halt_reason_table[];
extern char* ndmp4_mover_halt_reason_to_str(ndmp4_mover_halt_reason val);
extern int ndmp4_mover_halt_reason_from_str(ndmp4_mover_halt_reason* valp,
                                            char* str);

char* ndmp4_mover_halt_reason_to_str(ndmp4_mover_halt_reason val)
{
  return ndmp_enum_to_str((int)val, ndmp4_mover_halt_reason_table);
}

int ndmp4_mover_halt_reason_from_str(ndmp4_mover_halt_reason* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_mover_halt_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_mover_halt_reason_table[] = {
        { "NDMP4_MOVER_HALT_NA",        NDMP4_MOVER_HALT_NA, },
        { "NDMP4_MOVER_HALT_CONNECT_CLOSED",
                                        NDMP4_MOVER_HALT_CONNECT_CLOSED, },
        { "NDMP4_MOVER_HALT_ABORTED",   NDMP4_MOVER_HALT_ABORTED, },
        { "NDMP4_MOVER_HALT_INTERNAL_ERROR",
                                        NDMP4_MOVER_HALT_INTERNAL_ERROR, },
        { "NDMP4_MOVER_HALT_CONNECT_ERROR",
                                        NDMP4_MOVER_HALT_CONNECT_ERROR, },
        { "NDMP4_MOVER_HALT_MEDIA_ERROR",
                                        NDMP4_MOVER_HALT_MEDIA_ERROR, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_mover_mode_table[];
extern char* ndmp4_mover_mode_to_str(ndmp4_mover_mode val);
extern int ndmp4_mover_mode_from_str(ndmp4_mover_mode* valp, char* str);

char* ndmp4_mover_mode_to_str(ndmp4_mover_mode val)
{
  return ndmp_enum_to_str((int)val, ndmp4_mover_mode_table);
}

int ndmp4_mover_mode_from_str(ndmp4_mover_mode* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_mover_mode_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_mover_mode_table[] = {
        { "NDMP4_MOVER_MODE_READ",      NDMP4_MOVER_MODE_READ, },
        { "NDMP4_MOVER_MODE_WRITE",     NDMP4_MOVER_MODE_WRITE, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_data_operation_table[];
extern char* ndmp4_data_operation_to_str(ndmp4_data_operation val);
extern int ndmp4_data_operation_from_str(ndmp4_data_operation* valp, char* str);

char* ndmp4_data_operation_to_str(ndmp4_data_operation val)
{
  return ndmp_enum_to_str((int)val, ndmp4_data_operation_table);
}

int ndmp4_data_operation_from_str(ndmp4_data_operation* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_data_operation_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_data_operation_table[] = {
        { "NDMP4_DATA_OP_NOACTION",     NDMP4_DATA_OP_NOACTION, },
        { "NDMP4_DATA_OP_BACKUP",       NDMP4_DATA_OP_BACKUP, },
        { "NDMP4_DATA_OP_RECOVER",      NDMP4_DATA_OP_RECOVER, },
        { "NDMP4_DATA_OP_RECOVER_FILEHIST",
                                        NDMP4_DATA_OP_RECOVER_FILEHIST, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_data_state_table[];
extern char* ndmp4_data_state_to_str(ndmp4_data_state val);
extern int ndmp4_data_state_from_str(ndmp4_data_state* valp, char* str);

char* ndmp4_data_state_to_str(ndmp4_data_state val)
{
  return ndmp_enum_to_str((int)val, ndmp4_data_state_table);
}

int ndmp4_data_state_from_str(ndmp4_data_state* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_data_state_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_data_state_table[] = {
        { "NDMP4_DATA_STATE_IDLE",      NDMP4_DATA_STATE_IDLE, },
        { "NDMP4_DATA_STATE_ACTIVE",    NDMP4_DATA_STATE_ACTIVE, },
        { "NDMP4_DATA_STATE_HALTED",    NDMP4_DATA_STATE_HALTED, },
        { "NDMP4_DATA_STATE_LISTEN",    NDMP4_DATA_STATE_LISTEN, },
        { "NDMP4_DATA_STATE_CONNECTED", NDMP4_DATA_STATE_CONNECTED, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_data_halt_reason_table[];
extern char* ndmp4_data_halt_reason_to_str(ndmp4_data_halt_reason val);
extern int ndmp4_data_halt_reason_from_str(ndmp4_data_halt_reason* valp,
                                           char* str);

char* ndmp4_data_halt_reason_to_str(ndmp4_data_halt_reason val)
{
  return ndmp_enum_to_str((int)val, ndmp4_data_halt_reason_table);
}

int ndmp4_data_halt_reason_from_str(ndmp4_data_halt_reason* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_data_halt_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_data_halt_reason_table[] = {
        { "NDMP4_DATA_HALT_NA",         NDMP4_DATA_HALT_NA, },
        { "NDMP4_DATA_HALT_SUCCESSFUL", NDMP4_DATA_HALT_SUCCESSFUL, },
        { "NDMP4_DATA_HALT_ABORTED",    NDMP4_DATA_HALT_ABORTED, },
        { "NDMP4_DATA_HALT_INTERNAL_ERROR", NDMP4_DATA_HALT_INTERNAL_ERROR, },
        { "NDMP4_DATA_HALT_CONNECT_ERROR", NDMP4_DATA_HALT_CONNECT_ERROR, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_connection_status_reason_table[];
extern char* ndmp4_connection_status_reason_to_str(
    ndmp4_connection_status_reason val);
extern int ndmp4_connection_status_reason_from_str(
    ndmp4_connection_status_reason* valp,
    char* str);

char* ndmp4_connection_status_reason_to_str(ndmp4_connection_status_reason val)
{
  return ndmp_enum_to_str((int)val, ndmp4_connection_status_reason_table);
}

int ndmp4_connection_status_reason_from_str(
    ndmp4_connection_status_reason* valp,
    char* str)
{
  return ndmp_enum_from_str((int*)valp, str,
                            ndmp4_connection_status_reason_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_connection_status_reason_table[] = {
        { "NDMP4_CONNECTED",            NDMP4_CONNECTED, },
        { "NDMP4_SHUTDOWN",             NDMP4_SHUTDOWN, },
        { "NDMP4_REFUSED",              NDMP4_REFUSED, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_log_type_table[];
extern char* ndmp4_log_type_to_str(ndmp4_log_type val);
extern int ndmp4_log_type_from_str(ndmp4_log_type* valp, char* str);

char* ndmp4_log_type_to_str(ndmp4_log_type val)
{
  return ndmp_enum_to_str((int)val, ndmp4_log_type_table);
}

int ndmp4_log_type_from_str(ndmp4_log_type* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_log_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_log_type_table[] = {
        { "NDMP4_LOG_NORMAL",           NDMP4_LOG_NORMAL, },
        { "NDMP4_LOG_DEBUG",            NDMP4_LOG_DEBUG, },
        { "NDMP4_LOG_ERROR",            NDMP4_LOG_ERROR, },
        { "NDMP4_LOG_WARNING",          NDMP4_LOG_WARNING, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_fs_type_table[];
extern char* ndmp4_fs_type_to_str(ndmp4_fs_type val);
extern int ndmp4_fs_type_from_str(ndmp4_fs_type* valp, char* str);

char* ndmp4_fs_type_to_str(ndmp4_fs_type val)
{
  return ndmp_enum_to_str((int)val, ndmp4_fs_type_table);
}

int ndmp4_fs_type_from_str(ndmp4_fs_type* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_fs_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_fs_type_table[] = {
        { "NDMP4_FS_UNIX",              NDMP4_FS_UNIX, },
        { "NDMP4_FS_NT",                NDMP4_FS_NT, },
        { "NDMP4_FS_OTHER",             NDMP4_FS_OTHER, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_file_type_table[];
extern char* ndmp4_file_type_to_str(ndmp4_file_type val);
extern int ndmp4_file_type_from_str(ndmp4_file_type* valp, char* str);

char* ndmp4_file_type_to_str(ndmp4_file_type val)
{
  return ndmp_enum_to_str((int)val, ndmp4_file_type_table);
}

int ndmp4_file_type_from_str(ndmp4_file_type* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_file_type_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_file_type_table[] = {
        { "NDMP4_FILE_DIR",             NDMP4_FILE_DIR, },
        { "NDMP4_FILE_FIFO",            NDMP4_FILE_FIFO, },
        { "NDMP4_FILE_CSPEC",           NDMP4_FILE_CSPEC, },
        { "NDMP4_FILE_BSPEC",           NDMP4_FILE_BSPEC, },
        { "NDMP4_FILE_REG",             NDMP4_FILE_REG, },
        { "NDMP4_FILE_SLINK",           NDMP4_FILE_SLINK, },
        { "NDMP4_FILE_SOCK",            NDMP4_FILE_SOCK, },
        { "NDMP4_FILE_REGISTRY",        NDMP4_FILE_REGISTRY, },
        { "NDMP4_FILE_OTHER",           NDMP4_FILE_OTHER, },
        { 0 }
};
/* clang-format on */


extern struct ndmp_enum_str_table ndmp4_recovery_status_table[];
extern char* ndmp4_recovery_status_to_str(ndmp4_recovery_status val);
extern int ndmp4_recovery_status_from_str(ndmp4_recovery_status* valp,
                                          char* str);

char* ndmp4_recovery_status_to_str(ndmp4_recovery_status val)
{
  return ndmp_enum_to_str((int)val, ndmp4_recovery_status_table);
}

int ndmp4_recovery_status_from_str(ndmp4_recovery_status* valp, char* str)
{
  return ndmp_enum_from_str((int*)valp, str, ndmp4_recovery_status_table);
}

/* clang-format off */
struct ndmp_enum_str_table ndmp4_recovery_status_table[] = {
        { "NDMP4_RECOVERY_SUCCESSFUL",
                        NDMP4_RECOVERY_SUCCESSFUL, },
        { "NDMP4_RECOVERY_FAILED_PERMISSION",
                        NDMP4_RECOVERY_FAILED_PERMISSION, },
        { "NDMP4_RECOVERY_FAILED_NOT_FOUND",
                        NDMP4_RECOVERY_FAILED_NOT_FOUND, },
        { "NDMP4_RECOVERY_FAILED_NO_DIRECTORY",
                        NDMP4_RECOVERY_FAILED_NO_DIRECTORY, },
        { "NDMP4_RECOVERY_FAILED_OUT_OF_MEMORY",
                        NDMP4_RECOVERY_FAILED_OUT_OF_MEMORY, },
        { "NDMP4_RECOVERY_FAILED_IO_ERROR",
                        NDMP4_RECOVERY_FAILED_IO_ERROR, },
        { "NDMP4_RECOVERY_FAILED_UNDEFINED_ERROR",
                        NDMP4_RECOVERY_FAILED_UNDEFINED_ERROR, },
        { 0 }
};
/* clang-format on */

#endif /* !NDMOS_OPTION_NO_NDMP4 */
