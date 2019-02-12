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


#include "ndmlib.h"



xdrproc_t
ndmnmb_find_xdrproc (struct ndmp_msg_buf *nmb)
{
	struct ndmp_xdr_message_table *	xmte;

	xmte = ndmp_xmt_lookup (nmb->protocol_version, nmb->header.message);

	if (!xmte) {
		return 0;
	}

	if (nmb->header.message_type == NDMP0_MESSAGE_REQUEST) {
		return (xdrproc_t) xmte->xdr_request;
	}

	if (nmb->header.message_type == NDMP0_MESSAGE_REPLY) {
		return (xdrproc_t) xmte->xdr_reply;
	}

	return 0;
}

void
ndmnmb_free (struct ndmp_msg_buf *nmb)
{
	xdrproc_t xdr_body = ndmnmb_find_xdrproc (nmb);

	if (nmb->flags & NDMNMB_FLAG_NO_FREE)
		return;

	if (xdr_body) {
		xdr_free (xdr_body, (void*) &nmb->body);
	}
}

void
ndmnmb_snoop (
  struct ndmlog *log,
  char *tag,
  int level,
  struct ndmp_msg_buf *nmb,
  char *whence)
{
	int		rc, nl, i;
	char		buf[2048];
	int		(*ndmpp)();
	int		level5 = 5;
	int		level6 = 6;

	 if (level < 6 && nmb->protocol_version == 4) {
		ndmp4_header *header = (ndmp4_header *)&nmb->header;
		if ((header->message_code == NDMP4_NOTIFY_DATA_HALTED &&
		     header->error_code == NDMP4_DATA_HALT_SUCCESSFUL) ||
		    (header->message_code == NDMP4_NOTIFY_MOVER_HALTED &&
		     header->error_code == NDMP4_MOVER_HALT_CONNECT_CLOSED)) {
			level = 6;
			level5 = 0;
			level6 = 0;
		}
	}

	if (!log || level < 5) {
		return;
	}

	rc = ndmp_pp_header (nmb->protocol_version, &nmb->header, buf);
	{
		char combo[3];

		if (*whence == 'R') {
			combo[0] = '>';
			combo[1] = buf[0];
		} else {
			combo[0] = buf[0];
			combo[1] = '>';
		}
		combo[2] = 0;

		ndmlogf (log, tag, level5, "%s %s", combo, buf+2);
	}

	if (level < 6) {
		return;
	}
	if (rc <= 0) {		/* no body */
		return;
	}

	if (nmb->header.message_type == NDMP0_MESSAGE_REQUEST) {
		ndmpp = ndmp_pp_request;
	} else if (nmb->header.message_type == NDMP0_MESSAGE_REPLY) {
		ndmpp = ndmp_pp_reply;
	} else {
		return;		/* should not happen */
	}

	nl = 1;
	for (i = 0; i < nl; i++) {
		nl = (*ndmpp)(nmb->protocol_version,
			nmb->header.message, &nmb->body, i, buf);
		if (nl == 0)
			break;		/* no printable body (void) */

		ndmlogf (log, tag, level6, "   %s", buf);
	}
}

unsigned
ndmnmb_get_reply_error_raw (struct ndmp_msg_buf *nmb)
{
	unsigned	protocol_version = nmb->protocol_version;
	unsigned	msg = nmb->header.message;
	unsigned	raw_error;

	if (NDMNMB_IS_UNFORTUNATE_REPLY_TYPE(protocol_version, msg)) {
		raw_error = nmb->body.unf3_error.error;
	} else {
		raw_error = nmb->body.error;
	}

	return raw_error;
}

ndmp9_error
ndmnmb_get_reply_error (struct ndmp_msg_buf *nmb)
{
	unsigned	protocol_version = nmb->protocol_version;
	unsigned	raw_error = ndmnmb_get_reply_error_raw (nmb);
	ndmp9_error	error;

	switch (protocol_version) {
	default:
		/* best effort */
		error = (ndmp9_error) raw_error;
		break;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
		{
			ndmp2_error	error2 = raw_error;

			ndmp_2to9_error (&error2, &error);
		}
		break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
		{
			ndmp3_error	error3 = raw_error;

			ndmp_3to9_error (&error3, &error);
		}
		break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
		{
			ndmp4_error	error4 = raw_error;

			ndmp_4to9_error (&error4, &error);
		}
		break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return error;
}

int
ndmnmb_set_reply_error_raw (struct ndmp_msg_buf *nmb, unsigned raw_error)
{
	unsigned	protocol_version = nmb->protocol_version;
	unsigned	msg = nmb->header.message;

	if (NDMNMB_IS_UNFORTUNATE_REPLY_TYPE(protocol_version, msg)) {
		nmb->body.unf3_error.error = raw_error;
	} else {
		nmb->body.error = raw_error;
	}

	return 0;
}

int
ndmnmb_set_reply_error (struct ndmp_msg_buf *nmb, ndmp9_error error)
{
	unsigned	protocol_version = nmb->protocol_version;
	unsigned	raw_error;

	switch (protocol_version) {
	default:
		/* best effort */
		raw_error = (unsigned) error;
		break;

#ifndef NDMOS_OPTION_NO_NDMP2
	case NDMP2VER:
		{
			ndmp2_error	error2;

			ndmp_9to2_error (&error, &error2);
			raw_error = (unsigned) error2;
		}
		break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
	case NDMP3VER:
		{
			ndmp3_error	error3;

			ndmp_9to3_error (&error, &error3);
			raw_error = (unsigned) error3;
		}
		break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
	case NDMP4VER:
		{
			ndmp4_error	error4;

			ndmp_9to4_error (&error, &error4);
			raw_error = (unsigned) error4;
		}
		break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */
	}

	return ndmnmb_set_reply_error_raw (nmb, raw_error);
}
