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
 * Description: Tape/Robot and Authentication Simulation for testing purposes.
 * 		Implemented as callbacks from the generic framework.
 * 		Original code extracted from ndma_tape_simulator.c and
 * 		ndma_robot_simulator.c
 *
 */


#include "ndmjob.h"

#ifdef NDMOS_OPTION_TAPE_SIMULATOR
#ifdef NDMOS_OPTION_GAP_SIMULATOR
struct simu_gap {
	uint32_t	magic;
	uint32_t	rectype;
	uint32_t	prev_size;
	uint32_t	size;
};

#define SIMU_GAP_MAGIC		0x0BEEFEE0
#define SIMU_GAP_RT_(a,b,c,d) ((a<<0)+(b<<8)+(c<<16)+(d<<24))
#define SIMU_GAP_RT_BOT		SIMU_GAP_RT_('B','O','T','_')
#define SIMU_GAP_RT_DATA	SIMU_GAP_RT_('D','A','T','A')
#define SIMU_GAP_RT_FILE	SIMU_GAP_RT_('F','I','L','E')
#define SIMU_GAP_RT_EOT		SIMU_GAP_RT_('E','O','T','_')

/* send logical EOM with a bit less than 2 32k blocks left (due to SIMU_GAPs) */
#define TAPE_SIM_LOGICAL_EOM	32768*2

/* we sneak a peek at this global variable - probably not the best way, but
 * it works */
extern off_t o_tape_limit;

static int
simu_back_one (struct ndm_session *sess, int over_file_mark)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	struct simu_gap		gap;
	off_t			cur_pos;
	off_t			new_pos;
	int			rc;

	cur_pos = lseek (ta->tape_fd, (off_t)0, 1);

	rc = read (ta->tape_fd, &gap, sizeof gap);
	if (rc != sizeof gap || gap.magic != SIMU_GAP_MAGIC)
		goto bail_out;

	new_pos = cur_pos;
	new_pos -= sizeof gap + gap.prev_size;

	ta->sent_leom = 0;

	/*
	 * This is the new position. We need to update simu_prev_gap.
	 */

	lseek (ta->tape_fd, new_pos, 0);

	rc = read (ta->tape_fd, &gap, sizeof gap);
	if (rc != sizeof gap || gap.magic != SIMU_GAP_MAGIC)
		goto bail_out;

	switch (gap.rectype) {
	case SIMU_GAP_RT_BOT:
		/* can't actually back up to this, but update stuff */
		ta->tape_state.file_num.value = 0;
		ta->tape_state.blockno.value = 0;
		/* cur_pos is now just right */
		return 0;		/* can't back up */

	case SIMU_GAP_RT_EOT:
		/* this just isn't suppose to happen */
		goto bail_out;

	case SIMU_GAP_RT_DATA:
		/* this is always OK */
		if (ta->tape_state.blockno.value > 0)
			ta->tape_state.blockno.value--;
		lseek (ta->tape_fd, new_pos, 0);
		return SIMU_GAP_RT_DATA;

	case SIMU_GAP_RT_FILE:
		ta->tape_state.blockno.value = 0;
		if (!over_file_mark) {
			lseek (ta->tape_fd, cur_pos, 0);
			return 0;
		}
		if (ta->tape_state.file_num.value > 0)
			ta->tape_state.file_num.value--;
		lseek (ta->tape_fd, new_pos, 0);
		return SIMU_GAP_RT_FILE;

	default:
		/* this just isn't suppose to happen */
		goto bail_out;
	}

  bail_out:
	lseek (ta->tape_fd, cur_pos, 0);
	return -1;
}

static int
simu_forw_one (struct ndm_session *sess, int over_file_mark)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	struct simu_gap		gap;
	off_t			cur_pos;
	off_t			new_pos;
	int			rc;

	cur_pos = lseek (ta->tape_fd, (off_t)0, 1);

	rc = read (ta->tape_fd, &gap, sizeof gap);
	if (rc != sizeof gap || gap.magic != SIMU_GAP_MAGIC)
		goto bail_out;

	ta->sent_leom = 0;

	new_pos = cur_pos;
	new_pos += gap.size + sizeof gap;

	switch (gap.rectype) {
	case SIMU_GAP_RT_BOT:
		/* this just isn't suppose to happen */
		goto bail_out;

	case SIMU_GAP_RT_EOT:
		lseek (ta->tape_fd, cur_pos, 0);
		return 0;	/* can't go forward */

	case SIMU_GAP_RT_DATA:
		/* this is always OK */
		ta->tape_state.blockno.value++;
		lseek (ta->tape_fd, new_pos, 0);
		return SIMU_GAP_RT_DATA;

	case SIMU_GAP_RT_FILE:
		if (!over_file_mark) {
			lseek (ta->tape_fd, cur_pos, 0);
			return 0;
		}
		ta->tape_state.blockno.value = 0;
		ta->tape_state.file_num.value++;
		/* cur_pos is just right */
		return SIMU_GAP_RT_FILE;

	default:
		/* this just isn't suppose to happen */
		goto bail_out;
	}

  bail_out:
	lseek (ta->tape_fd, cur_pos, 0);
	return -1;
}

static int
simu_flush_weof (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->weof_on_close) {
		/* best effort */
		ndmos_tape_wfm (sess);
	}
	return 0;
}

static int
touch_tape_lockfile(char *drive_name)
{
	char 	lockfile_name[NDMOS_CONST_PATH_MAX];
	int	fd;

	snprintf (lockfile_name, sizeof(lockfile_name), "%s.lck", drive_name);
	if ((fd = open(lockfile_name, O_CREAT|O_EXCL, 0666)) < 0) {
		return -1;
	}

	close(fd);
	return 0;
}

static void
unlink_tape_lockfile(char *drive_name)
{
	char 	lockfile_name[NDMOS_CONST_PATH_MAX];

	snprintf (lockfile_name, sizeof(lockfile_name), "%s.lck", drive_name);
	unlink(lockfile_name);
}

