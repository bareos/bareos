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
 * NDMP Elements of a test-tape session
 *
 *                   +-----+     ###########
 *                   | Job |----># CONTROL #
 *                   +-----+     #  Agent  #
 *                               #         #
 *                               ###########
 *                                    |  |
 *                                    |  +---------------------+
 *                            control | connections            |
 *                                    V                        V
 *                               ############  +-------+   #########
 *                               #  TAPE    #  |       |   # ROBOT #
 *                               #  Agent   #  | ROBOT |<-># Agent #
 *                               #          #  |+-----+|   #       #
 *                               #        ======|DRIVE||   #       #
 *                               #          #  |+-----+|   #       #
 *                               ############  +-------+   #########
 *
 ****************************************************************
 */


#include "ndmagents.h"


#if !defined(NDMOS_OPTION_NO_CONTROL_AGENT) && !defined(NDMOS_OPTION_NO_TEST_AGENTS)


extern int	ndmca_tt_wrapper (struct ndm_session *sess,
				int (*func)(struct ndm_session *sess));


extern int	ndmca_op_test_tape (struct ndm_session *sess);
extern int	ndmca_tt_openclose (struct ndm_session *sess);
extern int	ndmca_tt_basic_getstate (struct ndm_session *sess);
extern int	ndmca_tt_basic_write (struct ndm_session *sess);
extern int	ndmca_tt_basic_read (struct ndm_session *sess);
extern int	ndmca_tt_basic_write_and_read (struct ndm_session *sess);
extern int	ndmca_tt_write (struct ndm_session *sess);
extern int	ndmca_tt_read (struct ndm_session *sess);
extern int	ndmca_tt_mtio (struct ndm_session *sess);

extern int	ndmca_tt_check_fileno_recno (struct ndm_session *sess,
			char *what, uint32_t file_num, uint32_t blockno,
			char *note);

extern int	ndmca_test_tape_open (struct ndm_session *sess,
			ndmp9_error expect_err,
			char *device, int mode);
