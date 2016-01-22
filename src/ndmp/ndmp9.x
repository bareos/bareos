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
 *	NDMPv9, represented here, is a ficticious version
 *	used internally and never over-the-wire. This
 *	isolates higher-level routines from variations
 *	between NDMP protocol version. At this time,
 *	NDMPv2, NDMPv3 and NDMPv4 are deployed. NDMPv9 tends
 *	to be bits and pieces of all supported protocol versions
 *	mashed together.
 *
 *	While we want the higher-level routines	isolated,
 *	for clarity we still want them to use data structures
 *	and construct that resemble NDMP. Higher-level routines
 *	manipulate NDMPv9 data structures. Mid-level routines
 *	translate between NDMPv9 and the over-the-wire version
 *	in use. Low-level routines do the over-the-wire functions.
 *
 *	The approach of using the latest version internally
 *	and retrofiting earlier versions was rejected for
 *	two reasons. First, it means a tear-up of higher-level
 *	functions as new versions are deployed. Second,
 *	it makes building with selected version impossible.
 *	No matter what approach is taken, there will be
 *	some sort of retrofit between versions. NDMPv9
 *	is simply the internal version, and all bona-fide
 *	versions are retrofitted. v9 was chosen because
 *	it is unlikely the NDMP version will reach 9
 *	within the useful life of the NDMP architecture.
 *
 *	NDMPv9 could be implemented in a hand-crafted header (.h)
 *	file, yet we continue to use the ONC RPC (.x) description
 *	for convenvience. It's easy to cut-n-paste from the other
 *	NDMP.x files. It's important that ndmp9_xdr.c never be
 *	generated nor compiled.
 */


/*
 * (from ndmp3.x)
 * ndmp.x
 *
 * Description	 : NDMP protocol rpcgen file.
 *
 * Copyright (c) 1999 Intelliguard Software, Network Appliance.
 * All Rights Reserved.
 */

/*
 * (from ndmp2.x)
 * Copyright (c) 1997 Network Appliance. All Rights Reserved.
 *
 * Network Appliance makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

%#if __clang__
%#pragma clang diagnostic ignored "-Wunused-const-variable"
%#elif __GNUC__
%#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
%#pragma GCC diagnostic ignored "-Wunused-variable"
%#endif
%#endif

const NDMP9VER = 9;


/*
 * General types
 ****************************************************************
 */

/*
 * Error codes
 */
enum ndmp9_error {
	NDMP9_NO_ERR,			/* No error */
	NDMP9_NOT_SUPPORTED_ERR,	/* Call is not supported */
	NDMP9_DEVICE_BUSY_ERR,		/* The device is in use */
	NDMP9_DEVICE_OPENED_ERR,	/* Another tape or scsi device
					 * is already open */
	NDMP9_NOT_AUTHORIZED_ERR,	/* connection has not been authorized*/
	NDMP9_PERMISSION_ERR,		/* some sort of permission problem */
	NDMP9_DEV_NOT_OPEN_ERR,		/* SCSI device is not open */
	NDMP9_IO_ERR,			/* I/O error */
	NDMP9_TIMEOUT_ERR,		/* command timed out */
	NDMP9_ILLEGAL_ARGS_ERR,		/* illegal arguments in request */
	NDMP9_NO_TAPE_LOADED_ERR,	/* Cannot open because there is
					   no tape loaded */
	NDMP9_WRITE_PROTECT_ERR,	/* tape cannot be open for write */
	NDMP9_EOF_ERR,			/* Command encountered EOF */
	NDMP9_EOM_ERR,			/* Command encountered EOM */
	NDMP9_FILE_NOT_FOUND_ERR,	/* File not found during recover */
	NDMP9_BAD_FILE_ERR,		/* The file descriptor is invalid */
	NDMP9_NO_DEVICE_ERR,		/* The device is not at that target */
	NDMP9_NO_BUS_ERR,		/* Invalid controller */
	NDMP9_XDR_DECODE_ERR,		/* Can't decode the request argument */
	NDMP9_ILLEGAL_STATE_ERR,	/* Call can't be done at this state */
	NDMP9_UNDEFINED_ERR,		/* Undefined Error */
	NDMP9_XDR_ENCODE_ERR,		/* Can't encode the reply argument */
	NDMP9_NO_MEM_ERR,		/* no memory */
	NDMP9_CONNECT_ERR,		/* Error connecting to another
					 * NDMP server */
	NDMP9_SEQUENCE_NUM_ERR,
	NDMP9_READ_IN_PROGRESS_ERR	= 25,
	NDMP9_PRECONDITION_ERR		= 26,
	NDMP9_CLASS_NOT_SUPPORTED	= 27,
	NDMP9_VERSION_NOT_SUPPORTED	= 28,
	NDMP9_EXT_DUPL_CLASSES		= 29,
	NDMP9_EXT_DN_ILLEGAL		= 30
};


/*
 * Message codes
 */
