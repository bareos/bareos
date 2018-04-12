/*
 * Copyright (c) 2000
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


#include "ndmjob.h"


#ifndef NDMOS_OPTION_NO_CONTROL_AGENT


#include "ndmjr_none.h"


int
apply_rules (struct ndm_job_param *job, char *rules)
{
	char			reason[100];
	int			rc = 0;

	strcpy (reason, "(no reason given)");

	if (NDMJR_NONE_RECOGNIZE(rules)) {
		rc = ndmjr_none_apply (job, reason);
		goto out;
	}


	error_byebye ("unknown rules: %s", rules);

  out:
	if (rc != 0) {
		error_byebye ("rules %s failed: %s", rules, reason);
	}

	return 0;
}

int
help_rules ()
{
	printf ("  %-8s -- %s\n",
		NDMJR_NONE_HELP_LINE_NAME,
		NDMJR_NONE_HELP_LINE_1);

	return 0;
}

#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */
