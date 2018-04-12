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

#ifdef NDMOS_OPTION_TAPE_SIMULATOR

void
ndmos_tape_register_callbacks (struct ndm_session *sess,
  struct ndm_tape_simulator_callbacks *callbacks)
{
	/*
	 * Only allow one register.
	 */
	if (!sess->ntsc) {
		sess->ntsc = NDMOS_API_MALLOC (sizeof(struct ndm_tape_simulator_callbacks));
		memcpy(sess->ntsc, callbacks, sizeof(struct ndm_tape_simulator_callbacks));
	}
}

void
ndmos_tape_unregister_callbacks (struct ndm_session *sess)
{
	if (!sess->ntsc) {
		NDMOS_API_FREE (sess->ntsc);
		sess->ntsc = NULL;
	}
}

int
ndmos_tape_initialize (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ta->tape_fd = -1;
	NDMOS_MACRO_ZEROFILL (&ta->tape_state);
	ta->tape_state.error = NDMP9_DEV_NOT_OPEN_ERR;
	ta->tape_state.state = NDMP9_TAPE_STATE_IDLE;

	return 0;
}

ndmp9_error
ndmos_tape_open (struct ndm_session *sess, char *drive_name, int will_write)
{
	ndmp9_error		err;
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->tape_fd >= 0) {
		ndma_send_logmsg(sess, NDMP9_LOG_ERROR, sess->plumb.control,
			"device simulator is already open");
		return NDMP9_DEVICE_OPENED_ERR;
	}

	if (sess->ntsc && sess->ntsc->tape_open) {
		err = sess->ntsc->tape_open(sess, drive_name, will_write);
		if (err != NDMP9_NO_ERR)
			return err;
	}

	return NDMP9_NO_ERR;
}

ndmp9_error
ndmos_tape_close (struct ndm_session *sess)
{
	ndmp9_error		err;
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	if (sess->ntsc && sess->ntsc->tape_close) {
		err = sess->ntsc->tape_close(sess);
		if (err != NDMP9_NO_ERR)
			return err;
	}

	ndmos_tape_initialize (sess);

	return NDMP9_NO_ERR;
}

void
ndmos_tape_sync_state (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->tape_fd < 0) {
		ta->tape_state.error = NDMP9_DEV_NOT_OPEN_ERR;
		ta->tape_state.state = NDMP9_TAPE_STATE_IDLE;
		ta->tape_state.file_num.valid = NDMP9_VALIDITY_INVALID;
		ta->tape_state.soft_errors.valid = NDMP9_VALIDITY_INVALID;
		ta->tape_state.block_size.valid = NDMP9_VALIDITY_INVALID;
		ta->tape_state.blockno.valid = NDMP9_VALIDITY_INVALID;
	} else {
		ta->tape_state.error = NDMP9_NO_ERR;
		if (ta->mover_state.state == NDMP9_MOVER_STATE_ACTIVE)
			ta->tape_state.state = NDMP9_TAPE_STATE_MOVER;
		else
			ta->tape_state.state = NDMP9_TAPE_STATE_OPEN;
		ta->tape_state.file_num.valid = NDMP9_VALIDITY_VALID;
		ta->tape_state.soft_errors.valid = NDMP9_VALIDITY_VALID;
		ta->tape_state.block_size.valid = NDMP9_VALIDITY_VALID;
		ta->tape_state.blockno.valid = NDMP9_VALIDITY_VALID;
	}

	return;
}

ndmp9_error
ndmos_tape_mtio (struct ndm_session *sess,
  ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid)
{
	ndmp9_error		err;
	struct ndm_tape_agent *	ta = sess->tape_acb;

	*resid = 0;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	/* audit for valid op and for tape mode */
	switch (op) {
	case NDMP9_MTIO_FSF:
		break;
	case NDMP9_MTIO_BSF:
		break;
	case NDMP9_MTIO_FSR:
		break;
	case NDMP9_MTIO_BSR:
		break;
	case NDMP9_MTIO_REW:
		break;
	case NDMP9_MTIO_OFF:
		break;
	case NDMP9_MTIO_EOF:
		break;
	default:
		return NDMP9_ILLEGAL_ARGS_ERR;
	}

	if (sess->ntsc && sess->ntsc->tape_mtio) {
		err = sess->ntsc->tape_mtio(sess, op, count, resid);
		if (err != NDMP9_NO_ERR)
			return err;
	}

	return NDMP9_NO_ERR;
}

ndmp9_error
ndmos_tape_write (struct ndm_session *sess,
  char *buf, uint32_t count, uint32_t *done_count)
{
	ndmp9_error		err;
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
		return NDMP9_PERMISSION_ERR;
	}

	if (count == 0) {
		/*
		 * NDMPv4 clarification -- a tape read or write with
		 * a count==0 is a no-op. This is undoubtedly influenced
		 * by the SCSI Sequential Access specification which
		 * says much the same thing.
		 */
		*done_count = 0;
		return NDMP9_NO_ERR;
	}

	if (sess->ntsc && sess->ntsc->tape_write) {
		err = sess->ntsc->tape_write(sess, buf, count, done_count);
		if (err != NDMP9_NO_ERR)
			return err;
	}

	return NDMP9_NO_ERR;
}

ndmp9_error
ndmos_tape_wfm (struct ndm_session *sess)
{
	ndmp9_error		err;
	struct ndm_tape_agent *	ta = sess->tape_acb;

	ta->weof_on_close = 0;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
		return NDMP9_PERMISSION_ERR;
	}

	if (sess->ntsc && sess->ntsc->tape_wfm) {
		err = sess->ntsc->tape_wfm(sess);
		if (err != NDMP9_NO_ERR)
			return err;
	}

	return NDMP9_NO_ERR;
}

ndmp9_error
ndmos_tape_read (struct ndm_session *sess,
  char *buf, uint32_t count, uint32_t *done_count)
{
	ndmp9_error		err;
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	if (count == 0) {
		/*
		 * NDMPv4 clarification -- a tape read or write with
		 * a count==0 is a no-op. This is undoubtedly influenced
		 * by the SCSI Sequential Access specification which
		 * says much the same thing.
		 */

		*done_count = 0;
		return NDMP9_NO_ERR;
	}

	if (sess->ntsc && sess->ntsc->tape_read) {
		err = sess->ntsc->tape_read(sess, buf, count, done_count);
		if (err != NDMP9_NO_ERR)
			return err;
	}

	return NDMP9_NO_ERR;
}

#endif /* NDMOS_OPTION_TAPE_SIMULATOR */

#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
