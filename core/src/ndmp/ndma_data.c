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
#include "wraplib.h"

#ifndef NDMOS_OPTION_NO_DATA_AGENT



/*
 * Initialization and Cleanup
 ****************************************************************
 */

/* Initialize -- Set data structure to know value, ignore current value */
int
ndmda_initialize (struct ndm_session *sess)
{
        sess->data_acb = NDMOS_API_MALLOC (sizeof(struct ndm_data_agent));
	if (!sess->data_acb)
		return -1;
        NDMOS_MACRO_ZEROFILL (sess->data_acb);

	sess->data_acb->data_state.state = NDMP9_DATA_STATE_IDLE;
	ndmchan_initialize (&sess->data_acb->formatter_error, "dfp-error");
	ndmchan_initialize (&sess->data_acb->formatter_wrap, "dfp-wrap");
	ndmchan_initialize (&sess->data_acb->formatter_image, "dfp-image");
	ndmda_fh_initialize (sess);

	return 0;
}

/* Commission -- Get agent ready. Entire session has been initialize()d */
int
ndmda_commission (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;

	da->data_state.state = NDMP9_DATA_STATE_IDLE;
	ndmda_fh_commission (sess);

	return 0;
}

/* Decommission -- Discard agent */
int
ndmda_decommission (struct ndm_session *sess)
{
	ndmis_data_close (sess);
	ndmda_purge_environment (sess);
	ndmda_purge_nlist (sess);
	ndmda_fh_decommission (sess);
	NDMOS_API_BZERO (sess->data_acb->bu_type,sizeof sess->data_acb->bu_type);

	ndmda_commission (sess);

	return 0;
}

/* Destroy -- destroy agent */
int
ndmda_destroy (struct ndm_session *sess)
{
	if (!sess->data_acb) {
		return 0;
	}

	if (sess->data_acb->fmt_image_buf) {
		NDMOS_API_FREE (sess->data_acb->fmt_image_buf);
	}
	if (sess->data_acb->fmt_error_buf) {
		NDMOS_API_FREE (sess->data_acb->fmt_error_buf);
	}
	if (sess->data_acb->fmt_wrap_buf) {
		NDMOS_API_FREE (sess->data_acb->fmt_wrap_buf);
	}

	ndmda_fh_destroy (sess);

	NDMOS_API_FREE (sess->data_acb);
	sess->data_acb = NULL;

	return 0;
}

/* Belay -- Cancel partially issued activation/start */
int
ndmda_belay (struct ndm_session *sess)
{
	ndmda_fh_belay (sess);
	return ndmda_decommission (sess);
}




/*
 * Semantic actions -- called from ndma_dispatch()
 ****************************************************************
 */

static int
add_env (struct ndm_env_table *envtab, char *cmd)
{
	char			buf[1024];
	struct ndm_env_entry *	entry;

	for (entry = envtab->head; entry; entry = entry->next) {
		snprintf (buf, sizeof(buf) - 1, "%s=%s", entry->pval.name, entry->pval.value);
		buf[sizeof(buf) - 1] = '\0';
		ndmda_add_to_cmd (cmd, "-E");
		ndmda_add_to_cmd (cmd, buf);
	}

	return 0;
}

static int
add_nlist (struct ndm_nlist_table *nlisttab, char *cmd)
{
	char				buf[32];
	struct ndm_nlist_entry *	entry;

	for (entry = nlisttab->head; entry; entry = entry->next) {
		ndmda_add_to_cmd (cmd, entry->name.original_path);
		if (entry->name.fh_info.valid == NDMP9_VALIDITY_VALID) {
			snprintf (buf, sizeof(buf), "@%llu", entry->name.fh_info.value);
			ndmda_add_to_cmd (cmd, buf);
		} else {
			ndmda_add_to_cmd (cmd, "@-");
		}
		ndmda_add_to_cmd (cmd, entry->name.destination_path);
	}

	return 0;
}


ndmp9_error
ndmda_data_start_backup (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;
	ndmp9_error		error = NDMP9_NO_ERR;
	char			cmd[NDMDA_MAX_CMD];

	strcpy (cmd, "wrap_");
	strcat (cmd, da->bu_type);

	if (sess->param->log_level > 0) {
	    char tmpbuf[40];
	    snprintf(tmpbuf, sizeof(tmpbuf), "-d%d", sess->param->log_level);
	    ndmda_add_to_cmd (cmd, tmpbuf);
	}

	ndmda_add_to_cmd (cmd, "-c");
	ndmda_add_to_cmd (cmd, "-I#3");
	add_env (&da->env_tab, cmd);

	ndma_send_logmsg (sess, NDMP9_LOG_DEBUG, sess->plumb.data,
		"CMD: %s", cmd);

	if (ndmda_pipe_fork_exec (sess, cmd, 1) < 0) {
		return NDMP9_UNDEFINED_ERR;
	}

	ndmis_data_start (sess, NDMCHAN_MODE_WRITE);

	da->data_state.state = NDMP9_DATA_STATE_ACTIVE;
	da->data_state.operation = NDMP9_DATA_OP_BACKUP;

	return error;
}

ndmp9_error
ndmda_data_start_recover (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;
	ndmp9_error		error = NDMP9_NO_ERR;
	char			cmd[NDMDA_MAX_CMD];

	strcpy (cmd, "wrap_");
	strcat (cmd, da->bu_type);

	if (sess->param->log_level > 0) {
	    char tmpbuf[40];
	    snprintf(tmpbuf, sizeof(tmpbuf), "-d%d", sess->param->log_level);
	    ndmda_add_to_cmd (cmd, tmpbuf);
	}

	ndmda_add_to_cmd (cmd, "-x");
	ndmda_add_to_cmd (cmd, "-I#3");
	add_env (&da->env_tab, cmd);
	add_nlist (&da->nlist_tab, cmd);

	ndma_send_logmsg (sess, NDMP9_LOG_DEBUG, sess->plumb.data,
		"CMD: %s", cmd);

	if (ndmda_pipe_fork_exec (sess, cmd, 0) < 0) {
		return NDMP9_UNDEFINED_ERR;
	}

	ndmis_data_start (sess, NDMCHAN_MODE_READ);

	da->data_state.state = NDMP9_DATA_STATE_ACTIVE;
	da->data_state.operation = NDMP9_DATA_OP_RECOVER;

	return error;
}

ndmp9_error
ndmda_data_start_recover_fh (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;
	ndmp9_error		error = NDMP9_NO_ERR;
	char			cmd[NDMDA_MAX_CMD];

	strcpy (cmd, "wrap_");
	strcat (cmd, da->bu_type);
	ndmda_add_to_cmd (cmd, "-t");
	ndmda_add_to_cmd (cmd, "-I#3");
	add_env (&da->env_tab, cmd);
	add_nlist (&da->nlist_tab, cmd);

	ndma_send_logmsg (sess, NDMP9_LOG_DEBUG,  sess->plumb.data,
		"CMD: %s", cmd);

	if (ndmda_pipe_fork_exec (sess, cmd, 0) < 0) {
		return NDMP9_UNDEFINED_ERR;
	}

	ndmis_data_start (sess, NDMCHAN_MODE_READ);

	da->data_state.state = NDMP9_DATA_STATE_ACTIVE;
	da->data_state.operation = NDMP9_DATA_OP_RECOVER_FILEHIST;

	return error;
}

void
ndmda_sync_state (struct ndm_session *sess)
{
	/* no-op, always accurate */
}

void
ndmda_data_abort (struct ndm_session *sess)
{
	ndmda_data_halt (sess, NDMP9_DATA_HALT_ABORTED);
}

void
ndmda_sync_environment (struct ndm_session *sess)
{
	/* no-op, always accurate */
}

ndmp9_error
ndmda_data_listen (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;

	da->data_state.state = NDMP9_DATA_STATE_LISTEN;
	da->data_state.halt_reason = NDMP9_DATA_HALT_NA;

	return NDMP9_NO_ERR;
}

ndmp9_error
ndmda_data_connect (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;

	da->data_state.state = NDMP9_DATA_STATE_CONNECTED;
	da->data_state.halt_reason = NDMP9_DATA_HALT_NA;

	return NDMP9_NO_ERR;
}

void
ndmda_data_halt (struct ndm_session *sess, ndmp9_data_halt_reason reason)
{
	struct ndm_data_agent *	da = sess->data_acb;

	da->data_state.state = NDMP9_DATA_STATE_HALTED;
	da->data_state.halt_reason = reason;
	da->data_notify_pending = 1;

	ndmda_fh_flush (sess);

	ndmis_data_close (sess);

	ndmchan_cleanup (&da->formatter_image);
	ndmchan_cleanup (&da->formatter_error);
	ndmchan_cleanup (&da->formatter_wrap);

	/* this needs to be better */
	if (da->formatter_pid) {
		sleep (1);	/* give gtar a chance to stop by itself */
		kill (da->formatter_pid, SIGTERM);
	}
}

