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


static struct timeval	start_time;


char *
ndmlog_time_stamp (void)
{
	static char		buf[40];
	struct timeval		now;
	uint32_t		elapsed;
	int			ms, sec, min, hour;

	if (!start_time.tv_sec) {
		gettimeofday (&start_time, (void*)0);
	}

	gettimeofday (&now, (void*)0);

	now.tv_sec -= start_time.tv_sec;
	now.tv_usec -= start_time.tv_usec;

	elapsed = now.tv_sec * 1000 + now.tv_usec / 1000;

	ms = elapsed % 1000;	elapsed /= 1000;
	sec = elapsed % 60;	elapsed /= 60;
	min = elapsed % 60;	elapsed /= 60;
	hour = elapsed;

	snprintf (buf, sizeof(buf), "%d:%02d:%02d.%03d", hour, min, sec, ms);

	return buf;
}

void
ndmlogf (struct ndmlog *log, char *tag, int level, char *fmt, ...)
{
	va_list		ap;
	char		buf[2048];

	va_start (ap, fmt);
	vsnprintf (buf, sizeof(buf), fmt, ap);
	va_end (ap);

	(*log->deliver)(log, tag, level, buf);
}

void
ndmlogfv (struct ndmlog *log, char *tag, int level, char *fmt, va_list ap)
{
	char		buf[2048];

	vsnprintf (buf, sizeof(buf), fmt, ap);
	(*log->deliver)(log, tag, level, buf);
}