enum ndmp9_message {
	NDMP9_CONNECT_OPEN = 0x900,	/* CONNECT INTERFACE */
	NDMP9_CONNECT_CLIENT_AUTH = 0x901,
	NDMP9_CONNECT_CLOSE = 0x902,
	NDMP9_CONNECT_SERVER_AUTH = 0x903,

	NDMP9_CONFIG_GET_HOST_INFO = 0x100, /* CONFIG INTERFACE */
	NDMP9_CONFIG_GET_CONNECTION_TYPE = 0x102, /* NDMP2_CONFIG_GET_MOVER_TYPE on v2*/
	NDMP9_CONFIG_GET_AUTH_ATTR = 0x103,
	NDMP9_CONFIG_GET_BUTYPE_INFO = 0x104,	/* NDMPv3 and forward */
	NDMP9_CONFIG_GET_FS_INFO = 0x105,	/* NDMPv3 and forward */
	NDMP9_CONFIG_GET_TAPE_INFO = 0x106,	/* NDMPv3 and forward */
	NDMP9_CONFIG_GET_SCSI_INFO = 0x107,	/* NDMPv3 and forward */
	NDMP9_CONFIG_GET_SERVER_INFO =0x108,	/* NDMPv3 and forward */

	NDMP9_SCSI_OPEN = 0x200,	/* SCSI INTERFACE */
	NDMP9_SCSI_CLOSE = 0x201,
	NDMP9_SCSI_GET_STATE = 0x202,
	NDMP9_SCSI_SET_TARGET = 0x203,
	NDMP9_SCSI_RESET_DEVICE = 0x204,
	NDMP9_SCSI_RESET_BUS = 0x205,
	NDMP9_SCSI_EXECUTE_CDB = 0x206,

	NDMP9_TAPE_OPEN = 0x300,	/* TAPE INTERFACE */
	NDMP9_TAPE_CLOSE = 0x301,
	NDMP9_TAPE_GET_STATE = 0x302,
	NDMP9_TAPE_MTIO = 0x303,
	NDMP9_TAPE_WRITE = 0x304,
	NDMP9_TAPE_READ = 0x305,
	NDMP9_TAPE_EXECUTE_CDB = 0x307,

	NDMP9_DATA_GET_STATE = 0x400,	/* DATA INTERFACE */
	NDMP9_DATA_START_BACKUP = 0x401,
	NDMP9_DATA_START_RECOVER = 0x402,
	NDMP9_DATA_ABORT = 0x403,
	NDMP9_DATA_GET_ENV = 0x404,
	NDMP9_DATA_STOP = 0x407,
	NDMP9_DATA_LISTEN = 0x409,
	NDMP9_DATA_CONNECT = 0x40a,
	NDMP9_DATA_START_RECOVER_FILEHIST = 0x40b,

	NDMP9_NOTIFY_DATA_HALTED =0x501,/* NOTIFY INTERFACE */
	NDMP9_NOTIFY_CONNECTED = 0x502,
	NDMP9_NOTIFY_MOVER_HALTED = 0x503,
	NDMP9_NOTIFY_MOVER_PAUSED = 0x504,
	NDMP9_NOTIFY_DATA_READ =0x505,

	NDMP9_LOG_FILE = 0x602,		/* LOGGING INTERFACE */
	NDMP9_LOG_MESSAGE = 0x603,

	NDMP9_FH_ADD_FILE = 0x703,	/* FILE HISTORY INTERFACE */
	NDMP9_FH_ADD_DIR = 0x704,
	NDMP9_FH_ADD_NODE = 0x705,

	NDMP9_MOVER_GET_STATE = 0xa00,	/* MOVER INTERFACE */
	NDMP9_MOVER_LISTEN = 0xa01,
	NDMP9_MOVER_CONTINUE = 0xa02,
	NDMP9_MOVER_ABORT = 0xa03,
	NDMP9_MOVER_STOP = 0xa04,
	NDMP9_MOVER_SET_WINDOW = 0xa05,
	NDMP9_MOVER_READ = 0xa06,
	NDMP9_MOVER_CLOSE =0xa07,
	NDMP9_MOVER_SET_RECORD_SIZE =0xa08,
	NDMP9_MOVER_CONNECT =0xa09
};


/*
 * Common message bodies
 */
%extern bool_t xdr_ndmp9_no_arguments();
%#define ndmp9_no_arguments int

struct ndmp9_just_error_reply {
	ndmp9_error	error;
};


/*
 * 64-bit integers
 */
%#define ndmp9_u_quad uint64_t
%extern bool_t xdr_ndmp9_u_quad(register XDR *xdrs, ndmp9_u_quad *objp);

/*
 * Valid values. Sometimes we have values, and sometimes we don't.
 */
enum ndmp9_validity {
	NDMP9_VALIDITY_INVALID = 0,
	NDMP9_VALIDITY_VALID,
	NDMP9_VALIDITY_MAYBE_INVALID,
	NDMP9_VALIDITY_MAYBE_VALID
};

