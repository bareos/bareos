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


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT

void
ndmca_media_register_callbacks (struct ndm_session *sess,
  struct ndmca_media_callbacks *callbacks)
{
	/*
	 * Only allow one register.
	 */
        if (!sess->nmc) {
		sess->nmc = NDMOS_API_MALLOC (sizeof(struct ndmca_media_callbacks));
		if (sess->nmc) {
			memcpy (sess->nmc, callbacks, sizeof(struct ndmca_media_callbacks));
		}
        }
}

void
ndmca_media_unregister_callbacks (struct ndm_session *sess)
{
        if (sess->nmc) {
		NDMOS_API_FREE (sess->nmc);
		sess->nmc = NULL;
	}
}

int
ndmca_media_load_first (struct ndm_session *sess)
{
	int		rc;

	if (sess->nmc && sess->nmc->load_first) {
		rc = sess->nmc->load_first (sess);
		if (rc) return rc;
        }

	sess->control_acb->cur_media_ix = 1;
	return ndmca_media_load_current (sess);
}

/*
 * TODO: It would be nice that if a media entry has a problem
 *       during load_current() to just skip over it and proceed
 *       to the next one. It will be really annoying to have a
 *       long running backup terminate because of a write-protect
 *       or label-check error when there are perfectly good
 *       tapes available.
 */
int
ndmca_media_load_next (struct ndm_session *sess)
{
	int		rc;
	int		n_media;

	if (sess->nmc && sess->nmc->load_next) {
		rc = sess->nmc->load_next (sess);
		if (rc) return rc;
        }

	n_media = sess->control_acb->job.media_tab.n_media;
	if (sess->control_acb->cur_media_ix >= n_media) {
		ndmalogf (sess, 0, 0, "Out of tapes");
		return -1;
	}
	sess->control_acb->cur_media_ix++;
	return ndmca_media_load_current (sess);
}

int
ndmca_media_unload_last (struct ndm_session *sess)
{
	return ndmca_media_unload_current(sess);
}

int
ndmca_media_change (struct ndm_session *sess)
{
	int		rc;

	rc = ndmca_media_unload_current(sess);
	if (rc) return rc;

	rc = ndmca_media_load_next (sess);
	if (rc) return rc;

	return 0;
}

int
ndmca_media_load_seek (struct ndm_session *sess, uint64_t pos)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	int			n_media = job->media_tab.n_media;
	struct ndmmedia *	me;

	for (me = job->media_tab.head; me; me = me->next) {
		if (me->begin_offset <= pos && pos < me->end_offset)
			break;
	}

	if (!me || me->index > n_media) {
		ndmalogf (sess, 0, 0, "Seek to unspecified media");
		return -1;
	}

	ca->cur_media_ix = me->index;
	return ndmca_media_load_current (sess);
}






int
ndmca_media_load_current (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	struct ndmmedia *	me;
	int			rc;
	unsigned		count;

	for (me = job->media_tab.head; me; me = me->next) {
		if (me->index == ca->cur_media_ix)
			break;
	}

	if (!me) {
		return -1;
	}

	if (job->have_robot) {
		rc = ndmca_robot_load (sess, me->slot_addr);
		if (rc) return rc;
	}

	me->media_used = 1;

	rc = ndmca_media_open_tape (sess);
	/*
	 * TODO: it would be nice to discriminate this and
	 *       set flags accordingly. For example,
	 *       indicate a write-protect tape.
	 */
	if (rc) {
		me->media_open_error = 1;

		/* if use_eject, this won't work */
		if (job->have_robot) {
			/* best-effort unload the robot */
			ndmca_robot_unload (sess, me->slot_addr);
		}
		return rc;
	}

	ca->media_is_loaded = 1;

	rc = ndmca_media_mtio_tape (sess, NDMP9_MTIO_REW, 1, 0);
	if (rc) {
		me->media_io_error = 1;
		goto close_and_unload;
	}

	if (ca->is_label_op) {
		if (ca->tape_mode == NDMP9_TAPE_RDWR_MODE)
			me->media_written = 1;	/* most likely */

		return 0;		/* ready to go */
	}

	if (me->valid_label) {
		rc = ndmca_media_check_label (sess, 'm', me->label);
		if (rc) {
			if (rc == -1) {
				me->label_io_error = 1;
			} else if (rc == -2) {
				me->label_read = 1;
				me->label_mismatch = 1;
			}
			goto close_and_unload;
		}
		me->label_read = 1;

		rc = ndmca_media_mtio_tape (sess, NDMP9_MTIO_REW, 1, 0);
		if (rc) {
			me->media_io_error = 1;
		}
	}

	if (!me->valid_filemark) {		/* synthetic */
		me->valid_filemark = 1;
		if (me->valid_label)
			me->file_mark_offset = 1;
		else
			me->file_mark_offset = 0;
	}

	count = me->file_mark_offset;

	if (count > 0) {
		rc = ndmca_media_mtio_tape (sess, NDMP9_MTIO_FSF, count, 0);
		if (rc) {
			me->fmark_error = 1;
			ndmca_media_mtio_tape (sess, NDMP9_MTIO_REW, 1, 0);
			goto close_and_unload;
		}
	}

	if (ca->tape_mode == NDMP9_TAPE_RDWR_MODE)
		me->media_written = 1;	/* most likely */

	return 0;

  close_and_unload:
	me->media_io_error = 1;
	ndmca_media_unload_best_effort (sess);
	return rc;
}

