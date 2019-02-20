/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYSLOG_H
#define _SYSLOG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  Facility codes
 */
#define LOG_KERN (0 << 3)   /* kernel messages */
#define LOG_USER (1 << 3)   /* random user-level messages */
#define LOG_MAIL (2 << 3)   /* mail system */
#define LOG_DAEMON (3 << 3) /* system daemons */
#define LOG_AUTH (4 << 3)   /* security/authorization messages */
#define LOG_SYSLOG (5 << 3) /* messages generated internally by syslogd */
#define LOG_LPR (6 << 3)    /* line printer subsystem */
#define LOG_NEWS (7 << 3)   /* netnews subsystem */
#define LOG_UUCP (8 << 3)   /* uucp subsystem */
#define LOG_AUDIT (13 << 3) /* audit subsystem */
#define LOG_CRON (15 << 3)  /* cron/at subsystem */
                            /* other codes through 15 reserved for system use */
#define LOG_LOCAL0 (16 << 3) /* reserved for local use */
#define LOG_LOCAL1 (17 << 3) /* reserved for local use */
#define LOG_LOCAL2 (18 << 3) /* reserved for local use */
#define LOG_LOCAL3 (19 << 3) /* reserved for local use */
#define LOG_LOCAL4 (20 << 3) /* reserved for local use */
#define LOG_LOCAL5 (21 << 3) /* reserved for local use */
#define LOG_LOCAL6 (22 << 3) /* reserved for local use */
#define LOG_LOCAL7 (23 << 3) /* reserved for local use */

#define LOG_NFACILITIES 24 /* maximum number of facilities */
#define LOG_FACMASK 0x03f8 /* mask to extract facility part */

/*
 *  Priorities (these are ordered)
 */
#define LOG_EMERG 0   /* system is unusable */
#define LOG_ALERT 1   /* action must be taken immediately */
#define LOG_CRIT 2    /* critical conditions */
#define LOG_ERR 3     /* error conditions */
#define LOG_WARNING 4 /* warning conditions */
#define LOG_NOTICE 5  /* normal but signification condition */
#define LOG_INFO 6    /* informational */
#define LOG_DEBUG 7   /* debug-level messages */

#define LOG_PRIMASK 0x0007 /* mask to extract priority part (internal) */

/*
 * arguments to setlogmask.
 */
#define LOG_MASK(pri) (1 << (pri)) /* mask for one priority */
#define LOG_UPTO(pri)                                    \
  ((1 << ((pri) + 1)) - 1) /* all priorities through pri \
                            */

/*
 *  Option flags for openlog.
 *
 *      LOG_ODELAY no longer does anything; LOG_NDELAY is the
 *      inverse of what it used to be.
 */
#define LOG_PID 0x01    /* log the pid with each message */
#define LOG_CONS 0x02   /* log on the console if errors in sending */
#define LOG_ODELAY 0x04 /* delay open until syslog() is called */
#define LOG_NDELAY 0x08 /* don't delay open */
#define LOG_NOWAIT 0x10 /* if forking to log on console, don't wait() */

void syslog(int type, const char* fmt, ...);
void openlog(const char* app, int, int);
void closelog(void);

#ifdef __cplusplus
}
#endif

#endif /* _SYSLOG_H */