extern int	ndmca_test_tape_close (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_tape_get_state (struct ndm_session *sess,
			ndmp9_error expect_err);
extern int	ndmca_test_tape_mtio (struct ndm_session *sess,
			ndmp9_error expect_err,
			ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid);
extern int	ndmca_check_tape_mtio (struct ndm_session *sess,
			ndmp9_error expect_err,
			ndmp9_tape_mtio_op op, uint32_t count, uint32_t resid);
extern int	ndmca_test_tape_write (struct ndm_session *sess,
			ndmp9_error expect_err,
			char *buf, unsigned count);
extern int	ndmca_test_tape_read (struct ndm_session *sess,
			ndmp9_error expect_err,
			char *buf, unsigned count);
extern int	ndmca_test_tape_read_2cnt (struct ndm_session *sess,
			ndmp9_error expect_err,
			char *buf, unsigned count, unsigned true_count);


struct series {
	unsigned	n_rec;
	unsigned	recsize;
};

struct series tt_series[] = {
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
ndmca_op_test_tape (struct ndm_session *sess)
{
	struct ndmconn *	conn;
	int			(*save_call) (struct ndmconn *conn,
						struct ndmp_xa_buf *xa);
	int			rc;

	rc = ndmca_test_load_tape (sess);
	if (rc) return rc;

	conn = sess->plumb.tape;
	save_call = conn->call;
	conn->call = ndma_call_no_tattle;

	if (rc == 0)
		rc = ndmca_tt_wrapper (sess, ndmca_tt_openclose);
	if (rc == 0)
		rc = ndmca_tt_wrapper (sess, ndmca_tt_basic_getstate);
	if (rc == 0)
		rc = ndmca_tt_wrapper (sess, ndmca_tt_basic_write);
	if (rc == 0)
		rc = ndmca_tt_wrapper (sess, ndmca_tt_basic_read);
	if (rc == 0)
		rc = ndmca_tt_wrapper (sess, ndmca_tt_basic_write_and_read);
	if (rc == 0)
		rc = ndmca_tt_wrapper (sess, ndmca_tt_write);
	if (rc == 0)
		rc = ndmca_tt_wrapper (sess, ndmca_tt_read);
	if (rc == 0)
		rc = ndmca_tt_wrapper (sess, ndmca_tt_mtio);

	ndmca_test_unload_tape (sess);

	ndmca_test_done_series (sess, "test-tape");

	conn->call = save_call;

	return 0;
}

int
ndmca_tt_wrapper (struct ndm_session *sess,
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
ndmca_tt_openclose (struct ndm_session *sess)
{
	int		rc;

	ndmca_test_phase (sess, "T-OC", "Tape Open/Close");

	rc = ndmca_test_tape_close (sess, NDMP9_DEV_NOT_OPEN_ERR);
	if (rc) return rc;

	rc = ndmca_test_tape_open (sess, NDMP9_NO_DEVICE_ERR,
			"bogus", NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_open (sess, NDMP9_ILLEGAL_ARGS_ERR, 0, 123);
	if (rc) return rc;

	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	rc = ndmca_test_tape_open (sess,NDMP9_NO_ERR,0,NDMP9_TAPE_RDWR_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_open (sess, NDMP9_DEVICE_OPENED_ERR,
			0, NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	return 0;	/* pass */
}

int
ndmca_tt_basic_getstate (struct ndm_session *sess)
{
	int		rc;

	ndmca_test_phase (sess, "T-BGS", "Tape Get State Basics");

	rc = ndmca_test_tape_get_state (sess, NDMP9_DEV_NOT_OPEN_ERR);
	if (rc) return rc;

	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_get_state (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	return 0;	/* pass */
}

/*
 * Precedes tt_basic_read() so that we can make a "known" tape.
 */
int
ndmca_tt_basic_write (struct ndm_session *sess)
{
	int		rc, ix;
	char		buf[1024];
	ndmp9_error	expect_errs[5];

	ndmca_test_phase (sess, "T-BW", "Tape Write Basics");

	rc = ndmca_test_tape_write (sess, NDMP9_DEV_NOT_OPEN_ERR, buf, 1024);
	if (rc) return rc;

	/*
	 * Write w/ read-only open mode
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_write (sess, NDMP9_PERMISSION_ERR, buf, 1024);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	/*
	 * Write w/ bogus lengths
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_RDWR_MODE);
	if (rc) return rc;

	/* OPEN Question: what does len==0 mean? */
	/* write/len=0 MUST be NDMP[234]_NO_ERR or NDMP[234]_ILLEGAL_ARGS */
	/* write/len=0 MUST be NDMP4_NO_ERR */
	ix = 0;
	if (sess->plumb.tape->protocol_version < 5) {
		expect_errs[ix++] = NDMP9_ILLEGAL_ARGS_ERR;
	}
	expect_errs[ix++] = NDMP9_NO_ERR;
	expect_errs[ix++] = -1;

	rc = ndmca_tape_write (sess, buf, 0);

	rc = ndmca_test_check_expect_errs (sess->plumb.tape, rc, expect_errs);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	/*
	 * TODO: bogus length
	 */

	/*
	 * Write works
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_RDWR_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_write (sess, NDMP9_NO_ERR, buf, 1024);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_EOF, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	return 0;	/* pass */
}


/*
 * Assumes tt_basic_write() passed. Uses resulting tape.
 */

int
ndmca_tt_basic_read (struct ndm_session *sess)
{
	int		rc, ix;
	char		buf[2048];
	ndmp9_error	expect_errs[5];

	ndmca_test_phase (sess, "T-BR", "Tape Read Basics");

	rc = ndmca_test_tape_read (sess, NDMP9_DEV_NOT_OPEN_ERR, buf, 1024);
	if (rc) return rc;


	/*
	 * Read w/ bogus lengths -- mode=READ_MODE
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	/* read/len=0 MUST be NDMP[23]_NO_ERR or NDMP[23]_ILLEGAL_ARGS */
	/* read/len=0 MUST be NDMP4_NO_ERR */
	ix = 0;
	if (sess->plumb.tape->protocol_version < 4) {
		expect_errs[ix++] = NDMP9_ILLEGAL_ARGS_ERR;
	}
	expect_errs[ix++] = NDMP9_NO_ERR;
	expect_errs[ix++] = -1;

	rc = ndmca_tape_read (sess, buf, 0);

	rc = ndmca_test_check_expect_errs (sess->plumb.tape, rc, expect_errs);
	if (rc) return rc;

	rc = ndmca_test_tape_read(sess,NDMP9_ILLEGAL_ARGS_ERR,buf,0x80000000);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	/*
	 * Read works -- mode=WRITE_MODE (just to mix it up)
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_RDWR_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_read (sess, NDMP9_NO_ERR, buf, 1024);
	if (rc) return rc;

	rc = ndmca_test_tape_read (sess, NDMP9_EOF_ERR, buf, 1024);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;


	/*
	 * Read works w/ oversize -- mode=READ_MODE (just to mix it up)
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_read_2cnt (sess, NDMP9_NO_ERR, buf, 2048, 1024);
	if (rc) return rc;

	rc = ndmca_test_tape_read_2cnt (sess, NDMP9_EOF_ERR, buf, 2048, 1024);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;


	/*
	 * Read works w/ undersize -- mode=READ_MODE (just to mix it up)
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_read_2cnt (sess, NDMP9_NO_ERR, buf, 512, 512);
	if (rc) return rc;

	rc = ndmca_test_tape_read_2cnt (sess, NDMP9_EOF_ERR, buf, 512, 512);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	return 0;	/* pass */
}

#define CHECK_FILENO_RECNO(WHAT,FILENO,RECNO) { \
	what = WHAT;					\
	rc = ndmca_tt_check_fileno_recno (sess,		\
			WHAT, FILENO, RECNO, note);	\
	if (rc) return -1;				\
  }

/*
 * Assumes tt_basic_read() and tt_basic_write() have been done verifying
 * READ and WRITE operations work...
 */
int
ndmca_tt_basic_write_and_read (struct ndm_session *sess)
{
    int rc, i, f, pass;
    char buf[64*1024];
    char *p;

    ndmca_test_phase (sess, "T-BWR", "Tape Write and Read Basics");

    /*
     * check EOF and EOM by rewinding and putting on 1 EOF mark
     */
    rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_RDWR_MODE);
    if (rc) return rc;

    rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
    if (rc) return rc;

    rc = ndmca_check_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_BSR, 100, 100);
    if (rc) return rc;

    rc = ndmca_check_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_BSF, 100, 100);
    if (rc) return rc;

    rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_EOF, 1, 0);
    if (rc) return rc;

    rc = ndmca_check_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_BSF, 100, 99);
    if (rc) return rc;

    rc = ndmca_check_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_FSF, 100, 99);
    if (rc) return rc;

    /* we are at EOM */
    if (sess->plumb.tape->protocol_version < 4) {
	rc = ndmca_test_tape_read (sess, NDMP9_EOF_ERR, buf, sizeof(buf));
	if (rc) return rc;

	/* check it again */
	rc = ndmca_test_tape_read (sess, NDMP9_EOF_ERR, buf, 1024);
	if (rc) return rc;

    } else {
	rc = ndmca_test_tape_read (sess, NDMP9_EOM_ERR, buf, sizeof(buf));
	if (rc) return rc;

	/* check it again */
	rc = ndmca_test_tape_read (sess, NDMP9_EOM_ERR, buf, 1024);
	if (rc) return rc;
    }

    /* rewind and place 1 record in tape -- no EOF marker by seeking */

    rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
    if (rc) return rc;

    rc = ndmca_test_tape_write (sess, NDMP9_NO_ERR, buf, 512);
    if (rc) return rc;

    rc = ndmca_check_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_BSR, 100, 99);
    if (rc) return rc;

    rc = ndmca_check_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_FSR, 100, 99);
    if (rc) return rc;

    rc = ndmca_check_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_FSR, 100, 100);
    if (rc) return rc;

    rc = ndmca_check_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_FSF, 100, 100);
    if (rc) return rc;

    rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
    if (rc) return rc;

    /*
     * perform tape label type processing with positioning ops
     */
    for(pass = 0; pass < 2; pass++) {
	/*
	 * open the tape and write 1 record and close it
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_RDWR_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	for(p = buf, i = 0; i < 1024; i++, p++)
	    *p = ((i - 4) & 0xff);

	rc = ndmca_test_tape_write (sess, NDMP9_NO_ERR, buf, 1024);
	if (rc) return rc;

	rc = ndmca_tape_mtio (sess, NDMP9_MTIO_EOF, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	/*
	 * open the tape and read it
	 */
	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_RDWR_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	if (pass == 1)
	    rc = ndmca_test_tape_read_2cnt (sess, NDMP9_NO_ERR, buf, sizeof(buf), 1024);
	else
	    rc = ndmca_test_tape_read (sess, NDMP9_NO_ERR, buf, 1024);
	if (rc) return rc;

	for(p = buf, f = i = 0;
	    f < 64 && i < 1024;
	    i++, p++)
	    if (*p != ((i - 4) & 0xff)) {
		char tmp[80];
		snprintf (tmp, sizeof(tmp),
			 "%d: 0x%x => 0x%x",
			 i, ((i - 4) & 0xff), *p);
		ndmalogf (sess, "DATA", 6, tmp);
		f++;
	    }
	if (f > 0) {
	    ndmca_test_fail (sess, "Failed compare");
	    return -1;
	}

	rc = ndmca_test_tape_read (sess, NDMP9_EOF_ERR, buf, 1024);
	if (rc) return rc;

	/* check EOM */
	if (sess->plumb.tape->protocol_version < 4) {
	    rc = ndmca_test_tape_read (sess, NDMP9_EOF_ERR, buf, 1024);
	    if (rc) return rc;
	} else {
	    /* skip over filemark */
	    rc = ndmca_tape_mtio (sess, NDMP9_MTIO_FSF, 1, 0);
	    /* read EOM */
	    rc = ndmca_test_tape_read (sess, NDMP9_EOM_ERR, buf, 1024);
	    if (rc) return rc;
	}

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;
    }

    return 0;	/* pass */
}

