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


#ifndef NDMOS_OPTION_NO_TAPE_AGENT


/*
 * Initialization and Cleanup
 ****************************************************************
 */

/* Initialize -- Set data structure to know value, ignore current value */
int
ndmta_initialize (struct ndm_session *sess)
{
	int			rc;

	sess->tape_acb = NDMOS_API_MALLOC (sizeof(struct ndm_tape_agent));
	if (!sess->tape_acb)
		return -1;
	NDMOS_MACRO_ZEROFILL (sess->tape_acb);

	ndmta_commission (sess);

	rc = ndmos_tape_initialize (sess);
	if (rc) return rc;

	return 0;
}

/* Commission -- Get agent ready. Entire session has been initialize()d */
int
ndmta_commission (struct ndm_session *sess)
{
	ndmta_init_mover_state (sess);
	return 0;
}

/* Decommission -- Discard agent */
int
ndmta_decommission (struct ndm_session *sess)
{
	if (!sess->tape_acb)
		return 0;

	ndmis_tape_close (sess);

	return 0;
}

/* Destroy -- Destroy agent */
int
ndmta_destroy (struct ndm_session *sess)
{
	if (!sess->tape_acb)
		return 0;

	if (sess->tape_acb->tape_buffer) {
		NDMOS_API_FREE (sess->tape_acb->tape_buffer);
	}

#ifdef NDMOS_OPTION_TAPE_SIMULATOR
	if (sess->tape_acb->drive_name) {
		NDMOS_API_FREE (sess->tape_acb->drive_name);
	}
#endif

	NDMOS_API_FREE (sess->tape_acb);
	sess->tape_acb = NULL;

	return 0;
}

/* helper for mover state */
int
ndmta_init_mover_state (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	NDMOS_MACRO_ZEROFILL (&ta->mover_state);

	ta->mover_state.state = NDMP9_MOVER_STATE_IDLE;
	ta->mover_state.window_offset = 0;
	ta->mover_state.record_num = 0; /* this should probably be -1, but spec says 0 */
	ta->mover_state.record_size = 20*512;	/* traditional tar default */
	ta->mover_state.window_length = NDMP_LENGTH_INFINITY;
	ta->mover_window_end = NDMP_LENGTH_INFINITY;
	ta->mover_want_pos = 0;

	ta->tb_blockno = -1;

	return 0;
}



/*
 * Semantic actions -- called from ndma_dispatch()
 ****************************************************************
 */

void
ndmta_mover_sync_state (struct ndm_session *sess)
{
	ndmos_tape_sync_state (sess);
}

ndmp9_error
ndmta_mover_listen (struct ndm_session *sess, ndmp9_mover_mode mover_mode)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ta->mover_state.mode = mover_mode;

	ta->mover_state.state = NDMP9_MOVER_STATE_LISTEN;
	ta->mover_state.halt_reason = NDMP9_MOVER_HALT_NA;
	ta->mover_state.pause_reason = NDMP9_MOVER_PAUSE_NA;

	return NDMP9_NO_ERR;
}

ndmp9_error
ndmta_mover_connect (struct ndm_session *sess, ndmp9_mover_mode mover_mode)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ta->mover_state.mode = mover_mode;

	ndmta_mover_start_active (sess);

	return NDMP9_NO_ERR;
}

void
ndmta_mover_halt (struct ndm_session *sess, ndmp9_mover_halt_reason reason)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ta->mover_state.state = NDMP9_MOVER_STATE_HALTED;
	ta->mover_state.halt_reason = reason;
	ta->mover_state.pause_reason = NDMP9_MOVER_PAUSE_NA;
	ta->pending_change_after_drain = 0;
	ta->mover_notify_pending = 1;

	ndmis_tape_close (sess);
}

void
ndmta_mover_pause (struct ndm_session *sess, ndmp9_mover_pause_reason reason)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ta->mover_state.state = NDMP9_MOVER_STATE_PAUSED;
	ta->mover_state.halt_reason = NDMP9_MOVER_HALT_NA;
	ta->mover_state.pause_reason = reason;
	ta->pending_change_after_drain = 0;
	ta->mover_notify_pending = 1;
}

void
ndmta_mover_pending (struct ndm_session *sess,
  ndmp9_mover_state pending_state,
  ndmp9_mover_halt_reason halt_reason,
  ndmp9_mover_pause_reason pause_reason)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->pending_change_after_drain) {
		/* internal botch */
	}

	ta->pending_mover_state = pending_state;
	ta->pending_mover_halt_reason = halt_reason;
	ta->pending_mover_pause_reason = pause_reason;
	ta->pending_change_after_drain = 1;
}

