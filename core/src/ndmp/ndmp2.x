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


/*
 * Copyright (c) 1997 Network Appliance. All Rights Reserved.
 *
 * Network Appliance makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 *
 */

%#if __clang__
%#pragma clang diagnostic ignored "-Wunused-const-variable"
%#elif __GNUC__
%#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
%#pragma GCC diagnostic ignored "-Wunused-variable"
%#endif
%#endif

%#ifndef NDMOS_OPTION_NO_NDMP2

const NDMP2VER = 2;
const NDMP2PORT = 10000;

%#define ndmp2_u_quad uint64_t
%extern bool_t xdr_ndmp2_u_quad(register XDR *xdrs, ndmp2_u_quad *objp);

struct _ndmp2_u_quad
{
	uint32_t	high;
	uint32_t	low;
};

struct ndmp2_pval
{
	string		name<>;
	string		value<>;
};

struct ndmp2_scsi_device
{
	string		name<>;
};

struct ndmp2_tape_device
{
	string		name<>;
};

enum ndmp2_error
{
	NDMP2_NO_ERR,			/* No error */
	NDMP2_NOT_SUPPORTED_ERR,	/* Call is not supported */
	NDMP2_DEVICE_BUSY_ERR,		/* The device is in use */
	NDMP2_DEVICE_OPENED_ERR,	/* Another tape or scsi device
					 * is already open */
	NDMP2_NOT_AUTHORIZED_ERR,	/* connection has not been authorized*/
	NDMP2_PERMISSION_ERR,		/* some sort of permission problem */
	NDMP2_DEV_NOT_OPEN_ERR,		/* SCSI device is not open */
	NDMP2_IO_ERR,			/* I/O error */
	NDMP2_TIMEOUT_ERR,		/* command timed out */
	NDMP2_ILLEGAL_ARGS_ERR,		/* illegal arguments in request */
	NDMP2_NO_TAPE_LOADED_ERR,	/* Cannot open because there is
					   no tape loaded */
	NDMP2_WRITE_PROTECT_ERR,	/* tape cannot be open for write */
	NDMP2_EOF_ERR,			/* Command encountered EOF */
	NDMP2_EOM_ERR,			/* Command encountered EOM */
	NDMP2_FILE_NOT_FOUND_ERR,	/* File not found during restore */
	NDMP2_BAD_FILE_ERR,		/* The file descriptor is invalid */
	NDMP2_NO_DEVICE_ERR,		/* The device is not at that target */
	NDMP2_NO_BUS_ERR,		/* Invalid controller */
	NDMP2_XDR_DECODE_ERR,		/* Can't decode the request argument */
	NDMP2_ILLEGAL_STATE_ERR,	/* Call can't be done at this state */
	NDMP2_UNDEFINED_ERR,		/* Undefined Error */
	NDMP2_XDR_ENCODE_ERR,		/* Can't encode the reply argument */
	NDMP2_NO_MEM_ERR		/* no memory */
};

enum ndmp2_header_message_type
{
	NDMP2_MESSAGE_REQUEST,
	NDMP2_MESSAGE_REPLY
};

enum ndmp2_message
{
	NDMP2_CONNECT_OPEN = 0x900,	/* CONNECT INTERFACE */
	NDMP2_CONNECT_CLIENT_AUTH,
	NDMP2_CONNECT_CLOSE,
	NDMP2_CONNECT_SERVER_AUTH,

	NDMP2_CONFIG_GET_HOST_INFO = 0x100, /* CONFIG INTERFACE */
	NDMP2_CONFIG_GET_BUTYPE_ATTR,
	NDMP2_CONFIG_GET_MOVER_TYPE,
	NDMP2_CONFIG_GET_AUTH_ATTR,

	NDMP2_SCSI_OPEN = 0x200,	/* SCSI INTERFACE */
	NDMP2_SCSI_CLOSE,
	NDMP2_SCSI_GET_STATE,
	NDMP2_SCSI_SET_TARGET,
	NDMP2_SCSI_RESET_DEVICE,
	NDMP2_SCSI_RESET_BUS,
	NDMP2_SCSI_EXECUTE_CDB,