%#define NDMP9_INVALID_U_LONG	0xFFFFFFFFul
struct ndmp9_valid_u_long {
	ndmp9_validity	valid;
	uint32_t	value;
};

%#define NDMP9_INVALID_U_QUAD	0xFFFFFFFFFFFFFFFFull
struct ndmp9_valid_u_quad {
	ndmp9_validity	valid;
	ndmp9_u_quad	value;
};


/*
 * Property values. A simple name/value pair. Used in lots of places.
 */
struct ndmp9_pval {
	string		name<>;
	string		value<>;
};


/*
 * Authorization data. Three authorization types each
 * with their particular values. Authorization is done
 * in three steps:
 *	1) Client determines which types of authorization are available
 *	   on the server.
 *	2) Client may get parameters (challenge) from server.
 *	3) Client requests authorization based on a shared
 *	   secret (password) with parameters (challenge) applied.
 */
enum ndmp9_auth_type {
	NDMP9_AUTH_NONE,		/* no password is required */
	NDMP9_AUTH_TEXT,		/* the clear text password */
	NDMP9_AUTH_MD5			/* md5 */
};

union ndmp9_auth_attr switch (enum ndmp9_auth_type auth_type) {
	case NDMP9_AUTH_NONE:
		void;
	case NDMP9_AUTH_TEXT:
		void;
	case NDMP9_AUTH_MD5:
		opaque	challenge[64];
};

struct ndmp9_auth_text {
	string		auth_id<>;	/* account/user name */
	string		auth_password<>; /* clear-text password */
};

struct ndmp9_auth_md5 {
	string		auth_id<>;	/* account/user name */
	opaque		auth_digest[16]; /* MD5 "hashed" password */
};

union ndmp9_auth_data switch (enum ndmp9_auth_type auth_type) {
	case NDMP9_AUTH_NONE:
		void;
	case NDMP9_AUTH_TEXT:
		struct ndmp9_auth_text	auth_text;
	case NDMP9_AUTH_MD5:
		struct ndmp9_auth_md5	auth_md5;
};


/*
 * The data connection (data stream, image stream, big ol' pipe)
 * has two endpoints, Once side instigates the connection (connects),
 * the other side receives the connection (listen/accept).
 * Appears in DATA and MOVER interfaces.
 */
enum ndmp9_addr_type {
	NDMP9_ADDR_LOCAL,
	NDMP9_ADDR_TCP,
	/* IPC and FC addr types contemplated but never deployed */
	NDMP9_ADDR_AS_CONNECTED = 0x1000
};

struct ndmp9_tcp_addr {
	uint32_t	ip_addr;
	uint16_t	port;
};

union ndmp9_addr switch (ndmp9_addr_type addr_type) {
	case NDMP9_ADDR_LOCAL:
	case NDMP9_ADDR_AS_CONNECTED:
		void;
	case NDMP9_ADDR_TCP:
		ndmp9_tcp_addr	tcp_addr;
};




/*
 * CONNECT INTERFACE
 ****************************************************************
 *
 * The CONNECT INTERFACE is used to condition and authorize
 * the control connection from the CONTROL Agent (DMA, Client)
 * to the DATA, TAPE, or SCSI Agent (DSP, Servers).
 *
 * Most of this is addressed by NDMP0 (zero), which is a companion
 * ficticious version. The NDMP0 features must never change to
 * support protocol version negotiation. Once the version is
 * negotiated, subsequent negotiations and authorization can
 * take place.
 */

/* NDMP9_CONNECT_OPEN -- must never change, negotiate protocol version */
struct ndmp9_connect_open_request {
	uint16_t	protocol_version;	/* the version of protocol supported */
};
typedef ndmp9_just_error_reply	ndmp9_connect_open_reply;

/* NDMP9_CONNECT_CLIENT_AUTH -- authorize client */
struct ndmp9_connect_client_auth_request {
	ndmp9_auth_data	auth_data;
};
typedef ndmp9_just_error_reply	ndmp9_connect_client_auth_reply;


/* NDMP9_CONNECT_CLOSE -- must never change, terminate control connection */
typedef ndmp9_no_arguments	ndmp9_connect_close_request;
typedef ndmp9_no_arguments	ndmp9_connect_close_reply;

/* NDMP9_CONNECT_SERVER_AUTH -- once client is authorized, ask server to
 * prove itself -- nobody is using this */
struct ndmp9_connect_server_auth_request {
	ndmp9_auth_attr		client_attr;
};

struct ndmp9_connect_server_auth_reply {
	ndmp9_error		error;
	ndmp9_auth_data		server_result;
};


/*
 * CONFIG INTERFACE
 ****************************************************************
 *
 * The CONFIG interfaces allow the CONTROL Agent (DMA, client) to
 * obtain resource and other information from the DATA/TAPE/SCSI
 * Agent (DSP, server).
 *
 * For NDMPv9, the whole show is lumped into a single data structure.
 * The specific CONFIG interfaces, which vary between versions,
 * pick-n-choose the info needed.
 */

