/*
 * Copyright (c) 1998,2001
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
 *	This contains code fragments common between the
 *	O/S (Operating System) portions of NDMJOBLIB.
 *
 *	This file is #include'd by the O/S specific ndmos_*.c
 *	file, and fragments are selected by #ifdef's.
 *
 *	There are four major portions:
 *	1) Misc support routines: password check, local info, etc
 *	2) Non-blocking I/O support routines
 *	3) Tape interfacs ndmos_tape_xxx()
 *	4) OS Specific NDMP request dispatcher which intercepts
 *	   requests implemented here, such as SCSI operations
 *	   and system configuration queries.
 */




/*
 * CONFIG SUPPORT
 ****************************************************************
 */

#ifdef NDMOS_COMMON_SYNC_CONFIG_INFO
/*
 * Get local info. Supports NDMPx_CONFIG_GET_HOST_INFO,
 * NDMP3_CONFIG_GET_SERVER_INFO, and NDMPx_CONFIG_GET_SCSI_INFO.
 */
void
ndmos_sync_config_info (struct ndm_session *sess)
{
	static struct utsname	unam;
	static char		osbuf[150];
	static char		idbuf[30];
	static char		revbuf[100];
	char			obuf[5];

	if (!sess->config_info) {
		sess->config_info = (ndmp9_config_info *)NDMOS_API_MALLOC (sizeof (ndmp9_config_info));
		if (!sess->config_info)
			return;
	}

	if (sess->config_info->hostname) {
		/* already set */
		return;
	}

	obuf[0] = (char)(NDMOS_ID >> 24);
	obuf[1] = (char)(NDMOS_ID >> 16);
	obuf[2] = (char)(NDMOS_ID >> 8);
	obuf[3] = (char)(NDMOS_ID >> 0);
	obuf[4] = 0;

	uname (&unam);
	snprintf (idbuf, sizeof(idbuf), "%lu", gethostid());
	/*
	 * give CONTROL via NDMPv2 a chance to recognize this
	 * implementation (no ndmp2_config_get_server).
	 */
	snprintf (osbuf, sizeof(osbuf), "%s (running %s from %s)",
			unam.sysname,
			NDMOS_CONST_PRODUCT_NAME,
			NDMOS_CONST_VENDOR_NAME);

	sess->config_info->hostname = unam.nodename;
	sess->config_info->os_type = osbuf;
	sess->config_info->os_vers = unam.release;
	sess->config_info->hostid = idbuf;

	sess->config_info->vendor_name = (char *)NDMOS_CONST_VENDOR_NAME;
	sess->config_info->product_name = (char *)NDMOS_CONST_PRODUCT_NAME;

	snprintf (revbuf, sizeof(revbuf), "%s LIB:%d.%d/%s OS:%s (%s)",
		NDMOS_CONST_PRODUCT_REVISION,
		NDMJOBLIB_VERSION, NDMJOBLIB_RELEASE,
		NDMOS_CONST_NDMJOBLIB_REVISION,
		NDMOS_CONST_NDMOS_REVISION,
		obuf);

	sess->config_info->revision_number = revbuf;

	/* best effort; note that this loads scsi and tape config */
	if (sess->param->config_file_name)
		ndmcfg_load (sess->param->config_file_name, sess->config_info);
}
#endif /* NDMOS_COMMON_SYNC_CONFIG_INFO */




/*
 * AUTHENTICATION SUPPORT
 ****************************************************************
 */

void
ndmos_auth_register_callbacks (struct ndm_session *sess,
  struct ndm_auth_callbacks *callbacks)
{
	/*
	 * Only allow one register.
	 */
	if (!sess->nac) {
		sess->nac = (struct ndm_auth_callbacks *) NDMOS_API_MALLOC (sizeof(struct ndm_auth_callbacks));
		if (sess->nac) {
			memcpy (sess->nac, callbacks, sizeof(struct ndm_auth_callbacks));
		}
	}
}