void
ndmda_data_stop (struct ndm_session *sess)
{
	ndmda_decommission (sess);
}




/*
 * Quantum -- get a bit of work done
 ****************************************************************
 */

int
ndmda_quantum (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;
	int			did_something = 0;	/* did nothing */


	switch (da->data_state.state) {
	default:
		ndmalogf (sess, 0, 0, "BOTCH data state");
		return -1;

	case NDMP9_DATA_STATE_IDLE:
	case NDMP9_DATA_STATE_HALTED:
	case NDMP9_DATA_STATE_CONNECTED:
		break;

	case NDMP9_DATA_STATE_LISTEN:
		switch (sess->plumb.image_stream->data_ep.connect_status) {
		case NDMIS_CONN_LISTEN:		/* no connection yet */
			break;

		case NDMIS_CONN_ACCEPTED:	/* we're in business */
			/* drum roll please... */
			da->data_state.state = NDMP9_DATA_STATE_CONNECTED;
			/* tah-dah */
			did_something++;	/* did something */
			break;

		case NDMIS_CONN_BOTCHED:	/* accept() went south */
		default:			/* ain't suppose to happen */
			ndmda_data_halt (sess, NDMP9_DATA_HALT_CONNECT_ERROR);
			did_something++;	/* did something */
			break;
		}
		break;

	case NDMP9_DATA_STATE_ACTIVE:
		did_something |= ndmda_quantum_stderr (sess);
		did_something |= ndmda_quantum_wrap (sess);
		did_something |= ndmda_quantum_image (sess);
		break;
	}

	ndmda_send_notice (sess);

	return did_something;
}

int
ndmda_quantum_stderr (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;
	struct ndmchan *	ch = &da->formatter_error;
	int			did_something = 0;
	char *			p;
	char *			data;
	char *			pend;
	unsigned		n_ready;

  again:
	n_ready = ndmchan_n_ready (ch);
	if (n_ready == 0)
		return did_something;

	data = p = &ch->data[ch->beg_ix];
	pend = p + n_ready;

	while (p < pend && *p != '\n') p++;


	if (p < pend && *p == '\n') {
		*p++ = 0;
		ndma_send_logmsg (sess, NDMP9_LOG_NORMAL,  sess->plumb.data,
			"%s", data);
		ch->beg_ix += p - data;
		did_something++;
		goto again;
	}

	if (!ch->eof)
		return did_something;

	/* content w/o newline, and EOF */
	/* p == pend */
	if (ch->end_ix >= ch->data_size) {
		if (data != ch->data) {
			ndmchan_compress (ch);
			goto again;
		}
		/* that's one huge message */
		p--;	/* lose last byte */
	}

	ch->data[ch->end_ix++] = '\n';
	did_something++;
	goto again;
}

int
ndmda_quantum_wrap (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;
	struct ndmchan *	ch = &da->formatter_wrap;
	int			did_something = 0;
	char *			p;
	char *			data;
	char *			pend;
	unsigned		n_ready;
	int			is_recover = 0;

	switch (da->data_state.operation) {
	default:
		assert (0);
		break;

	case NDMP9_DATA_OP_BACKUP:
		break;

	case NDMP9_DATA_OP_RECOVER:
	case NDMP9_DATA_OP_RECOVER_FILEHIST:
		is_recover = 1;
		break;
	}

  again:
	n_ready = ndmchan_n_ready (ch);
	if (n_ready == 0) {
		if (ch->eof && is_recover) {
			ndmda_data_halt (sess, NDMP9_DATA_HALT_SUCCESSFUL);
		}
		return did_something;
	}
	data = p = &ch->data[ch->beg_ix];
	pend = p + n_ready;

	while (p < pend && *p != '\n') p++;


	if (p < pend && *p == '\n') {
		*p++ = 0;
		ndmda_wrap_in (sess, data);
		ch->beg_ix += p - data;
		did_something++;
		goto again;
	}

	if (!ch->eof)
		return did_something;

	/* content w/o newline, and EOF */
	/* p == pend */
	if (ch->end_ix >= ch->data_size) {
		if (data != ch->data) {
			ndmchan_compress (ch);
			goto again;
		}
		/* that's one huge message */
		p--;	/* lose last byte */
	}

	ch->data[ch->end_ix++] = '\n';
	did_something++;
	goto again;
}