struct ndmp9_butype_info {
	string			butype_name<>;
	ndmp9_valid_u_long	v2attr;
	ndmp9_valid_u_long	v3attr;
	ndmp9_valid_u_long	v4attr;
	ndmp9_pval		default_env<>;
};

struct ndmp9_fs_info {
	string			fs_type<>;
	string			fs_logical_device<>;
	string			fs_physical_device<>;
	ndmp9_valid_u_quad	total_size;
	ndmp9_valid_u_quad	used_size;
	ndmp9_valid_u_quad	avail_size;
	ndmp9_valid_u_quad	total_inodes;
	ndmp9_valid_u_quad	used_inodes;
	ndmp9_pval		fs_env<>;
	string			fs_status<>;
};

struct ndmp9_device_capability {
	string			device<>;
	ndmp9_valid_u_long	v3attr;
	ndmp9_valid_u_long	v4attr;
	ndmp9_pval		capability<>;
};

struct ndmp9_device_info {
	string			model<>;
	ndmp9_device_capability	caplist<>;
};

const NDMP9_CONFIG_CONNTYPE_LOCAL	= 0x0001;
const NDMP9_CONFIG_CONNTYPE_TCP		= 0x0002;

const NDMP9_CONFIG_AUTHTYPE_NONE	= 0x0001;
const NDMP9_CONFIG_AUTHTYPE_TEXT	= 0x0002;
const NDMP9_CONFIG_AUTHTYPE_MD5		= 0x0004;

struct ndmp9_config_info {
	ndmp9_error		error;

	/* ndmp[23]_config_get_host_info_reply */
	string			hostname<>; /* host name */
	string			os_type<>;  /* The O/S type (e.g. SOLARIS) */
	string			os_vers<>;  /* The O/S version (e.g. 2.5) */
	string			hostid<>;

	/* ndmp[34]_config_get_server_info_reply */
	string			vendor_name<>;
	string			product_name<>;
	string			revision_number<>;

	/* ndmp2_config_get_host_info */
	/* ndmp[34]_config_get_server_info */
	uint32_t		authtypes;

	/* ndmp2_config_get_mover_type */
	/* ndmp[34]_config_get_connection_type */
	uint32_t		conntypes;

	/* ndmp2_config_get_butype_attr */
	/* ndmp[34]_config_get_butype_info */
	ndmp9_butype_info	butype_info<>;

	/* ndmp[34]_config_get_fs_info */
	ndmp9_fs_info		fs_info<>;

	/* ndmp[34]_config_get_tape_info */
	ndmp9_device_info	tape_info<>;

	/* ndmp[34]_config_get_scsi_info */
	ndmp9_device_info	scsi_info<>;
};

/* NDMP9_CONFIG_GET_INFO */
typedef ndmp9_no_arguments	ndmp9_config_get_info_request;
struct ndmp9_config_get_info_reply {
	ndmp9_error		error;
	ndmp9_config_info	config_info;
};

/* NDMP9_CONFIG_GET_HOST_INFO */
typedef ndmp9_no_arguments	ndmp9_config_get_host_info_request;
typedef ndmp9_config_get_info_reply ndmp9_config_get_host_info_reply;

/* NDMP9_CONFIG_GET_CONNECTION_TYPE */
typedef ndmp9_no_arguments	ndmp9_config_get_connection_type_request;
typedef ndmp9_config_get_info_reply ndmp9_config_get_connection_type_reply;

/* NDMP9_CONFIG_GET_SERVER_INFO */
typedef ndmp9_no_arguments	ndmp9_config_get_server_info_request;
typedef ndmp9_config_get_info_reply ndmp9_config_get_server_info_reply;

/* NDMP9_CONFIG_GET_BUTYPE_INFO */
typedef ndmp9_no_arguments	ndmp9_config_get_butype_info_request;
typedef ndmp9_config_get_info_reply ndmp9_config_get_butype_info_reply;

/* NDMP9_CONFIG_GET_FS_INFO */
typedef ndmp9_no_arguments	ndmp9_config_get_fs_info_request;
typedef ndmp9_config_get_info_reply ndmp9_config_get_fs_info_reply;

/* NDMP9_CONFIG_GET_TAPE_INFO */
typedef ndmp9_no_arguments	ndmp9_config_get_tape_info_request;
typedef ndmp9_config_get_info_reply ndmp9_config_get_tape_info_reply;

/* NDMP9_CONFIG_GET_SCSI_INFO */
typedef ndmp9_no_arguments	ndmp9_config_get_scsi_info_request;
typedef ndmp9_config_get_info_reply ndmp9_config_get_scsi_info_reply;


/* NDMP9_CONFIG_GET_AUTH_ATTR */
struct ndmp9_config_get_auth_attr_request {
	ndmp9_auth_type		auth_type;
};