/*
 * Precedes tt_read() so that we can make a "known" tape.
 */
int
ndmca_tt_write (struct ndm_session *sess)
{
	int		rc;
	unsigned	n_rec;
	unsigned	recsize;
	unsigned	fileno, recno;
	char *		what;
	char		note[128];
	char		buf[64*1024];

	ndmca_test_phase (sess, "T-WRITE", "Tape Write Series");

	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_RDWR_MODE);
	if (rc) return rc;

	for (fileno = 0; tt_series[fileno].n_rec > 0; fileno++) {
		n_rec = tt_series[fileno].n_rec;
		recsize = tt_series[fileno].recsize;

		snprintf (note, sizeof(note), "Write tape file %d", fileno+1);
		ndmca_test_open (sess, note, 0);

		snprintf (note, sizeof(note), "file #%d, %d records, %d bytes/rec",
				fileno+1, n_rec, recsize);
		ndmca_test_log_note (sess, 2, note);

		for (recno = 0; recno < n_rec; recno++) {
			ndmca_test_fill_data (buf, recsize, recno, fileno);

			what = "write";
			rc = ndmca_tape_write (sess, buf, recsize);
			if (rc) goto fail;

			CHECK_FILENO_RECNO ("write", fileno, recno+1);
		}

		what = "write filemark";
		rc = ndmca_tape_mtio (sess, NDMP9_MTIO_EOF, 1, 0);
		if (rc) goto fail;

		CHECK_FILENO_RECNO ("wfm", fileno+1, 0);

		/* no test calls so the file operation is the test */
		snprintf (buf, sizeof(buf), "Passed tape write %s", note);
		ndmca_test_log_step (sess, 2, buf);
	}

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	return 0;

  fail:
	snprintf (buf, sizeof(buf), "Failed %s recno=%d; %s", what, recno, note);
	ndmca_test_fail (sess, buf);
	return -1;
}