static ndmp9_error
ndmjob_tape_open (struct ndm_session *sess,
                char *drive_name, int will_write)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	struct simu_gap		gap;
	struct stat		st;
	int			read_only, omode;
	int			rc, fd;
	char			pos_symlink_name[NDMOS_CONST_PATH_MAX];
	char			pos_buf[32];
	off_t			pos = -1;

	if (stat (drive_name, &st) < 0) {
		return NDMP9_NO_DEVICE_ERR;
	}

	read_only = (st.st_mode & 0222) == 0;

	if (!will_write) {
		omode = 0;
	} else {
		if (read_only)
			return NDMP9_WRITE_PROTECT_ERR;
		omode = 2;		/* ndmp_write means read/write */
	}

	if (touch_tape_lockfile(drive_name) < 0)
	    return NDMP9_DEVICE_BUSY_ERR;

	fd = open (drive_name, omode);
	if (fd < 0) {
		return NDMP9_PERMISSION_ERR;
	}

	snprintf (pos_symlink_name, sizeof(pos_symlink_name), "%s.pos", drive_name);
	if (st.st_size == 0) {
		remove (pos_symlink_name);
		if (will_write) {
			gap.magic = SIMU_GAP_MAGIC;
			gap.rectype = SIMU_GAP_RT_BOT;
			gap.size = 0;
			gap.prev_size = 0;
			if (write (fd, &gap, sizeof gap) < (int)sizeof gap) {
			    close(fd);
			    return NDMP9_IO_ERR;
			}

			gap.rectype = SIMU_GAP_RT_EOT;
			if (write (fd, &gap, sizeof gap) < (int)sizeof gap) {
			    close(fd);
			    return NDMP9_IO_ERR;
			}
			lseek (fd, (off_t)0, 0);
		} else {
			goto skip_header_check;
		}
	}

	rc = read (fd, &gap, sizeof gap);
	if (rc != sizeof gap) {
		close (fd);
		return NDMP9_NO_TAPE_LOADED_ERR;
	}

#if 1
	if (gap.magic != SIMU_GAP_MAGIC) {
		close (fd);
		return NDMP9_IO_ERR;
	}
#else
	if (gap.magic != SIMU_GAP_MAGIC
	 || gap.rectype != SIMU_GAP_RT_BOT
	 || gap.size != 0) {
		close (fd);
		return NDMP9_IO_ERR;
	}
#endif

	rc = readlink (pos_symlink_name, pos_buf, sizeof pos_buf);
	if (rc > 0) {
		pos_buf[rc] = 0;
		pos = strtol (pos_buf, 0, 0);
		lseek (fd, pos, 0);
		rc = read (fd, &gap, sizeof gap);
		if (rc == sizeof gap && gap.magic == SIMU_GAP_MAGIC) {
		} else {
			pos = sizeof gap;
		}
		lseek (fd, pos, 0);
	}

  skip_header_check:

	ta->tape_fd = fd;
	if (ta->drive_name) {
		NDMOS_API_FREE (ta->drive_name);
	}
	ta->drive_name = NDMOS_API_STRDUP (drive_name);
	bzero (&ta->tape_state, sizeof ta->tape_state);
	ta->tape_state.error = NDMP9_NO_ERR;
	ta->tape_state.state = NDMP9_TAPE_STATE_OPEN;
	ta->tape_state.open_mode =
		will_write ? NDMP9_TAPE_RDWR_MODE : NDMP9_TAPE_READ_MODE;
	ta->tape_state.file_num.valid = NDMP9_VALIDITY_VALID;
	ta->tape_state.soft_errors.valid = NDMP9_VALIDITY_VALID;
	ta->tape_state.block_size.valid = NDMP9_VALIDITY_VALID;
	ta->tape_state.blockno.valid = NDMP9_VALIDITY_VALID;
	ta->tape_state.total_space.valid = NDMP9_VALIDITY_INVALID;
	ta->tape_state.space_remain.valid = NDMP9_VALIDITY_INVALID;

	ta->sent_leom = 0;
	if (o_tape_limit) {
	    ta->tape_state.total_space.valid = NDMP9_VALIDITY_VALID;
	    ta->tape_state.total_space.value = o_tape_limit;
	    ta->tape_state.space_remain.valid = NDMP9_VALIDITY_VALID;
	    ta->tape_state.space_remain.value = o_tape_limit - st.st_size;
	}

	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_tape_close (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	off_t			cur_pos;

	/* TODO this is not called on an EOF from the DMA, so the lockfile
	 * will remain, although the spec says the tape service should be
	 * automatically closed */

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	simu_flush_weof(sess);

#if 0
	uint32_t		resid;
	ndmos_tape_mtio (sess, NDMP9_MTIO_REW, 1, &resid);
#endif

	cur_pos = lseek (ta->tape_fd, (off_t)0, 1);
	if (cur_pos != -1) {
		char		pos_symlink_name[NDMOS_CONST_PATH_MAX];
		char		pos_buf[32];

		snprintf (pos_symlink_name, sizeof(pos_symlink_name), "%s.pos", ta->drive_name);
		snprintf (pos_buf, sizeof(pos_buf), "%ld", (long) cur_pos);
		if (symlink (pos_buf, pos_symlink_name) < 0) {
		    ; /* ignore error during close */
		}
	}

	close (ta->tape_fd);
	ta->tape_fd = -1;

	unlink_tape_lockfile(ta->drive_name);

	ndmos_tape_initialize (sess);

	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_tape_mtio (struct ndm_session *sess,
                ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	int			rc;

	*resid = count;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	/* audit for valid op and for tape mode */
	switch (op) {
	case NDMP9_MTIO_FSF:
		while (*resid > 0) {
			simu_flush_weof(sess);
			rc = simu_forw_one (sess, 1);
			if (rc < 0)
				return NDMP9_IO_ERR;
			if (rc == 0)
				break;
			if (rc == SIMU_GAP_RT_FILE)
				*resid -= 1;
		}
		break;

	case NDMP9_MTIO_BSF:
		while (*resid > 0) {
			simu_flush_weof(sess);
			rc = simu_back_one (sess, 1);
			if (rc < 0)
				return NDMP9_IO_ERR;
			if (rc == 0)
				break;
			if (rc == SIMU_GAP_RT_FILE)
				*resid -= 1;
		}
		break;

	case NDMP9_MTIO_FSR:
		while (*resid > 0) {
			simu_flush_weof(sess);
			rc = simu_forw_one (sess, 0);
			if (rc < 0)
				return NDMP9_IO_ERR;
			if (rc == 0)
				break;
			*resid -= 1;
		}
		break;

	case NDMP9_MTIO_BSR:
		while (*resid > 0) {
			simu_flush_weof(sess);
			rc = simu_back_one (sess, 0);
			if (rc < 0)
				return NDMP9_IO_ERR;
			if (rc == 0)
				break;
			*resid -= 1;
		}
		break;

	case NDMP9_MTIO_REW:
		simu_flush_weof(sess);
		*resid = 0;
		ta->tape_state.file_num.value = 0;
		ta->tape_state.blockno.value = 0;
		lseek (ta->tape_fd, (off_t)(sizeof (struct simu_gap)), 0);
		break;

	case NDMP9_MTIO_OFF:
		simu_flush_weof(sess);
		/* Hmmm. */
		break;

	case NDMP9_MTIO_EOF:		/* should be "WFM" write-file-mark */
		if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
			return NDMP9_PERMISSION_ERR;
		}
		while (*resid > 0) {
			ndmp9_error	err;

			err = ndmos_tape_wfm (sess);
			if (err != NDMP9_NO_ERR)
				return err;

			*resid -= 1;
		}
		break;

	default:
		return NDMP9_ILLEGAL_ARGS_ERR;
	}

	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_tape_write (struct ndm_session *sess,
                char *buf, uint32_t count, uint32_t *done_count)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	int			rc;
	struct simu_gap		gap;
	off_t			cur_pos;
	ndmp9_error		err;
	uint32_t		prev_size;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
		return NDMP9_PERMISSION_ERR;
	}

	cur_pos = lseek (ta->tape_fd, (off_t)0, 1);

	if (o_tape_limit) {
	    /* if cur_pos is past LEOM, but we haven't sent NDMP9_EOM_ERR yet,
	     * then do so now */
	    if (!ta->sent_leom && cur_pos > o_tape_limit - TAPE_SIM_LOGICAL_EOM) {
		ta->sent_leom = 1;
		return NDMP9_EOM_ERR;
	    }

	    /* if this write will put us over PEOM, then send NDMP9_IO_ERR */
	    if ((off_t)(cur_pos + sizeof gap + count) > o_tape_limit) {
		return NDMP9_IO_ERR;
	    }
	}

	rc = read (ta->tape_fd, &gap, sizeof gap);
	if (rc != sizeof gap
	 || gap.magic != SIMU_GAP_MAGIC) {
		lseek (ta->tape_fd, cur_pos, 0);
		return NDMP9_IO_ERR;
	}

	prev_size = gap.prev_size;

	gap.magic = SIMU_GAP_MAGIC;
	gap.rectype = SIMU_GAP_RT_DATA;
	gap.prev_size = prev_size;
	gap.size = count;

	lseek (ta->tape_fd, cur_pos, 0);

	if (write (ta->tape_fd, &gap, sizeof gap) == sizeof gap
	 && (uint32_t)write (ta->tape_fd, buf, count) == count) {
		cur_pos += count + sizeof gap;

		prev_size = count;

		ta->tape_state.blockno.value++;

		*done_count = count;

		err = NDMP9_NO_ERR;
	} else {
		err = NDMP9_IO_ERR;
	}


	if (ftruncate (ta->tape_fd, cur_pos) < 0)
	    return NDMP9_IO_ERR;

	lseek (ta->tape_fd, cur_pos, 0);

	gap.rectype = SIMU_GAP_RT_EOT;
	gap.size = 0;
	gap.prev_size = prev_size;

	if (write (ta->tape_fd, &gap, sizeof gap) < (int)sizeof gap)
	    return NDMP9_IO_ERR;
	lseek (ta->tape_fd, cur_pos, 0);

	if (o_tape_limit) {
	    ta->tape_state.space_remain.value = o_tape_limit - cur_pos;
	}

	ta->weof_on_close = 1;

	return err;
}

static ndmp9_error
ndmjob_tape_wfm (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	int			rc;
	struct simu_gap		gap;
	off_t			cur_pos;
	ndmp9_error		err;
	uint32_t		prev_size;

	ta->weof_on_close = 0;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
		return NDMP9_PERMISSION_ERR;
	}

	cur_pos = lseek (ta->tape_fd, (off_t)0, 1);

	if (o_tape_limit) {
	    /* note: filemarks *never* trigger NDMP9_EOM_ERR */

	    /* if this write will put us over PEOM, then send NDMP9_IO_ERR */
	    if ((off_t)(cur_pos + sizeof gap) > o_tape_limit) {
		return NDMP9_IO_ERR;
	    }
	}

	rc = read (ta->tape_fd, &gap, sizeof gap);
	if (rc != sizeof gap
	 || gap.magic != SIMU_GAP_MAGIC) {
		lseek (ta->tape_fd, cur_pos, 0);
		return NDMP9_IO_ERR;
	}

	prev_size = gap.prev_size;

	gap.magic = SIMU_GAP_MAGIC;
	gap.rectype = SIMU_GAP_RT_FILE;
	gap.prev_size = prev_size;
	gap.size = 0;

	lseek (ta->tape_fd, cur_pos, 0);

	if (write (ta->tape_fd, &gap, sizeof gap) == sizeof gap) {

		cur_pos += sizeof gap;

		prev_size = 0;

		ta->tape_state.file_num.value++;
		ta->tape_state.blockno.value = 0;

		err = NDMP9_NO_ERR;
	} else {
		err = NDMP9_IO_ERR;
	}

	if (ftruncate (ta->tape_fd, cur_pos) < 0)
	    return NDMP9_IO_ERR;
	lseek (ta->tape_fd, cur_pos, 0);

	gap.rectype = SIMU_GAP_RT_EOT;
	gap.size = 0;
	gap.prev_size = prev_size;

	if (write (ta->tape_fd, &gap, sizeof gap) < (int)sizeof gap)
		return NDMP9_IO_ERR;
	lseek (ta->tape_fd, cur_pos, 0);

	if (o_tape_limit) {
	    ta->tape_state.space_remain.value = o_tape_limit - cur_pos;
	}

	return err;
}

static ndmp9_error
ndmjob_tape_read (struct ndm_session *sess,
                char *buf, uint32_t count, uint32_t *done_count)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	int			rc;
	struct simu_gap		gap;
	off_t			cur_pos;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	cur_pos = lseek (ta->tape_fd, (off_t)0, 1);

	rc = read (ta->tape_fd, &gap, sizeof gap);
	if (rc != sizeof gap
	 || gap.magic != SIMU_GAP_MAGIC) {
		lseek (ta->tape_fd, cur_pos, 0);
		return NDMP9_IO_ERR;
	}

	if (gap.rectype == SIMU_GAP_RT_DATA) {
		unsigned	nb;

		nb = count;
		if (nb > gap.size)
			nb = gap.size;

		rc = read (ta->tape_fd, buf, nb);
		if (rc != (int)nb) {
			lseek (ta->tape_fd, cur_pos, 0);
			return NDMP9_IO_ERR;
		}

		if (gap.size != nb) {
			cur_pos += sizeof gap + gap.size;
			lseek (ta->tape_fd, cur_pos, 0);
		}

		ta->tape_state.blockno.value++;

		*done_count = nb;
	} else {
		/* all other record types are interpretted as EOF */
		lseek (ta->tape_fd, cur_pos, 0);
		*done_count = 0;
		return NDMP9_EOF_ERR;
	}
	return NDMP9_NO_ERR;
}
#else
static int
simu_flush_weof (struct ndm_session *sess)
{
	struct ndm_tape_agent * ta = sess->tape_acb;

	if (ta->weof_on_close) {
		/* best effort */
		ndmos_tape_wfm (sess);
	}
	return 0;
}