struct ndmp9_config_get_auth_attr_reply {
	ndmp9_error		error;
	ndmp9_auth_attr		server_attr;
};



/*
 * SCSI INTERFACE
 ****************************************************************
 *
 * A SCSI pass-thru service. The CONTROL Agent (DMA, Client)
 * manipulates a SCSI Media Changer through this interface.
 * It may be used for other purposes.
 */

/* NDMP9_SCSI_OPEN */
struct ndmp9_scsi_open_request {
	string		device<>;
};
typedef ndmp9_just_error_reply	ndmp9_scsi_open_reply;

/* NDMP9_SCSI_CLOSE */
typedef ndmp9_no_arguments	ndmp9_scsi_close_request;
typedef ndmp9_just_error_reply	ndmp9_scsi_close_reply;

/* NDMP9_SCSI_GET_STATE */
typedef ndmp9_no_arguments	ndmp9_scsi_get_state_request;
struct ndmp9_scsi_get_state_reply {
	ndmp9_error	error;
	short		target_controller;
	short		target_id;
	short		target_lun;
};

/* NDMP9_SCSI_SET_TARGET -- deleted for NDMPv4 */
struct ndmp9_scsi_set_target_request {
	string		device<>;
	uint16_t	target_controller;
	uint16_t	target_id;
	uint16_t	target_lun;
};
typedef ndmp9_just_error_reply	ndmp9_scsi_set_target_reply;


/* NDMP9_SCSI_RESET_DEVICE */
typedef ndmp9_no_arguments	ndmp9_scsi_reset_device_request;
typedef ndmp9_just_error_reply	ndmp9_scsi_reset_device_reply;

/* NDMP9_SCSI_RESET_BUS -- deleted for NDMPv4 */
typedef ndmp9_no_arguments	ndmp9_scsi_reset_bus_request;
typedef ndmp9_just_error_reply	ndmp9_scsi_reset_bus_reply;


/* NDMP9_SCSI_EXECUTE_CDB */
enum ndmp9_scsi_data_dir {
	NDMP9_SCSI_DATA_DIR_NONE = 0,
	NDMP9_SCSI_DATA_DIR_IN = 1,	/* Expect data from SCSI device */
	NDMP9_SCSI_DATA_DIR_OUT = 2	/* Transfer data to SCSI device */
};

struct ndmp9_execute_cdb_request {
	ndmp9_scsi_data_dir	data_dir;
	uint32_t		timeout;
	uint32_t		datain_len;	/* Set for expected datain */
	opaque			cdb<>;
	opaque			dataout<>;
};

struct ndmp9_execute_cdb_reply {
	ndmp9_error	error;
	u_char		status;		/* SCSI status bytes */
	uint32_t	dataout_len;
	opaque		datain<>;	/* SCSI datain */
	opaque		ext_sense<>;	/* Extended sense data */
};

typedef ndmp9_execute_cdb_request	ndmp9_scsi_execute_cdb_request;
typedef ndmp9_execute_cdb_reply		ndmp9_scsi_execute_cdb_reply;



/******************/
/* TAPE INTERFACE */
/******************/
/* NDMP9_TAPE_OPEN */
enum ndmp9_tape_open_mode {
	NDMP9_TAPE_READ_MODE,
	NDMP9_TAPE_RDWR_MODE,
	NDMP9_TAPE_RAW_MODE		/* new for NDMPv4 */
};

enum ndmp9_tape_state {
	NDMP9_TAPE_STATE_IDLE,		/* not doing anything */
	NDMP9_TAPE_STATE_OPEN,		/* open, tape operations OK */
	NDMP9_TAPE_STATE_MOVER		/* mover active, tape ops locked out */
					/* ie read, write, mtio, close, cdb */
};

struct ndmp9_tape_open_request {
	string			device<>;
	ndmp9_tape_open_mode	mode;
};
typedef ndmp9_just_error_reply	ndmp9_tape_open_reply;

/* NDMP9_TAPE_CLOSE */
typedef ndmp9_no_arguments	ndmp9_tape_close_request;
typedef ndmp9_just_error_reply	ndmp9_tape_close_reply;


/* NDMP9_TAPE_GET_STATE */
const NDMP9_TAPE_STATE_NOREWIND	= 0x0008;	/* non-rewind device */
const NDMP9_TAPE_STATE_WR_PROT	= 0x0010;	/* write-protected */
const NDMP9_TAPE_STATE_ERROR	= 0x0020;	/* media error */
const NDMP9_TAPE_STATE_UNLOAD	= 0x0040;	/* tape will be unloaded when
						 * the device is closed */

typedef ndmp9_no_arguments	ndmp9_tape_get_state_request;
struct ndmp9_tape_get_state_reply {
	ndmp9_error		error;
	uint32_t		flags;		/* compatible NDMP[2349] */
	ndmp9_tape_state	state;
	ndmp9_tape_open_mode 	open_mode;
	ndmp9_valid_u_long	file_num;
	ndmp9_valid_u_long	soft_errors;
	ndmp9_valid_u_long	block_size;
	ndmp9_valid_u_long	blockno;
	ndmp9_valid_u_quad	total_space;
	ndmp9_valid_u_quad	space_remain;
	ndmp9_valid_u_long	partition;
};