	NDMP2_TAPE_OPEN = 0x300,	/* TAPE INTERFACE */
	NDMP2_TAPE_CLOSE,
	NDMP2_TAPE_GET_STATE,
	NDMP2_TAPE_MTIO,
	NDMP2_TAPE_WRITE,
	NDMP2_TAPE_READ,
	NDMP2_TAPE_RESVD1,
	NDMP2_TAPE_EXECUTE_CDB,

	NDMP2_DATA_GET_STATE = 0x400,	/* DATA INTERFACE */
	NDMP2_DATA_START_BACKUP,
	NDMP2_DATA_START_RECOVER,
	NDMP2_DATA_ABORT,
	NDMP2_DATA_GET_ENV,
	NDMP2_DATA_RESVD1,
	NDMP2_DATA_RESVD2,
	NDMP2_DATA_STOP,
	NDMP2_DATA_START_RECOVER_FILEHIST = 0x40b, /* same as V3.1 */

	NDMP2_NOTIFY_RESVD1 = 0x500,	/* NOTIFY INTERFACE */
	NDMP2_NOTIFY_DATA_HALTED,
	NDMP2_NOTIFY_CONNECTED,
	NDMP2_NOTIFY_MOVER_HALTED,
	NDMP2_NOTIFY_MOVER_PAUSED,
	NDMP2_NOTIFY_DATA_READ,

	NDMP2_LOG_LOG = 0x600,		/* LOGGING INTERFACE */
	NDMP2_LOG_DEBUG,
	NDMP2_LOG_FILE,

	NDMP2_FH_ADD_UNIX_PATH = 0x700,	/* FILE HISTORY INTERFACE */
	NDMP2_FH_ADD_UNIX_DIR,
	NDMP2_FH_ADD_UNIX_NODE,

	NDMP2_MOVER_GET_STATE = 0xa00,	/* MOVER INTERFACE */
	NDMP2_MOVER_LISTEN,
	NDMP2_MOVER_CONTINUE,
	NDMP2_MOVER_ABORT,
	NDMP2_MOVER_STOP,
	NDMP2_MOVER_SET_WINDOW,
	NDMP2_MOVER_READ,
	NDMP2_MOVER_CLOSE,
	NDMP2_MOVER_SET_RECORD_SIZE,

	NDMP2_RESERVED = 0xff00		/* Reserved for prototyping */
};

struct ndmp2_header
{
	uint32_t		sequence;	/* monotonically increasing */
	uint32_t		time_stamp;	/* time stamp of message */
	ndmp2_header_message_type message_type;	/* what type of message */
	ndmp2_message		message;	/* message number */
	uint32_t		reply_sequence;	/* reply is in response to */
	ndmp2_error		error;		/* communications errors */
};

/**********************/
/*  CONNECT INTERFACE */
/**********************/
/* NDMP2_CONNECT_OPEN */
struct ndmp2_connect_open_request
{
	uint16_t	protocol_version;	/* the version of protocol supported */
};

struct ndmp2_connect_open_reply
{
	ndmp2_error	error;
};

/* NDMP2_CONNECT_CLIENT_AUTH */
enum ndmp2_auth_type
{
	NDMP2_AUTH_NONE,		/* no password is required */
	NDMP2_AUTH_TEXT,		/* the clear text password */
	NDMP2_AUTH_MD5			/* md5 */
};

struct ndmp2_auth_text
{
	string		auth_id<>;
	string		auth_password<>;
};

struct ndmp2_auth_md5
{
	string		auth_id<>;
	opaque		auth_digest[16];
};

union ndmp2_auth_data switch (enum ndmp2_auth_type auth_type)
{
    case NDMP2_AUTH_NONE:
	void;
    case NDMP2_AUTH_TEXT:
	ndmp2_auth_text	auth_text;
    case NDMP2_AUTH_MD5:
	ndmp2_auth_md5	auth_md5;
};