static ndmp9_error
ndmjob_tape_open (struct ndm_session *sess,
                char *drive_name, int will_write)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	struct stat		st;
	int			read_only, omode;
	int			fd;

	if (stat (drive_name, &st) < 0) {
		return NDMP9_NO_DEVICE_ERR;
	}

	read_only = (st.st_mode & 0222) == 0;

	if (!will_write) {
		omode = 0;
	} else {
		if (read_only)
			return NDMP9_WRITE_PROTECT_ERR;
		omode = 2;		/* ndmp_write means read/write */
	}

	fd = open (drive_name, omode);
	if (fd < 0) {
		return NDMP9_PERMISSION_ERR;
	}

	if (st.st_size == 0) {
		if (will_write) {
			lseek (fd, (off_t)0, 0);
		} else {
			goto skip_header_check;
		}
	}

  skip_header_check:
	ta->tape_fd = fd;
	if (ta->drive_name) {
		NDMOS_API_FREE (ta->drive_name);
	}
	ta->drive_name = NDMOS_API_STRDUP (drive_name);
	bzero (&ta->tape_state, sizeof ta->tape_state);
	ta->tape_state.error = NDMP9_NO_ERR;
	ta->tape_state.state = NDMP9_TAPE_STATE_OPEN;
	ta->tape_state.open_mode =
		will_write ? NDMP9_TAPE_RDWR_MODE : NDMP9_TAPE_READ_MODE;
	ta->tape_state.file_num.valid = NDMP9_VALIDITY_VALID;
	ta->tape_state.soft_errors.valid = NDMP9_VALIDITY_VALID;
	ta->tape_state.block_size.valid = NDMP9_VALIDITY_VALID;
	ta->tape_state.blockno.valid = NDMP9_VALIDITY_VALID;
	ta->tape_state.total_space.valid = NDMP9_VALIDITY_INVALID;
	ta->tape_state.space_remain.valid = NDMP9_VALIDITY_INVALID;

	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_tape_close (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	simu_flush_weof(sess);

#if 0
	uint32_t		resid;
	ndmos_tape_mtio (sess, NDMP9_MTIO_REW, 1, &resid);
#endif

	close (ta->tape_fd);
	ta->tape_fd = -1;

	ndmos_tape_initialize (sess);

	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_tape_mtio (struct ndm_session *sess,
                ndmp9_tape_mtio_op op, uint32_t count, uint32_t *resid)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	int			rc;

	*resid = 0;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}


	/* audit for valid op and for tape mode */
	switch (op) {
	case NDMP9_MTIO_FSF:
		return NDMP9_NO_ERR;

	case NDMP9_MTIO_BSF:
		return NDMP9_NO_ERR;

	case NDMP9_MTIO_FSR:
		return NDMP9_NO_ERR;

	case NDMP9_MTIO_BSR:
		return NDMP9_NO_ERR;

	case NDMP9_MTIO_REW:
		simu_flush_weof(sess);
		*resid = 0;
		ta->tape_state.file_num.value = 0;
		ta->tape_state.blockno.value = 0;
		lseek (ta->tape_fd, (off_t)0, 0);
		ndmalogf(sess, 0, 7, "NDMP9_MTIO_REW");
		break;

	case NDMP9_MTIO_OFF:
		return NDMP9_NO_ERR;

	case NDMP9_MTIO_EOF:		/* should be "WFM" write-file-mark */
		return NDMP9_NO_ERR;

	default:
		return NDMP9_ILLEGAL_ARGS_ERR;
	}

	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_tape_write (struct ndm_session *sess,
                char *buf, uint32_t count, uint32_t *done_count)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	off_t			cur_pos;
	ndmp9_error		err;
	uint32_t		prev_size;
	int			rc;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
		return NDMP9_PERMISSION_ERR;
	}

	cur_pos = lseek (ta->tape_fd, (off_t)0, 1);
	lseek (ta->tape_fd, cur_pos, 0);

	if ((uint32_t)write (ta->tape_fd, buf, count) == count) {
		cur_pos += count;

		prev_size = count;

		ta->tape_state.blockno.value++;

		*done_count = count;

		err = NDMP9_NO_ERR;
	} else {
		ndmalogf(sess, 0, 7, "write not %d", count);
		err = NDMP9_IO_ERR;
	}

	/* error ignored for pipe file descriptor */
	rc = ftruncate (ta->tape_fd, cur_pos);

	lseek (ta->tape_fd, cur_pos, 0);

	ta->weof_on_close = 1;

	return err;
}

static ndmp9_error
ndmjob_tape_wfm (struct ndm_session *sess)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	off_t			cur_pos;
	ndmp9_error		err;
	int			rc;

	ta->weof_on_close = 0;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	if (!NDMTA_TAPE_IS_WRITABLE(ta)) {
		return NDMP9_PERMISSION_ERR;
	}

	cur_pos = lseek (ta->tape_fd, (off_t)0, 1);

	lseek (ta->tape_fd, cur_pos, 0);
	err = NDMP9_NO_ERR;

	/* error ignored for pipe file descriptor */
	rc = ftruncate (ta->tape_fd, cur_pos);

	lseek (ta->tape_fd, cur_pos, 0);

	return err;
}

static ndmp9_error
ndmjob_tape_read (struct ndm_session *sess,
                char *buf, uint32_t count, uint32_t *done_count)
{
	struct ndm_tape_agent *	ta = sess->tape_acb;
	size_t rc, nb;

	if (ta->tape_fd < 0) {
		return NDMP9_DEV_NOT_OPEN_ERR;
	}

	nb = count;

	rc = read(ta->tape_fd, buf, nb);
	if (rc < 0) {
		return NDMP9_IO_ERR;
	}
	ta->tape_state.blockno.value++;

	*done_count = rc;

	if (rc == 0) {
		return NDMP9_EOF_ERR;
	}
	return NDMP9_NO_ERR;
}
#endif /* NDMOS_OPTION_GAP_SIMULATOR */
#endif /* NDMOS_OPTION_TAPE_SIMULATOR */

#ifdef NDMOS_OPTION_ROBOT_SIMULATOR