/* NDMP9_TAPE_MTIO */
enum ndmp9_tape_mtio_op {
	NDMP9_MTIO_FSF,
	NDMP9_MTIO_BSF,
	NDMP9_MTIO_FSR,
	NDMP9_MTIO_BSR,
	NDMP9_MTIO_REW,
	NDMP9_MTIO_EOF,
	NDMP9_MTIO_OFF
};

struct ndmp9_tape_mtio_request {
	ndmp9_tape_mtio_op	tape_op;
	uint32_t		count;
};

struct ndmp9_tape_mtio_reply {
	ndmp9_error	error;
	uint32_t	resid_count;
};


/* NDMP9_TAPE_WRITE */
struct ndmp9_tape_write_request {
	opaque		data_out<>;
};

struct ndmp9_tape_write_reply {
	ndmp9_error	error;
	uint32_t	count;
};


/* NDMP9_TAPE_READ */
struct ndmp9_tape_read_request {
	uint32_t	count;
};

struct ndmp9_tape_read_reply {
	ndmp9_error	error;
	opaque		data_in<>;
};

/* NDMP9_TAPE_EXECUTE_CDB */
typedef ndmp9_execute_cdb_request	ndmp9_tape_execute_cdb_request;
typedef ndmp9_execute_cdb_reply		ndmp9_tape_execute_cdb_reply;




/********************************/
/* MOVER INTERFACE              */
/********************************/
enum ndmp9_mover_state {
	NDMP9_MOVER_STATE_IDLE,
	NDMP9_MOVER_STATE_LISTEN,
	NDMP9_MOVER_STATE_ACTIVE,
	NDMP9_MOVER_STATE_PAUSED,
	NDMP9_MOVER_STATE_HALTED,
	NDMP9_MOVER_STATE_STANDBY	/* awaiting mover_read_request */
};

enum ndmp9_mover_mode {
	NDMP9_MOVER_MODE_READ,	/* read from data conn; write to tape */
	NDMP9_MOVER_MODE_WRITE	/* write to data conn; read from tape */
};

enum ndmp9_mover_pause_reason {
	NDMP9_MOVER_PAUSE_NA,
	NDMP9_MOVER_PAUSE_EOM,
	NDMP9_MOVER_PAUSE_EOF,
	NDMP9_MOVER_PAUSE_SEEK,
	NDMP9_MOVER_PAUSE_MEDIA_ERROR,
	NDMP9_MOVER_PAUSE_EOW
};

enum ndmp9_mover_halt_reason {
	NDMP9_MOVER_HALT_NA,
	NDMP9_MOVER_HALT_CONNECT_CLOSED,
	NDMP9_MOVER_HALT_ABORTED,
	NDMP9_MOVER_HALT_INTERNAL_ERROR,
	NDMP9_MOVER_HALT_CONNECT_ERROR,
	NDMP9_MOVER_HALT_MEDIA_ERROR
};

/* NDMP9_MOVER_GET_STATE */
typedef ndmp9_no_arguments	ndmp9_mover_get_state_request;
struct ndmp9_mover_get_state_reply {
	ndmp9_error		error;
	ndmp9_mover_state	state;
	ndmp9_mover_mode	mode;
	ndmp9_mover_pause_reason pause_reason;
	ndmp9_mover_halt_reason	halt_reason;
	uint32_t		record_size;
	uint32_t		record_num;
	ndmp9_u_quad		bytes_moved;
	ndmp9_u_quad		seek_position;
	ndmp9_u_quad		bytes_left_to_read;
	ndmp9_u_quad		window_offset;
	ndmp9_u_quad		window_length;
	ndmp9_addr		data_connection_addr;
};

/* NDMP9_MOVER_LISTEN */
struct ndmp9_mover_listen_request {
	ndmp9_mover_mode	mode;
	ndmp9_addr_type		addr_type;
};

struct ndmp9_mover_listen_reply {
	ndmp9_error		error;
	ndmp9_addr		data_connection_addr;
};

/* NDMP9_MOVER_CONNECT */
struct ndmp9_mover_connect_request {
	ndmp9_mover_mode	mode;
	ndmp9_addr		addr;
};
typedef ndmp9_just_error_reply	ndmp9_mover_connect_reply;

/* NDMP9_MOVER_SET_RECORD_SIZE */
struct ndmp9_mover_set_record_size_request {
	uint32_t		record_size;
};
typedef ndmp9_just_error_reply	ndmp9_mover_set_record_size_reply;

/* NDMP9_MOVER_SET_WINDOW */
struct ndmp9_mover_set_window_request {
	ndmp9_u_quad	offset;
	ndmp9_u_quad	length;
};
typedef ndmp9_just_error_reply	ndmp9_mover_set_window_reply;

