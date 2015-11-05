/*
 * Copyright (c) 1998,1999,2000
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

#define WRAP_INVALID_FHINFO	(-1ull)
#define WRAP_MAX_PATH		(1024+512)
#define WRAP_MAX_NAME		256
#define WRAP_MAX_ENV		100
#define WRAP_MAX_FILE		100
#define WRAP_MAX_COMMAND	(20*1024)
#define WRAP_MAX_O_OPTION	100

#ifdef  __cplusplus
extern "C" {
#endif

/* forward */
struct wrap_ccb;

/*
 * MAIN helpers
 ****************************************************************
 */

extern int	wrap_main (int ac, char *av[], struct wrap_ccb *wccb);
extern int	wrap_main_start_index_file (struct wrap_ccb *wccb);
extern int	wrap_main_start_image_file (struct wrap_ccb *wccb);

extern void	wrap_log (struct wrap_ccb *wccb, char *fmt, ...);

extern int	wrap_set_error (struct wrap_ccb *wccb, int error);




/*
 * Command Execution
 ****************************************************************
 *
 * This wrapper will spawn (fork/exec) a subprocess to do
 * the real work. These help form the sh(1) command line,
 * create the pipe fittings, and spawn the subprocess.
 *
 * The fdmap[3] corresponds to the subprocess stdin, stdout,
 * and stderr. A value >= 0 is assumed to be an inherited
 * file descriptor. A value < 0 is one of the special codes
 * WRAP_FDMAP_xxx. On return, the _PIPE special codes are
 * replaced with the file descriptor for the parent process
 * end of the pipe.
 */

#define WRAP_FDMAP_INPUT_PIPE	-2	/* input to child, parent writes */
#define WRAP_FDMAP_OUTPUT_PIPE	-3	/* output from child, parent reads */
#define WRAP_FDMAP_DEV_NULL	-4	/* /dev/null */

extern int	wrap_cmd_add_with_escapes (char *cmd, char *word,
				char *special);
extern int	wrap_cmd_add_with_sh_escapes (char *cmd, char *word);
extern int	wrap_cmd_add_allow_file_wildcards (char *cmd, char *word);
extern int	wrap_pipe_fork_exec (char *cmd, int fdmap[3]);




/*
 * CCB -- Command Control Block
 ****************************************************************
 *
 * A digested form of command line arguments
 */

enum wrap_ccb_op {
	WRAP_CCB_OP_NONE = 0,
	WRAP_CCB_OP_BACKUP = 1,			/* -c */
	WRAP_CCB_OP_RECOVER = 2,		/* -x */
	WRAP_CCB_OP_RECOVER_FILEHIST = 3	/* -t */
};

struct wrap_env {
	char *		name;
	char *		value;
};

struct wrap_file {
	uint64_t		fhinfo;
	char *			original_name;	/* relative to backup root */
	char *			save_to_name;	/* relative to file system */
};

struct wrap_ccb {
	int			error;
	int			log_seq_num;
	char			errmsg[WRAP_MAX_NAME];

	/* Raw arguments */
	char *			B_butype;		/* -B TYPE */
	int			d_debug;		/* -d N */
	struct wrap_env		env[WRAP_MAX_ENV];	/* -E NAME=VALUE */
	int			n_env;
	char *			f_file_name;		/* -f FILE */
	char *			I_index_file_name;	/* -I FILE */
	char *			o_option[WRAP_MAX_O_OPTION]; /* -o OPTION */
	int			n_o_option;

	struct wrap_file	file[WRAP_MAX_FILE];	/* recovery only */
	int			n_file;

	/* derived from arguments */
	char *			progname;
	enum wrap_ccb_op	op;
	FILE *			index_fp;
	int			data_conn_fd;

	/* Common interprettations of the env */
	int			hist_enable;
	int			direct_enable;
	char *			backup_root;

	/*
	 * Recovery variables.
	 *
	 * All offset/length pairs refer to a portion of the
	 * backup image.
	 *
	 * have		The portion currently in the buffer.
	 * want		The portion wanted by the formatter
	 *		(e.g. tar, dump).
	 * reading	The portion immediately after what we
	 *		"have" (in the buffer) still coming due
	 *		to the last NDMP_NOTIFY_DATA_READ.
	 * last_read	The portion requested by the last
	 *		NDMP_NOTIFY_DATA_READ.
	 * expect	Composite of have and reading.
	 */
	char *			iobuf;
	uint32_t		n_iobuf;

	char *			have;
	uint64_t		have_offset;
	uint32_t		have_length;	/* never bigger than iobuf */
	uint64_t		want_offset;
	uint64_t		want_length;
	uint64_t		reading_offset;
	uint64_t		reading_length;
	uint64_t		last_read_offset;
	uint64_t		last_read_length;
	uint64_t		expect_offset;
	uint64_t		expect_length;
	int			data_conn_mode;
};

extern int	wrap_process_args (int argc, char *argv[],
			struct wrap_ccb *wccb);
extern char *	wrap_find_env (struct wrap_ccb *wccb, char *name);


extern int	wrap_reco_seek (struct wrap_ccb *wccb,
			uint64_t want_offset,
			uint64_t want_length,
			uint32_t must_have_length);
extern int	wrap_reco_must_have (struct wrap_ccb *wccb,
			uint32_t length);
extern int	wrap_reco_pass (struct wrap_ccb *wccb, int write_fd,
			uint64_t length, unsigned write_bsize);
extern int	wrap_reco_align_to_wanted (struct wrap_ccb *wccb);
extern int	wrap_reco_receive (struct wrap_ccb *wccb);
extern int	wrap_reco_consume (struct wrap_ccb *wccb,
			uint32_t length);
extern int	wrap_reco_issue_read (struct wrap_ccb *wccb);





/*
 * WRAP Messages
 ****************************************************************
 *
 * A message is simply one text line following a format.
 * These structures are used to buffer incoming messages.
 */

struct wrap_log_message {				/* Lx */
	char			message[WRAP_MAX_PATH];
};

enum wrap_ftype {
	WRAP_FTYPE_INVALID = 0,
	WRAP_FTYPE_DIR = 1,		/* d */
	WRAP_FTYPE_FIFO = 2,		/* p */
	WRAP_FTYPE_CSPEC = 3,		/* c */
	WRAP_FTYPE_BSPEC = 4,		/* b */
	WRAP_FTYPE_REG = 5,		/* - */
	WRAP_FTYPE_SLINK = 6,		/* l */
	WRAP_FTYPE_SOCK = 7,		/* s */
	WRAP_FTYPE_REGISTRY = 8,	/* R */
	WRAP_FTYPE_OTHER = 9		/* o */
};

struct wrap_fstat {
	uint32_t		valid;
#define WRAP_FSTAT_VALID_FTYPE		(1ul<<0u)
#define WRAP_FSTAT_VALID_MODE		(1ul<<1u)
#define WRAP_FSTAT_VALID_LINKS		(1ul<<2u)
#define WRAP_FSTAT_VALID_SIZE		(1ul<<3u)
#define WRAP_FSTAT_VALID_UID		(1ul<<4u)
#define WRAP_FSTAT_VALID_GID		(1ul<<5u)
#define WRAP_FSTAT_VALID_ATIME		(1ul<<6u)
#define WRAP_FSTAT_VALID_MTIME		(1ul<<7u)
#define WRAP_FSTAT_VALID_CTIME		(1ul<<8u)
#define WRAP_FSTAT_VALID_FILENO		(1ul<<9u)

	enum wrap_ftype		ftype;		/* f%s */
	uint16_t		mode;		/* m%04o */
	uint32_t		links;		/* l%lu */
	uint64_t		size;		/* s%llu */
	uint32_t		uid;		/* u%lu */
	uint32_t		gid;		/* g%lu */
	uint32_t		atime;		/* ta%lu */
	uint32_t		mtime;		/* tm%lu */
	uint32_t		ctime;		/* tc%lu */
	uint64_t		fileno;		/* i%llu */
};

/*
 * HF path [@fhinfo] [stats]
 *
 * History File -- Corresponds to NDMPv?_FH_ADD_FILE
 */
struct wrap_add_file {
	uint64_t		fhinfo;		/* @%llu */
	struct wrap_fstat	fstat;
	char			path[WRAP_MAX_PATH];
};

/*
 * HD dir_fileno name fileno [@fhinfo]
 *
 * History Directory entry -- Corresponds to NDMPv?_FH_ADD_DIR
 */
struct wrap_add_dirent {
	uint64_t		fhinfo;		/* @%llu */
	uint64_t		dir_fileno;	/* %llu */
	uint64_t		fileno;		/* %llu */
	char			name[WRAP_MAX_NAME];
};

/*
 * HN [@fhinfo] [stats] -- iFILENO must be present
 *
 * History Node -- Corresponds to NDMPv?_FH_ADD_NODE
 */
struct wrap_add_node {					/* HN */
	uint64_t		fhinfo;		/* @%llu */
	struct wrap_fstat	fstat;
};

/*
 * DE name value
 *
 * Data Env -- Corresponds to NDMPv?_DATA_GET_ENV
 * This is used for the post backup processing env[].
 */
struct wrap_add_env {
	char			name[WRAP_MAX_NAME];
	char			value[WRAP_MAX_PATH];
};

/*
 * DR offset length
 *
 * Data Read -- Corresponds to NDMPv?_NOTIFY_DATA_READ
 * This is used during recovery operations to retrieve
 * portions of the backup image.
 */