int
ndmda_quantum_image (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;
	struct ndmchan *	from_chan;
	struct ndmchan *	to_chan;
	unsigned		n_ready, n_avail, n_copy;
	int			is_backup = 0;

	switch (da->data_state.operation) {
	default:
		assert (0);
		from_chan = 0;
		to_chan = 0;
		break;

	case NDMP9_DATA_OP_BACKUP:
		from_chan = &da->formatter_image;
		to_chan = &sess->plumb.image_stream->chan;
		is_backup = 1;
		break;

	case NDMP9_DATA_OP_RECOVER:
	case NDMP9_DATA_OP_RECOVER_FILEHIST:
		from_chan = &sess->plumb.image_stream->chan;
		to_chan = &da->formatter_image;
		break;
	}

  again:
	n_copy = n_ready = ndmchan_n_ready (from_chan);
	if (n_ready == 0) {
		if (from_chan->eof) {
			to_chan->eof = 1;
			if (ndmchan_n_ready (to_chan) == 0) {
				if (is_backup) {
					ndmda_data_halt (sess,
						NDMP9_DATA_HALT_SUCCESSFUL);
				}
			}
		}
		return 0;	/* data blocked */
	}

	n_avail = ndmchan_n_avail (to_chan);
	if (n_copy > n_avail)
		n_copy = n_avail;

	if (da->enable_hist) {
		if (n_copy > da->pass_resid)
			n_copy = da->pass_resid;
	}

	if (n_copy > 0) {
		bcopy (&from_chan->data[from_chan->beg_ix],
			&to_chan->data[to_chan->end_ix],
			n_copy);
		from_chan->beg_ix += n_copy;
		to_chan->end_ix += n_copy;
		da->data_state.bytes_processed += n_copy;
		da->pass_resid -= n_copy;
		goto again;	/* do as much as possible */
	}

	return 0;

}




/*
 * Process WRAP messages from the formatter. Called from
 * ndmda_quantum_wrap(). The formatter sends one line text
 * messages via the WRAP pipe (fd=3 on formatter).
 * The WRAP message contain log messages, file history,
 * status updates, etc, etc, etc.
 *
 * Here the messages are parsed and directed to the
 * right NDMP interface toward the Control Agent.
 */

void		ndmp9_fstat_from_wrap_fstat (ndmp9_file_stat *fstat9,
				struct wrap_fstat *fstatw);

int
ndmda_wrap_in (struct ndm_session *sess, char *wrap_line)
{
	struct wrap_msg_buf	_wmsg, *wmsg = &_wmsg;
	int			rc;
	ndmp9_file_stat		fstat9;

	NDMOS_MACRO_ZEROFILL (wmsg);

	rc = wrap_parse_msg (wrap_line, wmsg);
	if (rc != 0) {
		ndmalogf (sess, 0, 2, "Malformed wrap: %s", wrap_line);
		return -1;
	}

	switch (wmsg->msg_type) {
	case WRAP_MSGTYPE_LOG_MESSAGE:
		ndmalogf (sess, "WRAP", 2, "%s",
			wmsg->body.log_message.message);
		ndma_send_logmsg (sess, NDMP9_LOG_NORMAL, sess->plumb.data,
			"WRAP: %s", wmsg->body.log_message.message);
		break;

	case WRAP_MSGTYPE_ADD_FILE:
		ndmp9_fstat_from_wrap_fstat (&fstat9,
				&wmsg->body.add_file.fstat);
		fstat9.fh_info.valid = NDMP9_VALIDITY_VALID;
		fstat9.fh_info.value = wmsg->body.add_file.fhinfo;
		ndmda_fh_add_file (sess, &fstat9, wmsg->body.add_file.path);
		break;

	case WRAP_MSGTYPE_ADD_DIRENT:
		ndmda_fh_add_dir (sess,
			wmsg->body.add_dirent.dir_fileno,
			wmsg->body.add_dirent.name,
			wmsg->body.add_dirent.fileno);
		break;

	case WRAP_MSGTYPE_ADD_NODE:
		ndmp9_fstat_from_wrap_fstat (&fstat9,
				&wmsg->body.add_node.fstat);
		fstat9.fh_info.valid = NDMP9_VALIDITY_VALID;
		fstat9.fh_info.value = wmsg->body.add_node.fhinfo;
		ndmda_fh_add_node (sess, &fstat9);
		break;

	case WRAP_MSGTYPE_DATA_READ:
		ndmda_send_data_read (sess,
			wmsg->body.data_read.offset,
			wmsg->body.data_read.length);
		break;

	case WRAP_MSGTYPE_ADD_ENV:
	case WRAP_MSGTYPE_DATA_STATS:
	case WRAP_MSGTYPE_RECOVERY_RESULT:
		ndmalogf (sess, 0, 2, "Unimplemented wrap: %s", wrap_line);
		break;
	}

	return 0;
}

