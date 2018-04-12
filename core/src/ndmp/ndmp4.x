/*
 * Copyright (c) 2000,2001
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
 * ndmp4.x
 *
 * Description	 : NDMP protocol rpcgen file.
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

%#ifndef NDMOS_OPTION_NO_NDMP4

const NDMP4VER = 4;
const NDMP4PORT = 10000;

%#define ndmp4_u_quad uint64_t
%extern bool_t xdr_ndmp4_u_quad(register XDR *xdrs, ndmp4_u_quad *objp);

struct _ndmp4_u_quad
{
	uint32_t	high;
	uint32_t	low;
};



enum ndmp4_header_message_type
{
	NDMP4_MESSAGE_REQUEST,
	NDMP4_MESSAGE_REPLY
};

const NDMP4_MESSAGE_POST = NDMP4_MESSAGE_REQUEST;


/* Note: because of extensibility, this is */
/* Not a complete list of errors. */

enum ndmp4_error {
	NDMP4_NO_ERR			= 0,
	NDMP4_NOT_SUPPORTED_ERR		= 1,
	NDMP4_DEVICE_BUSY_ERR		= 2,
	NDMP4_DEVICE_OPENED_ERR		= 3,
	NDMP4_NOT_AUTHORIZED_ERR	= 4,
	NDMP4_PERMISSION_ERR		= 5,
	NDMP4_DEV_NOT_OPEN_ERR		= 6,
	NDMP4_IO_ERR			= 7,
	NDMP4_TIMEOUT_ERR		= 8,
	NDMP4_ILLEGAL_ARGS_ERR		= 9,
	NDMP4_NO_TAPE_LOADED_ERR	= 10,
	NDMP4_WRITE_PROTECT_ERR		= 11,
	NDMP4_EOF_ERR			= 12,
	NDMP4_EOM_ERR			= 13,
	NDMP4_FILE_NOT_FOUND_ERR	= 14,
	NDMP4_BAD_FILE_ERR		= 15,
	NDMP4_NO_DEVICE_ERR		= 16,
	NDMP4_NO_BUS_ERR		= 17,
	NDMP4_XDR_DECODE_ERR		= 18,
	NDMP4_ILLEGAL_STATE_ERR		= 19,
	NDMP4_UNDEFINED_ERR		= 20,
	NDMP4_XDR_ENCODE_ERR		= 21,
	NDMP4_NO_MEM_ERR		= 22,
	NDMP4_CONNECT_ERR		= 23,
	NDMP4_SEQUENCE_NUM_ERR		= 24,
	NDMP4_READ_IN_PROGRESS_ERR	= 25,
	NDMP4_PRECONDITION_ERR		= 26,
	NDMP4_CLASS_NOT_SUPPORTED	= 27,
	NDMP4_VERSION_NOT_SUPPORTED	= 28,
	NDMP4_EXT_DUPL_CLASSES		= 29,
	NDMP4_EXT_DN_ILLEGAL		= 30
};



/* Note: Because of extensibility, this */
/* is not a complete list of messages */

enum ndmp4_message {

/* CONNECT INTERFACE */
	NDMP4_CONNECT_OPEN		= 0x900,
	NDMP4_CONNECT_CLIENT_AUTH	= 0x901,
	NDMP4_CONNECT_CLOSE		= 0x902,
	NDMP4_CONNECT_SERVER_AUTH	= 0x903,

/* CONFIG INTERFACE */
	NDMP4_CONFIG_GET_HOST_INFO	= 0x100,
	NDMP4_CONFIG_GET_CONNECTION_TYPE = 0x102,
	NDMP4_CONFIG_GET_AUTH_ATTR	= 0x103,
	NDMP4_CONFIG_GET_BUTYPE_INFO	= 0x104,
	NDMP4_CONFIG_GET_FS_INFO	= 0x105,
	NDMP4_CONFIG_GET_TAPE_INFO	= 0x106,
	NDMP4_CONFIG_GET_SCSI_INFO	= 0x107,
	NDMP4_CONFIG_GET_SERVER_INFO	= 0x108,
	NDMP4_CONFIG_SET_EXT_LIST	= 0x109,
	NDMP4_CONFIG_GET_EXT_LIST	= 0x10A,

/* SCSI INTERFACE */
	NDMP4_SCSI_OPEN			= 0x200,
	NDMP4_SCSI_CLOSE		= 0x201,
	NDMP4_SCSI_GET_STATE		= 0x202,
	NDMP4_SCSI_RESET_DEVICE		= 0x204,
	NDMP4_SCSI_EXECUTE_CDB		= 0x206,

/* TAPE INTERFACE */
	NDMP4_TAPE_OPEN			= 0x300,
	NDMP4_TAPE_CLOSE		= 0x301,
	NDMP4_TAPE_GET_STATE		= 0x302,
	NDMP4_TAPE_MTIO			= 0x303,
	NDMP4_TAPE_WRITE		= 0x304,
	NDMP4_TAPE_READ			= 0x305,
	NDMP4_TAPE_EXECUTE_CDB		= 0x307,

/* DATA INTERFACE */
	NDMP4_DATA_GET_STATE		= 0x400,
	NDMP4_DATA_START_BACKUP		= 0x401,
	NDMP4_DATA_START_RECOVER	= 0x402,
	NDMP4_DATA_ABORT		= 0x403,
	NDMP4_DATA_GET_ENV		= 0x404,
	NDMP4_DATA_STOP			= 0x407,
	NDMP4_DATA_LISTEN		= 0x409,
	NDMP4_DATA_CONNECT		= 0x40A,
	NDMP4_DATA_START_RECOVER_FILEHIST = 0x40B,

/* NOTIFY INTERFACE */
	NDMP4_NOTIFY_DATA_HALTED	= 0x501,
	NDMP4_NOTIFY_CONNECTION_STATUS	= 0x502,
	NDMP4_NOTIFY_MOVER_HALTED	= 0x503,
	NDMP4_NOTIFY_MOVER_PAUSED	= 0x504,
	NDMP4_NOTIFY_DATA_READ		= 0x505,

/* LOGGING INTERFACE */
	NDMP4_LOG_FILE			= 0x602,
	NDMP4_LOG_MESSAGE		= 0x603,

/* FILE HISTORY INTERFACE */
	NDMP4_FH_ADD_FILE		= 0x703,
	NDMP4_FH_ADD_DIR		= 0x704,
	NDMP4_FH_ADD_NODE		= 0x705,

/* MOVER INTERFACE */
	NDMP4_MOVER_GET_STATE		= 0xA00,
	NDMP4_MOVER_LISTEN		= 0xA01,
	NDMP4_MOVER_CONTINUE		= 0xA02,
	NDMP4_MOVER_ABORT		= 0xA03,
	NDMP4_MOVER_STOP		= 0xA04,
	NDMP4_MOVER_SET_WINDOW		= 0xA05,
	NDMP4_MOVER_READ		= 0xA06,
	NDMP4_MOVER_CLOSE		= 0xA07,
	NDMP4_MOVER_SET_RECORD_SIZE	= 0xA08,
	NDMP4_MOVER_CONNECT		= 0xA09,

/* EXTENSIBILITY */

/* Reserved for Standard extensions */
	NDMP4_EXT_STANDARD_BASE		= 0x10000,

/* Reserved for Proprietary extensions */
	NDMP4_EXT_PROPRIETARY_BASE	= 0x20000000
};


struct ndmp4_header
{
	uint32_t		sequence;
	uint32_t		time_stamp;
	ndmp4_header_message_type message_type;
	ndmp4_message		message_code;
	uint32_t		reply_sequence;
	ndmp4_error		error_code;
};

struct ndmp4_pval
{
	string		name<>;
	string		value<>;
};