struct ndmp2_connect_client_auth_request
{
	ndmp2_auth_data	auth_data;
};

struct ndmp2_connect_client_auth_reply
{
	ndmp2_error	error;
};

/* NDMP2_CONNECT_CLOSE */
/* no request arguments */
/* no reply arguments */

/* NDMP2_CONNECT_SERVER_AUTH */
union ndmp2_auth_attr switch (enum ndmp2_auth_type auth_type)
{
    case NDMP2_AUTH_NONE:
	void;
    case NDMP2_AUTH_TEXT:
	void;
    case NDMP2_AUTH_MD5:
	opaque	challenge[64];
};

struct ndmp2_connect_server_auth_request
{
	ndmp2_auth_attr	client_attr;
};

struct ndmp2_connect_server_auth_reply
{
	ndmp2_error	error;
	ndmp2_auth_data	auth_result;
};

/********************/
/* CONFIG INTERFACE */
/********************/
/* NDMP2_CONFIG_GET_HOST_INFO */
/* no request arguments */
struct ndmp2_config_get_host_info_reply
{
	ndmp2_error	error;
	string		hostname<>;	/* host name */
	string		os_type<>;	/* The O/S type (e.g. SOLARIS) */
	string		os_vers<>;	/* The O/S version (e.g. 2.5) */
	string		hostid<>;
	ndmp2_auth_type	auth_type<>;
};

/* NDMP2_CONFIG_GET_BUTYPE_ATTR */
const NDMP2_NO_BACKUP_FILELIST	= 0x0001;
const NDMP2_NO_BACKUP_FHINFO	= 0x0002;
const NDMP2_NO_RECOVER_FILELIST	= 0x0004;
const NDMP2_NO_RECOVER_FHINFO	= 0x0008;
const NDMP2_NO_RECOVER_RESVD	= 0x0010;
const NDMP2_NO_RECOVER_INC_ONLY	= 0x0020;

struct ndmp2_config_get_butype_attr_request
{
	string		name<>;		/* backup type name */
};

struct ndmp2_config_get_butype_attr_reply
{
	ndmp2_error	error;
	uint32_t	attrs;
};

/* NDMP2_CONFIG_GET_MOVER_TYPE */
/* no request arguments */
enum ndmp2_mover_addr_type
{
	NDMP2_ADDR_LOCAL,
	NDMP2_ADDR_TCP
};

struct ndmp2_config_get_mover_type_reply
{
	ndmp2_error		error;
	ndmp2_mover_addr_type	methods<>;
};

/* NDMP2_CONFIG_GET_AUTH_ATTR */
struct ndmp2_config_get_auth_attr_request
{
	ndmp2_auth_type	auth_type;
};

struct ndmp2_config_get_auth_attr_reply
{
	ndmp2_error	error;
	ndmp2_auth_attr	server_attr;
};

/******************/
/* SCSI INTERFACE */
/******************/
/* NDMP2_SCSI_OPEN */
struct ndmp2_scsi_open_request
{
	ndmp2_scsi_device device;
};

struct ndmp2_scsi_open_reply
{
	ndmp2_error	error;
};

/* NDMP2_SCSI_CLOSE */
/* no request arguments */
struct ndmp2_scsi_close_reply
{
	ndmp2_error	error;
};

/* NDMP2_SCSI_GET_STATE */
/* no request arguments */
struct ndmp2_scsi_get_state_reply
{
	ndmp2_error	error;
	short		target_controller;
	short		target_id;
	short		target_lun;
};

/* NDMP2_SCSI_SET_TARGET */
struct ndmp2_scsi_set_target_request
{
	ndmp2_scsi_device device;
	uint16_t	target_controller;
	uint16_t	target_id;
	uint16_t	target_lun;
};

struct ndmp2_scsi_set_target_reply
{
	ndmp2_error	error;
};

/* NDMP2_SCSI_RESET_DEVICE */
/* no request arguments */
struct ndmp2_scsi_reset_device_reply
{
	ndmp2_error	error;
};

/* NDMP2_SCSI_RESET_BUS */
/* no request arguments */
struct ndmp2_scsi_reset_bus_reply
{
	ndmp2_error	error;
};

/* NDMP2_SCSI_EXECUTE_CDB */
const NDMP2_SCSI_DATA_IN  = 0x00000001;	/* Expect data from SCSI device */
const NDMP2_SCSI_DATA_OUT = 0x00000002;	/* Transfer data to SCSI device */

struct ndmp2_execute_cdb_request
{
	uint32_t	flags;
	uint32_t	timeout;
	uint32_t	datain_len;	/* Set for expected datain */
	opaque		cdb<>;
	opaque		dataout<>;
};

struct ndmp2_execute_cdb_reply
{
	ndmp2_error	error;
	u_char		status;		/* SCSI status bytes */
	uint32_t	dataout_len;
	opaque		datain<>;	/* SCSI datain */
	opaque		ext_sense<>;	/* Extended sense data */
};
typedef ndmp2_execute_cdb_request	ndmp2_scsi_execute_cdb_request;
typedef ndmp2_execute_cdb_reply		ndmp2_scsi_execute_cdb_reply;


/******************/
/* TAPE INTERFACE */
/******************/
/* NDMP2_TAPE_OPEN */
enum ndmp2_tape_open_mode
{
	NDMP2_TAPE_READ_MODE,
	NDMP2_TAPE_WRITE_MODE
};

struct ndmp2_tape_open_request
{
	ndmp2_tape_device	device;
	ndmp2_tape_open_mode	mode;
};

struct ndmp2_tape_open_reply
{
	ndmp2_error	error;
};

/* NDMP2_TAPE_CLOSE */
/* no request arguments */
struct ndmp2_tape_close_reply
{
	ndmp2_error	error;
};

/* NDMP2_TAPE_GET_STATE */
/* no request arguments */
const NDMP2_TAPE_NOREWIND = 0x0008;	/* non-rewind device */
const NDMP2_TAPE_WR_PROT  = 0x0010;	/* write-protected */
const NDMP2_TAPE_ERROR    = 0x0020;	/* media error */
const NDMP2_TAPE_UNLOAD   = 0x0040;	/* tape will be unloaded when
					 * the device is closed */
struct ndmp2_tape_get_state_reply
{
	ndmp2_error	error;
	uint32_t	flags;
	uint32_t	file_num;
	uint32_t	soft_errors;
	uint32_t	block_size;
	uint32_t	blockno;
	ndmp2_u_quad	total_space;
	ndmp2_u_quad	space_remain;
};

/* NDMP2_TAPE_MTIO */
enum ndmp2_tape_mtio_op
{
	NDMP2_MTIO_FSF,
	NDMP2_MTIO_BSF,
	NDMP2_MTIO_FSR,
	NDMP2_MTIO_BSR,
	NDMP2_MTIO_REW,
	NDMP2_MTIO_EOF,
	NDMP2_MTIO_OFF
};

struct ndmp2_tape_mtio_request
{
	ndmp2_tape_mtio_op	tape_op;
	uint32_t		count;
};

struct ndmp2_tape_mtio_reply
{
	ndmp2_error	error;
	uint32_t	resid_count;
};

/* NDMP2_TAPE_WRITE */
struct ndmp2_tape_write_request
{
	opaque		data_out<>;
};

struct ndmp2_tape_write_reply
{
	ndmp2_error	error;
	uint32_t	count;
};

/* NDMP2_TAPE_READ */
struct ndmp2_tape_read_request
{
	uint32_t	count;
};

struct ndmp2_tape_read_reply
{
	ndmp2_error	error;
	opaque		data_in<>;
};

