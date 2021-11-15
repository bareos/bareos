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
 *      Miscellaneous C functions to support protocol versions.
 *      See ndmprotocol.h for explanation.
 */


#include "ndmos.h"
#include "ndmprotocol.h"


/*
 * XDR MESSAGE TABLES
 ****************************************************************
 */

struct ndmp_xdr_message_table* ndmp_xmt_lookup(int protocol_version, int msg)
{
  struct ndmp_xdr_message_table* table;
  struct ndmp_xdr_message_table* ent;

  switch (protocol_version) {
    case 0:
      table = ndmp0_xdr_message_table;
      break;

#ifndef NDMOS_OPTION_NO_NDMP2
    case NDMP2VER:
      table = ndmp2_xdr_message_table;
      break;
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
    case NDMP3VER:
      table = ndmp3_xdr_message_table;
      break;
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
    case NDMP4VER:
      table = ndmp4_xdr_message_table;
      break;
#endif /* !NDMOS_OPTION_NO_NDMP4 */

    default:
      return 0;
  }

  for (ent = table; ent->msg; ent++) {
    if (ent->msg == msg) { return ent; }
  }

  return 0;
}


/*
 * ENUM STRING TABLES
 ****************************************************************
 */

char* ndmp_enum_to_str(int val, struct ndmp_enum_str_table* table)
{
  static char vbuf[8][32];
  static int vbix;
  char* vbp;

  for (; table->name; table++)
    if (table->value == val) return table->name;

  vbp = vbuf[vbix & 7];
  vbix++;

  sprintf(vbp, "?0x%x?", val);
  return vbp;
}

int ndmp_enum_from_str(int* valp, char* str, struct ndmp_enum_str_table* table)
{
  for (; table->name; table++) {
    if (strcmp(table->name, str) == 0) {
      *valp = table->value;
      return 1;
    }
  }

  return 0;
}


/*
 * MULTI-VERSION ENUM TO STRING CONVERTERS
 ****************************************************************
 */

char* ndmp_message_to_str(int protocol_version, int msg)
{
  static char yikes_buf[40]; /* non-reentrant */

  switch (protocol_version) {
    case 0:
      return ndmp0_message_to_str(msg);
#ifndef NDMOS_OPTION_NO_NDMP2
    case NDMP2VER:
      return ndmp2_message_to_str(msg);
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
    case NDMP3VER:
      return ndmp3_message_to_str(msg);
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
    case NDMP4VER:
      return ndmp4_message_to_str(msg);
#endif /* !NDMOS_OPTION_NO_NDMP4 */

    default: /* should never happen, if so should be rare */
      sprintf(yikes_buf, "v%dmsg0x%04x", protocol_version, msg);
      return yikes_buf;
  }
}

char* ndmp_error_to_str(int protocol_version, int err)
{
  static char yikes_buf[40]; /* non-reentrant */

  switch (protocol_version) {
    case 0:
      return ndmp0_error_to_str(err);
    case 9:
      return ndmp9_error_to_str(err);
#ifndef NDMOS_OPTION_NO_NDMP2
    case NDMP2VER:
      return ndmp2_error_to_str(err);
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
    case NDMP3VER:
      return ndmp3_error_to_str(err);
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
    case NDMP4VER:
      return ndmp4_error_to_str(err);
#endif /* !NDMOS_OPTION_NO_NDMP4 */

    default: /* should never happen, if so should be rare */
      sprintf(yikes_buf, "v%derr%d", protocol_version, err);
      return yikes_buf;
  }
}


/*
 * PRETTY PRINTERS
 ****************************************************************
 */

int ndmp_pp_header(int vers, void* data, char* buf)
{
  switch (vers) {
    case 0:
      return ndmp0_pp_header(data, buf);

#ifndef NDMOS_OPTION_NO_NDMP2
    case NDMP2VER:
      return ndmp2_pp_header(data, buf);
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
    case NDMP3VER:
      return ndmp3_pp_header(data, buf);
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
    case NDMP4VER:
      return ndmp4_pp_header(data, buf);
#endif /* !NDMOS_OPTION_NO_NDMP4 */

    default:
      sprintf(buf, "V%d? ", vers);
      return ndmp0_pp_header(data, NDMOS_API_STREND(buf));
  }
}

int ndmp_pp_request(int vers, int msg, void* data, int lineno, char* buf)
{
  switch (vers) {
    case 0:
      return ndmp0_pp_request(msg, data, lineno, buf);

#ifndef NDMOS_OPTION_NO_NDMP2
    case NDMP2VER:
      return ndmp2_pp_request(msg, data, lineno, buf);
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
    case NDMP3VER:
      return ndmp3_pp_request(msg, data, lineno, buf);
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
    case NDMP4VER:
      return ndmp4_pp_request(msg, data, lineno, buf);
#endif /* !NDMOS_OPTION_NO_NDMP4 */

    default:
      sprintf(buf, "<<INVALID MSG VERS=%d>>", vers);
      return -1;
  }
}

int ndmp_pp_reply(int vers, int msg, void* data, int lineno, char* buf)
{
  switch (vers) {
    case 0:
      return ndmp0_pp_reply(msg, data, lineno, buf);

#ifndef NDMOS_OPTION_NO_NDMP2
    case NDMP2VER:
      return ndmp2_pp_reply(msg, data, lineno, buf);
#endif /* !NDMOS_OPTION_NO_NDMP2 */

#ifndef NDMOS_OPTION_NO_NDMP3
    case NDMP3VER:
      return ndmp3_pp_reply(msg, data, lineno, buf);
#endif /* !NDMOS_OPTION_NO_NDMP3 */

#ifndef NDMOS_OPTION_NO_NDMP4
    case NDMP4VER:
      return ndmp4_pp_reply(msg, data, lineno, buf);
#endif /* !NDMOS_OPTION_NO_NDMP4 */

    default:
      sprintf(buf, "<<INVALID MSG VERS=%d>>", vers);
      return -1;
  }
}