/* Connect messages */
struct ndmp4_connect_open_request
{
	uint16_t	protocol_version;
};

struct ndmp4_connect_open_reply
{
	ndmp4_error	error;
};


enum ndmp4_auth_type
{
	NDMP4_AUTH_NONE=0,
	NDMP4_AUTH_TEXT=1,
	NDMP4_AUTH_MD5=2
};

struct ndmp4_auth_text
{
	string		auth_id<>;
	string		auth_password<>;
};

struct ndmp4_auth_md5
{
	string		auth_id<>;
	opaque		auth_digest[16];
};

union ndmp4_auth_data switch (enum ndmp4_auth_type auth_type)
{
	case NDMP4_AUTH_NONE:
		void;
	case NDMP4_AUTH_TEXT:
		struct ndmp4_auth_text	auth_text;
	case NDMP4_AUTH_MD5:
		struct ndmp4_auth_md5	auth_md5;
};

union ndmp4_auth_attr switch (enum ndmp4_auth_type auth_type)
{
	case NDMP4_AUTH_NONE:
		void;
	case NDMP4_AUTH_TEXT:
		void;
	case NDMP4_AUTH_MD5:
		opaque	challenge[64];
};


struct ndmp4_connect_client_auth_request
{
	ndmp4_auth_data	auth_data;
};

struct ndmp4_connect_client_auth_reply
{
	ndmp4_error	error;
};


struct ndmp4_connect_server_auth_request
{
	ndmp4_auth_attr	client_attr;
};

struct ndmp4_connect_server_auth_reply
{
	ndmp4_error	error;
	ndmp4_auth_data	server_result;
};


struct ndmp4_config_get_host_info_reply
{
	ndmp4_error	error;
	string		hostname<>;
	string		os_type<>;
	string		os_vers<>;
	string		hostid<>;
};


struct ndmp4_config_get_server_info_reply
{
	ndmp4_error	error;
	string		vendor_name<>;
	string		product_name<>;
	string		revision_number<>;
	ndmp4_auth_type	auth_type<>;
};


enum ndmp4_addr_type
{
	NDMP4_ADDR_LOCAL=0,
	NDMP4_ADDR_TCP=1,
	NDMP4_ADDR_RESERVED=2,
	NDMP4_ADDR_IPC=3
};

struct ndmp4_config_get_connection_type_reply
{
	ndmp4_error	error;
	ndmp4_addr_type	addr_types<>;
};


struct ndmp4_config_get_auth_attr_request
{
	ndmp4_auth_type	auth_type;
};



struct ndmp4_config_get_auth_attr_reply
{
	ndmp4_error	error;
	ndmp4_auth_attr	server_attr;
};


const NDMP4_BUTYPE_BACKUP_FILELIST	= 0x0002;
const NDMP4_BUTYPE_RECOVER_FILELIST	= 0x0004;
const NDMP4_BUTYPE_BACKUP_DIRECT	= 0x0008;
const NDMP4_BUTYPE_RECOVER_DIRECT	= 0x0010;
const NDMP4_BUTYPE_BACKUP_INCREMENTAL	= 0x0020;
const NDMP4_BUTYPE_RECOVER_INCREMENTAL	= 0x0040;
const NDMP4_BUTYPE_BACKUP_UTF8		= 0x0080;
const NDMP4_BUTYPE_RECOVER_UTF8		= 0x0100;
const NDMP4_BUTYPE_BACKUP_FH_FILE	= 0x0200;
const NDMP4_BUTYPE_BACKUP_FH_DIR	= 0x0400;
const NDMP4_BUTYPE_RECOVER_FILEHIST	= 0x0800;
const NDMP4_BUTYPE_RECOVER_FH_FILE	= 0x1000;
const NDMP4_BUTYPE_RECOVER_FH_DIR	= 0x2000;


struct ndmp4_butype_info
{
	string		butype_name<>;
	ndmp4_pval	default_env<>;
	uint32_t	attrs;
};