/* NDMP2_TAPE_EXECUTE_CDB */
typedef ndmp2_execute_cdb_request	ndmp2_tape_execute_cdb_request;
typedef ndmp2_execute_cdb_reply		ndmp2_tape_execute_cdb_reply;

/********************************/
/* MOVER INTERFACE              */
/********************************/
/* NDMP2_MOVER_GET_STATE */
enum ndmp2_mover_state
{
	NDMP2_MOVER_STATE_IDLE,
	NDMP2_MOVER_STATE_LISTEN,
	NDMP2_MOVER_STATE_ACTIVE,
	NDMP2_MOVER_STATE_PAUSED,
	NDMP2_MOVER_STATE_HALTED
};

enum ndmp2_mover_pause_reason
{
	NDMP2_MOVER_PAUSE_NA,
	NDMP2_MOVER_PAUSE_EOM,
	NDMP2_MOVER_PAUSE_EOF,
	NDMP2_MOVER_PAUSE_SEEK,
	NDMP2_MOVER_PAUSE_MEDIA_ERROR
};

enum ndmp2_mover_halt_reason
{
	NDMP2_MOVER_HALT_NA,
	NDMP2_MOVER_HALT_CONNECT_CLOSED,
	NDMP2_MOVER_HALT_ABORTED,
	NDMP2_MOVER_HALT_INTERNAL_ERROR,
	NDMP2_MOVER_HALT_CONNECT_ERROR
};

/* no request arguments */
struct ndmp2_mover_get_state_reply
{
	ndmp2_error		error;
	ndmp2_mover_state	state;
	ndmp2_mover_pause_reason pause_reason;
	ndmp2_mover_halt_reason	halt_reason;
	uint32_t		record_size;
	uint32_t		record_num;
	ndmp2_u_quad		data_written;
	ndmp2_u_quad		seek_position;
	ndmp2_u_quad		bytes_left_to_read;
	ndmp2_u_quad		window_offset;
	ndmp2_u_quad		window_length;
};

/* NDMP2_MOVER_LISTEN */
enum ndmp2_mover_mode
{
	NDMP2_MOVER_MODE_READ,	/* read from data conn; write to tape */
	NDMP2_MOVER_MODE_WRITE,	/* write to data conn; read from tape */
	NDMP2_MOVER_MODE_DATA	/* write to data conn; read from data conn */
};

struct ndmp2_mover_tcp_addr
{
	uint32_t	ip_addr;
	uint16_t	port;
};

union ndmp2_mover_addr switch (ndmp2_mover_addr_type addr_type)
{
    case NDMP2_ADDR_LOCAL:
	void;
    case NDMP2_ADDR_TCP:
	ndmp2_mover_tcp_addr	addr;
};

struct ndmp2_mover_listen_request
{
	ndmp2_mover_mode	mode;
	ndmp2_mover_addr_type	addr_type;
};

struct ndmp2_mover_listen_reply
{
	ndmp2_error		error;
	ndmp2_mover_addr	mover;
};

/* NDMP2_MOVER_SET_RECORD_SIZE */
struct ndmp2_mover_set_record_size_request
{
	uint32_t	len;
};

struct ndmp2_mover_set_record_size_reply
{
	ndmp2_error	error;
};

/* NDMP2_MOVER_SET_WINDOW */
struct ndmp2_mover_set_window_request
{
	ndmp2_u_quad	offset;
	ndmp2_u_quad	length;
};

struct ndmp2_mover_set_window_reply
{
	ndmp2_error	error;
};

/* NDMP2_MOVER_CONTINUE */
/* no request arguments */
struct ndmp2_mover_continue_reply
{
	ndmp2_error	error;
};

/* NDMP2_MOVER_ABORT */
/* no request arguments */
struct ndmp2_mover_abort_reply
{
	ndmp2_error	error;
};

/* NDMP2_MOVER_STOP */
/* no request arguments */
struct ndmp2_mover_stop_reply
{
	ndmp2_error	error;
};

