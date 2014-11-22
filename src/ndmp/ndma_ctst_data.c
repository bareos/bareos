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
 ********************************************************************
 *
 * NDMP Elements of a test-data session
 *
 *                   +-----+     ###########
 *                   | Job |----># CONTROL #
 *                   +-----+     #  Agent  #
 *                               #         #
 *                               ###########
 *                                 |      #
 *                +----------------+      #
 *                | control connection    #     CONTROL
 *                V                       #     impersonates
 *           ############                 #     TAPE side of
 *           #  DATA    #                 #     image stream
 *           #  Agent   #                 #
 *  +-----+  # +------+ # image           #
 *  |FILES|====|butype|===================#
 *  +-----+  # +------+ # stream
 *           ############
 *
 *
 ********************************************************************
 *
 */


#include "ndmagents.h"


#if !defined(NDMOS_OPTION_NO_CONTROL_AGENT) && !defined(NDMOS_OPTION_NO_TEST_AGENTS)


extern int	ndmca_td_wrapper (struct ndm_session *sess,
				int (*func)(struct ndm_session *sess));


extern int	ndmca_op_test_data (struct ndm_session *sess);
extern int	ndmca_td_idle (struct ndm_session *sess);

extern int	ndmca_td_listen (struct ndm_session *sess);
extern int	ndmca_td_listen_subr (struct ndm_session *sess,
				      ndmp9_error expect_err,
				      ndmp9_addr_type addr_type);
extern int	ndmca_test_check_data_state  (struct ndm_session *sess,
			ndmp9_data_state expected, int reason);
