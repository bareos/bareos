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


#include "ndmlib.h"


#ifndef NDMOS_OPTION_NO_NDMP4
#define MAX_PROTOCOL_VERSION	NDMP4VER
#else /* !NDMOS_OPTION_NO_NDMP4 */
#ifndef NDMOS_OPTION_NO_NDMP3
#define MAX_PROTOCOL_VERSION	NDMP3VER
#else /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP2
#define MAX_PROTOCOL_VERSION	NDMP2VER
#else /* !NDMOS_OPTION_NO_NDMP2 */
#define MAX_PROTOCOL_VERSION	0
#endif /* !NDMOS_OPTION_NO_NDM2 */
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#endif /* !NDMOS_OPTION_NO_NDMP4 */



/*
 * INITIALIZE AND DESTRUCT
 ****************************************************************
 *
 * Initialize an ndmconn. This pretty much amounts to
 * initializing the underlying ndmchan and stuffing
 * the function pointers.
 */

struct ndmconn *
ndmconn_initialize (struct ndmconn *aconn, char *name)
{
	struct ndmconn *	conn = aconn;

	if (!conn) {
		conn = NDMOS_MACRO_NEW(struct ndmconn);
		if (!conn)
			return 0;
	}

	NDMOS_MACRO_ZEROFILL (conn);

	if (!name) name = "#?";				/* default */

	ndmchan_initialize (&conn->chan, name);
	conn->was_allocated = aconn == 0;
	conn->next_sequence = 1;
	xdrrec_create (&conn->xdrs, 0, 0, (void*) conn,
				(void*)ndmconn_readit,
				(void*)ndmconn_writeit);
	conn->unexpected = ndmconn_unexpected;

	conn->call = ndmconn_call;

	conn->time_limit = 0;

	return conn;
}

/*
 * Get rid of an ndmconn.
 */
void
ndmconn_destruct (struct ndmconn *conn)
{
	if (conn->chan.fd >= 0) {
		close (conn->chan.fd);
		conn->chan.fd = -1;
	}

	if (conn->xdrs.x_ops) {
		xdr_destroy (&conn->xdrs);
		conn->xdrs.x_ops = NULL;
	}

	if (conn->was_allocated) {
		conn->was_allocated = 0;
		NDMOS_API_FREE (conn);
		conn = 0;
	}
}

/*
 * ESTABLISH CONNECTION
 ****************************************************************
 *
 * The following four routines establish the TCP/IP connection
 * between agents.
 *
 * ndmconn_connect_agent()
 *     make a connection per an ndmagent, uses ..._host_port()
 * ndmconn_connect_host_port ()
 *     make a connection per a hostname and port, uses ..._sockaddr_in()
 * ndmconn_connect_sockaddr_in()
 *     make a connection per sockaddr_in, performs NDMP_CONNECT_
 *     sequences, but no authentication
 * ndmconn_accept()
 *     make a connection (receive it really) from a file descriptor
 *     already accept()ed.
 */

int
ndmconn_connect_agent (struct ndmconn *conn, struct ndmagent *agent)
{
	if (agent->conn_type == NDMCONN_TYPE_RESIDENT) {
		conn->conn_type = NDMCONN_TYPE_RESIDENT;
		conn->protocol_version = agent->protocol_version;
		if (conn->protocol_version == 0) {
			/* Let's negotiate......MAX */
			conn->protocol_version = MAX_PROTOCOL_VERSION;
		}
		ndmchan_start_resident (&conn->chan);
		return 0;
	}

	if (agent->port == 0) agent->port = NDMPPORT;

	return ndmconn_connect_host_port (conn,
			agent->host, agent->port, agent->protocol_version);
}


int
ndmconn_connect_host_port (struct ndmconn *conn,
  char * hostname, int port, unsigned want_protocol_version)
{
	struct sockaddr_in	sin;
	char *			err = "???";

	if (conn->chan.fd >= 0) {
		err = "already-connected";
		return ndmconn_set_err_msg (conn, err);
	}

	if (ndmhost_lookup (hostname, &sin) != 0) {
		err = "bad-host-name";
		return ndmconn_set_err_msg (conn, err);
	}

	if (port == 0) port = NDMPPORT;

	sin.sin_port = htons(port);

	return ndmconn_connect_sockaddr_in (conn, &sin, want_protocol_version);
}