void
ndmta_mover_apply_pending (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (!ta->pending_change_after_drain) {
		/* internal botch */
	}

	ta->mover_state.state = ta->pending_mover_state;
	ta->mover_state.halt_reason = ta->pending_mover_halt_reason;
	ta->mover_state.pause_reason = ta->pending_mover_pause_reason;
	ta->pending_change_after_drain = 0;
	ta->mover_notify_pending = 1;
}

void
ndmta_mover_halt_pending (struct ndm_session *sess,
  ndmp9_mover_halt_reason halt_reason)
{
	ndmta_mover_pending (sess, NDMP9_MOVER_STATE_HALTED,
		halt_reason, NDMP9_MOVER_PAUSE_NA);
}

void
ndmta_mover_pause_pending (struct ndm_session *sess,
  ndmp9_mover_pause_reason pause_reason)
{
	ndmta_mover_pending (sess, NDMP9_MOVER_STATE_PAUSED,
		NDMP9_MOVER_HALT_NA, pause_reason);
}

void
ndmta_mover_active (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ta->mover_state.state = NDMP9_MOVER_STATE_ACTIVE;
	ta->mover_state.halt_reason = NDMP9_MOVER_HALT_NA;
	ta->mover_state.pause_reason = NDMP9_MOVER_PAUSE_NA;

	ta->tb_blockno = -1;	/* always mistrust after activating */
}

void
ndmta_mover_start_active (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ndmalogf (sess, 0, 6, "mover going active");
	ndma_send_logmsg(sess, NDMP9_LOG_DEBUG, sess->plumb.control,
		"mover going active");

	switch (ta->mover_state.mode) {
	case NDMP9_MOVER_MODE_READ:
		ndmis_tape_start (sess, NDMCHAN_MODE_READ);
		ndmta_mover_active (sess);
		break;

	case NDMP9_MOVER_MODE_WRITE:
		ndmis_tape_start (sess, NDMCHAN_MODE_WRITE);
		ndmta_mover_active (sess);
		break;

	default:
		ndmalogf (sess, 0, 0, "BOTCH mover listen, unknown mode");
		break;
	}
}

void
ndmta_mover_stop (struct ndm_session *sess)
{
	ndmta_init_mover_state (sess);
}

void
ndmta_mover_abort (struct ndm_session *sess)
{
	ndmta_mover_halt (sess, NDMP9_MOVER_HALT_ABORTED);
}

void
ndmta_mover_continue (struct ndm_session *sess)
{
	ndmta_mover_active (sess);
}

void
ndmta_mover_close (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->mover_state.state != NDMP9_MOVER_STATE_HALTED)
		ndmta_mover_halt (sess, NDMP9_MOVER_HALT_CONNECT_CLOSED);
}

void
ndmta_mover_read (struct ndm_session *sess,
  uint64_t offset, uint64_t length)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ta->mover_state.seek_position = offset;
	ta->mover_state.bytes_left_to_read = length;
	ta->mover_want_pos = offset;
}



/*
 * Quantum -- get a bit of work done
 ****************************************************************
 */

int
ndmta_quantum (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	int			rc = 0;	/* did nothing */

	switch (ta->mover_state.state) {
	default:
		ndmalogf (sess, 0, 0, "BOTCH mover state");
		return -1;

	case NDMP9_MOVER_STATE_IDLE:
	case NDMP9_MOVER_STATE_PAUSED:
	case NDMP9_MOVER_STATE_HALTED:
		break;

	case NDMP9_MOVER_STATE_LISTEN:
		switch (sess->plumb.image_stream->tape_ep.connect_status) {
		case NDMIS_CONN_LISTEN:		/* no connection yet */
			break;

		case NDMIS_CONN_ACCEPTED:	/* we're in business */
			ndmta_mover_start_active (sess);
			rc = 1;		/* did something */
			break;

		case NDMIS_CONN_BOTCHED:	/* accept() went south */
		default:			/* ain't suppose to happen */
			ndmta_mover_halt(sess,NDMP9_MOVER_HALT_CONNECT_ERROR);
			break;
		}
		break;

	case NDMP9_MOVER_STATE_ACTIVE:
		switch (ta->mover_state.mode) {
		case NDMP9_MOVER_MODE_READ:
			rc = ndmta_read_quantum (sess);
			break;

		case NDMP9_MOVER_MODE_WRITE:
			rc = ndmta_write_quantum (sess);
			break;

		default:
			ndmalogf (sess, 0, 0,
				"BOTCH mover active, unknown mode");
			return -1;
		}
		break;
	}

	ndmta_mover_send_notice (sess);

	return rc;
}