/* NDMP2_MOVER_READ */
struct ndmp2_mover_read_request
{
	ndmp2_u_quad	offset;
	ndmp2_u_quad	length;
};

struct ndmp2_mover_read_reply
{
	ndmp2_error	error;
};

/* NDMP2_MOVER_CLOSE */
/* no request arguments */
struct ndmp2_mover_close_reply
{
	ndmp2_error	error;
};

/****************************/
/* DATA INTERFACE	    */
/****************************/
/* NDMP2_DATA_GET_STATE */
/* no request arguments */
enum ndmp2_data_operation
{
	NDMP2_DATA_OP_NOACTION,
	NDMP2_DATA_OP_BACKUP,
	NDMP2_DATA_OP_RESTORE,
	NDMP2_DATA_OP_RESTORE_FILEHIST
};

enum ndmp2_data_state
{
	NDMP2_DATA_STATE_IDLE,
	NDMP2_DATA_STATE_ACTIVE,
	NDMP2_DATA_STATE_HALTED
};

enum ndmp2_data_halt_reason
{
	NDMP2_DATA_HALT_NA,
	NDMP2_DATA_HALT_SUCCESSFUL,
	NDMP2_DATA_HALT_ABORTED,
	NDMP2_DATA_HALT_INTERNAL_ERROR,
	NDMP2_DATA_HALT_CONNECT_ERROR
};

struct ndmp2_data_get_state_reply
{
	ndmp2_error		error;
	ndmp2_data_operation	operation;
	ndmp2_data_state	state;
	ndmp2_data_halt_reason	halt_reason;
	ndmp2_u_quad		bytes_processed;
	ndmp2_u_quad		est_bytes_remain;
	uint32_t		est_time_remain;
	ndmp2_mover_addr	mover;
	ndmp2_u_quad		read_offset;
	ndmp2_u_quad		read_length;
};

/* NDMP2_DATA_START_BACKUP */
struct ndmp2_data_start_backup_request
{
	ndmp2_mover_addr mover;		/* mover to receive data */
	string		bu_type<>;	/* backup method to use */
	ndmp2_pval	env<>;		/* Parameters that may modify backup */
};

struct ndmp2_data_start_backup_reply
{
	ndmp2_error	error;
};

/* NDMP2_DATA_START_RECOVER */
struct ndmp2_name
{
	string		name<>;
	string		dest<>;
	uint16_t	ssid;
	ndmp2_u_quad	fh_info;
};

struct ndmp2_data_start_recover_request
{
	ndmp2_mover_addr mover;
	ndmp2_pval	env<>;
	ndmp2_name	nlist<>;
	string		bu_type<>;
};

struct ndmp2_data_start_recover_reply
{
	ndmp2_error	error;
};

/* NDMP2_DATA_START_RECOVER_FILEHIST */
typedef ndmp2_data_start_recover_request ndmp2_data_start_recover_filehist_request;
typedef ndmp2_data_start_recover_reply ndmp2_data_start_recover_filehist_reply;

/* NDMP2_DATA_ABORT */
/* no request arguments */
struct ndmp2_data_abort_reply
{
	ndmp2_error	error;
};

/* NDMP2_DATA_STOP */
/* no request arguments */
struct ndmp2_data_stop_reply
{
	ndmp2_error	error;
};

/* NDMP2_DATA_GET_ENV */
/* no request arguments */
struct ndmp2_data_get_env_reply
{
	ndmp2_error	error;
	ndmp2_pval	env<>;
};

/****************************/
/* NOTIFY INTERFACE	    */
/****************************/
/* NDMP2_NOTIFY_DATA_HALTED */
struct ndmp2_notify_data_halted_request
{
	ndmp2_data_halt_reason	reason;
	string			text_reason<>;
};

/* No reply */

/* NDMP2_NOTIFY_CONNECTED */
enum ndmp2_connect_reason
{
	NDMP2_CONNECTED,	/* Connect sucessfully */
	NDMP2_SHUTDOWN,		/* Connection shutdown */
	NDMP2_REFUSED		/* reach the maximum number of connections */
};