#include "scsiconst.h"

/*
 * Robot state management
 ****************************************************************
 */

/* xxx_FIRST must be in order! */
#define IE_FIRST 0
#define IE_COUNT 2
#define MTE_FIRST 16
#define MTE_COUNT 1
#define DTE_FIRST 128
#define DTE_COUNT 2
#define STORAGE_FIRST 1024
#define STORAGE_COUNT 10

#if (IE_FIRST+IE_COUNT > MTE_FIRST) \
 || (MTE_FIRST+MTE_COUNT > DTE_FIRST) \
 || (DTE_FIRST+MTE_COUNT > STORAGE_FIRST)
#error element addresses overlap or are in the wrong order
#endif

#define IS_IE_ADDR(a) ((a) >= IE_FIRST && (a) < IE_FIRST+IE_COUNT)
#define IS_MTE_ADDR(a) ((a) >= MTE_FIRST && (a) < MTE_FIRST+MTE_COUNT)
#define IS_DTE_ADDR(a) ((a) >= DTE_FIRST && (a) < DTE_FIRST+DTE_COUNT)
#define IS_STORAGE_ADDR(a) ((a) >= STORAGE_FIRST && (a) < STORAGE_FIRST+STORAGE_COUNT)

struct element_state {
	int full;
	int medium_type;
	int source_element;
	char pvoltag[32];
	char avoltag[32];
};

struct robot_state {
	struct element_state mte[MTE_COUNT];
	struct element_state storage[STORAGE_COUNT];
	struct element_state ie[IE_COUNT];
	struct element_state dte[DTE_COUNT];
};

static void
robot_state_init(struct robot_state *rs)
{
	int i;

	/* invent some nice data, with some nice voltags and whatnot */

	NDMOS_API_BZERO(rs, sizeof(*rs));

	/* (nothing to do for MTEs) */

	for (i = 0; i < STORAGE_COUNT; i++) {
		struct element_state *es = &rs->storage[i];
		es->full = 1;

		es->medium_type = 1; /* data */
		es->source_element = 0;
		snprintf(es->pvoltag, sizeof(es->pvoltag), "PTAG%02XXX                        ", i);
		snprintf(es->avoltag, sizeof(es->avoltag), "ATAG%02XXX                        ", i);
	}

	/* (i/e are all empty) */

	/* (dte's are all empty) */
}

static void
robot_state_load(struct ndm_session *sess, struct robot_state *rs)
{
	int fd;
	char filename[NDMOS_CONST_PATH_MAX];

	/* N.B. writing a struct to disk like this isn't portable, but this
	 * is test code, so it's OK for now. */

	snprintf(filename, sizeof filename, "%s/state", sess->robot_acb->sim_dir);
	fd = open(filename, O_RDONLY, 0666);
	if (fd < 0) {
		robot_state_init(rs);
		return;
	}
	if (read(fd, (void *)rs, sizeof(*rs)) < sizeof(*rs)) {
		robot_state_init(rs);
		close(fd);
		return;
	}

	close(fd);
}

static int
robot_state_save(struct ndm_session *sess, struct robot_state *rs)
{
	int fd;
	char filename[NDMOS_CONST_PATH_MAX];

	/* N.B. writing a struct to disk like this isn't portable, but this
	 * is test code, so it's OK for now. */

	snprintf(filename, sizeof filename, "%s/state", sess->robot_acb->sim_dir);
	fd = open(filename, O_WRONLY|O_TRUNC|O_CREAT, 0666);
	if (fd < 0)
		return -1;
	if (write(fd, (void *)rs, sizeof(*rs)) < sizeof(*rs)) {
		close(fd);
		return -1;
	}
	close(fd);

	return 0;
}

static int
robot_state_move(struct ndm_session *sess, struct robot_state *rs, int src, int dest)
{
	char src_filename[NDMOS_CONST_PATH_MAX];
	struct element_state *src_elt;
	char dest_filename[NDMOS_CONST_PATH_MAX];
	struct element_state *dest_elt;
	struct stat st;
	char pos[NDMOS_CONST_PATH_MAX];

	/* TODO: audit that the tape device is not using this volume right now */

	ndmalogf(sess, 0, 3, "moving medium from %d to %d", src, dest);

	if (IS_IE_ADDR(src)) {
		src_elt = &rs->ie[src - IE_FIRST];
		snprintf(src_filename, sizeof(src_filename), "%s/ie%d",
		    sess->robot_acb->sim_dir, src - IE_FIRST);
	} else if (IS_DTE_ADDR(src)) {
		src_elt = &rs->dte[src - DTE_FIRST];
		snprintf(src_filename, sizeof(src_filename), "%s/drive%d",
		    sess->robot_acb->sim_dir, src - DTE_FIRST);
	} else if (IS_STORAGE_ADDR(src)) {
		src_elt = &rs->storage[src - STORAGE_FIRST];
		snprintf(src_filename, sizeof(src_filename), "%s/slot%d",
		    sess->robot_acb->sim_dir, src - STORAGE_FIRST);
	} else {
		ndmalogf(sess, 0, 3, "invalid src address %d", src);
		return -1;
	}

	if (IS_IE_ADDR(dest)) {
		dest_elt = &rs->ie[dest - IE_FIRST];
		snprintf(dest_filename, sizeof(dest_filename), "%s/ie%d",
		    sess->robot_acb->sim_dir, dest - IE_FIRST);
	} else if (IS_DTE_ADDR(dest)) {
		dest_elt = &rs->dte[dest - DTE_FIRST];
		snprintf(dest_filename, sizeof(dest_filename), "%s/drive%d",
		    sess->robot_acb->sim_dir, dest - DTE_FIRST);
	} else if (IS_STORAGE_ADDR(dest)) {
		dest_elt = &rs->storage[dest - STORAGE_FIRST];
		snprintf(dest_filename, sizeof(dest_filename), "%s/slot%d",
		    sess->robot_acb->sim_dir, dest - STORAGE_FIRST);
	} else {
		ndmalogf(sess, 0, 3, "invalid dst address %d", src);
		return -1;
	}

	if (!src_elt->full) {
		ndmalogf(sess, 0, 3, "src not full");
		return -1;
	}

	if (dest_elt->full) {
		ndmalogf(sess, 0, 3, "dest full");
		return -1;
	}

	/* OK, enough checking, let's do it */
	/* delete the destination, if it exists */
	if (stat (dest_filename, &st) >= 0) {
		ndmalogf(sess, 0, 3, "unlink %s", dest_filename);
		if (unlink(dest_filename) < 0) {
			ndmalogf(sess, 0, 0, "error unlinking: %s", strerror(errno));
			return -1;
		}
	}

	/* and move the source if it exists */
	if (stat (src_filename, &st) >= 0) {
		ndmalogf(sess, 0, 3, "move %s to %s", src_filename, dest_filename);
		if (rename(src_filename, dest_filename) < 0) {
			ndmalogf(sess, 0, 0, "error renaming: %s", strerror(errno));
			return -1;
		}
	} else {
		/* otherwise touch the destination file */
		ndmalogf(sess, 0, 3, "touch %s", dest_filename);
		int fd = open(dest_filename, O_CREAT | O_WRONLY, 0666);
		if (fd < 0) {
			ndmalogf(sess, 0, 0, "error touching: %s", strerror(errno));
			return -1;
		}
		close(fd);
	}

	/* blow away any tape-drive .pos files */
	snprintf(pos, sizeof(pos), "%s.pos", src_filename);
	unlink(pos); /* ignore errors */
	snprintf(pos, sizeof(pos), "%s.pos", dest_filename);
	unlink(pos); /* ignore errors */

	/* update state */
	*dest_elt = *src_elt;
	ndmalogf(sess, 0, 3, "setting dest's source_element to %d", src);
	dest_elt->source_element = src;
	src_elt->full = 0;


	ndmalogf(sess, 0, 3, "move successful");
	return 0;
}