/*
 * Assumes tt_write() passed
 */
int
ndmca_tt_read (struct ndm_session *sess)
{
	int		rc;
	unsigned	n_rec;
	unsigned	recsize;
	unsigned	fileno, recno;
	char *		what;
	char		note[128];
	char		pbuf[64*1024];
	char		buf[64*1024];

	ndmca_test_phase (sess, "T-READ", "Tape Read Series");

	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	for (fileno = 0; tt_series[fileno].n_rec > 0; fileno++) {
		n_rec = tt_series[fileno].n_rec;
		recsize = tt_series[fileno].recsize;

		snprintf (note, sizeof(note), "Read tape file %d", fileno+1);
		ndmca_test_open (sess, note, 0);

		snprintf (note, sizeof(note), "file #%d, %d records, %d bytes/rec",
				fileno+1, n_rec, recsize);
		ndmca_test_log_note (sess, 2, note);

		for (recno = 0; recno < n_rec; recno++) {
			ndmca_test_fill_data (pbuf, recsize, recno, fileno);

			what = "read";
			rc = ndmca_tape_read (sess, buf, recsize);
			if (rc) goto fail;

			CHECK_FILENO_RECNO ("read", fileno, recno+1);

			what = "compare";
			if (bcmp (buf, pbuf, recsize) != 0) {
				unsigned char *expect_p = (unsigned char *)pbuf;
				unsigned char *got_p = (unsigned char *)buf;
				unsigned int i, f;
				for(f = i = 0;
				    f < 64 && i < recsize;
				    i++, expect_p++, got_p++) {
				    if (*expect_p != *got_p) {
					char tmp[80];
					snprintf (tmp, sizeof(tmp),
						 "%d: 0x%x => 0x%x",
						 i, *expect_p, *got_p);
					ndmalogf (sess, "DATA", 6, tmp);
					f++;
				    }
				}
				goto fail;
			}
		}

		what = "eof read";
		rc = ndmca_test_tape_read (sess, NDMP9_EOF_ERR, buf, recsize);
		if (rc) goto fail;

		if (sess->plumb.tape->protocol_version > 3) {
		    CHECK_FILENO_RECNO ("eof", fileno, -1);

		    what = "skip filemark";
		    rc = ndmca_tape_mtio (sess, NDMP9_MTIO_FSF, 1, 0);
		    if (rc) goto fail;

		    CHECK_FILENO_RECNO ("skip", fileno+1, 0);
		} else {
		    CHECK_FILENO_RECNO ("eof", fileno+1, 0);
		}

		snprintf (buf, sizeof(buf), "Passed tape read %s", note);
		ndmca_test_log_step (sess, 2, buf);
	}

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	return 0;

  fail:
	snprintf (buf, sizeof(buf), "Failed %s recno=%d; %s", what, recno, note);
	ndmca_test_fail (sess, buf);
	return -1;
}


