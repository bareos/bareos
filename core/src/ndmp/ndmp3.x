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



/*
 * ndmp3.x
 *
 * Description   : NDMP protocol rpcgen file.
 *
 * Copyright (c) 1999 Intelliguard Software, Network Appliance.
 * All Rights Reserved.
 *
 * $Id: ndmp.x,v 1.11 1998/05/26 03:52:12 tim Exp $
 */

%#if __clang__
%#pragma clang diagnostic ignored "-Wunused-const-variable"
%#elif __GNUC__
%#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
%#pragma GCC diagnostic ignored "-Wunused-variable"
%#endif
%#endif

%#ifndef NDMOS_OPTION_NO_NDMP3

const NDMP3VER = 3;
const NDMP3PORT = 10000;

%#define ndmp3_u_quad uint64_t
%extern bool_t xdr_ndmp3_u_quad(register XDR *xdrs, ndmp3_u_quad *objp);

struct _ndmp3_u_quad
{
        uint32_t        high;
        uint32_t        low;
};

struct ndmp3_pval
{
        string          name<>;
        string          value<>;
};

enum ndmp3_error
{
        NDMP3_NO_ERR,                   /* No error */
        NDMP3_NOT_SUPPORTED_ERR,        /* Call is not supported */
        NDMP3_DEVICE_BUSY_ERR,          /* The device is in use */
        NDMP3_DEVICE_OPENED_ERR,        /* Another tape or scsi device
                                         * is already open */
        NDMP3_NOT_AUTHORIZED_ERR,       /* connection has not been authorized*/
        NDMP3_PERMISSION_ERR,           /* some sort of permission problem */
        NDMP3_DEV_NOT_OPEN_ERR,         /* SCSI device is not open */
        NDMP3_IO_ERR,                   /* I/O error */
        NDMP3_TIMEOUT_ERR,              /* command timed out */
        NDMP3_ILLEGAL_ARGS_ERR,         /* illegal arguments in request */
        NDMP3_NO_TAPE_LOADED_ERR,       /* Cannot open because there is
                                         * no tape loaded */
        NDMP3_WRITE_PROTECT_ERR,        /* tape cannot be open for write */
        NDMP3_EOF_ERR,                  /* Command encountered EOF */
        NDMP3_EOM_ERR,                  /* Command encountered EOM */
        NDMP3_FILE_NOT_FOUND_ERR,       /* File not found during restore */
        NDMP3_BAD_FILE_ERR,             /* The file descriptor is invalid */
        NDMP3_NO_DEVICE_ERR,            /* The device is not at that target */
        NDMP3_NO_BUS_ERR,               /* Invalid controller */
        NDMP3_XDR_DECODE_ERR,           /* Can't decode the request argument */
        NDMP3_ILLEGAL_STATE_ERR,        /* Call can't be done at this state */
        NDMP3_UNDEFINED_ERR,            /* Undefined Error */
        NDMP3_XDR_ENCODE_ERR,           /* Can't encode the reply argument */
        NDMP3_NO_MEM_ERR,               /* no memory */
        NDMP3_CONNECT_ERR               /* Error connecting to another
                                         * NDMP server */
};

enum ndmp3_header_message_type
{
        NDMP3_MESSAGE_REQUEST,
        NDMP3_MESSAGE_REPLY
};

enum ndmp3_message
{
        NDMP3_CONNECT_OPEN = 0x900,     /* CONNECT INTERFACE */
        NDMP3_CONNECT_CLIENT_AUTH = 0x901,
        NDMP3_CONNECT_CLOSE = 0x902,
        NDMP3_CONNECT_SERVER_AUTH = 0x903,

        NDMP3_CONFIG_GET_HOST_INFO = 0x100, /* CONFIG INTERFACE */
        NDMP3_CONFIG_GET_CONNECTION_TYPE = 0x102, /* NDMP2_CONFIG_GET_MOVER_TYPE on v2*/
        NDMP3_CONFIG_GET_AUTH_ATTR = 0x103,
        NDMP3_CONFIG_GET_BUTYPE_INFO = 0x104,   /* new from v3 */
        NDMP3_CONFIG_GET_FS_INFO = 0x105,       /* new from v3 */
        NDMP3_CONFIG_GET_TAPE_INFO = 0x106,     /* new from v3 */
        NDMP3_CONFIG_GET_SCSI_INFO = 0x107,     /* new from v3 */
        NDMP3_CONFIG_GET_SERVER_INFO =0x108,    /* new from v3 */

