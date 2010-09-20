/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
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

#include "dropletsp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/** 
 * delete a resource
 * 
 * @param ctx 
 * @param bucket 
 * @param resource 
 * @param subresource can be NULL 
 * 
 * @return 
 */
dpl_status_t
dpl_delete(dpl_ctx_t *ctx,
           char *bucket,
           char *resource,
           char *subresource)
{
  char          host[1024];
  int           ret, ret2;
  struct hostent hret, *hresult;
  char          hbuf[1024];
  int           herr;
  struct in_addr addr;
  dpl_conn_t    *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers = NULL;
  dpl_dict_t    *headers_returned = NULL;

  snprintf(host, sizeof (host), "%s.%s", bucket, ctx->host);

  DPRINTF("dpl_make_bucket: host=%s:%d\n", host, ctx->port);

  headers = dpl_dict_new(13);
  if (NULL == headers)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "Host", host, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "Connection", "keep-alive", 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = linux_gethostbyname_r(host, &hret, hbuf, sizeof (hbuf), &hresult, &herr); 
  if (0 != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (AF_INET != hresult->h_addrtype)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  memcpy(&addr, hresult->h_addr_list[0], hresult->h_length);

  conn = dpl_conn_open(ctx, addr, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //build request
  ret2 = dpl_build_s3_request(ctx, 
                              "DELETE",
                              bucket,
                              resource,
                              subresource,
                              NULL,
                              headers, 
                              header,
                              sizeof (header),
                              &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;
  
  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, NULL, NULL, &headers_returned);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      connection_close = dpl_connection_close(headers_returned);
    }

  (void) dpl_log_charged_event(ctx, "REQUEST", "DELETE", 0);

  ret = DPL_SUCCESS;

 end:

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers)
    dpl_dict_free(headers);

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);

  DPRINTF("ret=%d\n", ret);

  return ret;
}