struct wrap_data_read {					/* DR */
	uint64_t		offset;		/* %llu */
	uint64_t		length;		/* %llu */
};

/*
 * DS s{r|d|f} [wN] [etN] [ebN]
 *
 * Data Stats -- Supplemental info for NDMPv?_DATA_GET_STATE
 * Sent periodically to update certain fields of the
 * DATA_GET_STATE reply.
 */

enum wrap_data_status {
	WRAP_DS_INVALID = 0,
	WRAP_DS_RUNNING = 1,			/* sr */
	WRAP_DS_DONE_OK = 2,			/* sd */
	WRAP_DS_DONE_FAILED = 3			/* sf */
};

struct wrap_data_stats {				/* DS */
	uint32_t		valid;
#define WRAP_DATASTATS_VALID_BYTES_WRITTEN	(1ul<<0u)
#define WRAP_DATASTATS_VALID_EST_TIME_REMAINING	(1ul<<1u)
#define WRAP_DATASTATS_VALID_EST_BYTES_REMAINING (1ul<<2u)

	enum wrap_data_status	status;			/* s{r|d|f} */
	uint64_t		bytes_written;		/* w%llu */
	uint64_t 		est_time_remaining;	/* et%llu */
	uint64_t		est_bytes_remaining;	/* eb%llu */
};

/*
 * RR errno path
 *
 * Recovery Result -- Corresponds to NDMPv?_LOG_FILE
 * Sent during recovery operations to report the
 * success or failure of recovery.
 */
struct wrap_recovery_result {				/* RR */
	int			rr_errno;	/* sys/errno.h */
	char			path[WRAP_MAX_PATH];
};

enum wrap_msg_type {
	WRAP_MSGTYPE_LOG_MESSAGE = 1,
	WRAP_MSGTYPE_ADD_FILE = 2,
	WRAP_MSGTYPE_ADD_DIRENT = 3,
	WRAP_MSGTYPE_ADD_NODE = 4,
	WRAP_MSGTYPE_ADD_ENV = 5,
	WRAP_MSGTYPE_DATA_READ = 6,
	WRAP_MSGTYPE_DATA_STATS = 7,
	WRAP_MSGTYPE_RECOVERY_RESULT = 8,
};

struct wrap_msg_buf {
	enum wrap_msg_type	msg_type;
	union {
	  struct wrap_log_message	log_message;
	  struct wrap_add_file		add_file;
	  struct wrap_add_dirent	add_dirent;
	  struct wrap_add_node		add_node;
	  struct wrap_add_env		add_env;
	  struct wrap_data_read		data_read;
	  struct wrap_data_stats	data_stats;
	  struct wrap_recovery_result	recovery_result;
	} body;
};

extern int wrap_parse_msg (char *buf, struct wrap_msg_buf *wmsg);
extern int wrap_parse_log_message_msg (char *buf, struct wrap_msg_buf *wmsg);
extern int wrap_send_log_message (FILE *fp, char *message);
extern int wrap_parse_add_file_msg (char *buf, struct wrap_msg_buf *wmsg);
extern int wrap_send_add_file (FILE *fp, char *path, uint64_t fhinfo,
				struct wrap_fstat *fstat);
extern int wrap_parse_add_dirent_msg (char *buf, struct wrap_msg_buf *wmsg);
extern int wrap_send_add_dirent (FILE *fp, char *name,
				uint64_t fhinfo, uint64_t dir_fileno,
				uint64_t fileno);
extern int wrap_parse_add_node_msg (char *buf, struct wrap_msg_buf *wmsg);
extern int wrap_send_add_node (FILE *fp, uint64_t fhinfo,
				struct wrap_fstat *fstat);
extern int wrap_parse_fstat_subr (char **scanp, struct wrap_fstat *fstat);
extern int wrap_send_fstat_subr (FILE *fp, struct wrap_fstat *fstat);
extern int wrap_parse_add_env_msg (char *buf, struct wrap_msg_buf *wmsg);
extern int wrap_send_add_env (FILE *fp, char *name, char *value);
extern int wrap_parse_data_read_msg (char *buf, struct wrap_msg_buf *wmsg);
extern int wrap_send_data_read (FILE *fp, uint64_t offset, uint64_t length);




/*
 * Canonical strings
 ****************************************************************
 * Convert strings to/from HTTP-like canonical strings (%xx).
 * Example "a b%c" --> "a%20b%25c"
 */

#define NDMCSTR_WARN	'%'
extern int	wrap_cstr_from_str (char *src, char *dst, unsigned dst_max);
extern int	wrap_cstr_to_str (char *src, char *dst, unsigned dst_max);
extern int	wrap_cstr_from_hex (int c);

#ifdef  __cplusplus
}
#endif