struct ndmp4_config_get_butype_info_reply
{
	ndmp4_error		error;
	ndmp4_butype_info	butype_info<>;
};


const NDMP4_FS_INFO_TOTAL_SIZE_UNS	= 0x00000001;
const NDMP4_FS_INFO_USED_SIZE_UNS	= 0x00000002;
const NDMP4_FS_INFO_AVAIL_SIZE_UNS	= 0x00000004;
const NDMP4_FS_INFO_TOTAL_INODES_UNS	= 0x00000008;
const NDMP4_FS_INFO_USED_INODES_UNS	= 0x00000010;

struct ndmp4_fs_info
{
	uint32_t	unsupported;
	string		fs_type<>;
	string		fs_logical_device<>;
	string		fs_physical_device<>;
	ndmp4_u_quad	total_size;
	ndmp4_u_quad	used_size;
	ndmp4_u_quad	avail_size;
	ndmp4_u_quad	total_inodes;
	ndmp4_u_quad	used_inodes;
	ndmp4_pval	fs_env<>;
	string		fs_status<>;
};

struct ndmp4_config_get_fs_info_reply
{
	ndmp4_error	error;
	ndmp4_fs_info	fs_info<>;
};


const NDMP4_TAPE_ATTR_REWIND	= 0x00000001;
const NDMP4_TAPE_ATTR_UNLOAD	= 0x00000002;
const NDMP4_TAPE_ATTR_RAW	= 0x00000004;

struct ndmp4_device_capability
{
	string			device<>;
	uint32_t		attr;
	ndmp4_pval		capability<>;
};

struct ndmp4_device_info
{
	string			model<>;
	ndmp4_device_capability	caplist<>;
};

struct ndmp4_config_get_tape_info_reply
{
	ndmp4_error		error;
	ndmp4_device_info	tape_info<>;
};

struct ndmp4_config_get_scsi_info_reply
{
	ndmp4_error		error;
	ndmp4_device_info	scsi_info<>;
};

struct ndmp4_class_list
{
	uint16_t	class_id;
	uint16_t	class_version<>;
};

struct ndmp4_class_version
{
	uint16_t	class_id;
	uint16_t	class_version;
};

struct ndmp4_config_get_ext_list_reply
{
	ndmp4_error		error;
	ndmp4_class_list	class_list<>;
};

struct ndmp4_config_set_ext_list_request
{
	ndmp4_error		error;
	ndmp4_class_list	ndmp4_accepted_ext<>;
};

struct ndmp4_config_set_ext_list_reply
{
	ndmp4_error	error;
};




struct ndmp4_scsi_open_request
{
	string		device<>;
};

struct ndmp4_scsi_open_reply
{
	ndmp4_error	error;
};


struct ndmp4_scsi_close_reply
{
	ndmp4_error	error;
};


struct ndmp4_scsi_get_state_reply
{
	ndmp4_error	error;
	short		target_controller;
	short		target_id;
	short		target_lun;
};


struct ndmp4_scsi_reset_device_reply
{
	ndmp4_error	error;
};

const NDMP4_SCSI_DATA_IN	= 0x00000001;
const NDMP4_SCSI_DATA_OUT	= 0x00000002;

struct ndmp4_execute_cdb_request
{
	uint32_t	flags;
	uint32_t	timeout;
	uint32_t	datain_len;
	opaque		cdb<>;
	opaque		dataout<>;
};

struct ndmp4_execute_cdb_reply
{
	ndmp4_error	error;
	u_char		status;
	uint32_t	dataout_len;
	opaque		datain<>;
	opaque		ext_sense<>;
};

typedef ndmp4_execute_cdb_request	ndmp4_scsi_execute_cdb_request;
typedef ndmp4_execute_cdb_reply		ndmp4_scsi_execute_cdb_reply;