/*
 * SCSI commands
 ****************************************************************
 */

/*
 * Utilities
 */

static ndmp9_error
scsi_fail_with_sense_code(struct ndm_session *sess,
	    ndmp9_execute_cdb_reply *reply,
	    int status, int sense_key, int asq)
{
	unsigned char ext_sense[] = {
		0x72, /* current errors */
		sense_key & SCSI_SENSE_SENSE_KEY_MASK,
		(asq >> 8) & 0xff,
		(asq     ) & 0xff,
		0,
		0,
		0,
		0 };

	ndmalogf(sess, 0, 3, "sending failure; status=0x%02x sense_key=0x%02x asq=0x%04x",
		    status, sense_key, asq);

	reply->status = status;
	reply->ext_sense.ext_sense_len = sizeof(ext_sense);
	reply->ext_sense.ext_sense_val = NDMOS_API_MALLOC(sizeof(ext_sense));
	if (!reply->ext_sense.ext_sense_val)
		return NDMP9_NO_MEM_ERR;
	NDMOS_API_BCOPY(ext_sense, reply->ext_sense.ext_sense_val, sizeof(ext_sense));

	return NDMP9_NO_ERR;
}

/*
 * Command implementations
 */

static ndmp9_error
execute_cdb_test_unit_ready (struct ndm_session *sess,
  ndmp9_execute_cdb_request *request,
  ndmp9_execute_cdb_reply *reply)
{
	if (request->cdb.cdb_len != 6)
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_FIELD_IN_CDB);

	/* yep, we're ready! */

	return NDMP9_NO_ERR;
}

static ndmp9_error
execute_cdb_inquiry (struct ndm_session *sess,
  ndmp9_execute_cdb_request *request,
  ndmp9_execute_cdb_reply *reply)
{
	unsigned char *cdb = (unsigned char *)request->cdb.cdb_val;
	char *response;
	int response_len;
	char *p;

	/* N.B.: only page code 0 is supported */
	if (request->cdb.cdb_len != 6
	    || request->data_dir != NDMP9_SCSI_DATA_DIR_IN
	    || cdb[1] & 0x01
	    || cdb[2] != 0
	    || request->datain_len < 96
	    || ((cdb[3] << 8) + cdb[4]) < 96)
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_FIELD_IN_CDB);

	response_len = 96;
	p = response = NDMOS_API_MALLOC(response_len);
	if (!response)
		return NDMP9_NO_MEM_ERR;
	NDMOS_API_BZERO(response, response_len);
	*(p++) = 0x08;  /* media changer */
	*(p++) = 0;	/* RMB=0 */
	*(p++) = 6;	/* VERSION=SPC-4 */
	*(p++) = 2;	/* !NORMACA, !HISUP, RESPONSE DATA FORMAT = 2 */
	*(p++) = 92;	/* remaining bytes */
	*(p++) = 0;	/* lots of flags, all 0 */
	*(p++) = 0;	/* lots of flags, all 0 */
	*(p++) = 0;	/* lots of flags, all 0 */
	NDMOS_API_BCOPY("NDMJOB  ", p, 8); p += 8;
	NDMOS_API_BCOPY("FakeRobot        ", p, 16); p += 16;
	NDMOS_API_BCOPY("1.0 ", p, 4); p += 4;
	/* remainder is zero */

	reply->datain.datain_len = response_len;
	reply->datain.datain_val = response;

	return NDMP9_NO_ERR;
}

static ndmp9_error
execute_cdb_mode_sense_6 (struct ndm_session *sess,
  ndmp9_execute_cdb_request *request,
  ndmp9_execute_cdb_reply *reply)
{
	unsigned char *cdb = (unsigned char *)request->cdb.cdb_val;
	int page, subpage;
	char *response;
	int response_len;
	char *p;

	if (request->cdb.cdb_len != 6
	    || request->data_dir != NDMP9_SCSI_DATA_DIR_IN)
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_FIELD_IN_CDB);
	page = cdb[2] & 0x3f;
	subpage = cdb[3];

	switch ((page << 8) + subpage) {
	case 0x1D00: /* Element Address Assignment */
		if (request->datain_len < 20 || cdb[4] < 20)
			return scsi_fail_with_sense_code(sess, reply,
			    SCSI_STATUS_CHECK_CONDITION,
			    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
			    ASQ_INVALID_FIELD_IN_CDB);

		response_len = 24;
		p = response = NDMOS_API_MALLOC(response_len);
		if (!response)
			return NDMP9_NO_MEM_ERR;
		NDMOS_API_BZERO(response, response_len);
		*(p++) = response_len;
		*(p++) = 0; /* reserved medium type */
		*(p++) = 0; /* reserved device-specific parameter */
		*(p++) = 0; /* block descriptor length (DBD = 0 above)*/
		*(p++) = 0x1D; /* page code */
		*(p++) = 18; /* remaining bytes */
		*(p++) = (MTE_FIRST >> 8) & 0xff;
		*(p++) = MTE_FIRST & 0xff;
		*(p++) = (MTE_COUNT >> 8) & 0xff;
		*(p++) = MTE_COUNT & 0xff;
		*(p++) = (STORAGE_FIRST >> 8) & 0xff;
		*(p++) = STORAGE_FIRST & 0xff;
		*(p++) = (STORAGE_COUNT >> 8) & 0xff;
		*(p++) = STORAGE_COUNT & 0xff;
		*(p++) = (IE_FIRST >> 8) & 0xff;
		*(p++) = IE_FIRST & 0xff;
		*(p++) = (IE_COUNT >> 8) & 0xff;
		*(p++) = IE_COUNT & 0xff;
		*(p++) = (DTE_FIRST >> 8) & 0xff;
		*(p++) = DTE_FIRST & 0xff;
		*(p++) = (DTE_COUNT >> 8) & 0xff;
		*(p++) = DTE_COUNT & 0xff;
		/* remainder is zero */
		break;

	default:
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_FIELD_IN_CDB);
	}

	reply->datain.datain_len = response_len;
	reply->datain.datain_val = response;

	return NDMP9_NO_ERR;
}