void
ndmos_auth_unregister_callbacks (struct ndm_session *sess)
{
	if (sess->nac) {
		NDMOS_API_FREE (sess->nac);
		sess->nac = NULL;
	}
}

#ifdef NDMOS_COMMON_OK_NAME_PASSWORD
/*
 * Determine whether the clear-text account name and password
 * are valid. Supports NDMPx_CONNECT_CLIENT_AUTH requests.
 */
int
ndmos_ok_name_password (struct ndm_session *sess, char *name, char *pass)
{
	if (sess->nac && sess->nac->validate_password) {
		return sess->nac->validate_password(sess, name, pass);
	}
	return 0;	/* NOT OK */
}
#endif /* NDMOS_COMMON_OK_NAME_PASSWORD */




#ifdef NDMOS_COMMON_MD5
/*
 * MD5 authentication support
 *
 * See ndml_md5.c
 */

int
ndmos_get_md5_challenge (struct ndm_session *sess)
{
	ndmmd5_generate_challenge (sess->md5_challenge);
	sess->md5_challenge_valid = 1;
	return 0;
}

int
ndmos_ok_name_md5_digest (struct ndm_session *sess,
  char *name, char digest[16])
{
	if (sess->nac && sess->nac->validate_md5) {
		return sess->nac->validate_md5(sess, name, digest);
	}
	return 0;	/* NOT OK */
}
#endif /* NDMOS_COMMON_MD5 */



#ifdef NDMOS_COMMON_NONBLOCKING_IO_SUPPORT
/*
 * NON-BLOCKING I/O SUPPORT
 ****************************************************************
 * As support non-blocking I/O for NDMCHAN, condition different
 * types of file descriptors to not block.
 */

void
ndmos_condition_listen_socket (struct ndm_session *sess, int sock)
{
	int		flag;

	flag = 1;
	setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, (void*)&flag, sizeof flag);
}

void
ndmos_condition_control_socket (struct ndm_session *sess, int sock)
{
	/* nothing */
}

void
ndmos_condition_image_stream_socket (struct ndm_session *sess, int sock)
{
	fcntl (sock, F_SETFL, O_NONBLOCK);
	signal (SIGPIPE, SIG_IGN);
}

void
ndmos_condition_pipe_fd (struct ndm_session *sess, int fd)
{
	fcntl (fd, F_SETFL, O_NONBLOCK);
	signal (SIGPIPE, SIG_IGN);
}
#endif /* NDMOS_COMMON_NONBLOCKING_IO_SUPPORT */




#ifdef NDMOS_COMMON_TAPE_INTERFACE
#ifndef NDMOS_OPTION_NO_TAPE_AGENT
#ifndef NDMOS_OPTION_TAPE_SIMULATOR
/*
 * TAPE INTERFACE
 ****************************************************************
 * These interface to the O/S specific tape drivers and subsystem.
 * They must result in functionality equivalent to the reference
 * tape simulator. The NDMP TAPE model is demanding, and it is
 * often necessary to workaround the native device driver(s)
 * to achieve NDMP TAPE model conformance.
 *
 * It's easy to test this ndmos_tape_xxx() implementation
 * using the ndmjob(1) command in test-tape conformance mode.
 * The tape simulator passes this test. To test this implementation,
 * rebuild ndmjob, then use this command:
 *
 *	ndmjob -o test-tape -T. -f /dev/whatever
 *
 * These ndmos_tape_xxx() interfaces must maintain the tape state
 * (sess->tape_agent.tape_state). In particular, the position
 * information (file_num and blockno) must be accurate at all
 * times. A typical workaround is to maintain these here rather
 * than relying on the native device drivers. Another workaround
 * is to implement NDMP MTIO operations using repeated native MTIO
 * operations with count=1, then interpret the results and errors
 * to maintain accurate position and residual information.
 *
 * Workarounds in this implementation (please keep this updated):
 *
 */