/*
 * Assumes tt_write() passed
 */
int
ndmca_tt_mtio (struct ndm_session *sess)
{
	int		rc;
	unsigned	n_rec;
	unsigned	recsize;
	unsigned	fileno, recno;
	uint32_t	count, resid;
	char *		what;
	char		note[128];
	char		pbuf[64*1024];
	char		buf[64*1024];

	ndmca_test_phase (sess, "T-MTIO", "Tape MTIO");

	rc = ndmca_test_tape_open(sess,NDMP9_NO_ERR,0,NDMP9_TAPE_READ_MODE);
	if (rc) return rc;

	rc = ndmca_test_tape_mtio (sess, NDMP9_NO_ERR, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	for (fileno = 0; tt_series[fileno].n_rec > 0; fileno++) {
		n_rec = tt_series[fileno].n_rec;
		recsize = tt_series[fileno].recsize;

		snprintf (note, sizeof(note), "Seek around tape file %d", fileno+1);
		ndmca_test_open (sess, note, 0);

		snprintf (note, sizeof(note), "file #%d, %d records, %d bytes/rec",
				fileno+1, n_rec, recsize);
		ndmca_test_log_note (sess, 2, note);

		what = "rew";
		count = 1;
		rc = ndmca_tape_mtio (sess, NDMP9_MTIO_REW, count, &resid);
		if (rc) goto fail;

		what = "rew resid";
		if (resid != 0)
			goto fail;

		CHECK_FILENO_RECNO ("rew", 0, 0);


		what = "fsf(n)";
		count = fileno;
		rc = ndmca_tape_mtio (sess, NDMP9_MTIO_FSF, count, &resid);
		if (rc) goto fail;

		what = "fsf(n) resid";
		if (resid != 0)
			goto fail;

		CHECK_FILENO_RECNO ("fsf", fileno, 0);


		what = "fsr(1m)";
		count = 1000000;
		rc = ndmca_tape_mtio (sess, NDMP9_MTIO_FSR, count, &resid);
		if (rc) goto fail;

		what = "fsr(1m) resid";
		if (n_rec + resid != count)
			goto fail;

		if (sess->plumb.tape->protocol_version < 4) {
		    CHECK_FILENO_RECNO ("fsr(1m)", fileno + 1, 0);

		    what = "bsf 1 after fsr(1m)";
		    count = 1;
		    rc = ndmca_tape_mtio (sess, NDMP9_MTIO_BSF, count, 0);
		    if (rc) goto fail;

		    CHECK_FILENO_RECNO (what, fileno, -1);

		    recno = n_rec;
		} else {
		    /* EOT side of EOF marker */
		    recno = n_rec;
		    CHECK_FILENO_RECNO ("fsr(1m)", fileno, recno);
		}

		what = "bsr(1m)";
		count = 1000000;
		rc = ndmca_tape_mtio (sess, NDMP9_MTIO_BSR, count, &resid);
		if (rc) goto fail;

		what = "bsr(1m) resid";
		if (n_rec + resid != count)
			goto fail;

		if ((fileno > 0) && (sess->plumb.tape->protocol_version < 4)) {
		    /* at BOT side of EOF marker (not BOT) */
		    CHECK_FILENO_RECNO ("bsr(1m)", fileno - 1, -1);

		    what = "fsf 1 after bsr(1m)";
		    count = 1;
		    rc = ndmca_tape_mtio (sess, NDMP9_MTIO_FSF, count, 0);
		    if (rc) goto fail;
		}

		recno = 0;
		CHECK_FILENO_RECNO ("bsr(1m)", fileno, recno);

		what = "fsr(0)";
		count = 0;
		rc = ndmca_tape_mtio (sess, NDMP9_MTIO_FSR, count, &resid);
		if (rc) goto fail;

		what = "fsr(0) resid";
		if (resid != 0)
			goto fail;

		recno = 0;
		CHECK_FILENO_RECNO ("fsr(0)", fileno, recno);


		what = "fsr(x)";
		count = n_rec / 2;
		rc = ndmca_tape_mtio (sess, NDMP9_MTIO_FSR, count, &resid);
		if (rc) goto fail;

		what = "fsr(x) resid";
		if (resid != 0)
			goto fail;

		recno = n_rec / 2;
		CHECK_FILENO_RECNO ("fsr(x)", fileno, recno);

		what = "fsr(x) read";
		rc = ndmca_tape_read (sess, buf, recsize);
		if (rc) goto fail;

		what = "fsr(x) compare";
		ndmca_test_fill_data (pbuf, recsize, recno, fileno);
		if (bcmp (buf, pbuf, recsize) != 0)
			goto fail;

		recno++;	/* caused by tape_read */

		if (recno > 1) {
			what = "bsr(2)";
			count = 2;
			rc = ndmca_tape_mtio (sess, NDMP9_MTIO_BSR,
						count, &resid);
			if (rc) goto fail;

			what = "bsr(2) resid";
			if (resid != 0)
				goto fail;

			recno -= count;
			CHECK_FILENO_RECNO ("bsr(2)", fileno, recno);

			what = "bsr(2) read";
			rc = ndmca_tape_read (sess, buf, recsize);
			if (rc) goto fail;

			what = "bsr(2) compare";
			ndmca_test_fill_data (pbuf, recsize, recno, fileno);
			if (bcmp (buf, pbuf, recsize) != 0)
				goto fail;
		}

		snprintf (buf, sizeof(buf), "Passed %s", note);
		ndmca_test_log_step (sess, 2, buf);
	}

	rc = ndmca_test_tape_close (sess, NDMP9_NO_ERR);
	if (rc) return rc;

	return 0;

  fail:
	snprintf (buf, sizeof(buf), "Failed %s: %s", what, note);
	ndmca_test_fail (sess, buf);
	return -1;
}



/*
 * Check the tape_state accurately reflects position
 */

int
ndmca_tt_check_fileno_recno (struct ndm_session *sess,
  char *what, uint32_t file_num, uint32_t blockno, char *note)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndmp9_tape_get_state_reply *ts = 0;
	char			buf[100];
	int			rc;
	char *			oper;

	oper ="get_state";
	rc = ndmca_tape_get_state (sess);
	if (rc) goto fail;

	ts = &ca->tape_state;

	oper = "check file_num";
	if (ts->file_num.value != file_num)
		goto fail;

	oper = "check blockno";
	if ((ts->blockno.value != blockno) && (ts->blockno.value != NDMP9_INVALID_U_LONG))
		goto fail;

	return 0;

  fail:
	snprintf (buf, sizeof(buf), "Failed %s while testing %s", oper, what);
	ndmca_test_log_note (sess, 1, buf);
	if (ts) {
		snprintf (buf, sizeof(buf), "    expect file_num=%ld got file_num=%ld",
			(long)file_num, (long)ts->file_num.value);
		ndmca_test_log_note (sess, 1, buf);

		snprintf (buf, sizeof(buf), "    expect blockno=%ld got blockno=%ld",
			(long)blockno, (long)ts->blockno.value);
		ndmca_test_log_note (sess, 1, buf);
	}

	snprintf (buf, sizeof(buf), "    note: %s", note);
	ndmca_test_fail (sess, buf);
	return -1;
}