static ndmp9_error
execute_cdb_read_element_status (struct ndm_session *sess,
  ndmp9_execute_cdb_request *request,
  ndmp9_execute_cdb_reply *reply)
{
	unsigned char *cdb = (unsigned char *)request->cdb.cdb_val;
	struct robot_state rs;
	int min_addr, max_elts;
	char *response;
	int response_len;
	int required_len;
	int num_elts = IE_COUNT + MTE_COUNT + DTE_COUNT + STORAGE_COUNT;
	char *p;

	if (request->cdb.cdb_len != 12
	    || request->data_dir != NDMP9_SCSI_DATA_DIR_IN)
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_FIELD_IN_CDB);
	min_addr = (cdb[2] << 8) + cdb[3];
	max_elts = (cdb[4] << 8) + cdb[5];
	response_len = (cdb[7] << 16) + (cdb[8] << 8) + cdb[9];

	if (response_len < 8) {
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_FIELD_IN_CDB);
	}

	/* this is bogus, but we don't allow "partial" status requests */
	if (min_addr > IE_FIRST || max_elts < num_elts) {
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_FIELD_IN_CDB);
	}

	robot_state_load(sess, &rs);
	robot_state_save(sess, &rs);

	/* calculate the total space required */
	required_len = 8; /* element status data header */
	if (MTE_COUNT) {
		required_len += 8; /* element status page header */
		required_len += 12 * MTE_COUNT; /* element status descriptor w/o tags */
	}
	if (STORAGE_COUNT) {
		required_len += 8; /* element status page header */
		required_len += 84 * STORAGE_COUNT; /* element status descriptor w/ tags */
	}
	if (IE_COUNT) {
		required_len += 8; /* element status page header */
		required_len += 84 * IE_COUNT; /* element status descriptor w/ tags */
	}
	if (DTE_COUNT) {
		required_len += 8; /* element status page header */
		required_len += 84 * DTE_COUNT; /* element status descriptor w/ tags */
	}

	p = response = NDMOS_API_MALLOC(response_len);
	if (!response)
		return NDMP9_NO_MEM_ERR;
	NDMOS_API_BZERO(response, response_len);

	/* write the element status data header */
	*(p++) = IE_FIRST >> 8; /* first element address */
	*(p++) = IE_FIRST & 0xff;
	*(p++) = num_elts >> 8; /* number of elements */
	*(p++) = num_elts & 0xff;
	*(p++) = 0; /* reserved */
	*(p++) = (required_len-8) >> 16; /* remaining byte count of report */
	*(p++) = ((required_len-8) >> 8) & 0xff;
	*(p++) = (required_len-8) & 0xff;

	/* only fill in the rest if we have space */
	if (required_len <= response_len) {
		int i;
		struct {
			int first, count, have_voltags, eltype;
			int empty_flags, full_flags;
			struct element_state *es;
		} page[4] = {
			{ IE_FIRST, IE_COUNT, 1, 3, 0x38, 0x39, &rs.ie[0] },
			{ MTE_FIRST, MTE_COUNT, 0, 1, 0x00, 0x01, &rs.mte[0] },
			{ DTE_FIRST, DTE_COUNT, 1, 4, 0x08, 0x81, &rs.dte[0] },
			{ STORAGE_FIRST, STORAGE_COUNT, 1, 2, 0x08, 0x09, &rs.storage[0] },
		};

		for (i = 0; i < 4; i++) {
			int descr_size = page[i].have_voltags? 84 : 12;
			int totalsize = descr_size * page[i].count;
			int j;

			if (page[i].count == 0)
				continue;

			/* write the page header */
			*(p++) = page[i].eltype;
			*(p++) = page[i].have_voltags? 0xc0 : 0;
			*(p++) = 0;
			*(p++) = descr_size;
			*(p++) = 0; /* reserved */
			*(p++) = totalsize >> 16;
			*(p++) = (totalsize >> 8) & 0xff;
			*(p++) = totalsize & 0xff;

			/* and write each descriptor */
			for (j = 0; j < page[i].count; j++) {
				int elt_addr = page[i].first + j;
				int src_elt = page[i].es[j].source_element;
				unsigned char byte9 = page[i].es[j].medium_type;
				if (src_elt!= 0)
					byte9 |= 0x80; /* SVALID */

				*(p++) = elt_addr >> 8;
				*(p++) = elt_addr & 0xff;
				*(p++) = page[i].es[j].full?
					    page[i].full_flags : page[i].empty_flags;
				*(p++) = 0;
				*(p++) = 0;
				*(p++) = 0;
				*(p++) = 0;
				*(p++) = 0;
				*(p++) = 0;
				*(p++) = byte9;
				*(p++) = src_elt >> 8;
				*(p++) = src_elt & 0xff;

				if (page[i].have_voltags) {
					int k;
					if (page[i].es[j].full) {
						for (k = 0; k < 32; k++) {
							if (!page[i].es[j].pvoltag[k])
								break;
							p[k] = page[i].es[j].pvoltag[k];
						}
						for (k = 0; k < 32; k++) {
							if (!page[i].es[j].avoltag[k])
								break;
							p[k+36] = page[i].es[j].avoltag[k];
						}
					} else {
						for (k = 0; k < 32; k++) {
							p[k] = p[k+36] = ' ';
						}
					}
					p += 72;
				}
			}
		}
	}

	reply->datain.datain_len = response_len;
	reply->datain.datain_val = response;

	return NDMP9_NO_ERR;
}