        NDMP3_SCSI_OPEN = 0x200,        /* SCSI INTERFACE */
        NDMP3_SCSI_CLOSE = 0x201,
        NDMP3_SCSI_GET_STATE = 0x202,
        NDMP3_SCSI_SET_TARGET = 0x203,
        NDMP3_SCSI_RESET_DEVICE = 0x204,
        NDMP3_SCSI_RESET_BUS = 0x205,
        NDMP3_SCSI_EXECUTE_CDB = 0x206,

        NDMP3_TAPE_OPEN = 0x300,        /* TAPE INTERFACE */
        NDMP3_TAPE_CLOSE = 0x301,
        NDMP3_TAPE_GET_STATE = 0x302,
        NDMP3_TAPE_MTIO = 0x303,
        NDMP3_TAPE_WRITE = 0x304,
        NDMP3_TAPE_READ = 0x305,
        NDMP3_TAPE_EXECUTE_CDB = 0x307,

        NDMP3_DATA_GET_STATE = 0x400,   /* DATA INTERFACE */
        NDMP3_DATA_START_BACKUP = 0x401,
        NDMP3_DATA_START_RECOVER = 0x402,
        NDMP3_DATA_ABORT = 0x403,
        NDMP3_DATA_GET_ENV = 0x404,
        NDMP3_DATA_STOP = 0x407,
        NDMP3_DATA_LISTEN = 0x409,      /* new from V3 */
        NDMP3_DATA_CONNECT = 0x40a,     /* new from V3 */
        NDMP3_DATA_START_RECOVER_FILEHIST = 0x40b, /* same as V3.1 */

        NDMP3_NOTIFY_DATA_HALTED =0x501,/* NOTIFY INTERFACE */
        NDMP3_NOTIFY_CONNECTED = 0x502,
        NDMP3_NOTIFY_MOVER_HALTED = 0x503,
        NDMP3_NOTIFY_MOVER_PAUSED = 0x504,
        NDMP3_NOTIFY_DATA_READ =0x505,

        NDMP3_LOG_FILE = 0x602,         /* LOGGING INTERFACE */
        NDMP3_LOG_MESSAGE = 0x603,      /* new from v3 */

        NDMP3_FH_ADD_FILE = 0x703,      /* FILE HISTORY INTERFACE */
        NDMP3_FH_ADD_DIR = 0x704,
        NDMP3_FH_ADD_NODE = 0x705,

        NDMP3_MOVER_GET_STATE = 0xa00,  /* MOVER INTERFACE */
        NDMP3_MOVER_LISTEN = 0xa01,
        NDMP3_MOVER_CONTINUE = 0xa02,
        NDMP3_MOVER_ABORT = 0xa03,
        NDMP3_MOVER_STOP = 0xa04,
        NDMP3_MOVER_SET_WINDOW = 0xa05,
        NDMP3_MOVER_READ = 0xa06,
        NDMP3_MOVER_CLOSE =0xa07,
        NDMP3_MOVER_SET_RECORD_SIZE =0xa08,
        NDMP3_MOVER_CONNECT =0xa09,     /* new from V3 */


        NDMP3_VENDORS_BASE = 0xf000,    /* Reserved for the vendor
                                         * specific usage
                                         * from 0xf000 to 0xfeff */

        NDMP3_RESERVED_BASE = 0xff00    /* Reserved for prototyping
                                         * from 0xff00 to 0xffff */
};

struct ndmp3_header
{
        uint32_t                sequence;       /* monotonically increasing */
        uint32_t                time_stamp;     /* time stamp of message */
        ndmp3_header_message_type message_type; /* what type of message */
        ndmp3_message           message;        /* message number */
        uint32_t                reply_sequence; /* reply is in response to */
        ndmp3_error             error;          /* communications errors */
};

/**********************/
/*  CONNECT INTERFACE */
/**********************/

/* NDMP3_CONNECT_OPEN */
struct ndmp3_connect_open_request
{
        uint16_t        protocol_version;       /* the version of protocol supported */
};

struct ndmp3_connect_open_reply
{
        ndmp3_error     error;
};

/* NDMP3_CONNECT_CLIENT_AUTH */
enum ndmp3_auth_type
{
        NDMP3_AUTH_NONE,                /* no password is required */
        NDMP3_AUTH_TEXT,                /* the clear text password */
        NDMP3_AUTH_MD5                  /* md5 */
};

struct ndmp3_auth_text
{
        string          auth_id<>;
        string          auth_password<>;

};

struct ndmp3_auth_md5
{
        string          auth_id<>;
        opaque          auth_digest[16];
};

union ndmp3_auth_data switch (enum ndmp3_auth_type auth_type)
{
    case NDMP3_AUTH_NONE:
        void;
    case NDMP3_AUTH_TEXT:
        struct ndmp3_auth_text  auth_text;
    case NDMP3_AUTH_MD5:
        struct ndmp3_auth_md5   auth_md5;
};

struct ndmp3_connect_client_auth_request
{
        ndmp3_auth_data auth_data;
};

struct ndmp3_connect_client_auth_reply
{
        ndmp3_error     error;
};


/* NDMP3_CONNECT_CLOSE */
/* no request arguments */
/* no reply arguments */

/* NDMP3_CONNECT_SERVER_AUTH */
union ndmp3_auth_attr switch (enum ndmp3_auth_type auth_type)
{
    case NDMP3_AUTH_NONE:
        void;
    case NDMP3_AUTH_TEXT:
        void;
    case NDMP3_AUTH_MD5:
        opaque  challenge[64];
};

struct ndmp3_connect_server_auth_request
{
        ndmp3_auth_attr client_attr;
};

struct ndmp3_connect_server_auth_reply
{
        ndmp3_error             error;
        ndmp3_auth_data server_result;
};


/********************/
/* CONFIG INTERFACE */
/********************/

/* NDMP3_CONFIG_GET_HOST_INFO */
/* no request arguments */
struct ndmp3_config_get_host_info_reply
{
        ndmp3_error     error;
        string          hostname<>;     /* host name */
        string          os_type<>;      /* The O/S type (e.g. SOLARIS) */
        string          os_vers<>;      /* The O/S version (e.g. 2.5) */
        string          hostid<>;
};

enum ndmp3_addr_type
{
        NDMP3_ADDR_LOCAL,
        NDMP3_ADDR_TCP,
        NDMP3_ADDR_FC,
        NDMP3_ADDR_IPC
};

/* NDMP3_CONFIG_GET_CONNECTION_TYPE */
/* no request arguments */
struct ndmp3_config_get_connection_type_reply
{
        ndmp3_error     error;
        ndmp3_addr_type addr_types<>;
};

/* NDMP3_CONFIG_GET_AUTH_ATTR */
struct ndmp3_config_get_auth_attr_request
{
        ndmp3_auth_type auth_type;
};

struct ndmp3_config_get_auth_attr_reply
{
        ndmp3_error     error;
        ndmp3_auth_attr server_attr;
};

/* NDMP3_CONFIG_GET_SERVER_INFO */
/* no requset arguments */
struct ndmp3_config_get_server_info_reply
{
        ndmp3_error     error;
        string          vendor_name<>;
        string          product_name<>;
        string          revision_number<>;
        ndmp3_auth_type auth_type<>;
};

/* backup type attributes */
const NDMP3_BUTYPE_BACKUP_FILE_HISTORY  = 0x0001;
const NDMP3_BUTYPE_BACKUP_FILELIST      = 0x0002;
const NDMP3_BUTYPE_RECOVER_FILELIST     = 0x0004;
const NDMP3_BUTYPE_BACKUP_DIRECT        = 0x0008;
const NDMP3_BUTYPE_RECOVER_DIRECT       = 0x0010;
const NDMP3_BUTYPE_BACKUP_INCREMENTAL   = 0x0020;
const NDMP3_BUTYPE_RECOVER_INCREMENTAL  = 0x0040;
const NDMP3_BUTYPE_BACKUP_UTF8          = 0x0080;
const NDMP3_BUTYPE_RECOVER_UTF8         = 0x0100;
const NDMP3_BUTYPE_RECOVER_FILE_HISTORY = 0x0200;

struct ndmp3_butype_info
{
        string          butype_name<>;
        ndmp3_pval      default_env<>;
        uint32_t        attrs;
};

/* NDMP3_CONFIG_GET_BUTYPE_INFO */
/* no request arguments */
struct ndmp3_config_get_butype_info_reply
{
        ndmp3_error     error;
        ndmp3_butype_info butype_info<>;
};