int
ndmconn_connect_sockaddr_in (struct ndmconn *conn,
  struct sockaddr_in *sin, unsigned want_protocol_version)
{
	int			fd = -1;
	int			rc;
	char *			err = "???";
	unsigned		max_protocol_version = MAX_PROTOCOL_VERSION;

	if (conn->chan.fd >= 0) {
		err = "already-connected";
		return ndmconn_set_err_msg (conn, err);
	}

	fd = socket (AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		err = NDMOS_API_MALLOC (1024);
		if (err)
			snprintf(err, 1023, "open a socket failed: %s", strerror(errno));
		goto error_out;
	}

	/* reserved port? */
	if (connect (fd, (struct sockaddr *)sin, sizeof *sin) < 0) {
		err = NDMOS_API_MALLOC (1024);
		if (err)
			snprintf(err, 1023, "connect failed: %s", strerror(errno));
		goto error_out;
	}

	ndmchan_start_readchk (&conn->chan, fd);
	conn->conn_type = NDMCONN_TYPE_REMOTE;

	/*
	 * Await the NDMP_NOTIFY_CONNECTED request (no reply)
	 * Don't get confused that this client-side is awaiting
	 * a "request" from the server.
	 */
	NDMC_WITH_NO_REPLY(ndmp0_notify_connected,0)
		rc = ndmconn_recv_nmb(conn, &xa->request);
		if (rc != 0) {
			err = "recv-notify-connected";
			goto error_out;
		}
		if (xa->request.header.message_type != NDMP0_MESSAGE_REQUEST
		 || xa->request.header.message != NDMP0_NOTIFY_CONNECTED) {
			err = "msg-not-notify-connected";
			goto error_out;
		}

		if (request->reason != NDMP0_CONNECTED) {
			err = "notify-connected-not-connected";
			goto error_out;
		}

		if (max_protocol_version > request->protocol_version) {
			max_protocol_version = request->protocol_version;
		}
	NDMC_ENDWITH

	if (want_protocol_version == 0) {
		want_protocol_version = max_protocol_version;
	} else if (want_protocol_version > max_protocol_version) {
		err = "connect-want/max-version-mismatch";
		goto error_out;
	}

	/*
	 * Send the OPEN request
	 */
	NDMC_WITH(ndmp0_connect_open,0)
		request->protocol_version = want_protocol_version;
		rc = NDMC_CALL(conn);
		if (rc) {
			err = "connect-open-failed";
			goto error_out;
		}
	NDMC_ENDWITH

	/* GOOD! */

	conn->protocol_version = want_protocol_version;

	return 0;

  error_out:
	if (fd >= 0) {
		close (fd);
		fd = -1;
	}
	conn->chan.fd = -1;
	conn->chan.mode = NDMCHAN_MODE_IDLE;
	conn->conn_type = NDMCONN_TYPE_NONE;

	return ndmconn_set_err_msg (conn, err);
}

int
ndmconn_try_open (struct ndmconn *conn, unsigned protocol_version)
{
	int		rc;

	/*
	 * Send the OPEN request
	 */
	NDMC_WITH(ndmp0_connect_open,0)
		request->protocol_version = protocol_version;
		rc = NDMC_CALL(conn);
		if (rc) {
			ndmconn_set_err_msg (conn, "connect-open-failed");
		}
	NDMC_ENDWITH

	return rc;
}

int
ndmconn_accept (struct ndmconn *conn, int sock)
{
	char *			err = "???";

	if (conn->chan.fd >= 0) {
		err = "already-connected";
		return ndmconn_set_err_msg (conn, err);
	}

	ndmchan_start_readchk (&conn->chan, sock);
	conn->conn_type = NDMCONN_TYPE_REMOTE;

	/*
	 * Send the NDMP_NOTIFY_CONNECTED message, no reply
	 * The connect()er is waiting for it.
	 */
	NDMC_WITH_NO_REPLY(ndmp0_notify_connected,0)
		request->reason = NDMP0_CONNECTED;
		request->protocol_version = MAX_PROTOCOL_VERSION;
		request->text_reason = "Hello";
		NDMC_SEND(conn);
	NDMC_ENDWITH

	/* assume connection is running in offered protocol_version */
	conn->protocol_version = MAX_PROTOCOL_VERSION;

	return 0;
}




