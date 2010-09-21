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

dpl_canned_acl_t 
dpl_canned_acl(char *str)
{
  if (!strcasecmp(str, "private"))
    return DPL_CANNED_ACL_PRIVATE;
  else if (!strcasecmp(str, "public-read"))
    return DPL_CANNED_ACL_PUBLIC_READ;
  else if (!strcasecmp(str, "public-read-write"))
    return DPL_CANNED_ACL_PUBLIC_READ_WRITE;
  else if (!strcasecmp(str, "autenticated-read"))
    return DPL_CANNED_ACL_AUTHENTICATED_READ;
  else if (!strcasecmp(str, "bucket-owner-read"))
    return DPL_CANNED_ACL_BUCKET_OWNER_READ;
  else if (!strcasecmp(str, "bucket-owner-full-control"))
    return DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL;

  return -1;
}

char *
dpl_canned_acl_str(dpl_canned_acl_t canned_acl)
{
  switch (canned_acl)
    {
    case DPL_CANNED_ACL_PRIVATE:
      return "private";
    case DPL_CANNED_ACL_PUBLIC_READ:
      return "public-read";
    case DPL_CANNED_ACL_PUBLIC_READ_WRITE:
      return "public-read-write";
    case DPL_CANNED_ACL_AUTHENTICATED_READ:
      return "authenticated-read";
    case DPL_CANNED_ACL_BUCKET_OWNER_READ:
      return "bucket-owner-read";
    case DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL:
      return "bucket-owner-full-control";
    }
  
  return NULL;
}

/** 
 * parse a string of the form metadata1=value1;metadata2=value2...
 * 
 * @param metadata 
 * 
 * @return 
 */
dpl_dict_t *
dpl_parse_metadata(char *metadata)
{
  char *saveptr = NULL;
  char *str, *tok, *p;
  int ret;
  dpl_dict_t *dict;
  char *nmetadata;
  
  nmetadata = strdup(metadata);
  if (NULL == nmetadata)
    return NULL;
  
  dict = dpl_dict_new(13);
  if (NULL == dict)
    {
      free(nmetadata);
      return NULL;
    }

  for (str = metadata;;str = NULL)
    {
      tok = strtok_r(str, ";", &saveptr);
      if (NULL == tok)
        break ;

      DPRINTF("tok=%s\n", tok);

      p = index(tok, '=');
      if (NULL == p)
        p = "";
      else
        *p++ = 0;
      
      ret = dpl_dict_add(dict, tok, p, 0);
      if (DPL_SUCCESS != ret)
        {
          dpl_dict_free(dict);
          free(nmetadata);
          return NULL;
        }
    }

  free(nmetadata);

  return dict;
}

dpl_status_t
dpl_add_metadata_to_headers(dpl_dict_t *metadata,
                            dpl_dict_t *headers)
                            
{
  int bucket;
  dpl_var_t *var;
  char header[1024];
  int ret;

  for (bucket = 0;bucket < metadata->n_buckets;bucket++)
    {
      for (var = metadata->buckets[bucket];var;var = var->prev)
        {
          snprintf(header, sizeof (header), "x-amz-meta-%s", var->key);

          ret = dpl_dict_add(headers, header, var->value, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }
    }

  return DPL_SUCCESS;
}

/** 
 * put a resource
 * 
 * @param ctx 
 * @param bucket 
 * @param resource 
 * @param subresource can be NULL
 * @param metadata can be NULL
 * @param canned_acl 
 * @param data_buf 
 * @param data_len 
 * 
 * @return 
 */
dpl_status_t
dpl_put(dpl_ctx_t *ctx,
        char *bucket,
        char *resource,
        char *subresource,
        dpl_dict_t *metadata,
        dpl_canned_acl_t canned_acl,
        char *data_buf,
        u_int data_len)
{
  char          host[1024];
  char          data_len_str[64];
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
  char          *acl_str;
  dpl_dict_t    *headers = NULL;
  dpl_dict_t    *headers_returned = NULL;

  snprintf(host, sizeof (host), "%s.%s", bucket, ctx->host);

  DPL_TRACE(ctx, DPL_TRACE_S3, "put bucket=%s resource=%s", bucket, resource);

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

  snprintf(data_len_str, sizeof (data_len_str), "%u", data_len);
  ret2 = dpl_dict_add(headers, "Content-Length", data_len_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  acl_str = dpl_canned_acl_str(canned_acl);
  if (NULL == acl_str)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "x-amz-acl", acl_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != metadata)
    {
      ret2 = dpl_add_metadata_to_headers(metadata, headers);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
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
                              "PUT",
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

  //buffer
  iov[n_iov].iov_base = data_buf;
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

  if (NULL != headers)
    dpl_dict_free(headers);

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_put_buffered(dpl_ctx_t *ctx,
                 char *bucket,
                 char *resource,
                 char *subresource,
                 dpl_dict_t *metadata,
                 dpl_canned_acl_t canned_acl,
                 u_int data_len,
                 dpl_conn_t **connp)
{
  char          host[1024];
  char          data_len_str[64];
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
  char          *acl_str;
  dpl_dict_t    *headers = NULL;
  dpl_dict_t    *headers_returned = NULL;

  snprintf(host, sizeof (host), "%s.%s", bucket, ctx->host);

  DPL_TRACE(ctx, DPL_TRACE_S3, "put_buffered bucket=%s resource=%s", bucket, resource);

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

  ret2 = dpl_dict_add(headers, "Expect", "100-continue", 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  snprintf(data_len_str, sizeof (data_len_str), "%u", data_len);
  ret2 = dpl_dict_add(headers, "Content-Length", data_len_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  acl_str = dpl_canned_acl_str(canned_acl);
  if (NULL == acl_str)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "x-amz-acl", acl_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != metadata)
    {
      ret2 = dpl_add_metadata_to_headers(metadata, headers);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
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
                              "PUT",
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
      if (NULL != headers_returned) //possible if continue succeeded
        connection_close = dpl_connection_close(headers_returned);
    }

  (void) dpl_log_charged_event(ctx, "DATA", "IN", data_len);

  if (NULL != connp)
    {
      *connp = conn;
      conn = NULL; //consume it
    }

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
