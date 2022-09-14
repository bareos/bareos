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


#include "ndmos.h" /* rpc/rpc.h */
#include "ndmprotocol.h"


int ndmp0_pp_header(void* data, char* buf)
{
  ndmp0_header* mh = (ndmp0_header*)data;

  if (mh->message_type == NDMP0_MESSAGE_REQUEST) {
    sprintf(buf, "C %s %lu", ndmp0_message_to_str(mh->message), mh->sequence);
  } else if (mh->message_type == NDMP0_MESSAGE_REPLY) {
    sprintf(buf, "R %s %lu (%lu)", ndmp0_message_to_str(mh->message),
            mh->reply_sequence, mh->sequence);
    if (mh->error != NDMP0_NO_ERR) {
      sprintf(NDMOS_API_STREND(buf), " %s", ndmp0_error_to_str(mh->error));
      return 0; /* no body */
    }
  } else {
    strcpy(buf, "??? INVALID MESSAGE TYPE");
    return -1; /* no body */
  }
  return 1; /* body */
}

int ndmp0_pp_request(ndmp0_message msg,
                     void* data,
                     [[maybe_unused]] int lineno,
                     char* buf)
{
  switch (msg) {
    default:
      strcpy(buf, "<<INVALID MSG>>");
      return -1;

    case NDMP0_CONNECT_OPEN:
      NDMP_PP_WITH(ndmp0_connect_open_request)
      sprintf(buf, "version=%d", p->protocol_version);
      NDMP_PP_ENDWITH
      break;

    case NDMP0_CONNECT_CLOSE:
      *buf = 0; /* no body */
      return 0;

    case NDMP0_NOTIFY_CONNECTED:
      NDMP_PP_WITH(ndmp0_notify_connected_request)
      sprintf(buf, "reason=%s protocol_version=%d text_reason='%s'",
              ndmp0_connect_reason_to_str(p->reason), p->protocol_version,
              p->text_reason);
      NDMP_PP_ENDWITH
      break;
  }
  return 1; /* one line in buf */
}

int ndmp0_pp_reply(ndmp0_message msg,
                   void* data,
                   [[maybe_unused]] int lineno,
                   char* buf)
{
  switch (msg) {
    default:
      strcpy(buf, "<<INVALID MSG>>");
      return -1;

    case NDMP0_CONNECT_OPEN:
      NDMP_PP_WITH(ndmp0_error)
      sprintf(buf, "error=%s", ndmp0_error_to_str(*p));
      NDMP_PP_ENDWITH
      break;

    case NDMP0_NOTIFY_CONNECTED:
      strcpy(buf, "<<ILLEGAL REPLY>>");
      break;
  }

  return 1; /* one line in buf */
}
