/*
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#ifndef __DROPLET_HTTPREPLY_H__
#define __DROPLET_HTTPREPLY_H__ 1

/*
 *
 */
struct dpl_http_reply
{
#define DPL_HTTP_VERSION_1_0        0
#define DPL_HTTP_VERSION_1_1        1
  int	version;
#define DPL_HTTP_CODE_CONTINUE        100
#define DPL_HTTP_CODE_OK	      200
#define DPL_HTTP_CODE_NO_CONTENT      204
#define DPL_HTTP_CODE_PARTIAL_CONTENT 206
#define DPL_HTTP_CODE_NOT_FOUND       404
  int	code;
  char	*descr_start;
  char	*descr_end;
};

/*
 *
 */
typedef dpl_status_t (*dpl_header_func_t)(void *cb_arg, char *header, char*value);
typedef dpl_status_t (*dpl_buffer_func_t)(void *cb_arg, char *buf, unsigned int len);

/* PROTO httpreply.c */
/* src/httpreply.c */
dpl_status_t dpl_read_http_reply_buffered(dpl_conn_t *conn, int expect_data, dpl_header_func_t header_func, dpl_buffer_func_t buffer_func, void *cb_arg);
int dpl_connection_close(dpl_dict_t *headers_returned);
dpl_status_t dpl_read_http_reply(dpl_conn_t *conn, int expect_data, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **headersp);
#endif