#define NDMTEST_CALL(CONN) ndmca_test_call(CONN, xa, expect_err);


int
ndmca_test_tape_open (struct ndm_session *sess, ndmp9_error expect_err,
  char *device, int mode)
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
	    NDMC_WITH (ndmp2_tape_open, NDMP2VER)
		if (device)
			request->device.name = device;
		else
			request->device.name = ca->job.tape_device;
		if (mode != -1)
			request->mode = mode;
		else
			request->mode = ca->tape_mode;
		rc = NDMTEST_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH (ndmp3_tape_open, NDMP3VER)
		if (device)
			request->device = device;
		else
			request->device = ca->job.tape_device;
		if (mode != -1)
			request->mode = mode;
		else
			request->mode = ca->tape_mode;
		rc = NDMTEST_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH (ndmp4_tape_open, NDMP4VER)
		if (device)
			request->device = device;
		else
			request->device = ca->job.tape_device;
		if (mode != -1)
			request->mode = mode;
		else
			request->mode = ca->tape_mode;
		rc = NDMTEST_CALL(conn);
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return rc;
}

int
ndmca_test_tape_close (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_tape_close (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_tape_get_state (struct ndm_session *sess, ndmp9_error expect_err)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_tape_get_state (sess);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_tape_mtio (struct ndm_session *sess, ndmp9_error expect_err,
  ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_tape_mtio (sess, op, count, resid);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_check_tape_mtio (struct ndm_session *sess, ndmp9_error expect_err,
		       ndmp9_tape_mtio_op op, uint32_t count, uint32_t resid)
{
    struct ndmconn *	conn = sess->plumb.tape;
    uint32_t got_resid;
    int rc;

    /* close previous test if there is one */
    ndmca_test_close (sess);

    got_resid = ~resid;

    rc = ndmca_tape_mtio (sess, op, count, &got_resid);

    rc = ndmca_test_check_expect (conn, rc, expect_err);
    if (rc) return rc;

    if (resid != got_resid) {
	char tmp[128];
	snprintf (tmp, sizeof(tmp),
		 "Residual incorrect, got %lu expected %lu",
		 got_resid,
		 resid);
	ndmca_test_fail (sess, tmp);
	return -1;
    }

    return rc;
}


int
ndmca_test_tape_write (struct ndm_session *sess, ndmp9_error expect_err,
  char *buf, unsigned count)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_tape_write (sess, buf, count);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_tape_read (struct ndm_session *sess, ndmp9_error expect_err,
  char *buf, unsigned count)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	rc = ndmca_tape_read (sess, buf, count);

	rc = ndmca_test_check_expect (conn, rc, expect_err);

	return rc;
}

int
ndmca_test_tape_read_2cnt (struct ndm_session *sess, ndmp9_error expect_err,
  char *buf, unsigned count, unsigned true_count)
{
	struct ndmconn *	conn = sess->plumb.tape;
	int			rc;

	/* close previous test if there is one */
	ndmca_test_close (sess);

	switch (conn->protocol_version) {
	default:	return -1234;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
	    NDMC_WITH(ndmp2_tape_read, NDMP2VER)
		request->count = count;
		rc = NDMTEST_CALL(conn);
		if (rc == 0 && expect_err == NDMP9_NO_ERR) {
			if (reply->data_in.data_in_len == true_count) {
				bcopy (reply->data_in.data_in_val,
							buf, true_count);
			} else {
				rc = -1;
			}
		}
		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */
#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
	    NDMC_WITH(ndmp3_tape_read, NDMP3VER)
		request->count = count;
		rc = NDMTEST_CALL(conn);
		if (rc == 0 && expect_err == NDMP9_NO_ERR) {
			if (reply->data_in.data_in_len == true_count) {
				bcopy (reply->data_in.data_in_val,
							buf, true_count);
			} else {
				rc = -1;
			}
		}
		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */
#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
	    NDMC_WITH(ndmp4_tape_read, NDMP4VER)
		request->count = count;
		rc = NDMTEST_CALL(conn);
		if (rc == 0 && expect_err == NDMP9_NO_ERR) {
			if (reply->data_in.data_in_len == true_count) {
				bcopy (reply->data_in.data_in_val,
							buf, true_count);
			} else {
				rc = -1;
			}
		}
		NDMC_FREE_REPLY();
	    NDMC_ENDWITH
	    break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return rc;
}
#endif /* !defined(NDMOS_OPTION_NO_CONTROL_AGENT) && !defined(NDMOS_OPTION_NO_TEST_AGENTS) */
