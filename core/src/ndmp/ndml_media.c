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


#include "ndmlib.h"


/*
 * Media Entry (medent)
 *
 * [LABEL][+FILEMARKS][/NBYTES][@SLOT]
 *
 * LABEL is simple text and must be first.
 *
 * +FILEMARKS, /NBYTES, and @SLOT may occur in any order.
 *
 * FILEMARKS is a small decimal number and indicates
 * how many filemarks to skip when using the media.
 * 0 means begining of tape. If no +FILEMARKS is given,
 * 1 is used if LABEL is given, 0 otherwise.
 *
 * NBYTES indicates the maximum amount of data on the
 * media. This is a decimal number optionally followed
 * by a scale character (k, m, g).
 *
 * SLOT is the slot address in the tape robot. It is
 * the element address, not relative to the first
 * slot in the robot.
 */

int ndmmedia_from_str(struct ndmmedia* me, char* str)
{
  char* p;
  char* q;
  int c;

  NDMOS_MACRO_ZEROFILL(me);

  p = str;
  q = me->label;

  for (; *p; p++) {
    c = *p;
    if (c == '+' || c == '@' || c == '/') break;

    if (q < &me->label[NDMMEDIA_LABEL_MAX]) *q++ = c;
  }
  *q = 0;

  if (q > me->label) me->valid_label = 1;

  while (*p) {
    c = *p;
    switch (c) {
      default:
        return -1; /* what is this? */

      case '@':
        if (me->valid_slot) return -2;

        me->slot_addr = strtol(p + 1, &p, 0);
        me->valid_slot = 1;
        break;

      case '+':
        if (me->valid_filemark) return -3;

        me->file_mark_offset = strtol(p + 1, &p, 0);
        me->valid_filemark = 1;
        break;

      case '/':
        if (me->valid_n_bytes) return -4;

        me->n_bytes = ndmmedia_strtoll(p + 1, &p, 0);
        me->valid_n_bytes = 1;
        break;
    }
  }

  return 0;
}

int ndmmedia_to_str(struct ndmmedia* me, char* str)
{
  char* q = str;

  *q = 0;

  if (me->valid_label) {
    strcpy(q, me->label);
    while (*q) q++;
  }

  if (me->valid_filemark) {
    sprintf(q, "+%d", me->file_mark_offset);
    while (*q) q++;
  }

  if (me->valid_n_bytes) {
    if (me->n_bytes == 0)
      sprintf(q, "/0");
    else if (me->n_bytes % (1024 * 1024 * 1024) == 0)
      sprintf(q, "/%lldG", me->n_bytes / (1024 * 1024 * 1024));
    else if (me->n_bytes % (1024 * 1024) == 0)
      sprintf(q, "/%lldM", me->n_bytes / (1024 * 1024));
    else if (me->n_bytes % (1024) == 0)
      sprintf(q, "/%lldK", me->n_bytes / (1024));
    else
      sprintf(q, "/%lld", me->n_bytes);
    while (*q) q++;
  }

  if (me->valid_slot) {
    sprintf(q, "@%d", me->slot_addr);
    while (*q) q++;
  }

  return 0;
}


static char* flag_yes_or_no(int f) { return (f) ? "Y" : "N"; }

int ndmmedia_pp(struct ndmmedia* me, int lineno, char* buf)
{
  switch (lineno) {
    case 0:
      ndmmedia_to_str(me, buf);
      break;

    case 1:
      sprintf(
          buf, "valid label=%s filemark=%s n_bytes=%s slot=%s",
          flag_yes_or_no(me->valid_label), flag_yes_or_no(me->valid_filemark),
          flag_yes_or_no(me->valid_n_bytes), flag_yes_or_no(me->valid_slot));
      break;

    case 2:
      sprintf(buf, "media used=%s written=%s eof=%s eom=%s io_error=%s",
              flag_yes_or_no(me->media_used), flag_yes_or_no(me->media_written),
              flag_yes_or_no(me->media_eof), flag_yes_or_no(me->media_eom),
              flag_yes_or_no(me->media_io_error));
      break;

    case 3:
      sprintf(buf, "label read=%s written=%s io_error=%s mismatch=%s",
              flag_yes_or_no(me->label_read), flag_yes_or_no(me->label_written),
              flag_yes_or_no(me->label_io_error),
              flag_yes_or_no(me->label_mismatch));
      break;

    case 4:
      sprintf(buf, "fm_error=%s nb_determined=%s nb_aligned=%s",
              flag_yes_or_no(me->fmark_error),
              flag_yes_or_no(me->nb_determined),
              flag_yes_or_no(me->nb_aligned));
      break;

    case 5:
      sprintf(buf, "slot empty=%s bad=%s missing=%s",
              flag_yes_or_no(me->slot_empty), flag_yes_or_no(me->slot_bad),
              flag_yes_or_no(me->slot_missing));
      break;

    default:
      strcpy(buf, "<<INVALID>>");
      break;
  }

  return 6;
}

int64_t ndmmedia_strtoll(char* str, char** tailp, int defbase)
{
  int64_t val = 0;
  int c;

  for (;;) {
    c = *str;
    if (c < '0' || '9' < c) break;
    val *= 10;
    val += c - '0';
    str++;
  }

  switch (c) {
    default:
      break;

    case 'k':
    case 'K':
      val *= 1024LL;
      str++;
      break;

    case 'm':
    case 'M':
      val *= 1024 * 1024LL;
      str++;
      break;

    case 'g':
    case 'G':
      val *= 1024 * 1024 * 1024LL;
      str++;
      break;
  }

  if (tailp) *tailp = str;

  return val;
}