void
ndmp9_fstat_from_wrap_fstat (ndmp9_file_stat *fstat9,
  struct wrap_fstat *fstatw)
{
	NDMOS_MACRO_ZEROFILL (fstat9);

	switch (fstatw->ftype) {
	default:
	case WRAP_FTYPE_INVALID:fstat9->ftype = NDMP9_FILE_OTHER;	break;
	case WRAP_FTYPE_DIR:	fstat9->ftype = NDMP9_FILE_DIR;		break;
	case WRAP_FTYPE_FIFO:	fstat9->ftype = NDMP9_FILE_FIFO;	break;
	case WRAP_FTYPE_CSPEC:	fstat9->ftype = NDMP9_FILE_CSPEC;	break;
	case WRAP_FTYPE_BSPEC:	fstat9->ftype = NDMP9_FILE_BSPEC;	break;
	case WRAP_FTYPE_REG:	fstat9->ftype = NDMP9_FILE_REG;		break;
	case WRAP_FTYPE_SLINK:	fstat9->ftype = NDMP9_FILE_SLINK;	break;
	case WRAP_FTYPE_SOCK:	fstat9->ftype = NDMP9_FILE_SOCK;	break;
	case WRAP_FTYPE_REGISTRY:fstat9->ftype = NDMP9_FILE_REGISTRY;	break;
	case WRAP_FTYPE_OTHER:	fstat9->ftype = NDMP9_FILE_OTHER;	break;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_FTYPE) {
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_MODE) {
		fstat9->mode.valid = NDMP9_VALIDITY_VALID;
		fstat9->mode.value = fstatw->mode;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_SIZE) {
		fstat9->size.valid = NDMP9_VALIDITY_VALID;
		fstat9->size.value = fstatw->size;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_LINKS) {
		fstat9->links.valid = NDMP9_VALIDITY_VALID;
		fstat9->links.value = fstatw->size;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_UID) {
		fstat9->uid.valid = NDMP9_VALIDITY_VALID;
		fstat9->uid.value = fstatw->uid;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_GID) {
		fstat9->gid.valid = NDMP9_VALIDITY_VALID;
		fstat9->gid.value = fstatw->gid;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_ATIME) {
		fstat9->atime.valid = NDMP9_VALIDITY_VALID;
		fstat9->atime.value = fstatw->atime;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_MTIME) {
		fstat9->mtime.valid = NDMP9_VALIDITY_VALID;
		fstat9->mtime.value = fstatw->mtime;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_CTIME) {
		fstat9->ctime.valid = NDMP9_VALIDITY_VALID;
		fstat9->ctime.value = fstatw->ctime;
	}

	if (fstatw->valid & WRAP_FSTAT_VALID_FILENO) {
		fstat9->node.valid = NDMP9_VALIDITY_VALID;
		fstat9->node.value = fstatw->fileno;
	}




}




/*
 * Send LOG and NOTIFY messages
 ****************************************************************
 */

void
ndmda_send_notice (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;

	if (!da->data_notify_pending)
		return;

	da->data_notify_pending = 0;

	switch (da->data_state.state) {
	case NDMP9_DATA_STATE_HALTED:
		ndma_notify_data_halted (sess);
		break;

	default:
		/* Hmm. Why are we here. Race? */
		break;
	}
}