/* invalid bit */
const   NDMP3_FS_INFO_TOTAL_SIZE_INVALID        = 0x00000001;
const   NDMP3_FS_INFO_USED_SIZE_INVALID         = 0x00000002;
const   NDMP3_FS_INFO_AVAIL_SIZE_INVALID        = 0x00000004;
const   NDMP3_FS_INFO_TOTAL_INODES_INVALID      = 0x00000008;
const   NDMP3_FS_INFO_USED_INODES_INVALID       = 0x00000010;

struct ndmp3_fs_info
{
        uint32_t        invalid;
        string          fs_type<>;
        string          fs_logical_device<>;
        string          fs_physical_device<>;
        ndmp3_u_quad    total_size;
        ndmp3_u_quad    used_size;
        ndmp3_u_quad    avail_size;
        ndmp3_u_quad    total_inodes;
        ndmp3_u_quad    used_inodes;
        ndmp3_pval      fs_env<>;
        string          fs_status<>;
};

/* NDMP3_CONFIG_GET_FS_INFO */
/* no request arguments */
struct ndmp3_config_get_fs_info_reply
{
        ndmp3_error     error;
        ndmp3_fs_info   fs_info<>;
};

/* NDMP3_CONFIG_GET_TAPE_INFO */
/* no request arguments */
/* tape attributes */
const NDMP3_TAPE_ATTR_REWIND    = 0x00000001;
const NDMP3_TAPE_ATTR_UNLOAD    = 0x00000002;

struct ndmp3_device_capability
{
        string          device<>;
        uint32_t        attr;
        ndmp3_pval      capability<>;
};

struct ndmp3_device_info
{
        string                  model<>;
        ndmp3_device_capability caplist<>;

};
struct ndmp3_config_get_tape_info_reply
{
        ndmp3_error             error;
        ndmp3_device_info       tape_info<>;

};

/* NDMP3_CONFIG_GET_SCSI_INFO */
/* jukebox attributes */
struct ndmp3_config_get_scsi_info_reply
{
        ndmp3_error             error;
        ndmp3_device_info       scsi_info<>;
};

/******************/
/* SCSI INTERFACE */
/******************/

/* NDMP3_SCSI_OPEN */
struct ndmp3_scsi_open_request
{
        string          device<>;
};

struct ndmp3_scsi_open_reply
{
        ndmp3_error     error;
};

/* NDMP3_SCSI_CLOSE */
/* no request arguments */
struct ndmp3_scsi_close_reply
{
        ndmp3_error     error;
};

/* NDMP3_SCSI_GET_STATE */
/* no request arguments */
struct ndmp3_scsi_get_state_reply
{
        ndmp3_error     error;
        short           target_controller;
        short           target_id;
        short           target_lun;
};

/* NDMP3_SCSI_SET_TARGET */
struct ndmp3_scsi_set_target_request
{
        string          device<>;
        uint16_t        target_controller;
        uint16_t        target_id;
        uint16_t        target_lun;
};

struct ndmp3_scsi_set_target_reply
{
        ndmp3_error     error;
};

/* NDMP3_SCSI_RESET_DEVICE */
/* no request arguments */
struct ndmp3_scsi_reset_device_reply
{
        ndmp3_error     error;
};

/* NDMP3_SCSI_RESET_BUS */
/* no request arguments */
struct ndmp3_scsi_reset_bus_reply
{
        ndmp3_error     error;
};

/* NDMP3_SCSI_EXECUTE_CDB */
const NDMP3_SCSI_DATA_IN  = 0x00000001; /* Expect data from SCSI device */
const NDMP3_SCSI_DATA_OUT = 0x00000002; /* Transfer data to SCSI device */

struct ndmp3_execute_cdb_request
{
        uint32_t        flags;
        uint32_t        timeout;
        uint32_t        datain_len;     /* Set for expected datain */
        opaque          cdb<>;
        opaque          dataout<>;
};

struct ndmp3_execute_cdb_reply
{
        ndmp3_error     error;
        u_char          status;         /* SCSI status bytes */
        uint32_t        dataout_len;
        opaque          datain<>;       /* SCSI datain */
        opaque          ext_sense<>;    /* Extended sense data */
};
typedef ndmp3_execute_cdb_request       ndmp3_scsi_execute_cdb_request;
typedef ndmp3_execute_cdb_reply         ndmp3_scsi_execute_cdb_reply;