/* NDMP9_MOVER_CONTINUE */
typedef ndmp9_no_arguments	ndmp9_mover_continue_request;
typedef ndmp9_just_error_reply	ndmp9_mover_continue_reply;


/* NDMP9_MOVER_ABORT */
typedef ndmp9_no_arguments	ndmp9_mover_abort_request;
typedef ndmp9_just_error_reply	ndmp9_mover_abort_reply;

/* NDMP9_MOVER_STOP */
typedef ndmp9_no_arguments	ndmp9_mover_stop_request;
typedef ndmp9_just_error_reply	ndmp9_mover_stop_reply;

/* NDMP9_MOVER_READ */
struct ndmp9_mover_read_request {
	ndmp9_u_quad	offset;
	ndmp9_u_quad	length;
};
typedef ndmp9_just_error_reply	ndmp9_mover_read_reply;

/* NDMP9_MOVER_CLOSE */
typedef ndmp9_no_arguments	ndmp9_mover_close_request;
typedef ndmp9_just_error_reply	ndmp9_mover_close_reply;




/****************************/
/* DATA INTERFACE	    */
/****************************/

enum ndmp9_data_operation {
	NDMP9_DATA_OP_NOACTION,
	NDMP9_DATA_OP_BACKUP,
	NDMP9_DATA_OP_RECOVER,
	NDMP9_DATA_OP_RECOVER_FILEHIST
};

enum ndmp9_data_state {
	NDMP9_DATA_STATE_IDLE,
	NDMP9_DATA_STATE_ACTIVE,
	NDMP9_DATA_STATE_HALTED,
	NDMP9_DATA_STATE_LISTEN,
	NDMP9_DATA_STATE_CONNECTED
};

enum ndmp9_data_halt_reason {
	NDMP9_DATA_HALT_NA,
	NDMP9_DATA_HALT_SUCCESSFUL,
	NDMP9_DATA_HALT_ABORTED,
	NDMP9_DATA_HALT_INTERNAL_ERROR,
	NDMP9_DATA_HALT_CONNECT_ERROR
};

/* NDMP9_DATA_START_BACKUP */
typedef ndmp9_no_arguments	ndmp9_data_get_state_request;
struct ndmp9_data_get_state_reply {
	ndmp9_error		error;
	ndmp9_data_operation	operation;
	ndmp9_data_state	state;
	ndmp9_data_halt_reason	halt_reason;
	ndmp9_u_quad		bytes_processed;
	ndmp9_valid_u_quad	est_bytes_remain;
	ndmp9_valid_u_long	est_time_remain;
	ndmp9_addr		data_connection_addr;
	ndmp9_u_quad		read_offset;
	ndmp9_u_quad		read_length;
};

struct ndmp9_name {
	string			original_path<>; /* relative to backup root */
	string			destination_path<>;
	/* nt_destination_path<> */
	string			name<>;
	string			other_name<>;
	ndmp9_u_quad		node;
	ndmp9_valid_u_quad	fh_info;
};



/* NDMP9_DATA_START_BACKUP */
struct ndmp9_data_start_backup_request {
	string		bu_type<>;	/* backup method to use */
	ndmp9_pval	env<>;		/* Parameters that may modify backup */
	ndmp9_addr	addr;
};
typedef ndmp9_just_error_reply	ndmp9_data_start_backup_reply;

/* NDMP9_DATA_START_RECOVER */
struct ndmp9_data_start_recover_request {
	ndmp9_pval	env<>;
	ndmp9_name	nlist<>;
	string		bu_type<>;
	ndmp9_addr	addr;
};
typedef ndmp9_just_error_reply	ndmp9_data_start_recover_reply;

/* NDMP9_DATA_START_RECOVER_FILEHIST */
typedef ndmp9_data_start_recover_request ndmp9_data_start_recover_filehist_request;
typedef ndmp9_data_start_recover_reply ndmp9_data_start_recover_filehist_reply;


/* NDMP9_DATA_ABORT */
typedef ndmp9_no_arguments	ndmp9_data_abort_request;
typedef ndmp9_just_error_reply	ndmp9_data_abort_reply;

/* NDMP9_DATA_STOP */
typedef ndmp9_no_arguments	ndmp9_data_stop_request;
typedef ndmp9_just_error_reply	ndmp9_data_stop_reply;

/* NDMP9_DATA_GET_ENV */
typedef ndmp9_no_arguments	ndmp9_data_get_env_request;
struct ndmp9_data_get_env_reply {
	ndmp9_error	error;
	ndmp9_pval	env<>;
};

/* NDMP9_DATA_LISTEN */
struct ndmp9_data_listen_request {
	ndmp9_addr_type	addr_type;
};

struct ndmp9_data_listen_reply {
	ndmp9_error	error;
	ndmp9_addr	data_connection_addr;
};