int
ndmta_read_quantum (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	struct ndmchan *	ch = &sess->plumb.image_stream->chan;
	uint32_t		count = ta->mover_state.record_size;
	int			did_something = 0;
	unsigned		n_ready;
	char *			data;
	uint32_t		done_count;
	ndmp9_error		error;

  again:
	n_ready = ndmchan_n_ready (ch);
	if (ch->eof) {
		if (n_ready == 0) {
			/* done */
			if (ch->saved_errno)
				ndmta_mover_halt (sess,
					NDMP9_MOVER_HALT_CONNECT_ERROR);
			else
				ndmta_mover_halt (sess,
					NDMP9_MOVER_HALT_CONNECT_CLOSED);

			did_something++;

			return did_something;
		}

		if (n_ready < count) {
			int		n_pad = count - n_ready;
			int		n_avail;


			while (n_pad > 0) {
				n_avail = ndmchan_n_avail (ch);
				if (n_avail == 0) {
					/* Uh-oh */
				}
				data = &ch->data[ch->end_ix];
				if (n_avail > n_pad)
					n_avail = n_pad;
				bzero (data, n_avail);
				ch->end_ix += n_avail;
				n_pad -= n_avail;
			}
			n_ready = ndmchan_n_ready (ch);
		}
	}

	if (n_ready < count) {
		return did_something;	/* blocked */
	}

	if (ta->mover_want_pos >= ta->mover_window_end) {
		ndmta_mover_pause (sess, NDMP9_MOVER_PAUSE_SEEK);
		did_something++;
		return did_something;
	}

	data = &ch->data[ch->beg_ix];
	done_count = 0;

	error = ndmos_tape_write (sess, data, count, &done_count);

	switch (error) {
	case NDMP9_NO_ERR:
		if (done_count != count) {
			/* This ain't suppose to happen */
		}
		ta->mover_state.bytes_moved += count;
		ta->mover_want_pos += count;
		ta->mover_state.record_num = ta->mover_want_pos / ta->mover_state.record_size;
		ch->beg_ix += count;
		did_something++;
		goto again;	/* write as much to tape as possible */

	case NDMP9_EOM_ERR:
		ndmta_mover_pause (sess, NDMP9_MOVER_PAUSE_EOM);
		did_something++;
		break;

	default:
		ndmta_mover_halt (sess, NDMP9_MOVER_HALT_MEDIA_ERROR);
		did_something++;
		break;
	}

	return did_something;
}