struct ndmp2_notify_connected_request
{
	ndmp2_connect_reason	reason;
	uint16_t		protocol_version;
	string			text_reason<>;
};

/* NDMP2_NOTIFY_MOVER_PAUSED */
struct ndmp2_notify_mover_paused_request
{
	ndmp2_mover_pause_reason reason;
	ndmp2_u_quad		seek_position;
};
/* No reply */

/* NDMP2_NOTIFY_MOVER_HALTED */
struct ndmp2_notify_mover_halted_request
{
	ndmp2_mover_halt_reason	reason;
	string			text_reason<>;
};
/* No reply */

/* NDMP2_NOTIFY_DATA_READ */
struct ndmp2_notify_data_read_request
{
	ndmp2_u_quad	offset;
	ndmp2_u_quad	length;
};
/* No reply */

/********************************/
/* LOG INTERFACE				*/
/********************************/
/* NDMP2_LOG_LOG */
struct ndmp2_log_log_request
{
	string		entry<>;
};
/* No reply */

/* NDMP2_LOG_DEBUG */
enum ndmp2_debug_level
{
	NDMP2_DBG_USER_INFO,
	NDMP2_DBG_USER_SUMMARY,
	NDMP2_DBG_USER_DETAIL,
	NDMP2_DBG_DIAG_INFO,
	NDMP2_DBG_DIAG_SUMMARY,
	NDMP2_DBG_DIAG_DETAIL,
	NDMP2_DBG_PROG_INFO,
	NDMP2_DBG_PROG_SUMMARY,
	NDMP2_DBG_PROG_DETAIL
};

struct ndmp2_log_debug_request
{
	ndmp2_debug_level level;
	string		message<>;
};
/* No reply */

/* NDMP2_LOG_FILE */
struct ndmp2_log_file_request
{
	string		name<>;
	uint16_t	ssid;
	ndmp2_error	error;
};
/* No reply */

/********************************/
/* File History INTERFACE	    */
/********************************/
/* NDMP2_FH_ADD_UNIX_PATH */
typedef string ndmp2_unix_path<>;
enum ndmp2_unix_file_type
{
	NDMP2_FILE_DIR,
	NDMP2_FILE_FIFO,
	NDMP2_FILE_CSPEC,
	NDMP2_FILE_BSPEC,
	NDMP2_FILE_REG,
	NDMP2_FILE_SLINK,
	NDMP2_FILE_SOCK
};

struct ndmp2_unix_file_stat
{
	ndmp2_unix_file_type	ftype;
	uint32_t		mtime;
	uint32_t		atime;
	uint32_t		ctime;
	uint32_t		uid;
	uint32_t		gid;
	uint32_t		mode;
	ndmp2_u_quad		size;
	ndmp2_u_quad		fh_info;
};

struct ndmp2_fh_unix_path
{
	ndmp2_unix_path		name;
	ndmp2_unix_file_stat	fstat;
};

struct ndmp2_fh_add_unix_path_request
{
	ndmp2_fh_unix_path	paths<>;
};
/* No reply */

/* NDMP2_FH_ADD_UNIX_DIR */
struct ndmp2_fh_unix_dir
{
	ndmp2_unix_path		name;
	uint32_t		node;
	uint32_t		parent;
};

struct ndmp2_fh_add_unix_dir_request
{
	ndmp2_fh_unix_dir	dirs<>;
};
/* No reply */

struct ndmp2_fh_unix_node
{
	ndmp2_unix_file_stat	fstat;
	uint32_t		node;
};

struct ndmp2_fh_add_unix_node_request
{
	ndmp2_fh_unix_node	nodes<>;
};
/* No reply */


/****************************************************************
*
*	End of file	:	ndmp.x
*
****************************************************************/
%#endif /* !NDMOS_OPTION_NO_NDMP2 */
