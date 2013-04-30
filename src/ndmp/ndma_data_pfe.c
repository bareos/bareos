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

#ifndef NDMOS_OPTION_NO_DATA_AGENT


int
ndmda_pipe_fork_exec (struct ndm_session *sess, char *cmd, int is_backup)
{
	struct ndm_data_agent *	da = sess->data_acb;
	struct ndmchan *	ch;
	int			errpipe[2];
	int			datpipe[2];
	int			wrppipe[2];
	int			nullfd;
	int			rc = -1;

	ndmalogf (sess, 0, 2, "Starting %s", cmd);

	nullfd = open ("/dev/null", 2);
	if (nullfd < 0) {
		return rc;
	}

	rc = pipe (errpipe);
	if (rc < 0) {
		close (nullfd);
		return rc;
	}

	rc = pipe (datpipe);
	if (rc < 0) {
		close (nullfd);
		close (errpipe[0]);
		close (errpipe[1]);
		return rc;
	}

	rc = pipe (wrppipe);
	if (rc < 0) {
		close (nullfd);
		close (errpipe[0]);
		close (errpipe[1]);
		close (datpipe[0]);
		close (datpipe[1]);
		return rc;
	}

	rc = fork();
	if (rc < 0) {
		close (nullfd);
		close (errpipe[0]);
		close (errpipe[1]);
		close (datpipe[0]);
		close (datpipe[1]);
		close (wrppipe[0]);
		close (wrppipe[1]);
		return rc;
	}

	if (rc == 0) {
		/* child */
		dup2 (errpipe[1], 2);
		dup2 (wrppipe[1], 3);
		close (errpipe[0]);
		close (wrppipe[0]);

		if (is_backup) {
			dup2 (nullfd, 0);
			dup2 (datpipe[1], 1);
			close (datpipe[0]);
		} else {
			dup2 (datpipe[0], 0);
			dup2 (nullfd, 1);
			close (datpipe[1]);
		}

		/*
		 * 0 -- formatter stdin
		 * 1 -- formatter stdout
		 * 2 -- formatter stderr
		 * 3 -- formatter wrap chan (wraplib.c)
		 */
		for (rc = 4; rc < 100; rc++) {
			close(rc);
		}

		execl ("/bin/sh", "sh", "-c", cmd, NULL);

		fprintf (stderr, "EXEC FAILED %s\n", cmd);
		exit(127);
	}

	/* parent */
	close (nullfd);

	ch = &da->formatter_error;
	ndmchan_initialize (ch, "dfp-error");
	da->fmt_error_buf = NDMOS_API_MALLOC (NDMDA_N_FMT_ERROR_BUF);
	if (!da->fmt_error_buf)
		return -1;
	ndmchan_setbuf (ch, da->fmt_error_buf, NDMDA_N_FMT_ERROR_BUF);
	close (errpipe[1]);
	ndmos_condition_pipe_fd (sess, errpipe[0]);
	ndmchan_start_read (ch, errpipe[0]);

	ch = &da->formatter_wrap;
	ndmchan_initialize (ch, "dfp-wrap");
	da->fmt_wrap_buf = NDMOS_API_MALLOC (NDMDA_N_FMT_WRAP_BUF);
	if (!da->fmt_wrap_buf)
		return -1;
	ndmchan_setbuf (ch, da->fmt_wrap_buf, NDMDA_N_FMT_WRAP_BUF);
	close (wrppipe[1]);
	ndmos_condition_pipe_fd (sess, wrppipe[0]);
	ndmchan_start_read (ch, wrppipe[0]);

	ch = &da->formatter_image;
	ndmchan_initialize (ch, "dfp-image");
	da->fmt_image_buf = NDMOS_API_MALLOC (NDMDA_N_FMT_IMAGE_BUF);
	if (!da->fmt_image_buf)
		return -1;
	ndmchan_setbuf (ch, da->fmt_image_buf, NDMDA_N_FMT_IMAGE_BUF);

	if (is_backup) {
		ndmalogf (sess, 0, 2, "backup...");
		close (datpipe[1]);
		ndmos_condition_pipe_fd (sess, datpipe[0]);
		ndmchan_start_read (ch, datpipe[0]);
	} else {
		ndmalogf (sess, 0, 2, "recover...");
		close (datpipe[0]);
		ndmos_condition_pipe_fd (sess, datpipe[1]);
		ndmchan_start_write (ch, datpipe[1]);
	}

	da->formatter_pid = rc;

	return rc;	/* PID */
}

int
ndmda_add_to_cmd_with_escapes (char *cmd, char *word, char *special)
{
	char *		cmd_lim = &cmd[NDMDA_MAX_CMD-3];
	char *		p;
	int		c;

	p = cmd;
	while (*p) p++;
	if (p != cmd) *p++ = ' ';

	while ((c = *word++) != 0) {
		if (p >= cmd_lim)
			return -1;	/* overflow */
		if (c == '\\' || strchr (special, c))
			*p++ = '\\';
		*p++ = c;
	}
	*p = 0;

	return 0;
}

int
ndmda_add_to_cmd (char *cmd, char *word)
{
	return ndmda_add_to_cmd_with_escapes (cmd, word, " \t`'\"*?[]$");
}

int
ndmda_add_to_cmd_allow_file_wildcards (char *cmd, char *word)
{
	return ndmda_add_to_cmd_with_escapes (cmd, word, " \t`'\"$");
}

#endif /* !NDMOS_OPTION_NO_DATA_AGENT */