int
ndmta_write_quantum (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	struct ndmchan *	ch = &sess->plumb.image_stream->chan;
	uint32_t		count = ta->mover_state.record_size;
	int			did_something = 0;
	uint64_t		max_read;
	uint64_t		want_window_off;
	uint32_t		block_size;
	uint32_t		want_blockno;
	uint32_t		cur_blockno;
	unsigned		n_avail, n_read, record_off;
	char *			data;
	uint32_t		done_count = 0;
	ndmp9_error		error;

  again:
	n_read = n_avail = ndmchan_n_avail_record (ch, count);
	if (n_avail < count) {
		/* allow to drain */
		return did_something;
	}

	if (ta->pending_change_after_drain) {
		if (ndmchan_n_ready (ch) > 0) {
			/* allow to drain */
		} else {
			ndmta_mover_apply_pending (sess);
			did_something++;
		}
		return did_something;
	}

	if (n_read > ta->mover_state.bytes_left_to_read)
		n_read = ta->mover_state.bytes_left_to_read;

	if (n_read < count) {
		/* Active, but paused awaiting MOVER_READ request */
		return did_something;	/* mover blocked */
	}

	if (ta->mover_want_pos < ta->mover_state.window_offset
	 || ta->mover_want_pos >= ta->mover_window_end) {
		ndmta_mover_pause_pending (sess, NDMP9_MOVER_PAUSE_SEEK);
		goto again;
	}

	max_read = ta->mover_window_end - ta->mover_want_pos;
	if (n_read > max_read)
		n_read = max_read;

	want_window_off = ta->mover_want_pos - ta->mover_state.window_offset;

	/* make an estimate of the block size - the tape agent's block size, or
	 * if it's in variable block size mode, the mover's record size: "When
	 * in variable block mode, as indicated by a tape block_size value of
	 * zero, the mover record size defines the actual block size used by
	 * the tape subsystem." (NDMPv4 RFC, Section 3.6.2.1) */
	block_size = ta->tape_state.block_size.value;
	if (!block_size)
		block_size = ta->mover_state.record_size;

	want_blockno = ta->mover_window_first_blockno + want_window_off / block_size;

	if (ta->tb_blockno != want_blockno) {
		uint32_t	xsr_count, xsr_resid;

		ndmos_tape_sync_state(sess);
		cur_blockno = ta->tape_state.blockno.value;
		if (cur_blockno < want_blockno) {
			xsr_count = want_blockno - cur_blockno;
	      ndmalogf (sess, 0, 6, "NDMP9_MTIO_FSR %lu", xsr_count);
			error = ndmos_tape_mtio (sess, NDMP9_MTIO_FSR,
						xsr_count, &xsr_resid);
			if (error == NDMP9_EOF_ERR) {
				ndmta_mover_pause_pending (sess,
						NDMP9_MOVER_PAUSE_EOF);
				goto again;
			}
			if (error != NDMP9_NO_ERR) {
				ndmta_mover_halt_pending (sess,
						NDMP9_MOVER_HALT_MEDIA_ERROR);
				goto again;
			}
			if (xsr_resid > 0) {
				ndmta_mover_pause_pending (sess,
						NDMP9_MOVER_PAUSE_EOF);
				goto again;
			}
		} else if (cur_blockno > want_blockno) {
	      ndmalogf (sess, 0, 6, "NDMP9_MTIO_BSR %lu", xsr_count);
			xsr_count = cur_blockno - want_blockno;
			error = ndmos_tape_mtio (sess, NDMP9_MTIO_BSR,
						xsr_count, &xsr_resid);
			if (error != NDMP9_NO_ERR || xsr_resid > 0) {
				ndmta_mover_halt_pending (sess,
						NDMP9_MOVER_HALT_MEDIA_ERROR);
				goto again;
			}
		} else {
			/* in position */
		}

		/*
		 * We are about to read data into a tape buffer so make sure
		 * we have it available. We delay allocating buffers to the
		 * moment we first need them.
		 */
		if (!ta->tape_buffer) {
			ta->tape_buffer = NDMOS_API_MALLOC (NDMOS_CONST_TAPE_REC_MAX);
			if (!ta->tape_buffer) {
				ndmta_mover_pause_pending (sess,
							NDMP9_MOVER_HALT_NA);
				goto again;
			}
		}

		data = ta->tape_buffer;
		done_count = 0;
		error = ndmos_tape_read (sess, data, count, &done_count);
		did_something++;

		if (error == NDMP9_EOF_ERR) {
			ndmta_mover_pause_pending (sess,
						NDMP9_MOVER_PAUSE_EOF);
			goto again;
		}
		/* N.B. - handling of done_count = 0 here is hacked to support
		 * non-blocking writes to a socket in amndmjob */
		if (error != NDMP9_NO_ERR) {
			ndmta_mover_halt_pending (sess,
				NDMP9_MOVER_HALT_MEDIA_ERROR);
			goto again;
		}
		if (done_count == 0) {
			return did_something - 1;
		}
		if (done_count != count) {
			goto again;
		}
		ta->tb_blockno = want_blockno;
		/* re-calcluate this, since record_size may be > block_size, in which
		 * case the record_num may not change for each block read from tape */
		ta->mover_state.record_num = ta->mover_want_pos / ta->mover_state.record_size;
	}

	record_off = ta->mover_want_pos % ta->mover_state.record_size;

	n_avail = ta->mover_state.record_size - record_off;
	if (n_read > n_avail)
		n_read = n_avail;
	if (n_read != done_count) {
		printf("lost %lu bytes %lu %u\n", done_count - n_read, done_count, n_read);
		n_read = done_count;
	}

	/*
	 * We are about to read data into a tape buffer so make sure
	 * we have it available. We delay allocating buffers to the
	 * moment we first need them.
	 */
	if (!ta->tape_buffer) {
		ta->tape_buffer = NDMOS_API_MALLOC (NDMOS_CONST_TAPE_REC_MAX);
		if (!ta->tape_buffer) {
			ndmta_mover_pause_pending (sess,
						NDMP9_MOVER_HALT_NA);
			goto again;
		}
	}

	data = &ta->tape_buffer[record_off];

	bcopy (data, ch->data + ch->end_ix, n_read);
	ch->end_ix += n_read;
	ta->mover_state.bytes_moved += n_read;
	ta->mover_want_pos += n_read;
	ta->mover_state.bytes_left_to_read -= n_read;

	did_something++;

	goto again;	/* do as much as possible */
}

void
ndmta_mover_send_notice (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (!ta->mover_notify_pending)
		return;

	ta->mover_notify_pending = 0;

	switch (ta->mover_state.state) {
	case NDMP9_MOVER_STATE_HALTED:
		ndma_notify_mover_halted (sess);
		break;

	case NDMP9_MOVER_STATE_PAUSED:
		ndma_notify_mover_paused (sess);
		break;

	default:
		/* Hmm. Why are we here. Race? */
		break;
	}
}

#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