void
ndmda_send_data_read (struct ndm_session *sess,
  uint64_t offset, uint64_t length)
{
	struct ndm_data_agent *	da = sess->data_acb;
	ndmp9_addr_type		addr_type;

	addr_type = da->data_state.data_connection_addr.addr_type;

	if (NDMP9_ADDR_LOCAL == addr_type) {
#ifndef NDMOS_OPTION_NO_TAPE_AGENT
		if (ndmta_local_mover_read (sess, offset, length) != 0) {
			ndma_send_logmsg (sess, NDMP9_LOG_ERROR,
				sess->plumb.data,
				"local_mover_read failed");
			ndmda_data_halt (sess, NDMP9_DATA_HALT_INTERNAL_ERROR);
		}
#else /* !NDMOS_OPTION_NO_TAPE_AGENT */
		ndma_send_logmsg (sess, NDMP9_LOG_ERROR,
				sess->plumb.data,
				"local_mover_read not configured");
		ndmda_data_halt (sess, NDMP9_DATA_HALT_INTERNAL_ERROR);
#endif /* !NDMOS_OPTION_NO_TAPE_AGENT */
		return;
	}

	switch (addr_type) {
	case NDMP9_ADDR_TCP:
		ndma_notify_data_read (sess, offset, length);
		break;

	default:
		ndma_send_logmsg (sess, NDMP9_LOG_ERROR, sess->plumb.data,
			"bogus mover.addr_type");
		ndmda_data_halt (sess, NDMP9_DATA_HALT_INTERNAL_ERROR);
		break;
	}
}




/*
 * Misc -- env[] and nlist[] subroutines, etc
 ****************************************************************
 */

int
ndmda_copy_environment (struct ndm_session *sess,
  ndmp9_pval *env, unsigned n_env)
{
	struct ndm_data_agent *	da = sess->data_acb;
	unsigned int		i;

	for (i = 0; i < n_env; i++) {
		if (!ndma_store_env_list (&da->env_tab, &env[i]))
			goto fail;
	}

	return 0;

  fail:
	ndma_destroy_env_list (&da->env_tab);

	return -1;
}

struct ndmp9_pval *
ndmda_find_env (struct ndm_session *sess, char *name)
{
	struct ndm_data_agent *	da = sess->data_acb;
	struct ndm_env_entry *	entry;

	for (entry = da->env_tab.head; entry; entry = entry->next) {
		if (strcmp (entry->pval.name, name) == 0)
			return &entry->pval;
	}

	return 0;
}


int
ndmda_interpret_boolean_value (char *value_str, int default_value)
{
	if (strcasecmp (value_str, "y") == 0
	 || strcasecmp (value_str, "yes") == 0
	 || strcasecmp (value_str, "t") == 0
	 || strcasecmp (value_str, "true") == 0
	 || strcasecmp (value_str, "1") == 0)
		return 1;

	if (strcasecmp (value_str, "n") == 0
	 || strcasecmp (value_str, "no") == 0
	 || strcasecmp (value_str, "f") == 0
	 || strcasecmp (value_str, "false") == 0
	 || strcasecmp (value_str, "0") == 0)
		return 0;

	return default_value;
}

void
ndmda_purge_environment (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;

	ndma_destroy_env_list(&da->env_tab);
}


int
ndmda_copy_nlist (struct ndm_session *sess,
  ndmp9_name *nlist, unsigned n_nlist)
{
	struct ndm_data_agent *	da = sess->data_acb;
	unsigned int		i;

	for (i = 0; i < n_nlist; i++) {
		if (!ndma_store_nlist(&da->nlist_tab, &nlist[i])) {
			return -1;	/* no mem */
		}
	}

	/* TODO: sort */

	return 0;
}

void
ndmda_purge_nlist (struct ndm_session *sess)
{
	struct ndm_data_agent *	da = sess->data_acb;

	ndma_destroy_nlist(&da->nlist_tab);
}

int
ndmda_count_invalid_fh_info (struct ndm_session *sess)
{
	struct ndm_data_agent *		da = sess->data_acb;
	struct ndm_nlist_entry *	entry;
	int				count;

	count = 0;
	for (entry = da->nlist_tab.head; entry; entry = entry->next) {
		if (entry->name.fh_info.valid != NDMP9_VALIDITY_VALID) {
			count++;
		}
	}

	return count;
}

int
ndmda_count_invalid_fh_info_pending (struct ndm_session *sess)
{
	struct ndm_data_agent *		da = sess->data_acb;
	struct ndm_nlist_entry *	entry;
	int				count;

	count = 0;
	for (entry = da->nlist_tab.head; entry; entry = entry->next) {
		if (entry->result_err == NDMP9_UNDEFINED_ERR &&
		    entry->name.fh_info.valid != NDMP9_VALIDITY_VALID) {
			count++;
		}
	}

	return count;
}

#endif /* !NDMOS_OPTION_NO_DATA_AGENT */
