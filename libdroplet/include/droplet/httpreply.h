/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
typedef dpl_status_t (*dpl_buffer_func_t)(void *cb_arg, char *buf, u_int len);

/* PROTO droplet_httpreply.c */
/* src/droplet_httpreply.c */
dpl_status_t dpl_read_http_reply_buffered(dpl_conn_t *conn, dpl_header_func_t header_func, dpl_buffer_func_t buffer_func, void *cb_arg);
int dpl_connection_close(dpl_dict_t *headers_returned);
dpl_status_t dpl_read_http_reply(dpl_conn_t *conn, char **data_bufp, u_int *data_lenp, dpl_dict_t **headersp);
#endif