enum ndmp4_tape_open_mode
{
	NDMP4_TAPE_READ_MODE	= 0,
	NDMP4_TAPE_RDWR_MODE	= 1,
	NDMP4_TAPE_RAW_MODE	= 2
};


struct ndmp4_tape_open_request {
	string			device<>;
	ndmp4_tape_open_mode	mode;
};

struct ndmp4_tape_open_reply {
	ndmp4_error	error;
};


struct ndmp4_tape_close_reply
{
	ndmp4_error	error;
};


/* flags */
const NDMP4_TAPE_STATE_NOREWIND	= 0x0008;	/* non-rewind	device */
const NDMP4_TAPE_STATE_WR_PROT	= 0x0010;	/* write-protected */
const NDMP4_TAPE_STATE_ERROR	= 0x0020;	/* media	error */
const NDMP4_TAPE_STATE_UNLOAD	= 0x0040;	/* tape	unloaded	upon
	close */

/* unsupported bits */
const NDMP4_TAPE_STATE_FILE_NUM_UNS	= 0x00000001;
const NDMP4_TAPE_STATE_SOFT_ERRORS_UNS	= 0x00000002;
const NDMP4_TAPE_STATE_BLOCK_SIZE_UNS	= 0x00000004;
const NDMP4_TAPE_STATE_BLOCKNO_UNS	= 0x00000008;
const NDMP4_TAPE_STATE_TOTAL_SPACE_UNS	= 0x00000010;
const NDMP4_TAPE_STATE_SPACE_REMAIN_UNS	= 0x00000020;

struct ndmp4_tape_get_state_reply
{
	uint32_t	unsupported;
	ndmp4_error	error;
	uint32_t	flags;
	uint32_t	file_num;
	uint32_t	soft_errors;
	uint32_t	block_size;
	uint32_t	blockno;

	ndmp4_u_quad	total_space;
	ndmp4_u_quad	space_remain;
};


enum ndmp4_tape_mtio_op
{
	NDMP4_MTIO_FSF=0,
	NDMP4_MTIO_BSF=1,
	NDMP4_MTIO_FSR=2,
	NDMP4_MTIO_BSR=3,
	NDMP4_MTIO_REW=4,
	NDMP4_MTIO_EOF=5,
	NDMP4_MTIO_OFF=6,
	NDMP4_MTIO_TUR=7
};

struct ndmp4_tape_mtio_request
{
	ndmp4_tape_mtio_op	tape_op;
	uint32_t		count;
};

struct ndmp4_tape_mtio_reply
{
	ndmp4_error	error;
	uint32_t	resid_count;
};


struct ndmp4_tape_write_request
{
	opaque		data_out<>;
};

struct ndmp4_tape_write_reply
{
	ndmp4_error	error;
	uint32_t	count;
};


struct ndmp4_tape_read_request
{
	uint32_t	count;
};

struct ndmp4_tape_read_reply
{
	ndmp4_error	error;
	opaque		data_in<>;
};


typedef ndmp4_scsi_execute_cdb_request	ndmp4_tape_execute_cdb_request;
typedef ndmp4_scsi_execute_cdb_reply	ndmp4_tape_execute_cdb_reply;


enum ndmp4_data_operation
{
	NDMP4_DATA_OP_NOACTION		= 0,
	NDMP4_DATA_OP_BACKUP		= 1,
	NDMP4_DATA_OP_RECOVER		= 2,
	NDMP4_DATA_OP_RECOVER_FILEHIST	= 3

};

enum ndmp4_data_state
{
	NDMP4_DATA_STATE_IDLE=0,
	NDMP4_DATA_STATE_ACTIVE=1,
	NDMP4_DATA_STATE_HALTED=2,
	NDMP4_DATA_STATE_LISTEN=3,
	NDMP4_DATA_STATE_CONNECTED=4
};