int
ndmos_tape_initialize (struct ndm_session *sess)
{
	return -1;
}

void
ndmos_tape_sync_state (struct ndm_session *sess)
{
	struct ndm_tape_agent * ta = sess->tape_acb;

	ta->tape_state.error = NDMP9_DEV_NOT_OPEN_ERR;
}

ndmp9_error
ndmos_tape_open (struct ndm_session *sess, char *name, int will_write)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error
ndmos_tape_close (struct ndm_session *sess)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error
ndmos_tape_write (struct ndm_session *sess, char *data,
  uint32_t count, uint32_t * done_count)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error
ndmos_tape_read (struct ndm_session *sess, char *data,
  uint32_t count, uint32_t * done_count)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error
ndmos_tape_mtio (struct ndm_session *sess, ndmp9_tape_mtio_op op,
  uint32_t count, uint32_t * done_count)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error
ndmos_tape_execute_cdb (struct ndm_session *sess,
  ndmp9_execute_cdb_request *request,
  ndmp9_execute_cdb_reply *reply)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

#endif /* !NDMOS_OPTION_TAPE_SIMULATOR */
#else /* !NDMOS_OPTION_NO_TAPE_AGENT */
/* tape interfaces implemented in ndma_tape_simulator.c */
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
#endif /* NDMOS_COMMON_TAPE_INTERFACE */


#ifdef NDMOS_COMMON_ROBOT_INTERFACE
#ifndef NDMOS_OPTION_NO_ROBOT_AGENT
#ifndef NDMOS_OPTION_ROBOT_SIMULATOR

/* ndmos_robot_* functions here */

#endif /* !NDMOS_OPTION_ROBOT_SIMULATOR */
#else /* !NDMOS_OPTION_NO_ROBOT_AGENT */
/* robot interfaces implemented in ndma_robot_simulator.c */
#endif /* !NDMOS_OPTION_NO_ROBOT_AGENT */
#endif /* NDMOS_COMMON_ROBOT_INTERFACE */


#ifdef NDMOS_COMMON_SCSI_INTERFACE
#ifndef NDMOS_OPTION_NO_ROBOT_AGENT	/* Surrounds all SCSI intfs */
#ifndef NDMOS_OPTION_ROBOT_SIMULATOR
/*
 * SCSI INTERFACE
 ****************************************************************
 */

int
ndmos_scsi_initialize (struct ndm_session *sess)
{
	return -1;
}

void
ndmos_scsi_sync_state (struct ndm_session *sess)
{
	sess->robot_acb->scsi_state.error = NDMP9_DEV_NOT_OPEN_ERR;
}

ndmp9_error
ndmos_scsi_open (struct ndm_session *sess, char *name)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error
ndmos_scsi_close (struct ndm_session *sess)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

/* deprecated */
ndmp9_error
ndmos_scsi_set_target (struct ndm_session *sess)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}


ndmp9_error
ndmos_scsi_reset_device (struct ndm_session *sess)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

/* deprecated */
ndmp9_error
ndmos_scsi_reset_bus (struct ndm_session *sess)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

ndmp9_error
ndmos_scsi_execute_cdb (struct ndm_session *sess,
  ndmp9_execute_cdb_request *request,
  ndmp9_execute_cdb_reply *reply)
{
	return NDMP9_NOT_SUPPORTED_ERR;
}

#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */
#endif /* NDMOS_OPTION_NO_ROBOT_AGENT	Surrounds all SCSI intfs */
#endif /* NDMOS_COMMON_SCSI_INTERFACE */