/******************/
/* TAPE INTERFACE */
/******************/
/* NDMP3_TAPE_OPEN */
enum ndmp3_tape_open_mode
{
        NDMP3_TAPE_READ_MODE,
        NDMP3_TAPE_RDWR_MODE
};

struct ndmp3_tape_open_request
{
        string                  device<>;
        ndmp3_tape_open_mode    mode;
};

struct ndmp3_tape_open_reply
{
        ndmp3_error     error;
};

/* NDMP3_TAPE_CLOSE */
/* no request arguments */
struct ndmp3_tape_close_reply
{
        ndmp3_error     error;
};

/*NDMP3_TAPE_GET_STATE */
/* no request arguments */
const NDMP3_TAPE_STATE_NOREWIND = 0x0008;       /* non-rewind device */
const NDMP3_TAPE_STATE_WR_PROT  = 0x0010;       /* write-protected */
const NDMP3_TAPE_STATE_ERROR    = 0x0020;       /* media error */
const NDMP3_TAPE_STATE_UNLOAD   = 0x0040;       /* tape will be unloaded when
                                                 * the device is closed */

/* invalid bit */
const NDMP3_TAPE_STATE_FILE_NUM_INVALID         = 0x00000001;
const NDMP3_TAPE_STATE_SOFT_ERRORS_INVALID      = 0x00000002;
const NDMP3_TAPE_STATE_BLOCK_SIZE_INVALID       = 0x00000004;
const NDMP3_TAPE_STATE_BLOCKNO_INVALID          = 0x00000008;
const NDMP3_TAPE_STATE_TOTAL_SPACE_INVALID      = 0x00000010;
const NDMP3_TAPE_STATE_SPACE_REMAIN_INVALID     = 0x00000020;
const NDMP3_TAPE_STATE_PARTITION_INVALID        = 0x00000040;

struct ndmp3_tape_get_state_reply
{
        uint32_t        invalid;
        ndmp3_error     error;
        uint32_t        flags;
        uint32_t        file_num;
        uint32_t        soft_errors;
        uint32_t        block_size;
        uint32_t        blockno;
        ndmp3_u_quad    total_space;
        ndmp3_u_quad    space_remain;
        uint32_t        partition;
};

/* NDMP3_TAPE_MTIO */
enum ndmp3_tape_mtio_op
{
        NDMP3_MTIO_FSF,
        NDMP3_MTIO_BSF,
        NDMP3_MTIO_FSR,
        NDMP3_MTIO_BSR,
        NDMP3_MTIO_REW,
        NDMP3_MTIO_EOF,
        NDMP3_MTIO_OFF
};

struct ndmp3_tape_mtio_request
{
        ndmp3_tape_mtio_op      tape_op;
        uint32_t                count;
};

struct ndmp3_tape_mtio_reply
{
        ndmp3_error     error;
        uint32_t        resid_count;
};

/* NDMP3_TAPE_WRITE */
struct ndmp3_tape_write_request
{
        opaque          data_out<>;
};

struct ndmp3_tape_write_reply
{
        ndmp3_error     error;
        uint32_t        count;
};

/* NDMP3_TAPE_READ */
struct ndmp3_tape_read_request
{
        uint32_t        count;
};

struct ndmp3_tape_read_reply
{
        ndmp3_error     error;
        opaque          data_in<>;
};


/* NDMP3_TAPE_EXECUTE_CDB */
typedef ndmp3_execute_cdb_request       ndmp3_tape_execute_cdb_request;
typedef ndmp3_execute_cdb_reply         ndmp3_tape_execute_cdb_reply;


/********************************/
/* MOVER INTERFACE              */
/********************************/
/* NDMP3_MOVER_GET_STATE */
enum ndmp3_mover_state
{
        NDMP3_MOVER_STATE_IDLE,
        NDMP3_MOVER_STATE_LISTEN,
        NDMP3_MOVER_STATE_ACTIVE,
        NDMP3_MOVER_STATE_PAUSED,
        NDMP3_MOVER_STATE_HALTED
};

enum ndmp3_mover_pause_reason
{
        NDMP3_MOVER_PAUSE_NA,
        NDMP3_MOVER_PAUSE_EOM,
        NDMP3_MOVER_PAUSE_EOF,
        NDMP3_MOVER_PAUSE_SEEK,
        NDMP3_MOVER_PAUSE_MEDIA_ERROR,
        NDMP3_MOVER_PAUSE_EOW
};