int
ndmca_media_unload_current (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	struct ndmmedia *	me;
	int			rc;

	if (!ca->media_is_loaded)
		return 0;

	rc = ndmca_media_mtio_tape (sess, NDMP9_MTIO_REW, 1, 0);
	if (rc) return rc;

	if (ca->job.use_eject) {
		rc = ndmca_media_mtio_tape (sess, NDMP9_MTIO_OFF, 1, 0);
		if (rc) return rc;
	}

	rc = ndmca_media_close_tape (sess);
	if (rc) return rc;

	for (me = job->media_tab.head; me; me = me->next) {
		if (me->index == ca->cur_media_ix)
			break;
	}

	if (me && job->have_robot) {
		rc = ndmca_robot_unload (sess, me->slot_addr);
		if (rc) return rc;
	}

	ca->media_is_loaded = 0;

	if (sess->nmc && sess->nmc->unload_current) {
		rc = sess->nmc->unload_current (sess);
		if (rc) return rc;
        }

	return 0;
}

int
ndmca_media_unload_best_effort (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	struct ndmmedia *	me;
	int			errors = 0;
	int			rc;

	if (!ca->media_is_loaded)
		return 0;

	rc = ndmca_media_mtio_tape (sess, NDMP9_MTIO_REW, 1, 0);
	if (rc) errors++;

	if (ca->job.use_eject) {
		rc = ndmca_media_mtio_tape (sess, NDMP9_MTIO_OFF, 1, 0);
		if (rc) errors++;
	}

	rc = ndmca_media_close_tape (sess);
	if (rc) errors++;

	if (job->have_robot) {
		for (me = job->media_tab.head; me; me = me->next) {
			if (me->index == ca->cur_media_ix)
				break;
		}

		if (me) {
			rc = ndmca_robot_unload (sess, me->slot_addr);
			if (rc) errors++;
		} else {
			errors++;
		}
	}

	ca->media_is_loaded = 0;

	return errors ? -1 : 0;
}






int
ndmca_media_open_tape (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	int			rc;
	unsigned int		t;

	ndmalogf (sess, 0, 1, "Opening tape drive %s %s",
			ca->job.tape_device,
			(ca->tape_mode == NDMP9_TAPE_RDWR_MODE)
				? "read/write" : "read-only");

	rc = -1;
	for (t = 0; t <= ca->job.tape_timeout; t += 10) {
		if (t > 0) {
			ndmalogf (sess, 0, 1,
				"Pausing ten seconds before retry (%d/%d)",
				t, ca->job.tape_timeout);
			sleep (10);
		}
		rc = ndmca_tape_open(sess);
		if (rc == 0) break;
	}

	if (rc) {
		/* should interpret the error */
		ndmalogf (sess, 0, 0, "failed open tape drive %s %s",
			ca->job.tape_device,
			(ca->tape_mode == NDMP9_TAPE_RDWR_MODE)
				? "read/write" : "read-only");
	}

	return rc;
}

