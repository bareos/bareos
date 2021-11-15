/*
 * Copyright (c) 1998,1999,2000
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
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
int start_index_file(void)
{
  if (I_index_file && strcmp(I_index_file, "-") != 0) {
    FILE* ifp;

    if (atoi(I_index_file) != 0) {
      ndmjob_log(1, "Writing index (-I%s)", I_index_file);
      ifp = fdopen(atoi(I_index_file), "w");
    } else {
      ndmjob_log(1, "Writing index (-I%s)", I_index_file);
      ifp = fopen(I_index_file, "w");
    }
    if (!ifp) { error_byebye("can't open -I logfile"); }
    index_fp = ifp;
    fprintf(ifp, "##ndmjob -I\n");
  } else {
    index_fp = stderr;
  }

  return 0;
}

int sort_index_file(void)
{
  if (I_index_file && strcmp(I_index_file, "-") != 0 &&
      atoi(I_index_file) == 0) {
    char cmd[512];

    fprintf(index_fp, "##ndmjob -J\n"); /* sorts to 2nd line */
    fclose(index_fp);
    index_fp = stderr; /* in case anything else happens */

    snprintf(cmd, sizeof(cmd), "LC_ALL=C sort %s -o %s\n", I_index_file,
             I_index_file);
    ndmjob_log(3, "sort command: %s", cmd);
    ndmjob_log(1, "sorting index");
    if (system(cmd) < 0) error_byebye("sort index failed");
    ndmjob_log(1, "sort index done");
  }

  return 0;
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

void error_byebye(char* fmt, ...)
{
  va_list ap;
  char buf[4096];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  ndmjob_log(0, "FATAL: %s", buf);
  fprintf(stderr, "%s: %s\n", progname, buf);
  exit(1);
}

void ndmjob_log_deliver(struct ndmlog* log, char* tag, int lev, char* msg)
{
  char tagbuf[32];

  if (the_mode == 'D') {
    char buf[32];

    snprintf(buf, sizeof(buf), "%s(%d)", tag, (int)getpid());
    snprintf(tagbuf, sizeof(tagbuf), "%-11s", buf);
  } else {
    snprintf(tagbuf, sizeof(tagbuf), "%-4s", tag);
  }

  if (d_debug >= lev) {
    if (!o_no_time_stamps) {
      fprintf(log_fp, "%s %s %s\n", tagbuf, ndmlog_time_stamp(), msg);
    } else {
      fprintf(log_fp, "%s t[x] %s\n", tagbuf, msg);
    }
    fflush(log_fp);
  }

  if (v_verbose >= lev) {
    printf("%s\n", msg);
    fflush(stdout);
  }
}

#ifndef NDMOS_OPTION_NO_CONTROL_AGENT
void ndmjob_ixlog_deliver(struct ndmlog* log, char* tag, int lev, char* msg)
{
  fprintf(index_fp, "%s %s\n", tag, msg);
  fflush(index_fp); /* this doesn't change the run time */
}
#endif /* !NDMOS_OPTION_NO_CONTROL_AGENT */

void ndmjob_log(int lev, char* fmt, ...)
{
  va_list ap;
  char buf[4096];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  ndmjob_log_deliver(&the_param.log, the_param.log_tag, lev, buf);
}