enum ndmp3_mover_halt_reason
{
        NDMP3_MOVER_HALT_NA,
        NDMP3_MOVER_HALT_CONNECT_CLOSED,
        NDMP3_MOVER_HALT_ABORTED,
        NDMP3_MOVER_HALT_INTERNAL_ERROR,
        NDMP3_MOVER_HALT_CONNECT_ERROR
};

/* mover address */
enum ndmp3_mover_mode
{
        NDMP3_MOVER_MODE_READ,  /* read from data connection; write to tape */
        NDMP3_MOVER_MODE_WRITE  /* write to data connection; read from tape */
};

struct ndmp3_tcp_addr
{
        uint32_t        ip_addr;
        uint16_t        port;
};

struct ndmp3_fc_addr
{
        uint32_t        loop_id;
};

struct ndmp3_ipc_addr
{
        opaque  comm_data<>;
};
union ndmp3_addr switch (ndmp3_addr_type addr_type)
{
    case NDMP3_ADDR_LOCAL:
        void;
    case NDMP3_ADDR_TCP:
        ndmp3_tcp_addr  tcp_addr;
    case NDMP3_ADDR_FC:
        ndmp3_fc_addr   fc_addr;
    case NDMP3_ADDR_IPC:
        ndmp3_ipc_addr  ipc_addr;

};

/* no request arguments */
struct ndmp3_mover_get_state_reply
{
        ndmp3_error             error;
        ndmp3_mover_state       state;
        ndmp3_mover_pause_reason pause_reason;
        ndmp3_mover_halt_reason halt_reason;
        uint32_t                record_size;
        uint32_t                record_num;
        ndmp3_u_quad            data_written;
        ndmp3_u_quad            seek_position;
        ndmp3_u_quad            bytes_left_to_read;
        ndmp3_u_quad            window_offset;
        ndmp3_u_quad            window_length;
        ndmp3_addr              data_connection_addr;
};

/* NDMP3_MOVER_LISTEN */
struct ndmp3_mover_listen_request
{
        ndmp3_mover_mode        mode;
        ndmp3_addr_type         addr_type;
};

struct ndmp3_mover_listen_reply
{
        ndmp3_error             error;
        ndmp3_addr              data_connection_addr;
};

/* NDMP3_MOVER_CONNECT */
struct ndmp3_mover_connect_request
{
        ndmp3_mover_mode        mode;
        ndmp3_addr              addr;
};

struct ndmp3_mover_connect_reply
{
        ndmp3_error     error;
};
/* NDMP3_MOVER_SET_RECORD_SIZE */
struct ndmp3_mover_set_record_size_request
{
        uint32_t        len;
};

struct ndmp3_mover_set_record_size_reply
{
        ndmp3_error     error;
};

/* NDMP3_MOVER_SET_WINDOW */
struct ndmp3_mover_set_window_request
{
        ndmp3_u_quad    offset;
        ndmp3_u_quad    length;
};

struct ndmp3_mover_set_window_reply
{
        ndmp3_error     error;
};

/* NDMP3_MOVER_CONTINUE */
/* no request arguments */
struct ndmp3_mover_continue_reply
{
        ndmp3_error     error;
};


/* NDMP3_MOVER_ABORT */
/* no request arguments */
struct ndmp3_mover_abort_reply
{
        ndmp3_error     error;
};

/* NDMP3_MOVER_STOP */
/* no request arguments */
struct ndmp3_mover_stop_reply
{
        ndmp3_error     error;
};

/* NDMP3_MOVER_READ */
struct ndmp3_mover_read_request
{
        ndmp3_u_quad    offset;
        ndmp3_u_quad    length;
};

struct ndmp3_mover_read_reply
{
        ndmp3_error     error;
};

/* NDMP3_MOVER_CLOSE */
/* no request arguments */
struct ndmp3_mover_close_reply
{
        ndmp3_error     error;
};

/********************************/
/* DATA INTERFACE               */
/********************************/
/* NDMP3_DATA_GET_STATE */
/* no request arguments */
enum ndmp3_data_operation
{
        NDMP3_DATA_OP_NOACTION,
        NDMP3_DATA_OP_BACKUP,
        NDMP3_DATA_OP_RESTORE,
        NDMP3_DATA_OP_RESTORE_FILEHIST
};