extern int	ndmca_test_data_get_state (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_data_abort (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_data_stop (struct ndm_session *sess,
			ndmp9_error expect_err);



int
ndmca_op_test_data (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndmconn *	conn;
	int			(*save_call) (struct ndmconn *conn,
						struct ndmp_xa_buf *xa);
	int			rc;

	rc = ndmca_connect_data_agent(sess);
	if (rc) {
		ndmconn_destruct (sess->plumb.data);
		sess->plumb.data = NULL;
		return rc;
	}

	conn = sess->plumb.data;
	save_call = conn->call;
	conn->call = ndma_call_no_tattle;

	/* perform query to find out about TCP and LOCAL support */
	rc = ndmca_test_query_conn_types (sess, conn);
	if (rc) return rc;

	rc = ndmca_td_wrapper (sess, ndmca_td_idle);
	if (sess->plumb.data->protocol_version >= 3) {
	    // version 3 and later adds LISTEN
	    rc = ndmca_td_wrapper (sess, ndmca_td_listen);
	}

	ndmca_test_done_series (sess, "test-data");

	ca = sess->control_acb;
	if (ca->has_tcp_addr && ca->has_local_addr) {
	    ndmalogf (sess, "TEST", 0, "LOCAL and TCP addressing tested.");
	} else if (ca->has_tcp_addr) {
	    ndmalogf (sess, "TEST", 0, "TCP addressing ONLY tested.");
	} else if (ca->has_local_addr) {
	    ndmalogf (sess, "TEST", 0, "LOCAL addressing ONLY tested.");
	} else {
	    ndmalogf (sess, "TEST", 0, "Neither TCP or LOCAL addressing tested.");
	}

	return 0;
}

int
ndmca_td_wrapper (struct ndm_session *sess,
  int (*func)(struct ndm_session *sess))
{
	int		rc;

	rc = (*func)(sess);

	if (rc != 0) {
		ndmalogf (sess, "Test", 1, "Failure");
	}

	ndmca_test_done_phase (sess);

	/* clean up mess */
	ndmca_test_log_note (sess, 2, "Cleaning up...");

	rc = 0;

	return rc;
}

int
ndmca_td_idle (struct ndm_session *sess)
{
	int		rc;

	ndmca_test_phase (sess, "D-IDLE", "Data IDLE State Series");

	rc = ndmca_test_check_data_state  (sess, NDMP9_DATA_STATE_IDLE, 0);
	if (rc) return rc;

	rc = ndmca_test_data_abort (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	rc = ndmca_test_data_stop (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	return 0;	/* pass */
}



#define NDMTEST_CALL(CONN) ndmca_test_call(CONN, xa, expect_err);



int
ndmca_test_data_listen (struct ndm_session *sess, ndmp9_error expect_err,
			ndmp9_addr_type addr_type)
{
	struct ndmconn *	conn = sess->plumb.data;
	struct ndm_control_agent *ca = sess->control_acb;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	switch (conn->protocol_version) {
	default:	return -1234;

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH(ndmp3_data_listen, NDMP3VER)
		request->addr_type = addr_type;

		rc = NDMTEST_CALL(conn);
		if (rc) return rc;

		if (expect_err == NDMP9_NO_ERR
		 && request->addr_type
		    != reply->data_connection_addr.addr_type) {
			/* TODO: use proper test format */
			ndmalogf (sess, "Test", 1,
				"DATA_LISTEN addr_type mismatch");
			return -1;
		}
		ndmp_3to9_addr (&reply->data_connection_addr, &ca->data_addr);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH(ndmp4_data_listen, NDMP4VER)
		request->addr_type = addr_type;

		rc = NDMTEST_CALL(conn);
		if (rc) return rc;

		if (expect_err == NDMP9_NO_ERR
		 && request->addr_type
		    != reply->connect_addr.addr_type) {
			/* TODO: use proper test format */
			ndmalogf (sess, "Test", 1,
				"DATA_LISTEN addr_type mismatch");
			return -1;
		}
		ndmp_4to9_addr (&reply->connect_addr, &ca->data_addr);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return 0;
}


int
ndmca_td_listen (struct ndm_session *sess)
{
        struct ndm_control_agent *ca = sess->control_acb;
	int		rc;

	ndmca_test_phase (sess, "D-LISTEN", "Data LISTEN State Series");

	rc = ndmca_test_check_data_state  (sess, NDMP9_DATA_STATE_IDLE, 0);
	if (rc) return rc;

	if (ca->has_tcp_addr) {
	    rc = ndmca_td_listen_subr (sess, NDMP9_NO_ERR, NDMP9_ADDR_TCP);
	    if (rc) return rc;
	}

	if (ca->has_local_addr) {
	    rc = ndmca_td_listen_subr (sess, NDMP9_NO_ERR, NDMP9_ADDR_LOCAL);
	    if (rc) return rc;
	}

	ndmca_test_done_phase (sess);

	/*
	 * Bogus arguments
	 */
	ndmca_test_phase (sess, "D-LISTEN/bogus-args",
				"Data LISTEN State Series w/ bogus args");

	rc = ndmca_test_data_listen (sess, NDMP9_ILLEGAL_ARGS_ERR,
				     123);
	if (rc) return rc;

	ndmca_test_done_phase (sess);


	return 0;	/* pass */
}

int
ndmca_td_listen_subr (struct ndm_session *sess,
		      ndmp9_error expect_err,
		      ndmp9_addr_type addr_type)
{
	int		rc;

	rc = ndmca_test_check_data_state  (sess, NDMP9_DATA_STATE_IDLE, 0);
	if (rc) return rc;

	rc = ndmca_test_data_listen (sess, expect_err, addr_type);
	if (rc) return rc;

	if (expect_err != NDMP9_NO_ERR)
		return 0;		/* got expected error */

	rc = ndmca_test_check_data_state  (sess, NDMP9_DATA_STATE_LISTEN, 0);
	if (rc) return rc;

	rc = ndmca_test_data_listen (sess, NDMP9_ILLEGAL_STATE_ERR,
		addr_type);
	if (rc) return rc;

	rc = ndmca_test_data_stop (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	rc = ndmca_test_data_abort (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	rc = ndmca_test_check_data_state  (sess,
		NDMP9_DATA_STATE_HALTED, NDMP9_DATA_HALT_ABORTED);
	if (rc) return rc;

	rc = ndmca_test_data_stop (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	rc = ndmca_test_check_data_state  (sess, NDMP9_DATA_STATE_IDLE, 0);
	if (rc) return rc;

	return 0;
}

int
ndmca_test_check_data_state  (struct ndm_session *sess,
  ndmp9_data_state expected, int reason)
{
	struct ndm_control_agent *	ca = sess->control_acb;
	ndmp9_data_get_state_reply *	ds = &ca->data_state;
	int				rc;
	char *				what;
	char				errbuf[100];
	char				tmpbuf[256];

	/* close previous test if there is one */
	ndmca_test_close (sess);

	/* open new test */
	ndmca_test_open (sess,
			 "data check",
			 ndmp9_data_state_to_str (expected));

	strcpy (errbuf, "???");

	what = "get_state";
	rc = ndmca_data_get_state (sess);
	if (rc) goto fail;

	what = "state self-consistent";
	/* make sure the sensed state is self consistent */
	switch (ds->state) {
	case NDMP9_DATA_STATE_IDLE:
	case NDMP9_DATA_STATE_ACTIVE:
	case NDMP9_DATA_STATE_LISTEN:
	case NDMP9_DATA_STATE_CONNECTED:
		if (ds->halt_reason != NDMP9_DATA_HALT_NA) {
			strcpy (errbuf, "reason != NA");
			goto fail;
		}
		break;

	case NDMP9_DATA_STATE_HALTED:
		break;

	default:
		strcpy (errbuf, "bogus state");
		goto fail;
	}

	what = "state";
	if (ds->state != expected) {
		snprintf (errbuf, sizeof(errbuf), "expected %s got %s",
			ndmp9_data_state_to_str (expected),
			ndmp9_data_state_to_str (ds->state));
		goto fail;
	}

	what = "reason";
	switch (ds->state) {
	case NDMP9_DATA_STATE_HALTED:
		if (ds->halt_reason != (ndmp9_data_halt_reason)reason) {
			snprintf (errbuf, sizeof(errbuf), "expected %s got %s",
			    ndmp9_data_halt_reason_to_str (reason),
			    ndmp9_data_halt_reason_to_str (ds->halt_reason));
			goto fail;
		}
		break;

	default:
		break;
	}

	/* test passed */
	ndmca_test_close (sess);

	return 0;

  fail:
	/* test failed */
	snprintf(tmpbuf, sizeof(tmpbuf), "%s: %s", what, errbuf);
	ndmca_test_fail(sess, tmpbuf);

	ndmca_test_close (sess);

	return -1;
}



int
ndmca_test_data_get_state (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.data;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_data_get_state (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_data_abort (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.data;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_data_abort (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_data_stop (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.data;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_data_stop (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}






#endif /* !defined(NDMOS_OPTION_NO_CONTROL_AGENT) && !defined(NDMOS_OPTION_NO_TEST_AGENTS) */