/*
 * TERMINATE CONNECTION
 ****************************************************************
 *
 * These two routines are about terminating a connection.
 * They are incomplete.
 */

/* hangup */
int
ndmconn_abort (struct ndmconn *conn)
{
	return 0;
}

/* orderly close */
int
ndmconn_close (struct ndmconn *conn)
{
	return 0;
}


/*
 * Return the underlying fd of the ndmconn.
 * This is no longer used since the ndmchan stuff was done.
 */

int
ndmconn_fileno (struct ndmconn *conn)
{
	return conn->chan.fd;
}





/*
 * AUTHENTICATION
 *
 * The following three routines do the NDMP_CONNECT_AUTH sequences.
 */

int
ndmconn_auth_agent (struct ndmconn *conn, struct ndmagent *agent)
{
	int		rc;

	if (conn->conn_type == NDMCONN_TYPE_RESIDENT)
		return 0;

	switch (agent->auth_type) {
	case 'n':		/* NDMP_AUTH_NONE */
		rc = ndmconn_auth_none (conn);
		break;

	case 't':		/* NDMP_AUTH_TEXT */
		rc = ndmconn_auth_text (conn, agent->account, agent->password);
		break;

	case 'm':		/* NDMP_AUTH_MD5 */
		rc = ndmconn_auth_md5 (conn, agent->account, agent->password);
		break;

	case 'v':		/* void (don't auth) */
		rc = 0;
		break;

	default:
		ndmconn_set_err_msg (conn, "connect-auth-unknown");
		rc = -1;
		break;
	}

	return rc;
}