/* NDMP9_DATA_CONNECT */
struct ndmp9_data_connect_request {
	ndmp9_addr	addr;
};
typedef ndmp9_just_error_reply	ndmp9_data_connect_reply;




/****************************/
/* NOTIFY INTERFACE	    */
/****************************/

/* NDMP9_NOTIFY_DATA_HALTED */
struct ndmp9_notify_data_halted_request {
	ndmp9_data_halt_reason	reason;
};

enum ndmp9_connect_reason {
	NDMP9_CONNECTED,	/* Connect sucessfully */
	NDMP9_SHUTDOWN,		/* Connection shutdown */
	NDMP9_REFUSED		/* reach the maximum number of connections */
};

/* NDMP9_NOTIFY_CONNECTED */
struct ndmp9_notify_connected_request {
	ndmp9_connect_reason	reason;
	uint16_t		protocol_version;
	string			text_reason<>;
};

/* NDMP9_NOTIFY_MOVER_HALTED */
struct ndmp9_notify_mover_halted_request {
	ndmp9_mover_halt_reason	reason;
};

/* NDMP9_NOTIFY_MOVER_PAUSED */
struct ndmp9_notify_mover_paused_request {
	ndmp9_mover_pause_reason reason;
	ndmp9_u_quad		seek_position;
};

/* NDMP9_NOTIFY_DATA_READ */
struct ndmp9_notify_data_read_request {
	ndmp9_u_quad	offset;
	ndmp9_u_quad	length;
};


/********************************/
/* LOG INTERFACE		*/
/********************************/
/* NDMP9_LOG_MESSAGE */
enum ndmp9_log_type {
	NDMP9_LOG_NORMAL,
	NDMP9_LOG_DEBUG,
	NDMP9_LOG_ERROR,
	NDMP9_LOG_WARNING
};

struct ndmp9_log_message_request {
	ndmp9_log_type		log_type;
	uint32_t		message_id;
	string			entry<>;
	ndmp9_valid_u_long	associated_message_sequence;
};
/* No reply */

enum ndmp9_recovery_status {
	NDMP9_RECOVERY_SUCCESSFUL             = 0,
	NDMP9_RECOVERY_FAILED_PERMISSION      = 1,
	NDMP9_RECOVERY_FAILED_NOT_FOUND       = 2,
	NDMP9_RECOVERY_FAILED_NO_DIRECTORY    = 3,
	NDMP9_RECOVERY_FAILED_OUT_OF_MEMORY   = 4,
	NDMP9_RECOVERY_FAILED_IO_ERROR        = 5,
	NDMP9_RECOVERY_FAILED_UNDEFINED_ERROR = 6
};

/* NDMP9_LOG_FILE */
struct ndmp9_log_file_request {
	string			name<>;
	ndmp9_recovery_status	recovery_status;
};
/* No reply */



/*
 * FILE HISTORY INTERFACES
 ****************************************************************
 */


enum ndmp9_file_type {
	NDMP9_FILE_DIR,
	NDMP9_FILE_FIFO,
	NDMP9_FILE_CSPEC,
	NDMP9_FILE_BSPEC,
	NDMP9_FILE_REG,
	NDMP9_FILE_SLINK,
	NDMP9_FILE_SOCK,
	NDMP9_FILE_REGISTRY,
	NDMP9_FILE_OTHER
};

struct ndmp9_file_stat {
	ndmp9_file_type		ftype;
	ndmp9_valid_u_long	mtime;
	ndmp9_valid_u_long	atime;
	ndmp9_valid_u_long	ctime;
	ndmp9_valid_u_long	uid;
	ndmp9_valid_u_long	gid;
	ndmp9_valid_u_long	mode;
	ndmp9_valid_u_quad	size;
	ndmp9_valid_u_long	links;

	/*
	 * Add NT attributes here as ndmp9_valid_....
	 */

	ndmp9_valid_u_quad	node;	    /* id on disk at backup time */
	ndmp9_valid_u_quad	fh_info;    /* id on tape at backup time */
};


/*
 * ndmp_fh_add_file
 * no reply
 */
struct ndmp9_file {
	string			unix_path<>;
	/* nt_path<> here */
	/* dos_path<> here */
	ndmp9_file_stat		fstat;
};

struct ndmp9_fh_add_file_request {
	ndmp9_file		files<>;
};


/*
 * ndmp_fh_add_dir
 * no reply
 */
struct ndmp9_dir {
	string			unix_name<>;
	/* nt_name<> here */
	/* dos_name<> here */
	ndmp9_u_quad		node;
	ndmp9_u_quad		parent;
};

struct ndmp9_fh_add_dir_request {
	ndmp9_dir		dirs<>;
};


/*
 * ndmp_fh_add_node
 * no reply
 */
struct ndmp9_node {
	ndmp9_file_stat		fstat;
};

struct ndmp9_fh_add_node_request {
	ndmp9_node		nodes<>;
};
/* No reply */