enum ndmp3_data_state
{
        NDMP3_DATA_STATE_IDLE,
        NDMP3_DATA_STATE_ACTIVE,
        NDMP3_DATA_STATE_HALTED,
        NDMP3_DATA_STATE_LISTEN,
        NDMP3_DATA_STATE_CONNECTED
};

enum ndmp3_data_halt_reason
{
        NDMP3_DATA_HALT_NA,
        NDMP3_DATA_HALT_SUCCESSFUL,
        NDMP3_DATA_HALT_ABORTED,
        NDMP3_DATA_HALT_INTERNAL_ERROR,
        NDMP3_DATA_HALT_CONNECT_ERROR
};

/* invalid bit */
const NDMP3_DATA_STATE_EST_BYTES_REMAIN_INVALID = 0x00000001;
const NDMP3_DATA_STATE_EST_TIME_REMAIN_INVALID  = 0x00000002;

struct ndmp3_data_get_state_reply
{
        uint32_t                invalid;
        ndmp3_error             error;
        ndmp3_data_operation    operation;
        ndmp3_data_state        state;
        ndmp3_data_halt_reason  halt_reason;
        ndmp3_u_quad            bytes_processed;
        ndmp3_u_quad            est_bytes_remain;
        uint32_t                est_time_remain;
        ndmp3_addr              data_connection_addr;
        ndmp3_u_quad            read_offset;
        ndmp3_u_quad            read_length;
};

/* NDMP3_DATA_START_BACKUP */
struct ndmp3_data_start_backup_request
{
        string          bu_type<>;      /* backup method to use */
        ndmp3_pval      env<>;          /* Parameters that may modify backup */
};

struct ndmp3_data_start_backup_reply
{
        ndmp3_error     error;
};

/* NDMP3_DATA_START_RECOVER */
struct ndmp3_name
{
        string          original_path<>;
        string          destination_dir<>;
        string          new_name<>;     /* Direct access restore only */
        string          other_name<>;   /* Direct access restore only */
        ndmp3_u_quad    node;           /* Direct access restore only */
        ndmp3_u_quad    fh_info;        /* Direct access restore only */
};

struct ndmp3_data_start_recover_request
{
        ndmp3_pval      env<>;
        ndmp3_name      nlist<>;
        string          bu_type<>;
};

struct ndmp3_data_start_recover_reply
{
        ndmp3_error     error;
};

/* NDMP3_DATA_START_RECOVER_FILEHIST */
typedef ndmp3_data_start_recover_request ndmp3_data_start_recover_filehist_request;
typedef ndmp3_data_start_recover_reply ndmp3_data_start_recover_filehist_reply;


/* NDMP3_DATA_ABORT */
/* no request arguments */
struct ndmp3_data_abort_reply
{
        ndmp3_error     error;
};

/* NDMP3_DATA_STOP */
/* no request arguments */
struct ndmp3_data_stop_reply
{
        ndmp3_error     error;
};

/* NDMP3_DATA_GET_ENV */
/* no request arguments */
struct ndmp3_data_get_env_reply
{
        ndmp3_error     error;
        ndmp3_pval      env<>;
};

/* NDMP3_DATA_LISTEN */
struct ndmp3_data_listen_request
{
        ndmp3_addr_type addr_type;
};

struct ndmp3_data_listen_reply
{
        ndmp3_error     error;
        ndmp3_addr      data_connection_addr;
};

/* NDMP3_DATA_CONNECT */
struct ndmp3_data_connect_request
{
        ndmp3_addr      addr;
};
struct ndmp3_data_connect_reply
{
        ndmp3_error     error;
};

/****************************************/
/* NOTIFY INTERFACE                     */
/****************************************/
/* NDMP3_NOTIFY_DATA_HALTED */
struct ndmp3_notify_data_halted_request
{
        ndmp3_data_halt_reason  reason;
        string                  text_reason<>;
};

/* NDMP3_NOTIFY_CONNECTED */
enum ndmp3_connect_reason
{
        NDMP3_CONNECTED,        /* Connect sucessfully */
        NDMP3_SHUTDOWN,         /* Connection shutdown */
        NDMP3_REFUSED           /* reach the maximum number of connections */
};

struct ndmp3_notify_connected_request
{
        ndmp3_connect_reason    reason;
        uint16_t                protocol_version;
        string                  text_reason<>;
};

