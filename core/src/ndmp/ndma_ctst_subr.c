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


#include "ndmagents.h"


#if !defined(NDMOS_OPTION_NO_CONTROL_AGENT) && !defined(NDMOS_OPTION_NO_TEST_AGENTS)

int
ndmca_test_query_conn_types (struct ndm_session *sess,
			     struct ndmconn *ref_conn)
{
    struct ndmconn *conn = ref_conn;
    struct ndm_control_agent *ca = sess->control_acb;
    int	rc;
    unsigned int i;

    switch (conn->protocol_version) {
	default:	return -1234;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH_VOID_REQUEST(ndmp2_config_get_mover_type, NDMP2VER)
	        rc = NDMC_CALL(conn);
		if (rc) {
		    ndmalogf (sess, "Test", 1, "GET_MOVER_TYPE failed");
		    return rc;
		}

		for (i = 0; i < reply->methods.methods_len; i++) {
		    switch(reply->methods.methods_val[i]) {
			case NDMP2_ADDR_LOCAL:
			    ca->has_local_addr = 1;
			    break;
			case NDMP2_ADDR_TCP:
			    ca->has_tcp_addr = 1;
			    break;
			default:
			    break;
		    }
		}

		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH_VOID_REQUEST(ndmp3_config_get_connection_type, NDMP3VER)
		rc = NDMC_CALL(conn);
		if (rc) {
		    ndmalogf (sess, "Test", 1, "GET_CONNECTION_TYPE failed");
		    return rc;
		}

		for (i = 0; i < reply->addr_types.addr_types_len; i++) {
		    switch(reply->addr_types.addr_types_val[i]) {
			case NDMP3_ADDR_LOCAL:
			    ca->has_local_addr = 1;
			    break;
			case NDMP3_ADDR_TCP:
			    ca->has_tcp_addr = 1;
			    break;
			default:
			    break;
		    }
		}
		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH_VOID_REQUEST(ndmp4_config_get_connection_type, NDMP4VER)
		rc = NDMC_CALL(conn);
		if (rc) {
		    ndmalogf (sess, "Test", 1, "GET_CONNECTION_TYPE failed");
		    return rc;
		}

		for (i = 0; i < reply->addr_types.addr_types_len; i++) {
		    switch(reply->addr_types.addr_types_val[i]) {
			case NDMP4_ADDR_LOCAL:
			    ca->has_local_addr = 1;
			    break;
			case NDMP4_ADDR_TCP:
			    ca->has_tcp_addr = 1;
			    break;
			default:
			    break;
		    }
		}
		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return 0;
}


int
ndmca_test_load_tape (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	int			rc;

	ca->tape_mode = NDMP9_TAPE_READ_MODE;
	ca->is_label_op = 1;

	rc = ndmca_op_robot_startup (sess, 1);
	if (rc) return rc;

	rc = ndmca_connect_tape_agent(sess);
	if (rc) {
		ndmconn_destruct (sess->plumb.tape);
		sess->plumb.tape = NULL;
		return rc;	/* already tattled */
	}

	rc = ndmca_media_load_first (sess);
	if (rc) return rc;

	ndmca_tape_close (sess);

	return 0;
}

int
ndmca_test_unload_tape (struct ndm_session *sess)
{
	ndmca_tape_open (sess);

	ndmca_media_unload_current(sess);

	return 0;
}

int
ndmca_test_check_expect_errs (struct ndmconn *conn, int rc,
  ndmp9_error expect_errs[])
{
	struct ndm_session *sess = conn->context;
	int		protocol_version = conn->protocol_version;
	struct ndmp_xa_buf *xa = &conn->call_xa_buf;
	unsigned	msg = xa->request.header.message;
	char *		msgname = ndmp_message_to_str (protocol_version, msg);
	ndmp9_error	reply_error = conn->last_reply_error;
	int		i;

	/* make sure we have a 'test' active */
	ndmca_test_open (sess, msgname, ndmp9_error_to_str (expect_errs[0]));

	if (rc >= 0) {
		/* Call succeeded. Body valid */
		rc = 1;
		for (i = 0; (int)expect_errs[i] >= 0; i++) {
			if (reply_error == expect_errs[i]) {
				rc = 0;
				break;
			}
		}

		if (rc) {
			if (reply_error != NDMP9_NO_ERR
			  && expect_errs[0] != NDMP9_NO_ERR) {
				/* both are errors, don't be picky */
				rc = 2;
			} else {
				/* intolerable mismatch */
			}
		} else {
			/* Worked as expected */
		}
	}

	if (rc != 0) {
	    char tmpbuf[128];

	    for (i = 0; (int)expect_errs[i] >= 0; i++) {
		ndmalogf (sess, "Test", 1,
			  "%s #%d -- .... %s %s",
			  sess->control_acb->test_phase,
			  sess->control_acb->test_step,
			  (i==0) ? "expected" : "or",
			  ndmp9_error_to_str (expect_errs[i]));
	    }

	    snprintf(tmpbuf, sizeof(tmpbuf), "got %s (error expected)", ndmp9_error_to_str (reply_error));

	    if (rc == 2)
		ndmca_test_warn (sess, tmpbuf);
	    else
		ndmca_test_fail (sess, tmpbuf);

	    ndma_tattle (conn, xa, rc);

	    if (rc == 2)
		rc = 0;
	}

	return rc;
}

int
ndmca_test_check_expect (struct ndmconn *conn, int rc, ndmp9_error expect_err)
{
	ndmp9_error		errs[2];

	errs[0] = expect_err;
	errs[1] = -1;

	return ndmca_test_check_expect_errs (conn, rc, errs);
}

int
ndmca_test_check_expect_no_err (struct ndmconn *conn, int rc)
{
	return ndmca_test_check_expect (conn, rc, NDMP9_NO_ERR);
}

int
ndmca_test_check_expect_illegal_state (struct ndmconn *conn, int rc)
{
	return ndmca_test_check_expect (conn, rc, NDMP9_ILLEGAL_STATE_ERR);
}

int
ndmca_test_check_expect_illegal_args (struct ndmconn *conn, int rc)
{
	return ndmca_test_check_expect (conn, rc, NDMP9_ILLEGAL_ARGS_ERR);
}


int
ndmca_test_call (struct ndmconn *conn,
  struct ndmp_xa_buf *xa, ndmp9_error expect_err)
{
	struct ndm_session *sess = conn->context;
	int		protocol_version = conn->protocol_version;
	unsigned	msg = xa->request.header.message;
	char *		msgname = ndmp_message_to_str (protocol_version, msg);
	unsigned	reply_error;
	int		rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	/* open new 'test' */
	ndmca_test_open (sess, msgname, ndmp9_error_to_str (expect_err));