enum ndmp4_data_halt_reason
{
	NDMP4_DATA_HALT_NA=0,
	NDMP4_DATA_HALT_SUCCESSFUL=1,
	NDMP4_DATA_HALT_ABORTED=2,
	NDMP4_DATA_HALT_INTERNAL_ERROR=3,
	NDMP4_DATA_HALT_CONNECT_ERROR=4
};

/* ndmp4_addr */
struct ndmp4_tcp_addr
{
	uint32_t	ip_addr;
	uint16_t	port;
	ndmp4_pval	addr_env<>;
};

struct ndmp4_ipc_addr
{
	opaque		comm_data<>;
};

union ndmp4_addr switch (ndmp4_addr_type addr_type)
{
	case NDMP4_ADDR_LOCAL:
		void;
	case NDMP4_ADDR_TCP:
		ndmp4_tcp_addr	tcp_addr<>;
	case NDMP4_ADDR_IPC:
		ndmp4_ipc_addr	ipc_addr;
};

/* unsupported bitmask bits */
const NDMP4_DATA_STATE_EST_BYTES_REMAIN_UNS	= 0x00000001;
const NDMP4_DATA_STATE_EST_TIME_REMAIN_UNS	= 0x00000002;

struct ndmp4_data_get_state_reply
{
	uint32_t		unsupported;
	ndmp4_error		error;
	ndmp4_data_operation	operation;
	ndmp4_data_state	state;
	ndmp4_data_halt_reason	halt_reason;
	ndmp4_u_quad		bytes_processed;
	ndmp4_u_quad		est_bytes_remain;
	uint32_t		est_time_remain;
	ndmp4_addr		data_connection_addr;
	ndmp4_u_quad		read_offset;
	ndmp4_u_quad		read_length;
};



struct ndmp4_data_listen_request
{
	ndmp4_addr_type	addr_type;
};

struct ndmp4_data_listen_reply
{
	ndmp4_error	error;
	ndmp4_addr	connect_addr;
};


struct ndmp4_data_connect_request
{
	ndmp4_addr	addr;
};

struct ndmp4_data_connect_reply
{
	ndmp4_error	error;
};


struct ndmp4_data_start_backup_request
{
	string		butype_name<>;
	ndmp4_pval	env<>;
};

struct ndmp4_data_start_backup_reply
{
	ndmp4_error	error;
};


struct ndmp4_name
{
	string		original_path<>;
	string		destination_path<>;
	string		name<>;
	string		other_name<>;
	ndmp4_u_quad	node;
	ndmp4_u_quad	fh_info;
};

struct ndmp4_data_start_recover_request
{
	ndmp4_pval	env<>;
	ndmp4_name	nlist<>;
	string		butype_name<>;
};

struct ndmp4_data_start_recover_reply
{
	ndmp4_error	error;
};

struct ndmp4_data_start_recover_filehist_request
{
	ndmp4_pval	env<>;
	ndmp4_name	nlist<>;
	string		butype_name<>;
};

struct ndmp4_data_start_recover_filehist_reply
{
	ndmp4_error	error;
};



struct ndmp4_data_abort_reply
{
	ndmp4_error	error;
};


struct ndmp4_data_stop_reply
{
	ndmp4_error	error;
};


struct ndmp4_data_get_env_reply
{
	ndmp4_error	error;
	ndmp4_pval	env<>;
};


enum ndmp4_mover_mode
{
	NDMP4_MOVER_MODE_READ		= 0,
	NDMP4_MOVER_MODE_WRITE		= 1,
	NDMP4_MOVER_MODE_NOACTION	= 2
};

enum ndmp4_mover_state
{
	NDMP4_MOVER_STATE_IDLE,
	NDMP4_MOVER_STATE_LISTEN,
	NDMP4_MOVER_STATE_ACTIVE,
	NDMP4_MOVER_STATE_PAUSED,
	NDMP4_MOVER_STATE_HALTED
};

