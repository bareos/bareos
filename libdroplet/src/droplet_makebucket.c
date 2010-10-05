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

#include "dropletp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/** 
 * make a bucket
 * 
 * @param ctx 
 * @param bucket 
 * @param location_constraint 
 * @param canned_acl 
 * 
 * @return 
 */
dpl_status_t
dpl_make_bucket(dpl_ctx_t *ctx,
                char *bucket,
                dpl_location_constraint_t location_constraint,
                dpl_canned_acl_t canned_acl)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t    *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  char          *location_constraint_str;
  char          data_str[1024];
  u_int         data_len;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;

  DPL_TRACE(ctx, DPL_TRACE_CONV, "makebucket bucket=%s", bucket);

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  ret2 = dpl_req_set_bucket(req, bucket);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_set_resource(req, "/");
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_LOCATION_CONSTRAINT_US_STANDARD == location_constraint)
    {
      data_str[0] = 0;
      data_len = 0;
    }
  else
    {
      location_constraint_str = dpl_location_constraint_str(location_constraint);
      if (NULL == location_constraint_str)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      
      snprintf(data_str, sizeof (data_str), 
               "<CreateBucketConfiguration xmlns=\"http://s3.amazonaws.com/doc/2006-03-01/\">\n"
               "<LocationConstraint>%s</LocationConstraint>\n"
               "</CreateBucketConfiguration>\n",
               location_constraint_str);
      
      data_len = strlen(data_str);
    }

  chunk.buf = data_str;
  chunk.len = data_len;
  dpl_req_set_chunk(req, &chunk);

  dpl_req_set_canned_acl(req, canned_acl);

  //build request
  ret2 = dpl_req_build(req, &headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret2 = dpl_req_gen(req, headers_request, NULL, header, sizeof (header), &header_len);
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

  //buffer
  iov[n_iov].iov_base = data_str;
  iov[n_iov].iov_len = data_len;
  n_iov++;
  
  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, NULL, NULL, &headers_reply);
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
      connection_close = dpl_connection_close(headers_reply);
    }

  (void) dpl_log_charged_event(ctx, "DATA", "IN", data_len);

  ret = DPL_SUCCESS;

 end:

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPRINTF("ret=%d\n", ret);

  return ret;
}