	rc = ndma_call_no_tattle (conn, xa);

	reply_error = ndmnmb_get_reply_error (&xa->reply);

	if (rc >= 0) {
		/* Call succeeded. Body valid */
		if (reply_error == expect_err) {
			/* Worked exactly as expected */
			rc = 0;
		} else if (reply_error != NDMP9_NO_ERR
		        && expect_err != NDMP9_NO_ERR) {
			/* both are errors, don't be picky about the codes */
			rc = 2;
		} else {
			/* intolerable mismatch */
			rc = 1;
		}
	}

	if (rc != 0) {
	    char tmpbuf[128];
	    snprintf(tmpbuf, sizeof(tmpbuf), "got %s (call)", ndmp9_error_to_str (reply_error));
	    if (rc == 2)
		ndmca_test_warn (sess, tmpbuf);
	    else
		ndmca_test_fail (sess, tmpbuf);

	    ndma_tattle (conn, xa, rc);

	    if (rc == 2)
		rc = 0;
	}

	return rc;
}

/*
 * start or open a test if not already opened
 */
void
ndmca_test_open (struct ndm_session *sess, char *test_name, char *sub_test_name)
{
	static char test_name_buf[512];

	if (sess->control_acb->active_test == 0) {
		/* record name */
		if (sub_test_name)
			snprintf(test_name_buf, sizeof(test_name_buf), "%s/%s", test_name, sub_test_name);
		else
			strcpy(test_name_buf, test_name);
		sess->control_acb->active_test = test_name_buf;

		/* make sure flags are cleared */
		sess->control_acb->active_test_failed = (char *)0;
		sess->control_acb->active_test_warned = (char *)0;
	}
}

void
ndmca_test_warn (struct ndm_session *sess, char *warn_msg)
{
	static char warn_msg_buf[512];

	ndmca_test_open (sess, "UNKNOWN WARN", 0);

	strcpy(warn_msg_buf, warn_msg);
	sess->control_acb->active_test_warned = warn_msg_buf;
}

void
ndmca_test_fail (struct ndm_session *sess, char *fail_msg)
{
	static char fail_msg_buf[512];

	ndmca_test_open (sess, "UNKNOWN FAIL", 0);

	strcpy(fail_msg_buf, fail_msg);
	sess->control_acb->active_test_failed = fail_msg_buf;
}



/*
 * close or end a test if not already closed
 */
void
ndmca_test_close (struct ndm_session *sess)
{
	if (sess->control_acb->active_test != 0) {
		/* count test */
		sess->control_acb->n_step_tests++;

		/* display results */
		if (sess->control_acb->active_test_failed) {
			ndmalogf (sess, "Test", 1,
				"%s #%d -- Failed %s %s",
				sess->control_acb->test_phase,
				sess->control_acb->test_step,
				sess->control_acb->active_test,
				sess->control_acb->active_test_failed);
			sess->control_acb->n_step_fail++;
			exit(1);
		} else if (sess->control_acb->active_test_warned) {
			ndmalogf (sess, "Test", 1,
				"%s #%d -- Almost %s %s",
				sess->control_acb->test_phase,
				sess->control_acb->test_step,
				sess->control_acb->active_test,
				sess->control_acb->active_test_warned);
			sess->control_acb->n_step_warn++;
			exit(1);
		} else {
			ndmalogf (sess, "Test", 2,
				"%s #%d -- Passed %s",
				sess->control_acb->test_phase,
				sess->control_acb->test_step,
				sess->control_acb->active_test);
			sess->control_acb->n_step_pass++;
		}

		/* clear flags */
		sess->control_acb->active_test = (char *)0;
		sess->control_acb->active_test_failed = (char *)0;
		sess->control_acb->active_test_warned = (char *)0;

		/* advance test count */
		sess->control_acb->test_step++;
	}
}


/*
 * start a test phase (part of a series)
 */
void
ndmca_test_phase (struct ndm_session *sess, char *test_phase, char *desc)
{
	ndmalogf (sess, "TEST", 0, "Test %s -- %s", test_phase, desc);

	sess->control_acb->test_phase = test_phase;
	sess->control_acb->test_step = 1;
	sess->control_acb->n_step_pass = 0;
	sess->control_acb->n_step_fail = 0;
	sess->control_acb->n_step_warn = 0;
	sess->control_acb->n_step_tests = 0;
}

void
ndmca_test_log_step (struct ndm_session *sess, int level, char *msg)
{
	int had_active = (sess->control_acb->active_test != 0);

	ndmalogf (sess, "Test", level, "%s #%d -- %s",
		  sess->control_acb->test_phase,
		  sess->control_acb->test_step,
		  msg);

	/* in case we have a open test -- close it */
	ndmca_test_close (sess);

	/* advance test count if we didn't have an active test */
	if (!had_active)
		sess->control_acb->test_step++;

}

void
ndmca_test_log_note (struct ndm_session *sess, int level, char *msg)
{
	ndmalogf (sess, "Test", level, "%s #%d %s",
		sess->control_acb->test_phase,
		sess->control_acb->test_step,
		msg);
}

/*
 * finish a test phase (part of a series)
 */
void
ndmca_test_done_phase (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	char *			status;
	int had_active = (sess->control_acb->active_test != 0);

	/* close previous test if there is one */
	ndmca_test_close (sess);

	if (ca->n_step_fail)
		status = "Failed";
	else if (ca->n_step_warn)
		status = "Almost";
	else if (ca->n_step_pass > 0)
		status = "Passed";
	else
		status = "Whiffed";

	ndmalogf (sess, "TEST", 0, "Test %s %s -- pass=%d warn=%d fail=%d (total %d)",
			ca->test_phase,
			status,
			ca->n_step_pass,
			ca->n_step_warn,
			ca->n_step_fail,
			ca->n_step_tests);

	ca->total_n_step_pass += ca->n_step_pass;
	ca->total_n_step_warn += ca->n_step_warn;
	ca->total_n_step_fail += ca->n_step_fail;
	ca->total_n_step_tests += ca->n_step_tests;

	/* advance test count if we didn't have an active test so
	 * clean up phases have a new test count
         */
	if (!had_active)
	    sess->control_acb->test_step++;
}

/*
 * finish a test series (which may include test phases)
 */
void
ndmca_test_done_series (struct ndm_session *sess, char *series_name)
{
	struct ndm_control_agent *ca = sess->control_acb;
	char *			status;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	if (ca->total_n_step_fail)
		status = "Failed";
	else if (ca->total_n_step_warn)
		status = "Almost";
	else
		status = "Passed";

	ndmalogf (sess, "TEST", 0, "FINAL %s %s -- pass=%d warn=%d fail=%d (total %d)",
			series_name,
			status,
			ca->total_n_step_pass,
			ca->total_n_step_warn,
			ca->total_n_step_fail,
			ca->total_n_step_tests);
}



void
ndmca_test_fill_data (char *buf, int bufsize, int recno, int fileno)
{
	char *		src;
	char *		srcend;
	char *		dst = buf;
	char *		dstend = buf+bufsize;
	uint16_t	sequence = 0;
	struct {
		uint16_t	fileno;
		uint16_t	sequence;
		uint32_t	recno;
	}		x;

	x.fileno = fileno;
	x.recno = recno;

	srcend = (char *) &x;
	srcend += sizeof x;

	while (dst < dstend) {
		x.sequence = sequence++;
		src = (char *) &x;

		while (src < srcend && dst < dstend)
			*dst++ = *src++;
	}
}
#endif /* !defined(NDMOS_OPTION_NO_CONTROL_AGENT) && !defined(NDMOS_OPTION_NO_TEST_AGENTS) */