enum ndmp4_mover_pause_reason
{
	NDMP4_MOVER_PAUSE_NA		= 0,
	NDMP4_MOVER_PAUSE_EOM		= 1,
	NDMP4_MOVER_PAUSE_EOF		= 2,
	NDMP4_MOVER_PAUSE_SEEK		= 3,
	/* NDMPv4 does not have MOVER_PAUSE_MEDIA_ERROR = 4 */
	NDMP4_MOVER_PAUSE_EOW		= 5
};

enum ndmp4_mover_halt_reason
{
	NDMP4_MOVER_HALT_NA,
	NDMP4_MOVER_HALT_CONNECT_CLOSED,
	NDMP4_MOVER_HALT_ABORTED,
	NDMP4_MOVER_HALT_INTERNAL_ERROR,
	NDMP4_MOVER_HALT_CONNECT_ERROR,
	NDMP4_MOVER_HALT_MEDIA_ERROR
};




struct ndmp4_mover_set_record_size_request
{
	uint32_t	len;
};

struct ndmp4_mover_set_record_size_reply
{
	ndmp4_error	error;
};


struct ndmp4_mover_set_window_request
{
	ndmp4_u_quad	offset;
	ndmp4_u_quad	length;
};


struct ndmp4_mover_set_window_reply
{
	ndmp4_error	error;
};


struct ndmp4_mover_connect_request
{
	ndmp4_mover_mode	mode;
	ndmp4_addr		addr;
};

struct ndmp4_mover_connect_reply
{
	ndmp4_error	error;
};


struct ndmp4_mover_listen_request
{
	ndmp4_mover_mode	mode;
	ndmp4_addr_type		addr_type;
};

struct ndmp4_mover_listen_reply
{
	ndmp4_error	error;
	ndmp4_addr	connect_addr;
};


struct ndmp4_mover_read_request
{
	ndmp4_u_quad	offset;
	ndmp4_u_quad	length;
};

struct ndmp4_mover_read_reply
{
	ndmp4_error	error;
};



struct ndmp4_mover_get_state_reply
{
	ndmp4_error		error;
	ndmp4_mover_mode	mode;
	ndmp4_mover_state	state;
	ndmp4_mover_pause_reason pause_reason;
	ndmp4_mover_halt_reason	halt_reason;
	uint32_t		record_size;
	uint32_t		record_num;
	ndmp4_u_quad		bytes_moved;
	ndmp4_u_quad		seek_position;
	ndmp4_u_quad		bytes_left_to_read;
	ndmp4_u_quad		window_offset;
	ndmp4_u_quad		window_length;
	ndmp4_addr		data_connection_addr;
};


struct ndmp4_mover_continue_reply
{
	ndmp4_error	error;
};


struct ndmp4_mover_close_reply
{
	ndmp4_error	error;
};


struct ndmp4_mover_abort_reply
{
	ndmp4_error	error;
};


struct ndmp4_mover_stop_reply
{
	ndmp4_error	error;
};


struct ndmp4_notify_data_halted_post
{
	ndmp4_data_halt_reason	reason;
};


enum ndmp4_connection_status_reason
{
	NDMP4_CONNECTED=0,
	NDMP4_SHUTDOWN=1,
	NDMP4_REFUSED=2
};

struct ndmp4_notify_connection_status_post
{
	ndmp4_connection_status_reason	reason;
	uint16_t			protocol_version;
	string				text_reason<>;
};


struct ndmp4_notify_mover_halted_post
{
	ndmp4_mover_halt_reason		reason;
};


struct ndmp4_notify_mover_paused_post
{
	ndmp4_mover_pause_reason	reason;
	ndmp4_u_quad			seek_position;
};


struct ndmp4_notify_data_read_post
{
	ndmp4_u_quad	offset;
	ndmp4_u_quad	length;
};


enum ndmp4_has_associated_message
{
	NDMP4_NO_ASSOCIATED_MESSAGE	= 0,
	NDMP4_HAS_ASSOCIATED_MESSAGE	= 1
};

