/*
 * Copyright (c) 2000
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
 *      Convert strings to/from a canonical strings (CSTR).
 *
 *      The main reason for this is to eliminate spaces
 *      in strings thus making multiple strings easily
 *      delimited by white space.
 *
 *      Canonical strings use the HTTP convention of
 *      percent sign followed by two hex digits (%xx).
 *      Characters outside the printable ASCII range,
 *      space, and percent sign are so converted.
 *
 *      Both interfaces return the length of the resulting
 *      string, -1 if there is an overflow, or -2
 *      there is a conversion error.
 */


#include "ndmlib.h"


static char ndmcstr_to_hex[] = "0123456789ABCDEF";

extern int ndmcstr_from_hex(int c);

int ndmcstr_from_str(char* src, char* dst, unsigned dst_max)
{
  unsigned char* p = (unsigned char*)src;
  unsigned char* q = (unsigned char*)dst;
  unsigned char* q_end = q + dst_max - 1;
  int c;

  while ((c = *p++) != 0) {
    if (c <= ' ' || c > 0x7E || c == NDMCSTR_WARN) {
      if (q + 3 > q_end) return -1;
      *q++ = NDMCSTR_WARN;
      *q++ = ndmcstr_to_hex[(c >> 4) & 0xF];
      *q++ = ndmcstr_to_hex[c & 0xF];
    } else {
      if (q + 1 > q_end) return -1;
      *q++ = c;
    }
  }
  *q = 0;

  return q - (unsigned char*)dst;
}

int ndmcstr_to_str(char* src, char* dst, unsigned dst_max)
{
  unsigned char* p = (unsigned char*)src;
  unsigned char* q = (unsigned char*)dst;
  unsigned char* q_end = q + dst_max - 1;
  int c, c1, c2;

  while ((c = *p++) != 0) {
    if (q + 1 > q_end) return -1;
    if (c != NDMCSTR_WARN) {
      *q++ = c;
      continue;
    }
    c1 = ndmcstr_from_hex(p[0]);
    c2 = ndmcstr_from_hex(p[1]);

    if (c1 < 0 || c2 < 0) {
      /* busted conversion */
      return -2;
    }

    c = (c1 << 4) + c2;
    *q++ = c;
    p += 2;
  }
  *q = 0;

  return q - (unsigned char*)dst;
}

int ndmcstr_from_hex(int c)
{
  if ('0' <= c && c <= '9') return c - '0';
  if ('a' <= c && c <= 'f') return (c - 'a') + 10;
  if ('A' <= c && c <= 'F') return (c - 'A') + 10;
  return -1;
}