static ndmp9_error
execute_cdb_move_medium (struct ndm_session *sess,
  ndmp9_execute_cdb_request *request,
  ndmp9_execute_cdb_reply *reply)
{
	unsigned char *cdb = (unsigned char *)request->cdb.cdb_val;
	struct robot_state rs;
	int mte, src, dest;

	if (request->cdb.cdb_len != 12)
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_FIELD_IN_CDB);
	mte = (cdb[2] << 8) + cdb[3];
	src = (cdb[4] << 8) + cdb[5];
	dest = (cdb[6] << 8) + cdb[7];

	if (!IS_MTE_ADDR(mte))
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_ELEMENT_ADDRESS);

	robot_state_load(sess, &rs);
	if (robot_state_move(sess, &rs, src, dest) < 0)
		return scsi_fail_with_sense_code(sess, reply,
		    SCSI_STATUS_CHECK_CONDITION,
		    SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		    ASQ_INVALID_ELEMENT_ADDRESS);
	robot_state_save(sess, &rs);

	return NDMP9_NO_ERR;
}

static struct {
	unsigned char cdb_byte;
	ndmp9_error (* execute_cdb)(
		  struct ndm_session *sess,
		  ndmp9_execute_cdb_request *request,
		  ndmp9_execute_cdb_reply *reply);
} cdb_executors[] = {
	{ SCSI_CMD_TEST_UNIT_READY, execute_cdb_test_unit_ready },
	{ SCSI_CMD_INQUIRY, execute_cdb_inquiry },
	{ SCSI_CMD_MODE_SENSE_6, execute_cdb_mode_sense_6 },
	{ SCSI_CMD_READ_ELEMENT_STATUS, execute_cdb_read_element_status },
	{ SCSI_CMD_MOVE_MEDIUM, execute_cdb_move_medium },
	{ 0, 0 },
};

static ndmp9_error
ndmjob_scsi_open (struct ndm_session *sess, char *name)
{
	struct stat			st;
	struct ndm_robot_agent *	ra = sess->robot_acb;

//	if (!name || strlen(name) > sizeof(ra->sim_dir)-1)
//		return NDMP9_NO_DEVICE_ERR;

	/* check that it's a directory */
	if (stat (name, &st) < 0)
		return NDMP9_NO_DEVICE_ERR;
	if (!S_ISDIR(st.st_mode))
		return NDMP9_NO_DEVICE_ERR;

	ra->sim_dir = NDMOS_API_STRDUP (name);
	ra->scsi_state.error = NDMP9_NO_ERR;

	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_scsi_close (struct ndm_session *sess)
{
	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_scsi_reset (struct ndm_session *sess)
{
	return NDMP9_NO_ERR;
}

static ndmp9_error
ndmjob_scsi_execute_cdb (struct ndm_session *sess,
                ndmp9_execute_cdb_request *request,
                ndmp9_execute_cdb_reply *reply)
{
	unsigned char cdb_byte;
	int i;

	cdb_byte = request->cdb.cdb_val[0];
	for (i = 0; cdb_executors[i].execute_cdb; i++) {
		if (cdb_executors[i].cdb_byte == cdb_byte)
			return cdb_executors[i].execute_cdb(sess, request, reply);
	}

	return NDMP9_ILLEGAL_ARGS_ERR;
}
#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */

static int
ndmjob_validate_password (struct ndm_session *sess, char *name, char *pass)
{
	if (strcmp (name, "ndmp") != 0)
		return 0;

	if (strcmp (pass, "ndmp") != 0)
		return 0;

	return 1;       /* OK */
}

static int
ndmjob_validate_md5 (struct ndm_session *sess, char *name, char digest[16])
{
	if (strcmp (name, "ndmp") != 0)
		return 0;

	if (!ndmmd5_ok_digest (sess->md5_challenge, "ndmp", digest))
		return 0;

	return 1;       /* OK */
}

void
ndmjob_register_callbacks (struct ndm_session *sess, struct ndmlog *ixlog)
{
#ifdef NDMOS_OPTION_TAPE_SIMULATOR
	struct ndm_tape_simulator_callbacks tape_callbacks;
#endif /* NDMOS_OPTION_TAPE_SIMULATOR */
#ifdef NDMOS_OPTION_ROBOT_SIMULATOR
	struct ndm_robot_simulator_callbacks robot_callbacks;
#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */
	struct ndm_auth_callbacks auth_callbacks;
	struct ndm_fhdb_callbacks fhdb_callbacks;

#ifdef NDMOS_OPTION_TAPE_SIMULATOR
	tape_callbacks.tape_open = ndmjob_tape_open;
	tape_callbacks.tape_close = ndmjob_tape_close;
	tape_callbacks.tape_mtio = ndmjob_tape_mtio;
	tape_callbacks.tape_write = ndmjob_tape_write;
	tape_callbacks.tape_wfm = ndmjob_tape_wfm;
	tape_callbacks.tape_read = ndmjob_tape_read;

	ndmos_tape_register_callbacks (sess, &tape_callbacks);
#endif /* NDMOS_OPTION_TAPE_SIMULATOR */

#ifdef NDMOS_OPTION_ROBOT_SIMULATOR
	robot_callbacks.scsi_open = ndmjob_scsi_open;
	robot_callbacks.scsi_close = ndmjob_scsi_close;
	robot_callbacks.scsi_reset = ndmjob_scsi_reset;
	robot_callbacks.scsi_execute_cdb = ndmjob_scsi_execute_cdb;

	ndmos_scsi_register_callbacks (sess, &robot_callbacks);
#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */

	auth_callbacks.validate_password = ndmjob_validate_password;
	auth_callbacks.validate_md5 = ndmjob_validate_md5;

	ndmos_auth_register_callbacks (sess, &auth_callbacks);

	fhdb_callbacks.add_file = ndmjobfhdb_add_file;
	fhdb_callbacks.add_dir = ndmjobfhdb_add_dir;
	fhdb_callbacks.add_node = ndmjobfhdb_add_node;
	fhdb_callbacks.add_dirnode_root = ndmjobfhdb_add_dirnode_root;

	ndmfhdb_register_callbacks (ixlog, &fhdb_callbacks);
}

void
ndmjob_unregister_callbacks (struct ndm_session *sess, struct ndmlog *ixlog)
{
	ndmfhdb_unregister_callbacks (ixlog);
	ndmos_auth_unregister_callbacks (sess);
#ifdef NDMOS_OPTION_ROBOT_SIMULATOR
	ndmos_scsi_unregister_callbacks (sess);
#endif /* NDMOS_OPTION_ROBOT_SIMULATOR */
#ifdef NDMOS_OPTION_TAPE_SIMULATOR
	ndmos_auth_unregister_callbacks (sess);
#endif /* NDMOS_OPTION_TAPE_SIMULATOR */
}
