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
 *****************************************************************
 *
 * NDMP Elements of a test-mover session
 *
 *                   +-----+     ###########
 *                   | Job |----># CONTROL #
 *                   +-----+     #  Agent  #
 *                               #         #
 *                               ###########
 *                                #   |  |
 *                  #=============#   |  +---------------------+
 *                  #                 |                        |
 *   CONTROL        #         control | connections            |
 *   impersonates   #                 V                        V
 *   DATA side of   #            ############  +-------+   #########
 *   image stream   #            #  TAPE    #  |       |   # ROBOT #
 *                  #            #  Agent   #  | ROBOT |<-># Agent #
 *                  #     image  # +------+ #  |+-----+|   #       #
 *                  #==============|mover |=====|DRIVE||   #       #
 *                        stream # +------+ #  |+-----+|   #       #
 *                               ############  +-------+   #########
 *
 ****************************************************************
 *
 */


#include "ndmagents.h"


#if !defined(NDMOS_OPTION_NO_CONTROL_AGENT) && !defined(NDMOS_OPTION_NO_TEST_AGENTS)


extern int	ndmca_tm_wrapper (struct ndm_session *sess,
				int (*func)(struct ndm_session *sess));


extern int	ndmca_op_test_mover (struct ndm_session *sess);
extern int	ndmca_tm_idle (struct ndm_session *sess);

extern int	ndmca_tm_listen (struct ndm_session *sess);
extern int	ndmca_tm_listen_subr (struct ndm_session *sess,
			ndmp9_error expect_err,
			ndmp9_addr_type addr_type,
			ndmp9_mover_mode mode);
extern int	ndmca_test_check_mover_state  (struct ndm_session *sess,
			ndmp9_mover_state expected, int reason);
