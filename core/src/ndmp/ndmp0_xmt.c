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


#include "ndmos.h"
#include "ndmprotocol.h"


#define xdr_ndmp0_connect_close_request xdr_void
#define xdr_ndmp0_connect_close_reply xdr_void

#define xdr_ndmp0_notify_connected_reply 0


struct ndmp_xdr_message_table ndmp0_xdr_message_table[] = {
    {
        NDMP0_CONNECT_OPEN,
        xdr_ndmp0_connect_open_request,
        xdr_ndmp0_connect_open_reply,
    },
    {
        NDMP0_CONNECT_CLOSE,
        xdr_ndmp0_connect_close_request,
        xdr_ndmp0_connect_close_reply,
    },
    {
        NDMP0_NOTIFY_CONNECTED,
        xdr_ndmp0_notify_connected_request,
        xdr_ndmp0_notify_connected_reply,
    },
    {0}};