int
ndmca_media_close_tape (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	int			rc;

	ndmalogf (sess, 0, 2, "Closing tape drive %s", ca->job.tape_device);

	rc = ndmca_tape_close (sess);

	return 0;
}

int
ndmca_media_mtio_tape (struct ndm_session *sess,
  ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid)
{
	int			rc;

	if (op == NDMP9_MTIO_REW) {
		ndmalogf (sess, 0, 1, "Commanding tape drive to rewind");
	} else if (op == NDMP9_MTIO_OFF) {
		ndmalogf (sess, 0, 1,
			"Commanding tape drive to eject (go offline)");
	} else {
		ndmalogf (sess, 0, 2, "Commanding tape drive to %s %d times",
			ndmp9_tape_mtio_op_to_str (op),
			count);
	}

	rc = ndmca_tape_mtio (sess, op, count, resid);

	return rc;
}

int
ndmca_media_write_filemarks (struct ndm_session *sess)
{
	int		rc;

	rc = ndmca_media_mtio_tape (sess, NDMP9_MTIO_EOF, 2, 0);

	return rc;
}





/*
 * Returns: 'm' for tape (media) label
 *          'V' for volume label
 *          '?' for unknown content
 *          -1  for some kind of error
 */

int
ndmca_media_read_label (struct ndm_session *sess, char labbuf[])
{
	char		tape_read_buf[512];
	int		rc;
	char *		p;
	char *		q;

	ndmalogf (sess, 0, 2, "Reading label");

	*labbuf = 0;

	rc = ndmca_tape_read (sess, tape_read_buf, 512);

	if (rc == 0) {
		p = tape_read_buf;
		if (strncmp (p, "##ndmjob -m ", 12) == 0) {
			p += 12;
			rc = 'm';
		} else if (strncmp (p, "##ndmjob -V ", 12) == 0) {
			p += 12;
			rc = 'V';
		} else {
			rc = '?';
			p = 0;
		}
		if (p) {
			q = labbuf;
			while (*p && *p != '\n'
			 && q < &labbuf[NDMMEDIA_LABEL_MAX-1]) {
				*q++ = *p++;
			}
			*q = 0;
		}
	} else {
		rc = -1;
	}

	return rc;
}

/*
 * type is either 'm' or 'V', see above
 */
int
ndmca_media_write_label (struct ndm_session *sess, int type, char labbuf[])
{
	int		rc;
	char		buf[512];
	char *		p;

	ndmalogf (sess, 0, 1, "Writing tape label '%s' type=%c", labbuf, type);

	for (p = buf; p < &buf[512]; p++) *p = '#';
	for (p = buf+63; p < &buf[512]; p += 64) *p = '\n';

	snprintf (buf, sizeof(buf), "##ndmjob -%c %s", type, labbuf);
	for (p = buf; *p; p++) continue;
	*p++ = '\n';

	rc = ndmca_tape_write (sess, buf, 512);

	return rc;
}

/*
 * type is either 'm' or 'V', see above
 */
int
ndmca_media_check_label (struct ndm_session *sess, int type, char labbuf[])
{
	int		rc;
	char		mylabbuf[NDMMEDIA_LABEL_MAX];

	ndmalogf (sess, 0, 1, "Checking tape label, expect '%s'", labbuf);

	rc = ndmca_media_read_label (sess, mylabbuf);
	if (rc < 0) {
		ndmalogf (sess, 0, 0, "Label read error");
		return -1;
	}

	if (rc != type || strcmp (labbuf, mylabbuf) != 0) {
		ndmalogf (sess, 0, 0,
			"Label mismatch, expected -%c'%s', got -%c'%s'",
			type, labbuf, rc, mylabbuf);
		return -2;
	}

	return 0;
}









int
ndmca_media_verify (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	int			rc;

	if (job->have_robot)
		return 0;		/* not much we can do in advance */

	rc = ndmca_robot_verify_media (sess);
	if (rc == 0)
		return rc;

	ndmca_media_tattle (sess);

	return -1;
}