/* NDMP3_NOTIFY_MOVER_PAUSED */
struct ndmp3_notify_mover_paused_request
{
        ndmp3_mover_pause_reason reason;
        ndmp3_u_quad            seek_position;
};
/* No reply */

/* NDMP3_NOTIFY_MOVER_HALTED */
struct ndmp3_notify_mover_halted_request
{
        ndmp3_mover_halt_reason reason;
        string                  text_reason<>;
};
/* No reply */

/* NDMP3_NOTIFY_DATA_READ */
struct ndmp3_notify_data_read_request
{
        ndmp3_u_quad    offset;
        ndmp3_u_quad    length;
};
/* No reply */

/********************************/
/* LOG INTERFACE                */
/********************************/
/* NDMP3_LOG_MESSAGE */
enum ndmp3_log_type
{
        NDMP3_LOG_NORMAL,
        NDMP3_LOG_DEBUG,
        NDMP3_LOG_ERROR,
        NDMP3_LOG_WARNING
};


struct ndmp3_log_message_request
{
        ndmp3_log_type  log_type;
        uint32_t        message_id;
        string          entry<>;
};
/* No reply */

/* NDMP3_LOG_FILE */
struct ndmp3_log_file_request
{
        string          name<>;
        ndmp3_error     error;
};
/* No reply */


/*****************************/
/* File History INTERFACE    */
/*****************************/
/* NDMP3_FH_ADD_FILE */
enum ndmp3_fs_type
{
        NDMP3_FS_UNIX,  /* UNIX */
        NDMP3_FS_NT,    /* NT */
        NDMP3_FS_OTHER
};

typedef string ndmp3_path<>;
struct ndmp3_nt_path
{
        ndmp3_path      nt_path;
        ndmp3_path      dos_path;
};

union ndmp3_file_name switch (ndmp3_fs_type fs_type)
{
    case NDMP3_FS_UNIX:
        ndmp3_path      unix_name;
    case NDMP3_FS_NT:
        ndmp3_nt_path   nt_name;
    default:
        ndmp3_path      other_name;
};

enum ndmp3_file_type
{
        NDMP3_FILE_DIR,
        NDMP3_FILE_FIFO,
        NDMP3_FILE_CSPEC,
        NDMP3_FILE_BSPEC,
        NDMP3_FILE_REG,
        NDMP3_FILE_SLINK,
        NDMP3_FILE_SOCK,
        NDMP3_FILE_REGISTRY,
        NDMP3_FILE_OTHER
};

/* invalid bit */
const NDMP3_FILE_STAT_ATIME_INVALID     = 0x00000001;
const NDMP3_FILE_STAT_CTIME_INVALID     = 0x00000002;
const NDMP3_FILE_STAT_GROUP_INVALID     = 0x00000004;

struct ndmp3_file_stat
{
        uint32_t                invalid;
        ndmp3_fs_type           fs_type;
        ndmp3_file_type         ftype;
        uint32_t                mtime;
        uint32_t                atime;
        uint32_t                ctime;
        uint32_t                owner; /* uid for UNIX, owner for NT */
        uint32_t                group; /* gid for UNIX, NA for NT */
        uint32_t                fattr; /* mode for UNIX, fattr for NT */
        ndmp3_u_quad            size;
        uint32_t                links;
};


/* one file could have both UNIX and NT name and attributes */
struct ndmp3_file
{
        ndmp3_file_name         names<>;
        ndmp3_file_stat         stats<>;
        ndmp3_u_quad            node;   /* used for the direct access */
        ndmp3_u_quad            fh_info;/* used for the direct access */
};

struct ndmp3_fh_add_file_request
{
        ndmp3_file              files<>;
};

/* No reply */
/* NDMP3_FH_ADD_DIR */

struct ndmp3_dir
{
        ndmp3_file_name         names<>;
        ndmp3_u_quad            node;
        ndmp3_u_quad            parent;
};

struct ndmp3_fh_add_dir_request
{
        ndmp3_dir               dirs<>;
};
/* No reply */

/* NDMP3_FH_ADD_NODE */
struct ndmp3_node
{
        ndmp3_file_stat         stats<>;
        ndmp3_u_quad            node;
        ndmp3_u_quad            fh_info;
};

struct ndmp3_fh_add_node_request
{
        ndmp3_node              nodes<>;
};
/* No reply */

%#endif /* !NDMOS_OPTION_NO_NDMP3 */