#ifdef NDMOS_COMMON_DISPATCH_REQUEST
/*
 * ndmos_dispatch_request() -- O/S Specific Agent Dispatch Request (ADR)
 ****************************************************************
 * Some NDMP requests can only be handled as O/S specific portions,
 * and are implemented here.
 *
 * The more generic NDMP requests may be re-implemented here rather
 * than modifying the main body of code. Extensions to the NDMP protocol
 * may also be implemented here. Neither is ever a good idea beyond
 * experimentation. The structures in ndmagents.h provide for O/S
 * specific extensions. Such extensions are #define'd in the ndmos_xxx.h.
 *
 * The return value from ndmos_dispatch_request() tells the main
 * dispatcher, ndma_dispatch_request(), whether or not the request
 * was intercepted. ndmos_dispatch_request() is called after basic
 * reply setup is done (message headers and buffers), but before
 * the request is interpretted.
 */

#ifndef I_HAVE_DISPATCH_REQUEST

/*
 * If we're not intercepting, keep it simple
 */
int
ndmos_dispatch_request (struct ndm_session *sess,
  struct ndmp_xa_buf *xa, struct ndmconn *ref_conn)
{
	return -1;	/* not intercepted */
}

#else /* !I_HAVE_DISPATCH_REQUEST */

/*
 * The following fragment is here for reference.
 * If the O/S module intercepts requests, copy
 * all this into the module source file and
 * #undef NDMOS_COMMON_DISPATCH_REQUEST.
 */

extern struct ndm_dispatch_version_table ndmos_dispatch_version_table[];

int
ndmos_dispatch_request (struct ndm_session *sess,
  struct ndmp_xa_buf *xa, struct ndmconn *ref_conn)
{
	struct ndm_dispatch_request_table *drt;
	unsigned	protocol_version = ref_conn->protocol_version;
	unsigned	msg = xa->request.header.message;
	int		rc;

	drt = ndma_drt_lookup (ndmos_dispatch_version_table,
					protocol_version, msg);

	if (!drt) {
		return -1;	/* not intercepted */
	}

	/*
	 * Replicate the ndma_dispatch_request() permission checks
	 */
	if (!sess->conn_open
	 && !(drt->flags & NDM_DRT_FLAG_OK_NOT_CONNECTED)) {
		xa->reply.header.error = NDMP0_PERMISSION_ERR;
		return 0;
	}

	if (!sess->conn_authorized
	 && !(drt->flags & NDM_DRT_FLAG_OK_NOT_AUTHORIZED)) {
		xa->reply.header.error = NDMP9_NOT_AUTHORIZED_ERR;
		return 0;
	}

	rc = (*drt->dispatch_request)(sess, xa, ref_conn);

	if (rc < 0) {
		xa->reply.header.error = NDMP0_NOT_SUPPORTED_ERR;
	}

	return 0;
}




/*
 * Dispatch Version Table and Dispatch Request Tables (DVT/DRT)
 ****************************************************************
 */

struct ndm_dispatch_request_table ndmos_dispatch_request_table_v0[] = {
   {0}
};

#ifndef NDMOS_OPTION_NO_NDMP2
struct ndm_dispatch_request_table ndmos_dispatch_request_table_v2[] = {
   {0}
};
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
struct ndm_dispatch_request_table ndmos_dispatch_request_table_v3[] = {
   {0}
};
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
struct ndm_dispatch_request_table ndmos_dispatch_request_table_v4[] = {
   {0}
};
#endif /* !NDMOS_OPTION_NO_NDMP4 */


struct ndm_dispatch_version_table ndmos_dispatch_version_table[] = {
	{ 0,		ndmos_dispatch_request_table_v0 },
#ifndef NDMOS_OPTION_NO_NDMP2
	{ NDMP2VER,	ndmos_dispatch_request_table_v2 },
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
	{ NDMP3VER,	ndmos_dispatch_request_table_v3 },
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP4
	{ NDMP4VER,	ndmos_dispatch_request_table_v4 },
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	{ -1 }
};
#endif /* !I_HAVE_DISPATCH_REQUEST */
#endif /* NDMOS_COMMON_DISPATCH_REQUEST */