int
ndmconn_auth_none (struct ndmconn *conn)
{
	int		rc;

	switch (conn->protocol_version) {
	default:
		ndmconn_set_err_msg (conn, "connect-auth-none-vers-botch");
		return -1;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH(ndmp2_connect_client_auth, NDMP2VER)
		request->auth_data.auth_type = NDMP2_AUTH_NONE;
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH(ndmp3_connect_client_auth, NDMP3VER)
		request->auth_data.auth_type = NDMP3_AUTH_NONE;
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH(ndmp4_connect_client_auth, NDMP4VER)
		request->auth_data.auth_type = NDMP4_AUTH_NONE;
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	if (rc) {
		ndmconn_set_err_msg (conn, "connect-auth-none-failed");
		return -1;
	}

	return 0;
}

int
ndmconn_auth_text (struct ndmconn *conn, char *id, char *pw)
{
	int		rc;

	switch (conn->protocol_version) {
	default:
		ndmconn_set_err_msg (conn, "connect-auth-text-vers-botch");
		return -1;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH(ndmp2_connect_client_auth, NDMP2VER)
		struct ndmp2_auth_text *at;

		request->auth_data.auth_type = NDMP2_AUTH_TEXT;
		at = &request->auth_data.ndmp2_auth_data_u.auth_text;
		at->auth_id = id;
		at->auth_password = pw;
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH(ndmp3_connect_client_auth, NDMP3VER)
		struct ndmp3_auth_text *at;

		request->auth_data.auth_type = NDMP3_AUTH_TEXT;
		at = &request->auth_data.ndmp3_auth_data_u.auth_text;
		at->auth_id = id;
		at->auth_password = pw;
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH(ndmp4_connect_client_auth, NDMP4VER)
		struct ndmp4_auth_text *at;

		request->auth_data.auth_type = NDMP4_AUTH_TEXT;
		at = &request->auth_data.ndmp4_auth_data_u.auth_text;
		at->auth_id = id;
		at->auth_password = pw;
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	if (rc) {
		ndmconn_set_err_msg (conn, "connect-auth-text-failed");
		return -1;
	}

	return 0;
}

int
ndmconn_auth_md5 (struct ndmconn *conn, char *id, char *pw)
{
	int		rc;
	char		challenge[NDMP_MD5_CHALLENGE_LENGTH];
	char		digest[NDMP_MD5_DIGEST_LENGTH];

	switch (conn->protocol_version) {
	default:
		ndmconn_set_err_msg (conn, "connect-auth-md5-vers-botch");
		return -1;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH(ndmp2_config_get_auth_attr, NDMP2VER)
		request->auth_type = NDMP2_AUTH_MD5;
		rc = NDMC_CALL(conn);
		if (rc == 0) {
			if (reply->server_attr.auth_type != NDMP2_AUTH_MD5) {
				ndmconn_set_err_msg (conn,
					"connect-auth-md5-attr-type-botch");
				return -1;
			}
			NDMOS_API_BCOPY (
				reply->server_attr.ndmp2_auth_attr_u.challenge,
				challenge, sizeof challenge);
		}
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH(ndmp3_config_get_auth_attr, NDMP3VER)
		request->auth_type = NDMP3_AUTH_MD5;
		rc = NDMC_CALL(conn);
		if (rc == 0) {
			if (reply->server_attr.auth_type != NDMP3_AUTH_MD5) {
				ndmconn_set_err_msg (conn,
					"connect-auth-md5-attr-type-botch");
				return -1;
			}
			NDMOS_API_BCOPY (
				reply->server_attr.ndmp3_auth_attr_u.challenge,
				challenge, sizeof challenge);
		}
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH(ndmp4_config_get_auth_attr, NDMP4VER)
		request->auth_type = NDMP4_AUTH_MD5;
		rc = NDMC_CALL(conn);
		if (rc == 0) {
			if (reply->server_attr.auth_type != NDMP4_AUTH_MD5) {
				ndmconn_set_err_msg (conn,
					"connect-auth-md5-attr-type-botch");
				return -1;
			}
			NDMOS_API_BCOPY (
				reply->server_attr.ndmp4_auth_attr_u.challenge,
				challenge, sizeof challenge);
		}
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	if (rc) {
		ndmconn_set_err_msg (conn, "connect-auth-md5-attr-failed");
		return -1;
	}

	ndmmd5_digest (challenge, pw, digest);

	switch (conn->protocol_version) {
	default:
		ndmconn_set_err_msg (conn, "connect-auth-text-vers-botch");
		return -1;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH(ndmp2_connect_client_auth, NDMP2VER)
		struct ndmp2_auth_md5 *am;

		request->auth_data.auth_type = NDMP2_AUTH_MD5;
		am = &request->auth_data.ndmp2_auth_data_u.auth_md5;
		am->auth_id = id;
		NDMOS_API_BCOPY (digest, am->auth_digest, sizeof digest);
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH(ndmp3_connect_client_auth, NDMP3VER)
		struct ndmp3_auth_md5 *am;

		request->auth_data.auth_type = NDMP3_AUTH_MD5;
		am = &request->auth_data.ndmp3_auth_data_u.auth_md5;
		am->auth_id = id;
		NDMOS_API_BCOPY (digest, am->auth_digest, sizeof digest);
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH(ndmp4_connect_client_auth, NDMP4VER)
		struct ndmp4_auth_md5 *am;

		request->auth_data.auth_type = NDMP4_AUTH_MD5;
		am = &request->auth_data.ndmp4_auth_data_u.auth_md5;
		am->auth_id = id;
		NDMOS_API_BCOPY (digest, am->auth_digest, sizeof digest);
		rc = NDMC_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	if (rc) {
		ndmconn_set_err_msg (conn, "connect-auth-md5-failed");
		return -1;
	}

	return 0;
}




/*
 * CALL (REQUEST/REPLY), SEND, and RECEIVE
 ****************************************************************
 */

int
ndmconn_call (struct ndmconn *conn, struct ndmp_xa_buf *xa)
{
	unsigned		protocol_version = conn->protocol_version;
	unsigned		msg = xa->request.header.message;
	int			rc;
	struct ndmp_xdr_message_table *	xmte;

	conn->last_message = msg;
	conn->last_call_status = NDMCONN_CALL_STATUS_BOTCH;
	conn->last_header_error = -1;	/* invalid */
	conn->last_reply_error = -1;	/* invalid */

	if (protocol_version != xa->request.protocol_version) {
		ndmconn_set_err_msg (conn, "protocol-version-mismatch");
		return NDMCONN_CALL_STATUS_BOTCH;
	}

	xmte = ndmp_xmt_lookup (protocol_version, msg);
	if (!xmte) {
		ndmconn_set_err_msg (conn, "no-xdr-found");
		return NDMCONN_CALL_STATUS_BOTCH;
	}

	xa->request.header.message_type = NDMP0_MESSAGE_REQUEST;

	if (!xmte->xdr_reply) {
		/* no reply expected, just a send (eg NOTIFY) */
		return ndmconn_send_nmb (conn, &xa->request);
	}

	rc = ndmconn_exchange_nmb (conn, &xa->request, &xa->reply);
	if (rc) {
		ndmconn_set_err_msg (conn, "exchange-failed");
		return NDMCONN_CALL_STATUS_BOTCH;
	}

	if (xa->reply.header.message != msg) {
		ndmconn_set_err_msg (conn, "msg-mismatch");
		return NDMCONN_CALL_STATUS_BOTCH;
	}

	/* TODO: this should be converted ndmp_xto9_error(....) */
	conn->last_header_error = xa->reply.header.error;

	if (xa->reply.header.error) {
		conn->last_call_status = NDMCONN_CALL_STATUS_HDR_ERROR;
		ndmconn_set_err_msg (conn, "reply-error-hdr");
		return NDMCONN_CALL_STATUS_HDR_ERROR;
	}

	conn->last_reply_error = ndmnmb_get_reply_error (&xa->reply);

	switch (conn->last_reply_error) {
        case NDMP9_NO_ERR:
        case NDMP9_DEV_NOT_OPEN_ERR:
		/*
		 * We allow NDMP9_DEV_NOT_OPEN_ERR as answer to NDMP4_TAPE_GET_STATE
		 * This always happens during ndmca_monitor_shutdown()
		 */
		break;
        default:
		conn->last_call_status = NDMCONN_CALL_STATUS_REPLY_ERROR;
		ndmconn_set_err_msg (conn, "reply-error");
		return NDMCONN_CALL_STATUS_REPLY_ERROR;
	}

	return NDMCONN_CALL_STATUS_OK;
}

int
ndmconn_exchange_nmb (struct ndmconn *conn,
  struct ndmp_msg_buf *request_nmb,
  struct ndmp_msg_buf *reply_nmb)
{
	int			rc;

	if ((rc = ndmconn_send_nmb (conn, request_nmb)) != 0)
		return rc;
	conn->received_time = 0;
	conn->sent_time = time(0);

	for (;;) {
		if ((rc = ndmconn_recv_nmb (conn, reply_nmb)) != 0)
			return rc;

		if (reply_nmb->header.message_type == NDMP0_MESSAGE_REPLY
		 && reply_nmb->header.reply_sequence
		    == request_nmb->header.sequence) {
			conn->received_time = time(0);
			return 0;
		}

		(*conn->unexpected)(conn, reply_nmb);
	}
}

int
ndmconn_send_nmb (struct ndmconn *conn, struct ndmp_msg_buf *nmb)
{
	return ndmconn_xdr_nmb (conn, nmb, XDR_ENCODE);
}

int
ndmconn_recv_nmb (struct ndmconn *conn, struct ndmp_msg_buf *nmb)
{
	NDMOS_MACRO_ZEROFILL (nmb);
	nmb->protocol_version = conn->protocol_version;

	return ndmconn_xdr_nmb (conn, nmb, XDR_DECODE);
}

void
ndmconn_free_nmb (struct ndmconn *conn, struct ndmp_msg_buf *nmb)
{
	ndmnmb_free (nmb);
}


int
ndmconn_xdr_nmb (struct ndmconn *conn,
  struct ndmp_msg_buf *nmb,
  enum xdr_op x_op)
{
	xdrproc_t xdr_body = 0;

	assert (conn->conn_type == NDMCONN_TYPE_REMOTE);

	if (conn->chan.fd < 0) {
		return ndmconn_set_err_msg (conn, "not-open");
	}

	conn->xdrs.x_op = x_op;

	if (x_op == XDR_ENCODE) {
		xdr_body = ndmnmb_find_xdrproc (nmb);

		if (nmb->header.error == NDMP0_NO_ERR && !xdr_body) {
			return ndmconn_set_err_msg (conn, "unknown-body");
		}
		nmb->header.sequence = conn->next_sequence++;
		nmb->header.time_stamp = time(0);
		ndmconn_snoop_nmb (conn, nmb, "Send");
	}
	if (x_op == XDR_DECODE) {
		if (!xdrrec_skiprecord (&conn->xdrs)) {
			return ndmconn_set_err_msg (conn, "xdr-get-next");
		}
	}

	if (!xdr_ndmp0_header (&conn->xdrs, &nmb->header)) {
		ndmconn_abort (conn);
		if (x_op == XDR_DECODE
		 && conn->chan.eof && !conn->chan.error) {
			return ndmconn_set_err_msg (conn, "EOF");
		} else {
			return ndmconn_set_err_msg (conn, "xdr-hdr");
		}
	}

	if (x_op == XDR_DECODE) {
		xdr_body = ndmnmb_find_xdrproc (nmb);

		if (nmb->header.error == NDMP0_NO_ERR && !xdr_body) {
			return ndmconn_set_err_msg (conn, "unknown-body");
		}
	}
	if (nmb->header.error == NDMP0_NO_ERR) {
		if (!(*xdr_body) (&conn->xdrs, &nmb->body, 0)) {
			ndmconn_abort (conn);
			return ndmconn_set_err_msg (conn, "xdr-body");
		}
	}

	if (x_op == XDR_ENCODE) {
		if (!xdrrec_endofrecord(&conn->xdrs, 1)) {
			ndmconn_abort (conn);
			return ndmconn_set_err_msg (conn, "xdr-send");
		}
	}
	if (x_op == XDR_DECODE) {
		ndmconn_snoop_nmb (conn, nmb, "Recv");
	}

	return 0;
}




/*
 * XDR READ/WRITE CALLBACKS
 ****************************************************************
 *
 * ndmconn_readit() and ndmconn_writeit() are the XDR callbacks
 * used by xdrrec_create(). They are fundamentally wrappers
 * around read() and write(), and have very similar parameters.
 * See the xdr(3) manual page (or try "man xdrrec_create").
 *
 * ndmconn_readit() tracks the XDR record marks, and never
 * reads across a record boundary. This keeps select() an
 * indicator of when there is a (single) request pending.
 * Otherwise, we have to check buffers internal to XDR
 * as well as the file descriptor (via select) to determine
 * if a request is pending.
 */

int
ndmconn_readit (void *a_conn, char *buf, int len)
{
	struct ndmconn *conn = (struct ndmconn *)a_conn;
	int		rc, i, c;

	/* could impose timeout here */
	if (conn->chan.fd < 0 || conn->chan.eof)
		return -1;

	ndmconn_snoop (conn, 8,
		"frag_resid=%d fhb_off=%d", conn->frag_resid, conn->fhb_off);

	if (conn->frag_resid == 0) {
		i = 0;
		while (i < 4) {
			c = 4 - i;

			rc = ndmconn_sys_read (conn, (void *)(conn->frag_hdr_buf+i), c);
			if (rc <= 0) {
				return rc;
			}
			i += rc;
		}
		conn->frag_resid = conn->frag_hdr_buf[0] << 24;
		conn->frag_resid |= conn->frag_hdr_buf[1] << 16;
		conn->frag_resid |= conn->frag_hdr_buf[2] << 8;
		conn->frag_resid |= conn->frag_hdr_buf[3];
		conn->frag_resid &= 0xFFFFFF;
		conn->fhb_off = 0;
	}
	if (conn->fhb_off < 4) {
		i = 0;
		while (conn->fhb_off < 4 && len > 0) {
			buf[i++] = conn->frag_hdr_buf[conn->fhb_off++];
			len--;
		}
		return i;
	}

	if ((unsigned int)len > conn->frag_resid)
		len = (unsigned int)conn->frag_resid;

	rc = ndmconn_sys_read (conn, buf, len);

	if (rc > 0) {
		conn->frag_resid -= rc;
	}

	return rc;
}

int
ndmconn_writeit (void *a_conn, char *buf, int len)
{
	struct ndmconn *conn = (struct ndmconn *)a_conn;

	/* could impose timeout here */
	if (conn->chan.fd < 0)
		return -1;

	return ndmconn_sys_write (conn, buf, len);
}

/*
 * ndmconn_sys_read() and ndmconn_sys_write() are simply
 * wrappers around read() and write(). They implement
 * the low-level snooping.
 */

int
ndmconn_sys_read (struct ndmconn *conn, char *buf, unsigned len)
{
	int		rc;

	ndmconn_snoop (conn, 9, "reading %d ...", len);

	rc = read (conn->chan.fd, buf, len);

	ndmconn_snoop (conn, 8, "read=%d len=%d", rc, len);

	if (rc <= 0) {
		conn->chan.eof = 1;
		if (rc < 0)
			conn->chan.error = 1;
	} else {
		ndmconn_hex_dump (conn, buf, rc);
	}

	return rc;
}

int
ndmconn_sys_write (struct ndmconn *conn, char *buf, unsigned len)
{
	int		rc;

	ndmconn_snoop (conn, 9, "writing %d ...", len);
	ndmconn_hex_dump (conn, buf, len);

	rc = write (conn->chan.fd, buf, len);

	ndmconn_snoop (conn, 8, "write=%d len=%d", rc, len);

	if (rc !=(int)len) {
		conn->chan.eof = 1;
		conn->chan.error = 1;
	}

	return rc;
}




/*
 * UNEXPECTED
 ****************************************************************
 *
 * The default unexpected() handler for a connection. It is
 * called when ndmconn_exchange_nmb() receives something
 * other than the reply for which it is waiting.
 * This default routine silently dumps the message.
 */

void
ndmconn_unexpected (struct ndmconn *conn, struct ndmp_msg_buf *nmb)
{
	xdrproc_t xdr_body = ndmnmb_find_xdrproc (nmb);

	if (xdr_body) {
		xdr_free (xdr_body, (void*) &nmb->body);
	}
}




/*
 * SNOOP
 ****************************************************************
 *
 * The ndmconn snoop stuff. The cool part. This pretty prints
 * NDMP messages as they go flying by this end-point.
 */

int
ndmconn_set_snoop (struct ndmconn *conn, struct ndmlog *log, int level)
{
	conn->snoop_log = log;
	conn->snoop_level = level;
	return 0;
}

void
ndmconn_clear_snoop (struct ndmconn *conn)
{
	conn->snoop_log = 0;
	conn->snoop_level = 0;
}

void
ndmconn_snoop_nmb (struct ndmconn *conn,
  struct ndmp_msg_buf *nmb,
  char *whence)
{
	if (!conn->snoop_log) {
		return;
	}

	ndmnmb_snoop (conn->snoop_log, conn->chan.name, conn->snoop_level,
				nmb, whence);
}

void
ndmconn_snoop (struct ndmconn *conn, int level, char *fmt, ...)
{
	va_list		ap;

	if (conn->snoop_log && conn->snoop_level >= level) {
		va_start (ap, fmt);
		ndmlogfv (conn->snoop_log, conn->chan.name, level, fmt, ap);
		va_end (ap);
	}
}

/* used by ndmconn_sys_read() and ndmconn_sys_write() to show low-level */
void
ndmconn_hex_dump (struct ndmconn *conn, char *buf, unsigned len)
{
	struct ndmlog *	log = conn->snoop_log;
	char *		tag = conn->chan.name;
	char		linebuf[16*3+3];
	char *		p = linebuf;
	int		b;
	unsigned	i;

	if (log && conn->snoop_level > 8) {
		for (i = 0; i < len; i++) {
			b = buf[i] & 0xFF;
			sprintf (p, " %02x", b);
			while (*p) p++;
			if ((i&0xF) == 0xF) {
				ndmlogf (log,tag,9,"%s",linebuf+1);
				p = linebuf;
			}
		}
		if (p > linebuf) {
			ndmlogf (log,tag,9,"%s",linebuf+1);
		}
	}
}




/*
 * ERRORS
 ****************************************************************
 *
 * Possible errors for ndmconn are not enumerated.
 * Instead, errors are indicated by a -1 return, and
 * a simple string error message is available for details.
 * Appologies for the english-centric design, but it
 * is quick and easy, and better than using printf().
 */

int
ndmconn_set_err_msg (struct ndmconn *conn, char *err_msg)
{
	conn->last_err_msg = err_msg;
	ndmconn_snoop (conn, 4, "ERR=%s", err_msg);
	return -1;
}

char *
ndmconn_get_err_msg (struct ndmconn *conn)
{
	if (!conn->last_err_msg)
		return "-no-error-";
	else
		return conn->last_err_msg;
}