enum ndmp4_log_type
{
	NDMP4_LOG_NORMAL	= 0,
	NDMP4_LOG_DEBUG		= 1,
	NDMP4_LOG_ERROR		= 2,
	NDMP4_LOG_WARNING	= 3
};

struct ndmp4_log_message_post
{
	ndmp4_log_type		log_type;
	uint32_t		message_id;
	string			entry<>;
	ndmp4_has_associated_message associated_message_valid;
	uint32_t		associated_message_sequence;
};


enum ndmp4_recovery_status
{
	NDMP4_RECOVERY_SUCCESSFUL		= 0,
	NDMP4_RECOVERY_FAILED_PERMISSION	= 1,
	NDMP4_RECOVERY_FAILED_NOT_FOUND		= 2,
	NDMP4_RECOVERY_FAILED_NO_DIRECTORY	= 3,
	NDMP4_RECOVERY_FAILED_OUT_OF_MEMORY	= 4,
	NDMP4_RECOVERY_FAILED_IO_ERROR		= 5,
	NDMP4_RECOVERY_FAILED_UNDEFINED_ERROR	= 6
};

struct ndmp4_log_file_post
{
	string			name<>;
	ndmp4_recovery_status	recovery_status;
};


enum ndmp4_fs_type
{
	NDMP4_FS_UNIX=0,
	NDMP4_FS_NT=1,
	NDMP4_FS_OTHER=2
};

typedef	string	ndmp4_path<>;

struct ndmp4_nt_path
{
	ndmp4_path	nt_path;
	ndmp4_path	dos_path;
};

union ndmp4_file_name	switch	(ndmp4_fs_type	fs_type)
{
	case NDMP4_FS_UNIX:
		ndmp4_path	unix_name;
	case NDMP4_FS_NT:
		ndmp4_nt_path	nt_name;
	default:
		ndmp4_path	other_name;
};

/* file type */
enum ndmp4_file_type
{
	NDMP4_FILE_DIR=0,
	NDMP4_FILE_FIFO=1,
	NDMP4_FILE_CSPEC=2,
	NDMP4_FILE_BSPEC=3,
	NDMP4_FILE_REG=4,
	NDMP4_FILE_SLINK=5,
	NDMP4_FILE_SOCK=6,
	NDMP4_FILE_REGISTRY=7,
	NDMP4_FILE_OTHER=8
};

/* file stat */
/* unsupported bitmask */
const NDMP4_FILE_STAT_ATIME_UNS	= 0x00000001;
const NDMP4_FILE_STAT_CTIME_UNS	= 0x00000002;
const NDMP4_FILE_STAT_GROUP_UNS	= 0x00000004;

struct ndmp4_file_stat
{
	uint32_t	unsupported;
	ndmp4_fs_type	fs_type;
	ndmp4_file_type	ftype;
	uint32_t	mtime;
	uint32_t	atime;
	uint32_t	ctime;
	uint32_t	owner;
	uint32_t	group;
	uint32_t	fattr;
	ndmp4_u_quad	size;
	uint32_t	links;
};

struct ndmp4_file
{
	ndmp4_file_name	names<>;
	ndmp4_file_stat	stats<>;
	ndmp4_u_quad	node;
	ndmp4_u_quad	fh_info;
};

struct ndmp4_fh_add_file_post
{
	ndmp4_file	files<>;
};

struct ndmp4_dir
{
	ndmp4_file_name	names<>;
	ndmp4_u_quad	node;
	ndmp4_u_quad	parent;
};

struct ndmp4_fh_add_dir_post
{
	ndmp4_dir	dirs<>;
};


struct ndmp4_node
{
	ndmp4_file_stat	stats<>;
	ndmp4_u_quad	node;
	ndmp4_u_quad	fh_info;
};

struct ndmp4_fh_add_node_post
{
	ndmp4_node	nodes<>;
};

%#endif /* !NDMOS_OPTION_NO_NDMP4 */
