/*
 * Copyright (c) 1998,1999,2000,2002
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


#ifndef _NDMLIB_H_
#define _NDMLIB_H_

#include "ndmos.h"

#include "ndmprotocol.h"
#include "ndmp_msg_buf.h"
#include "ndmp_translate.h"

/* Probably unnecessary, yet prudent. Compilers/debuggers sometimes goof. */
#ifndef NDM_FLAG_DECL
#define NDM_FLAG_DECL(XXX) unsigned XXX : 1;
#endif /* !NDM_FLAG_DECL */

#ifdef  __cplusplus
extern "C" {
#endif

/* boring forward reference stuff */
struct ndmagent;




/*
 * NDMLOG
 ****************************************************************
 *
 * ndmlog is a simple abstraction for log messages.
 * Each log entry has:
 *   - a tag, which is a short string indicating origin or purpose
 *   - a level between 0-9, the higher the value the greater the detail
 *   - a message
 * The application will typically direct log messages to a file.
 * Yet, logging directly to a FILE tends to be restrictive. Hence
 * this abstraction.
 *
 * The time stamp is relative to the start time, and has millisecond
 * granularity.
 */

struct ndmlog {
	void	(*deliver)(struct ndmlog *log, char *tag, int lev, char *msg);
	void *	ctx;
	struct ndm_fhdb_callbacks *nfc;
};
extern char *	ndmlog_time_stamp (void);
extern void	ndmlogf (struct ndmlog *log, char *tag,
					int level, char *fmt, ...);
extern void	ndmlogfv (struct ndmlog *log, char *tag,
					int level, char *fmt, va_list ap);




/*
 * NDMNMB -- NDMP Message Buffer
 ****************************************************************
 *
 * The ndmnmb routines are trivial aids for handling
 * NMB (NDMP Messsage Buffer). ndmp_msg_buf is defined in
 * ndmp_msg_buf.h, and pretty much amounts to a huge
 * union of all NDMP request and reply types.
 */

extern xdrproc_t	ndmnmb_find_xdrproc (struct ndmp_msg_buf *nmb);
extern void		ndmnmb_free (struct ndmp_msg_buf *nmb);
extern void		ndmnmb_snoop (struct ndmlog *log, char *tag, int level,
				struct ndmp_msg_buf *nmb, char *whence);
extern unsigned		ndmnmb_get_reply_error_raw (struct ndmp_msg_buf *nmb);
extern ndmp9_error	ndmnmb_get_reply_error (struct ndmp_msg_buf *nmb);
extern int		ndmnmb_set_reply_error_raw (struct ndmp_msg_buf *nmb,
				unsigned raw_error);
extern int		ndmnmb_set_reply_error (struct ndmp_msg_buf *nmb,
				ndmp9_error error);







/*
 * NDMCHAN -- Async I/O channel
 ****************************************************************
 *
 * ndmchan is a wrapper around I/O channels, and is used
 * to juggle (manage) multiple I/O activities at one time.
 * The data buffer is used linearly. beg_ix and end_ix
 * bracket the valid data. When the end of the buffer is reached,
 * the remaining valid data is moved to the begining.
 */
struct ndmchan {
	char *		name;		/* short name, helps debugging */

	char		mode;		/* NDMCHAN_MODE_... (see below) */

	NDM_FLAG_DECL(check)		/* Want select()/poll() to check */
	NDM_FLAG_DECL(ready)		/* select()/poll() indicates ready */
	NDM_FLAG_DECL(eof)		/* eof pending upon n_ready()==0 */
	NDM_FLAG_DECL(error)		/* error (channel shutdown) */

	int		fd;		/* der eff dee */
	int		saved_errno;	/* errno captured if ->error occurs */

	unsigned	beg_ix;		/* relative to ->data */
	unsigned	end_ix;		/* relative to ->data */
	char *		data;		/* data buffer (READ/WRITE/RESIDENT) */
	unsigned	data_size;	/* size of data buffer */
};
#define NDMCHAN_MODE_IDLE	0	/* not doing anything */
#define NDMCHAN_MODE_RESIDENT	1	/* resident, within this process */
#define NDMCHAN_MODE_READ	2	/* read from ->fd into ->data */
#define NDMCHAN_MODE_WRITE	3	/* write to ->fd from ->data */
#define NDMCHAN_MODE_READCHK	4	/* check ->fd readable, no ->data */
#define NDMCHAN_MODE_LISTEN	5	/* ->fd listen()ing */
#define NDMCHAN_MODE_PENDING	6	/* ->fd and ->data ready */
#define NDMCHAN_MODE_CLOSED	7	/* ->fd closed */

enum ndmchan_read_interpretation {
	NDMCHAN_RI_EMPTY = 10,	/* no data, might be more coming */
	NDMCHAN_RI_READY,	/* data ready */
	NDMCHAN_RI_READY_FULL,	/* data ready, no more until consumed */
	NDMCHAN_RI_DRAIN_EOF,	/* data ready, DONE_EOF after consumed */
	NDMCHAN_RI_DRAIN_ERROR,	/* data ready, DONE_ERROR after consumed */
	NDMCHAN_RI_DONE_EOF,	/* no data, no more coming, normal EOF */
	NDMCHAN_RI_DONE_ERROR,	/* no data, no more coming, something wrong */
	NDMCHAN_RI_FAULT 	/* crazy request */
};

enum ndmchan_write_interpretation {
	NDMCHAN_WI_FULL = 30,	/* no buffer, no more until some sent */
	NDMCHAN_WI_AVAIL,	/* buffer ready, sending in progress */
	NDMCHAN_WI_AVAIL_EMPTY,	/* buffer ready, done sending */
	NDMCHAN_WI_DRAIN_EOF,	/* no more buffer, DONE_EOF after sent */
	NDMCHAN_WI_DRAIN_ERROR,	/* no more buffer, DONE_ERROR after sent */
	NDMCHAN_WI_DONE_EOF,	/* no more buffer, done sending, normal EOF */
	NDMCHAN_WI_DONE_ERROR,	/* no more buffer, done sending, went wrong */
	NDMCHAN_WI_FAULT 	/* crazy request */
};

extern void	ndmchan_initialize (struct ndmchan *ch, char *name);
extern int	ndmchan_setbuf (struct ndmchan *ch, char *data,
			unsigned data_size);
extern int	ndmchan_start_mode (struct ndmchan *ch, int fd, int chan_mode);
extern int	ndmchan_start_read (struct ndmchan *ch, int fd);
extern int	ndmchan_start_write (struct ndmchan *ch, int fd);
extern int	ndmchan_start_readchk (struct ndmchan *ch, int fd);
extern int	ndmchan_start_listen (struct ndmchan *ch, int fd);
extern int	ndmchan_start_resident (struct ndmchan *ch);
extern int	ndmchan_start_pending (struct ndmchan *ch, int fd);
extern int	ndmchan_pending_to_mode (struct ndmchan *ch, int chan_mode);
extern int	ndmchan_pending_to_read (struct ndmchan *ch);
extern int	ndmchan_pending_to_write (struct ndmchan *ch);
extern void	ndmchan_set_eof (struct ndmchan *ch);
extern void	ndmchan_close_set_errno (struct ndmchan *ch, int err_no);
extern void	ndmchan_close (struct ndmchan *ch);
extern void	ndmchan_abort (struct ndmchan *ch);
extern void	ndmchan_close_as_is (struct ndmchan *ch);
extern void	ndmchan_cleanup (struct ndmchan *ch);
extern int	ndmchan_quantum (struct ndmchan *chtab[],
			unsigned n_chtab, int milli_timo);
extern int	ndmchan_pre_poll (struct ndmchan *chtab[], unsigned n_chtab);
extern int	ndmchan_post_poll (struct ndmchan *chtab[], unsigned n_chtab);
extern void	ndmchan_compress (struct ndmchan *ch);
extern int	ndmchan_n_avail (struct ndmchan *ch);
extern int	ndmchan_n_avail_record (struct ndmchan *ch, uint32_t size);
extern int	ndmchan_n_avail_total (struct ndmchan *ch);
extern int	ndmchan_n_ready (struct ndmchan *ch);
extern enum ndmchan_read_interpretation
		ndmchan_read_interpret (struct ndmchan *ch, char **data_p,
			unsigned *n_ready_p);
extern enum ndmchan_write_interpretation
		ndmchan_write_interpret (struct ndmchan *ch, char **data_p,
			unsigned *n_avail_p);

extern void	ndmchan_pp (struct ndmchan *ch, char *buf);

extern int	ndmos_chan_poll (struct ndmchan *chtab[],
			unsigned n_chtab, int milli_timo);




/*
 * NDMCONN -- Bidirectional control connections
 ****************************************************************
 */

#define NDMCONN_TYPE_NONE		0
#define NDMCONN_TYPE_RESIDENT		1
#define NDMCONN_TYPE_REMOTE		2


#define NDMCONN_CALL_STATUS_HDR_ERROR	(-2)
#define NDMCONN_CALL_STATUS_BOTCH	(-1)
#define NDMCONN_CALL_STATUS_OK		0
#define NDMCONN_CALL_STATUS_REPLY_ERROR	1
#define NDMCONN_CALL_STATUS_REPLY_LATE	2


struct ndmconn {
	struct sockaddr	sa;

	struct ndmchan	chan;

	char		conn_type;
	char		protocol_version;
	char		was_allocated;

	void *		context;

	XDR		xdrs;
	unsigned char	frag_hdr_buf[4];	/* see ndmconn_readit() */
	unsigned	fhb_off;
	uint32_t	frag_resid;

	uint32_t	next_sequence;

	void		(*unexpected)(struct ndmconn *conn,
					struct ndmp_msg_buf *nmb);

	int		snoop_level;
	struct ndmlog *	snoop_log;

	char *		last_err_msg;

	int		(*call) (struct ndmconn *conn, struct ndmp_xa_buf *xa);

	struct ndmp_xa_buf call_xa_buf;

	int		last_message;
	int		last_call_status;
	ndmp9_error	last_header_error;
	ndmp9_error	last_reply_error;

	long   		sent_time;
	long		received_time;
	long            time_limit;
};

extern struct ndmconn *	ndmconn_initialize (struct ndmconn *aconn, char *name);
extern void		ndmconn_destruct (struct ndmconn *conn);
extern int		ndmconn_connect_agent (struct ndmconn *conn,
				struct ndmagent *agent);
extern int		ndmconn_connect_host_port (struct ndmconn *conn,
				char * hostname, int port,
				unsigned want_protocol_version);
extern int		ndmconn_connect_sockaddr_in (struct ndmconn *conn,
				struct sockaddr_in *sin,
				unsigned want_protocol_version);
extern int		ndmconn_try_open (struct ndmconn *conn,
				unsigned protocol_version);
extern int		ndmconn_accept (struct ndmconn *conn, int sock);
extern int		ndmconn_abort (struct ndmconn *conn);
extern int		ndmconn_close (struct ndmconn *conn);
extern int		ndmconn_fileno (struct ndmconn *conn);
extern int		ndmconn_auth_agent (struct ndmconn *conn,
				struct ndmagent *agent);
extern int		ndmconn_auth_none (struct ndmconn *conn);
extern int		ndmconn_auth_text (struct ndmconn *conn,
				char *id, char *pw);
extern int		ndmconn_auth_md5 (struct ndmconn *conn,
				char *id, char *pw);
extern int		ndmconn_call (struct ndmconn *conn,
				struct ndmp_xa_buf *xa);
extern int		ndmconn_exchange_nmb (struct ndmconn *conn,
				struct ndmp_msg_buf *request_nmb,
				struct ndmp_msg_buf *reply_nmb);
extern int		ndmconn_send_nmb (struct ndmconn *conn,
				struct ndmp_msg_buf *nmb);
extern int		ndmconn_recv_nmb (struct ndmconn *conn,
				struct ndmp_msg_buf *nmb);
extern void		ndmconn_free_nmb (struct ndmconn *conn,
				struct ndmp_msg_buf *nmb);
extern int		ndmconn_xdr_nmb (struct ndmconn *conn,
				struct ndmp_msg_buf *nmb,
				enum xdr_op x_op);
extern int		ndmconn_readit (void *a_conn, char *buf, int len);
extern int		ndmconn_writeit (void *a_conn, char *buf, int len);
extern int		ndmconn_sys_read (struct ndmconn *conn,
				char *buf, unsigned len);
extern int		ndmconn_sys_write (struct ndmconn *conn,
				char *buf, unsigned len);
extern void		ndmconn_unexpected (struct ndmconn *conn,
				struct ndmp_msg_buf *nmb);
extern int		ndmconn_set_snoop (struct ndmconn *conn,
				struct ndmlog *log, int level);
extern void		ndmconn_clear_snoop (struct ndmconn *conn);
extern void		ndmconn_snoop_nmb (struct ndmconn *conn,
				struct ndmp_msg_buf *nmb,
				char *whence);
extern void		ndmconn_snoop (struct ndmconn *conn,
				int level, char *fmt, ...);
extern void		ndmconn_hex_dump (struct ndmconn *conn,
				char *buf, unsigned len);
extern int		ndmconn_set_err_msg (struct ndmconn *conn,
				char *err_msg);
extern char *		ndmconn_get_err_msg (struct ndmconn *conn);




/*
 * NDMC_WITH() AND FRIENDS
 ****************************************************************
 *
 * Macro NDMC_WITH() and friends. These are patterned after
 * the Pascal "with" construct. These macros take care of
 * the tedious, error prone, mind-numbing, and distracting
 * code required to perform NDMP RPCs. These greatly
 * facilitate the clarity of the main body of code.
 * Code sequences look something like:
 *
 *	NDMC_WITH(ndmp_config_get_butype_attr)
 *		request->xxx = yyy;
 *		...
 *		rc = NDMC_CALL(ndmconn);
 *		if (rc == 0) {
 *			reply->xxx ...
 *			....
 *		}
 *		NDMC_FREE_REPLY()
 *	NDMC_ENDWITH
 *
 * The NDMC macros are for client-side (caller) sequences.
 * The NDMS macros are for server-side (callee) sequences.
 *
 * These macros are very dependent on ndmp_msg_buf.h and ndmp_ammend.h
 *
 * Implementation note: initialization of *request and *reply
 * are separate from their declarations. They used to be
 * initialized declarators. The separation made gcc -Wall happy.
 */

#define NDMC_WITH(TYPE,VERS) \
  { \
	struct ndmp_xa_buf *	xa = &conn->call_xa_buf; \
	TYPE##_request * request; \
	TYPE##_reply   * reply; \
	request = &xa->request.body.TYPE##_request_body; \
	reply = &xa->reply.body.TYPE##_reply_body; \
	NDMOS_MACRO_ZEROFILL (xa); \
	xa->request.protocol_version = VERS; \
	xa->request.header.message = (ndmp0_message) MT_##TYPE; \
     {


#define NDMC_WITH_VOID_REQUEST(TYPE,VERS) \
  { \
	struct ndmp_xa_buf *	xa = &conn->call_xa_buf; \
	TYPE##_reply   * reply; \
	reply = &xa->reply.body.TYPE##_reply_body; \
	NDMOS_MACRO_ZEROFILL (xa); \
	xa->request.protocol_version = VERS; \
	xa->request.header.message = (ndmp0_message) MT_##TYPE; \
     {

#define NDMC_WITH_NO_REPLY(TYPE,VERS) \
  { \
	struct ndmp_xa_buf *	xa = &conn->call_xa_buf; \
	TYPE##_request * request; \
	request = &xa->request.body.TYPE##_request_body; \
	NDMOS_MACRO_ZEROFILL (xa); \
	xa->request.protocol_version = VERS; \
	xa->request.header.message = (ndmp0_message) MT_##TYPE; \
     {

#ifndef NDMOS_OPTION_NO_NDMP4
#define NDMC_WITH_POST(TYPE,VERS) \
  { \
	struct ndmp_xa_buf *	xa = &conn->call_xa_buf; \
	TYPE##_post * request; \
	request = &xa->request.body.TYPE##_post_body; \
	NDMOS_MACRO_ZEROFILL (xa); \
	xa->request.protocol_version = VERS; \
	xa->request.header.message = (ndmp0_message) MT_##TYPE; \
     {
#endif /* !NDMOS_OPTION_NO_NDMP4 */


#define NDMC_ENDWITH \
  } }

#define NDMC_CALL(CONN) (*(CONN)->call)(CONN, xa);
#define NDMC_SEND(CONN) (*(CONN)->call)(CONN, xa);
#define NDMC_FREE_REPLY() ndmconn_free_nmb ((void*)0, &xa->reply)



#define NDMS_WITH(TYPE) \
  { \
	TYPE##_request * request; \
	TYPE##_reply   * reply; \
	request = &xa->request.body.TYPE##_request_body; \
	reply = &xa->reply.body.TYPE##_reply_body; \
     {

#define NDMS_WITH_VOID_REQUEST(TYPE) \
  { \
	TYPE##_reply   * reply; \
	reply = &xa->reply.body.TYPE##_reply_body; \
     {

#define NDMS_WITH_NO_REPLY(TYPE) \
  { \
	TYPE##_request * request; \
	request = &xa->request.body.TYPE##_request_body; \
     {

#ifndef NDMOS_OPTION_NO_NDMP4
#define NDMS_WITH_POST(TYPE) \
  { \
	TYPE##_post * request; \
	request = &xa->request.body.TYPE##_post_body; \
     {
#endif /* !NDMOS_OPTION_NO_NDMP4 */

#define NDMS_ENDWITH \
  } }






/*
 * NDMAGENT -- "Address" of agent
 ****************************************************************
 *
 * A struct ndmagent contains the information necessary
 * to establish a connection with an NDMP agent (server).
 * An agent can be remote (NDMCONN_TYPE_REMOTE) or resident
 * (...._RESIDENT).
 * TODO: MD5
 */
#define NDMAGENT_HOST_MAX	63
#define NDMAGENT_ACCOUNT_MAX	15
#define NDMAGENT_PASSWORD_MAX	32

struct ndmagent {
	char		conn_type;	/* NDMCONN_TYPE_... (see above) */
	char		protocol_version; /* 0->best, 2->v2 3->v3 */
	char		host[NDMAGENT_HOST_MAX+1];	/* name */
	int		port;		/* 0->default (NDMPPORT) */
	char		account[NDMAGENT_ACCOUNT_MAX+1]; /* clear text */
	char		password[NDMAGENT_PASSWORD_MAX+1]; /* clear text */
#if 0
	ndmp_auth_type	auth_type;
#else
	int		auth_type;
#endif
};
extern int	ndmagent_from_str (struct ndmagent *agent, char *str);
extern int	ndmhost_lookup (char *hostname, struct sockaddr_in *sin);
extern int	ndmagent_to_sockaddr_in (struct ndmagent *agent,
			struct sockaddr_in *sin);




/*
 * NDMSCSI -- "Address" of SCSI device
 ****************************************************************
 */

struct ndmscsi_target {
	char		dev_name[NDMOS_CONST_PATH_MAX];
	int		controller;
	int		sid;
	int		lun;
};

#define NDMSCSI_MAX_SENSE_DATA	127
struct ndmscsi_request {
	unsigned char	completion_status;
	unsigned char	status_byte;
	unsigned char	data_dir;
	unsigned char	n_cmd;

	unsigned char	cmd[12];

	unsigned char *	data;
	unsigned	n_data_avail;
	unsigned	n_data_done;
	uint32_t	_pad;

	unsigned char	n_sense_data;
	unsigned char	sense_data[NDMSCSI_MAX_SENSE_DATA];
};

#define NDMSCSI_CS_GOOD	0
#define NDMSCSI_CS_FAIL	1
/* more? */

#define NDMSCSI_DD_NONE	0
#define NDMSCSI_DD_IN	1	/* adapter->app */
#define NDMSCSI_DD_OUT	2	/* app->adapter */


extern int		ndmscsi_target_from_str (struct ndmscsi_target *targ,
				char *str);
extern int		ndmscsi_open (struct ndmconn *conn, char *dev_name);
extern int		ndmscsi_close (struct ndmconn *conn);
extern int		ndmscsi_get_state (struct ndmconn *conn,
				struct ndmscsi_target *targ);
extern int		ndmscsi_set_target (struct ndmconn *conn,
				struct ndmscsi_target *targ);
extern int		ndmscsi_use (struct ndmconn *conn,
				struct ndmscsi_target *targ);
extern int		ndmscsi_execute (struct ndmconn *conn,
				struct ndmscsi_request *req,
				struct ndmscsi_target *targ);




/*
 * NDMMEDIA -- media (tape) labels, position, and status
 ****************************************************************
 */

#define NDMMEDIA_LABEL_MAX	31

struct ndmmedia {
	NDM_FLAG_DECL(valid_label)	/* ->label[] valid */
	NDM_FLAG_DECL(valid_filemark)	/* ->file_mark_skip valid */
	NDM_FLAG_DECL(valid_n_bytes)	/* ->n_bytes valid */
	NDM_FLAG_DECL(valid_slot)	/* ->slot_addr valid */

	/* results flags */
	NDM_FLAG_DECL(media_used)	/* was used (loaded) */
	NDM_FLAG_DECL(media_written)	/* media was written */
	NDM_FLAG_DECL(media_eof)	/* reached EOF of tape file */
	NDM_FLAG_DECL(media_eom)	/* reached EOM (tape full) */
	NDM_FLAG_DECL(media_open_error)	/* open-time error (write-protect?) */
	NDM_FLAG_DECL(media_io_error)	/* media error */

	NDM_FLAG_DECL(label_read)	/* ->label[] read fm media */
	NDM_FLAG_DECL(label_written)	/* ->label[] written to media */
	NDM_FLAG_DECL(label_io_error)	/* error label read/write */
	NDM_FLAG_DECL(label_mismatch)	/* label wasn't as expected */

	NDM_FLAG_DECL(fmark_error)	/* error skipping file marks */

	NDM_FLAG_DECL(nb_determined)	/* true ->n_bytes determined */
	NDM_FLAG_DECL(nb_aligned)	/* ->n_bytes aligned per rec_size */

	NDM_FLAG_DECL(slot_empty)	/* slot empty per robot */
	NDM_FLAG_DECL(slot_bad)		/* ->slot_addr invalid */
	NDM_FLAG_DECL(slot_missing)	/* !->valid_slot */

	/* all fields are specified/actual depending on context */
	char		label[NDMMEDIA_LABEL_MAX+1];
	unsigned	file_mark_offset;
	uint64_t	n_bytes;
	unsigned	slot_addr;

	/* scratch pad */
	uint64_t	begin_offset, end_offset;

	int		index;
	struct ndmmedia *next;
};

extern int	ndmmedia_from_str (struct ndmmedia *me, char *str);
extern int	ndmmedia_to_str (struct ndmmedia *me, char *str);
extern int	ndmmedia_pp (struct ndmmedia *me, int lineno, char *buf);
extern int64_t	ndmmedia_strtoll (char *str, char **tailp, int defbase);




/*
 * NDMFHH -- file history (FH) heap
 ****************************************************************
 *
 * As DATA accumulates individual File History (FH) entries they
 * are saved into a heap buffer. When the heap is full it is flushed
 * from DATA to CONTROL using NDMP?_FH_ADD_.... requests/posts.
 */

struct ndmfhheap {
	int		fhtype;
	int		entry_size;

	void *		table;
	void *		allo_entry;

	void *		allo_item;

	void *		heap_base;
	void *		heap_end;
	unsigned	heap_size;

	void *		heap_top;
	void *		heap_bot;
};

struct ndmfhh_generic_table {
	u_int	table_len;
	void *	table_val;
};

#define NDMFHH_RET_OK		(0)
#define NDMFHH_RET_OVERFLOW	(-1)
#define NDMFHH_RET_TYPE_CHANGE	(-2)
#define NDMFHH_RET_NO_HEAP	(-3)
#define NDMFHH_RET_ENTRY_SIZE_MISMATCH (-4)


extern int	ndmfhh_initialize (struct ndmfhheap *fhh);
extern int	ndmfhh_commission (struct ndmfhheap *fhh,
					void *heap, unsigned size);
extern int	ndmfhh_prepare (struct ndmfhheap *fhh,
					int fhtype, int entry_size,
					unsigned n_item,
					unsigned total_size_of_items);
extern void *	ndmfhh_add_entry (struct ndmfhheap *fhh);
extern void *	ndmfhh_add_item (struct ndmfhheap *fhh, unsigned size);
extern void *	ndmfhh_save_item (struct ndmfhheap *fhh,
					void *item, unsigned size);
extern int	ndmfhh_reset (struct ndmfhheap *fhh);
extern int	ndmfhh_get_table (struct ndmfhheap *fhh,
					int *fhtype_p, void **table_p,
					unsigned *n_entry_p);




/*
 * NDMCSTR -- canonical strings
 ****************************************************************
 * Convert strings to/from HTTP-like canonical strings (%xx).
 * Example "a b%c" --> "a%20b%25c"
 */

#define NDMCSTR_WARN	'%'
extern int	ndmcstr_from_str (char *src, char *dst, unsigned dst_max);
extern int	ndmcstr_to_str (char *src, char *dst, unsigned dst_max);
extern int	ndmcstr_from_hex (int c);




/*
 * NDMMD5 -- MD5 helpers
 ****************************************************************
 * This is a wrapper around the MD5 functions. ndml_md5.c
 * is the only thing that needs to #include md5.h.
 * The NDMP rules for converting a clear-text password
 * into an MD5 digest are implemented here.
 */

#define NDMP_MD5_CHALLENGE_LENGTH	64
#define NDMP_MD5_DIGEST_LENGTH		16
#define NDMP_MD5_MESSAGE_LENGTH		128
#define NDMP_MD5_MAX_PASSWORD_LENGTH	32


extern int	ndmmd5_digest (char challenge[NDMP_MD5_CHALLENGE_LENGTH],
				char *clear_text_password,
				char digest[NDMP_MD5_DIGEST_LENGTH]);
extern int	ndmmd5_generate_challenge (
				char challenge[NDMP_MD5_CHALLENGE_LENGTH]);

extern int	ndmmd5_ok_digest (char challenge[NDMP_MD5_CHALLENGE_LENGTH],
				char *clear_text_password,
				char digest[NDMP_MD5_DIGEST_LENGTH]);




/*
 * NDMBSTF -- Binary Search Text File
 ****************************************************************
 * Use conventional binary search method on a sorted text
 * file. The file MUST be sorted in ascending, lexicographic
 * order. This is the default order of sort(1).
 */

extern int ndmbstf_first (FILE *fp, char *key, char *buf, unsigned max_buf);
extern int ndmbstf_next (FILE *fp, char *key, char *buf, unsigned max_buf);
extern int ndmbstf_first_with_bounds (FILE *fp, char *key,
	    char *buf, unsigned max_buf, off_t lower_bound, off_t upper_bound);
extern int ndmbstf_getline (FILE *fp, char *buf, unsigned max_buf);
extern int ndmbstf_seek_and_align (FILE *fp, off_t *off);
extern int ndmbstf_match (char *key, char *buf);
extern int ndmbstf_compare (char *key, char *buf);




/*
 * NDMSTZF -- Stanza File
 ****************************************************************
 *
 * Stanza files look about like this:
 *	[stanza name line]
 *	stanza body lines
 *
 * These are used for config files.
 */
extern int ndmstz_getline (FILE *fp, char *buf, int n_buf);
extern int ndmstz_getstanza (FILE *fp, char *buf, int n_buf);
extern int ndmstz_parse (char *buf, char *argv[], int max_argv);




/*
 * NDMCFG -- Config File
 ****************************************************************
 *
 * Config files are stanza files (see above) which describe
 * backup types, tape and scsi devices, etc.
 * See ndml_config.c for details and stanza formats.
 */
extern int	ndmcfg_load (char *filename, ndmp9_config_info *config_info);
extern int	ndmcfg_loadfp (FILE *fp, ndmp9_config_info *config_info);




/*
 * NDMFHDB -- File History Database
 *
 * The File History is generated by the DATA and sent to the CONTROL
 * using NDMP?_FH_ADD_... requests/posts. During backup the CONTROL
 * writes the File History info to a text file as it arrives. Upon
 * completion of the backup the text file should be sorted (UNIX
 * sort(1) command). For recovery the file history index is searched
 * using binary search (see NDMBSTF above). The fh_info, a 64-bit
 * cookie used by DATA to identify the region of the backup image
 * containing the corresponding object, is retreived from the index.
 */

struct ndmfhdb {
	FILE *			fp;
	int			use_dir_node;
	uint64_t		root_node;
};

struct ndm_fhdb_callbacks {
	int (*add_file) (struct ndmlog *ixlog, int tagc,
		char *raw_name, ndmp9_file_stat *fstat);
	int (*add_dir) (struct ndmlog *ixlog, int tagc,
		char *raw_name, ndmp9_u_quad dir_node, ndmp9_u_quad node);
	int (*add_node) (struct ndmlog *ixlog, int tagc,
		ndmp9_u_quad node, ndmp9_file_stat *fstat);
	int (*add_dirnode_root) (struct ndmlog *ixlog, int tagc,
		ndmp9_u_quad root_node);
};

extern void	ndmfhdb_register_callbacks (struct ndmlog *ixlog,
			struct ndm_fhdb_callbacks *callbacks);
extern void	ndmfhdb_unregister_callbacks (struct ndmlog *ixlog);
extern int	ndmfhdb_add_file (struct ndmlog *ixlog, int tagc,
			char *raw_name, ndmp9_file_stat *fstat);
extern int	ndmfhdb_add_dir (struct ndmlog *ixlog, int tagc,
			char *raw_name, ndmp9_u_quad dir_node,
			ndmp9_u_quad node);
extern int	ndmfhdb_add_node (struct ndmlog *ixlog, int tagc,
			ndmp9_u_quad node, ndmp9_file_stat *fstat);
extern int	ndmfhdb_add_dirnode_root (struct ndmlog *ixlog, int tagc,
			ndmp9_u_quad root_node);

extern int	ndmfhdb_add_fh_info_to_nlist (FILE *fp,
			ndmp9_name *nlist, int n_nlist);
extern int	ndmfhdb_open (FILE *fp, struct ndmfhdb *fhcb);
extern int	ndmfhdb_lookup (struct ndmfhdb *fhcb, char *path,
			ndmp9_file_stat *fstat);
extern int	ndmfhdb_dirnode_root (struct ndmfhdb *fhcb);
extern int	ndmfhdb_dirnode_lookup (struct ndmfhdb *fhcb, char *path,
			ndmp9_file_stat *fstat);
extern int	ndmfhdb_dir_lookup (struct ndmfhdb *fhcb,
			uint64_t dir_node, char *name, uint64_t *node_p);
extern int	ndmfhdb_node_lookup (struct ndmfhdb *fhcb,
			uint64_t node, ndmp9_file_stat *fstat);
extern int	ndmfhdb_file_root (struct ndmfhdb *fhcb);
extern int	ndmfhdb_file_lookup (struct ndmfhdb *fhcb, char *path,
			ndmp9_file_stat *fstat);
extern char *	ndm_fstat_to_str (ndmp9_file_stat *fstat, char *buf);
extern int	ndm_fstat_from_str (ndmp9_file_stat *fstat, char *buf);

#ifdef  __cplusplus
}
#endif

#endif /* _NDMLIB_H_ */