int
ndmca_media_tattle (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	struct ndmmedia *	me;
	int			line, nline;

	if (!sess->dump_media_info) {
		return 0;
        }

	for (me = job->media_tab.head; me; me = me->next) {
		char			buf[80];

		nline = ndmmedia_pp (me, 0, buf);
		ndmalogf (sess, 0, 1, "media #%d %s", me->index, buf);

		for (line = 1; line < nline; line++) {
			nline = ndmmedia_pp (me, line, buf);
			ndmalogf (sess, 0, 2, "         %s", buf);
		}
	}
	return 0;
}


/*
 * Determine the current byte offset in the current
 * tape file.
 */

uint64_t
ndmca_media_capture_tape_offset (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	int			rc;
	uint64_t		off;

	rc = ndmca_tape_get_state(sess);
	if (rc) return NDMP_LENGTH_INFINITY;	/* invalid? */

	if (!ca->tape_state.blockno.valid)
		return NDMP_LENGTH_INFINITY;	/* invalid? */

	off = ca->tape_state.blockno.value;
	off *= job->record_size;

	return off;
}

int
ndmca_media_capture_mover_window (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndmlog *		ixlog = &ca->job.index_log;
	struct ndm_job_param *	job = &ca->job;
	struct ndmmedia *	me;
	ndmp9_mover_state	ms = ca->mover_state.state;
	ndmp9_mover_pause_reason pr = ca->mover_state.pause_reason;
	char			buf[100];
	uint64_t		wlen;

	for (me = job->media_tab.head; me; me = me->next) {
		if (me->index == ca->cur_media_ix)
			break;
	}

	if (!me) {
		return -1;
	}

	if (ms == NDMP9_MOVER_STATE_PAUSED) {
		if (pr == NDMP9_MOVER_PAUSE_SEEK) {
			/* end-of-window */
		} else if (pr == NDMP9_MOVER_PAUSE_EOM) {
			me->media_eom = 1;	/* tape full */
		} else if (pr == NDMP9_MOVER_PAUSE_EOF) {
			me->media_eof = 1;
		} else if (pr == NDMP9_MOVER_PAUSE_MEDIA_ERROR) {
			me->media_io_error = 1;
		} else {
			/* what */
		}
	} else if (ms == NDMP9_MOVER_STATE_HALTED) {
		/* if tape_mode == READ, this may not actually be the window */
	/* TODO: should STATE_LISTEN be considered? */
	} else {
		ndmalogf (sess, 0, 1,
			"Warning: capturing offset w/o quiescent mover");
	}

	wlen = ca->mover_state.record_num;
	wlen *= job->record_size;
	wlen -= job->last_w_offset;	/* want the size of this image */

	me->valid_n_bytes = 1;
	me->nb_determined = 1;
	me->n_bytes = wlen;

	/*
	 * Only print the data when a deliver function was defined.
	 */
	if (ixlog->deliver) {
		ndmmedia_pp (me, 0, buf);
		ndmlogf (ixlog, "CM", 0, "%02d %s", ca->cur_media_ix, buf);
	}

	return 0;
}

int
ndmca_media_calculate_windows (struct ndm_session *sess)
{
	return 0;
}

int
ndmca_media_calculate_offsets (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	struct ndmmedia *	me;
	uint64_t		offset = 0;

	for (me = job->media_tab.head; me; me = me->next) {
		me->begin_offset = offset;
		if (me->valid_n_bytes) {
			offset += me->n_bytes;
			me->end_offset = offset;
		} else {
			me->n_bytes = NDMP_LENGTH_INFINITY;
			me->end_offset = NDMP_LENGTH_INFINITY;
			/* offset unchanged */
		}
	}

	return 0;
}

int
ndmca_media_set_window_current (struct ndm_session *sess)
{
	struct ndm_control_agent *ca = sess->control_acb;
	struct ndm_job_param *	job = &ca->job;
	struct ndmmedia *	me;
	int			rc;

	for (me = job->media_tab.head; me; me = me->next) {
		if (me->index == ca->cur_media_ix)
			break;
	}

	if (!me) {
		return -1;
	}

	rc = ndmca_mover_set_window (sess, me->begin_offset, me->n_bytes);
	if (rc == 0)
	    job->last_w_offset = me->begin_offset;
	return rc;
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