extern int	ndmca_test_mover_get_state (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_mover_listen (struct ndm_session *sess,
			ndmp9_error expect_err,
			ndmp9_addr_type addr_type,
			ndmp9_mover_mode mode);
extern int	ndmca_test_mover_continue (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_mover_abort (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_mover_stop (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_mover_set_window (struct ndm_session *sess,
			ndmp9_error expect_err,
			uint64_t offset,
			uint64_t length);
extern int	ndmca_test_mover_read (struct ndm_session *sess,
			ndmp9_error expect_err,
			uint64_t offset,
			uint64_t length);
extern int	ndmca_test_mover_close (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_mover_set_record_size (struct ndm_session *sess,
			ndmp9_error expect_err);



struct series {
	unsigned	n_rec;
	unsigned	recsize;
};

struct series tm_series[] = {
	{ 1,	512 },
	{ 100,	1024 },
	{ 1,	512 },
	{ 100,	139 },
	{ 1,	512 },
	{ 99,	10240 },
	{ 1,	512 },
	{ 3,	32768 },
	{ 1,	512 },
	{ 0 }
};


int
ndmca_op_test_mover (struct ndm_session *sess)
{
	struct ndmconn *	conn;
	int			(*save_call) (struct ndmconn *conn,
						struct ndmp_xa_buf *xa);
	int			rc;
	struct ndm_control_agent *ca = sess->control_acb;

	if (sess->control_acb->job.data_agent.conn_type != NDMCONN_TYPE_NONE) {
		/*
		 * Sometimes needed to aid resident tape agent test
		 */
		rc = ndmca_connect_data_agent (sess);
		if (rc) {
			ndmconn_destruct (sess->plumb.data);
			sess->plumb.data = NULL;
			return rc;
		}
	}

	rc = ndmca_test_load_tape (sess);
	if (rc) return rc;

	conn = sess->plumb.tape;
	save_call = conn->call;
	conn->call = ndma_call_no_tattle;

	/* perform query to find out about TCP and LOCAL support */
	rc = ndmca_test_query_conn_types (sess, conn);
	if (rc) return rc;

	rc = ndmca_tm_wrapper (sess, ndmca_tm_idle);
	rc = ndmca_tm_wrapper (sess, ndmca_tm_listen);

	ndmca_test_unload_tape (sess);

	ndmca_test_done_series (sess, "test-mover");

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
ndmca_tm_wrapper (struct ndm_session *sess,
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

	ndmca_tape_open (sess);	/* Open the tape, OK if already opened */
	ndmca_tape_mtio (sess, NDMP9_MTIO_REW, 1, 0);
	rc = ndmca_tape_close (sess);	/* close, collective error */
	if (rc != 0) {
		ndmca_test_log_note (sess, 0, "Cleaning up failed, quiting");
	} else {
		ndmca_test_log_note (sess, 2, "Cleaning up done");
	}

	return rc;
}

int
ndmca_tm_idle (struct ndm_session *sess)
{
	int		rc;

	ndmca_test_phase (sess, "M-IDLE", "Mover IDLE State Series");

	rc = ndmca_test_check_mover_state  (sess, NDMP9_MOVER_STATE_IDLE, 0);
	if (rc) return rc;

	rc = ndmca_test_mover_continue (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	rc = ndmca_test_mover_abort (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	rc = ndmca_test_mover_stop (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	rc = ndmca_test_mover_close (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	rc = ndmca_test_mover_set_window (sess, NDMP9_NO_ERR, 0, 0);
	if (rc) return rc;

	rc = ndmca_test_mover_set_record_size (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	return 0;	/* pass */
}

extern int	ndmca_test_tape_open (struct ndm_session *sess,
			ndmp9_error expect_err,
			char *device, int mode);
extern int	ndmca_test_tape_close (struct ndm_session *sess,
			ndmp9_error expect_err);

int
ndmca_tm_listen (struct ndm_session *sess)
{
        struct ndm_control_agent *ca = sess->control_acb;
	int		rc;

	ndmca_test_phase (sess, "M-LISTEN", "Mover LISTEN State Series");

	rc = ndmca_test_check_mover_state  (sess, NDMP9_MOVER_STATE_IDLE, 0);
	if (rc) return rc;

	rc = ndmca_test_mover_set_record_size (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	rc = ndmca_test_mover_set_window (sess, NDMP9_NO_ERR, 0, 0);
	if (rc) return rc;

	ndmca_test_done_phase (sess);

	/*
	 * Bogus arguments
	 */
	ndmca_test_phase (sess, "M-LISTEN/bogus-args",
				"Mover LISTEN State Series w/ bogus args");

	if (ca->has_local_addr) {
	    rc = ndmca_test_mover_listen (sess, NDMP9_ILLEGAL_ARGS_ERR,
					  NDMP9_ADDR_LOCAL, 123);
	    if (rc) return rc;
	}

	rc = ndmca_test_mover_listen (sess, NDMP9_ILLEGAL_ARGS_ERR,
		123, NDMP9_MOVER_MODE_READ);
	if (rc) return rc;

	ndmca_test_done_phase (sess);

	/*
	 * Tape drive not open
	 */

	ndmca_test_phase (sess, "M-LISTEN/not-open",
				"Mover LISTEN State Series w/o tape open");

	if (ca->has_local_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_DEV_NOT_OPEN_ERR,
				       NDMP9_ADDR_LOCAL, NDMP9_MOVER_MODE_READ);
	    if (rc) return rc;
	}

	if (ca->has_tcp_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_DEV_NOT_OPEN_ERR,
				       NDMP9_ADDR_TCP, NDMP9_MOVER_MODE_READ);
	    if (rc) return rc;
	}

	if (ca->has_local_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_DEV_NOT_OPEN_ERR,
				       NDMP9_ADDR_LOCAL, NDMP9_MOVER_MODE_WRITE);
	    if (rc) return rc;
	}

	if (ca->has_tcp_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_DEV_NOT_OPEN_ERR,
				       NDMP9_ADDR_TCP, NDMP9_MOVER_MODE_WRITE);
	    if (rc) return rc;
	}

	ndmca_test_done_phase (sess);

	/*
	 * Tape drive open for READ
	 */
	ndmca_test_phase (sess, "M-LISTEN/tape-ro",
				"Mover LISTEN State Series w/ tape r/o");

	rc = ndmca_test_tape_open(sess, NDMP9_NO_ERR, 0, NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	if (ca->has_local_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_PERMISSION_ERR,
				       NDMP9_ADDR_LOCAL, NDMP9_MOVER_MODE_READ);
	    if (rc) return rc;
	}

	if (ca->has_tcp_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_PERMISSION_ERR,
				       NDMP9_ADDR_TCP, NDMP9_MOVER_MODE_READ);
	    if (rc) return rc;
	}

	if (ca->has_local_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_NO_ERR,
				       NDMP9_ADDR_LOCAL, NDMP9_MOVER_MODE_WRITE);
	    if (rc) return rc;
	}

	if (ca->has_tcp_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_NO_ERR,
				       NDMP9_ADDR_TCP, NDMP9_MOVER_MODE_WRITE);
	    if (rc) return rc;
	}

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	ndmca_test_done_phase (sess);

	/*
	 * Tape drive open for WRITE
	 */
	ndmca_test_phase (sess, "M-LISTEN/tape-rw",
				"Mover LISTEN State Series w/ tape r/w");

	rc = ndmca_test_tape_open(sess, NDMP9_NO_ERR, 0, NDMP9_TAPE_RDWR_MODE);
	if (rc) return rc;

	if (ca->has_local_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_NO_ERR,
				       NDMP9_ADDR_LOCAL, NDMP9_MOVER_MODE_READ);
	    if (rc) return rc;
	}

	if (ca->has_tcp_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_NO_ERR,
				       NDMP9_ADDR_TCP, NDMP9_MOVER_MODE_READ);
	    if (rc) return rc;
	}

	if (ca->has_local_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_NO_ERR,
				       NDMP9_ADDR_LOCAL, NDMP9_MOVER_MODE_WRITE);
	    if (rc) return rc;
	}

	if (ca->has_tcp_addr) {
	    rc = ndmca_tm_listen_subr (sess,
				       NDMP9_NO_ERR,
				       NDMP9_ADDR_TCP, NDMP9_MOVER_MODE_WRITE);
	    if (rc) return rc;
	}

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	/*
	 * TODO: NDMP9_MOVER_MODE_DATA
	 */

	/*
	 * Good enough
	 */

	return 0;	/* pass */
}

int
ndmca_tm_listen_subr (struct ndm_session *sess,
 ndmp9_error expect_err,
 ndmp9_addr_type addr_type, ndmp9_mover_mode mode)
{
	int		rc;

	rc = ndmca_test_check_mover_state  (sess, NDMP9_MOVER_STATE_IDLE, 0);
	if (rc) return rc;

	rc = ndmca_test_mover_listen (sess, expect_err, addr_type, mode);
	if (rc) return rc;

	if (expect_err != NDMP9_NO_ERR)
		return 0;		/* got expected error */

	rc = ndmca_test_check_mover_state  (sess, NDMP9_MOVER_STATE_LISTEN, 0);
	if (rc) return rc;

	rc = ndmca_test_mover_listen (sess, NDMP9_ILLEGAL_STATE_ERR,
		addr_type, mode);
	if (rc) return rc;

	rc = ndmca_test_mover_continue (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	rc = ndmca_test_mover_stop (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	/* setting the window size in LISTEN mode is not legal in
	 * version 4 and is required to work in earlier versions
	 */
	if (sess->plumb.tape->protocol_version < 4) {
	    rc = ndmca_test_mover_set_window (sess, NDMP9_NO_ERR, 0, 0);
	    if (rc) return rc;

	} else {
	    rc = ndmca_test_mover_set_window (sess, NDMP9_ILLEGAL_STATE_ERR, 0, 0);
	    if (rc) return rc;

	}

	rc = ndmca_test_mover_set_record_size (sess, NDMP9_ILLEGAL_STATE_ERR);
	if (rc) return rc;

	rc = ndmca_test_mover_abort (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	rc = ndmca_test_check_mover_state  (sess,
		NDMP9_MOVER_STATE_HALTED, NDMP9_MOVER_HALT_ABORTED);
	if (rc) return rc;

	rc = ndmca_test_mover_stop (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	rc = ndmca_test_check_mover_state  (sess, NDMP9_MOVER_STATE_IDLE, 0);
	if (rc) return rc;

	return 0;
}



int
ndmca_test_check_mover_state  (struct ndm_session *sess,
  ndmp9_mover_state expected, int reason)
{
	struct ndm_control_agent *	ca = sess->control_acb;
	ndmp9_mover_get_state_reply *	ms = &ca->mover_state;
	int				rc;
	char *				what;
	char				errbuf[100];
	char				tmpbuf[256];

	/* close previous test if there is one */
	ndmca_test_close (sess);

	/* open new test */
	ndmca_test_open (sess,
			 "mover check",
			 ndmp9_mover_state_to_str (expected));

	strcpy (errbuf, "???");

	what = "get_state";
	rc = ndmca_mover_get_state (sess);
	if (rc) goto fail;

	what = "state self-consistent";
	/* make sure the sensed state is self consistent */
	switch (ms->state) {
	case NDMP9_MOVER_STATE_IDLE:
	case NDMP9_MOVER_STATE_LISTEN:
	case NDMP9_MOVER_STATE_ACTIVE:
		if (ms->pause_reason != NDMP9_MOVER_PAUSE_NA
		 || ms->halt_reason != NDMP9_MOVER_HALT_NA) {
			strcpy (errbuf, "reason(s) != NA");
			goto fail;
		}
		break;

	case NDMP9_MOVER_STATE_PAUSED:
		if (ms->halt_reason != NDMP9_MOVER_HALT_NA) {
			strcpy (errbuf, "halt_reason != NA");
			goto fail;
		}
		break;

	case NDMP9_MOVER_STATE_HALTED:
		if (ms->pause_reason != NDMP9_MOVER_PAUSE_NA) {
			strcpy (errbuf, "pause_reason != NA");
			goto fail;
		}
		break;

	default:
		strcpy (errbuf, "bogus state");
		goto fail;
	}

	what = "state";
	if (ms->state != expected) {
		snprintf (errbuf, sizeof(errbuf), "expected %s got %s",
			ndmp9_mover_state_to_str (expected),
			ndmp9_mover_state_to_str (ms->state));
		goto fail;
	}

	what = "reason";
	switch (ms->state) {
	case NDMP9_MOVER_STATE_PAUSED:
		if (ms->pause_reason != (ndmp9_mover_pause_reason)reason) {
			snprintf (errbuf, sizeof(errbuf), "expected %s got %s",
			    ndmp9_mover_pause_reason_to_str (reason),
			    ndmp9_mover_pause_reason_to_str (ms->pause_reason));
			goto fail;
		}
		break;

	case NDMP9_MOVER_STATE_HALTED:
		if (ms->halt_reason != (ndmp9_mover_halt_reason)reason) {
			snprintf (errbuf, sizeof(errbuf), "expected %s got %s",
			    ndmp9_mover_halt_reason_to_str (reason),
			    ndmp9_mover_halt_reason_to_str (ms->halt_reason));
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



#define NDMTEST_CALL(CONN) ndmca_test_call(CONN, xa, expect_err);


int
ndmca_test_mover_get_state (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_mover_get_state (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_mover_listen (struct ndm_session *sess, ndmp9_error expect_err,
  ndmp9_addr_type addr_type, ndmp9_mover_mode mode)
{
	struct ndmconn *	conn = sess->plumb.tape;
	struct ndm_control_agent *ca = sess->control_acb;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	switch (conn->protocol_version) {
	default:	return -1234;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH(ndmp2_mover_listen, NDMP2VER)
		request->mode = mode;

		request->addr_type = addr_type;

		rc = NDMTEST_CALL(conn);
		if (rc) return rc;

		if (expect_err == NDMP9_NO_ERR
		 && request->addr_type != reply->mover.addr_type) {
			/* TODO: use proper test format */
			ndmalogf (sess, "Test", 1,
				"MOVER_LISTEN addr_type mismatch");
			return -1;
		}
		ndmp_2to9_mover_addr (&reply->mover, &ca->mover_addr);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH(ndmp3_mover_listen, NDMP3VER)
		request->mode = mode;

		request->addr_type = addr_type;

		rc = NDMTEST_CALL(conn);
		if (rc) return rc;

		if (expect_err == NDMP9_NO_ERR
		 && request->addr_type
		    != reply->data_connection_addr.addr_type) {
			/* TODO: use proper test format */
			ndmalogf (sess, "Test", 1,
				"MOVER_LISTEN addr_type mismatch");
			return -1;
		}
		ndmp_3to9_addr (&reply->data_connection_addr, &ca->mover_addr);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH(ndmp4_mover_listen, NDMP4VER)
		request->mode = mode;

		request->addr_type = addr_type;

		rc = NDMTEST_CALL(conn);
		if (rc) return rc;

		if (expect_err == NDMP9_NO_ERR
		 && request->addr_type
		    != reply->connect_addr.addr_type) {
			/* TODO: use proper test format */
			ndmalogf (sess, "Test", 1,
				"MOVER_LISTEN addr_type mismatch");
			return -1;
		}
		ndmp_4to9_addr (&reply->connect_addr, &ca->mover_addr);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return 0;
}

int
ndmca_test_mover_continue (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_mover_continue (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_mover_abort (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_mover_abort (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_mover_stop (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_mover_stop (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_mover_set_window (struct ndm_session *sess, ndmp9_error expect_err,
  uint64_t offset, uint64_t length)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_mover_set_window (sess, offset, length);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_mover_read (struct ndm_session *sess, ndmp9_error expect_err,
  uint64_t offset, uint64_t length)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_mover_read (sess, offset, length);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_mover_close (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_mover_close (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_mover_set_record_size (struct ndm_session *sess,
  ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.tape;
	struct ndm_control_agent *ca = sess->control_acb;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	switch (conn->protocol_version) {
	default:	return -1234;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH(ndmp2_mover_set_record_size, NDMP2VER)
		request->len = ca->job.record_size;
		rc = NDMTEST_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH(ndmp3_mover_set_record_size, NDMP3VER)
		request->len = ca->job.record_size;
		rc = NDMTEST_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH(ndmp4_mover_set_record_size, NDMP4VER)
		request->len = ca->job.record_size;
		rc = NDMTEST_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return rc;
}
#endif /* !defined(NDMOS_OPTION_NO_CONTROL_AGENT) && !defined(NDMOS_OPTION_NO_TEST_AGENTS) */
