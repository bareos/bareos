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
#include "dropletp.h"
#include <droplet/cdmi/cdmi.h>
#include <droplet/cdmi/object_id.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

const char *
dpl_cdmi_who_str(dpl_ace_who_t who)
{
  switch (who)
    {
    case DPL_ACE_WHO_OWNER:
      return "OWNER@";
    case DPL_ACE_WHO_GROUP:
      return "GROUP@";
    case DPL_ACE_WHO_EVERYONE:
      return "EVERYONE@";
    case DPL_ACE_WHO_ANONYMOUS:
      return "ANONYMOUS@";
    case DPL_ACE_WHO_AUTHENTICATED:
      return "AUTHENTICATED@";
    case DPL_ACE_WHO_ADMINISTRATOR:
      return "ADMINISTRATOR@";
    case DPL_ACE_WHO_ADMINUSERS:
      return "ADMINUSERS@";
    case DPL_ACE_WHO_INTERACTIVE:
    case DPL_ACE_WHO_NETWORK:
    case DPL_ACE_WHO_DIALUP:
    case DPL_ACE_WHO_BATCH:
    case DPL_ACE_WHO_SERVICE:
      return NULL;
    }

  return NULL;
}

static dpl_status_t
add_sysmd_to_req(const dpl_sysmd_t *sysmd,
                 dpl_req_t *req)
{
  dpl_status_t ret, ret2;
  char buf[256];
  dpl_dict_t *tmp_dict = NULL;
  dpl_dict_t *tmp_dict2 = NULL;
  dpl_vec_t *tmp_vec = NULL;
  int do_acl = 0;
  uint32_t n_aces;
  dpl_ace_t aces[DPL_SYSMD_ACE_MAX], *acesp;
  
  tmp_dict = dpl_dict_new(13);
  if (NULL == tmp_dict)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (sysmd->mask & DPL_SYSMD_MASK_SIZE)
    {
      /* optional (computed remotely by storage) */
      snprintf(buf, sizeof (buf), "%ld", sysmd->size);
      
      ret2 = dpl_dict_add(tmp_dict, "cdmi_size", buf, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (sysmd->mask & DPL_SYSMD_MASK_ATIME)
    {
      /* optional */
      ret2 = dpl_timetoiso8601(sysmd->atime, buf, sizeof (buf));
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
      
      ret2 = dpl_dict_add(tmp_dict, "cdmi_atime", buf, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (sysmd->mask & DPL_SYSMD_MASK_MTIME)
    {
      /* optional */
      ret2 = dpl_timetoiso8601(sysmd->mtime, buf, sizeof (buf));
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_dict_add(tmp_dict, "cdmi_mtime", buf, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (sysmd->mask & DPL_SYSMD_MASK_CTIME)
    {
      /* optional */
      ret2 = dpl_timetoiso8601(sysmd->ctime, buf, sizeof (buf));
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_dict_add(tmp_dict, "cdmi_ctime", buf, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (sysmd->mask & DPL_SYSMD_MASK_OWNER)
    {
      ret2 = dpl_dict_add(tmp_dict, "cdmi_owner", sysmd->owner, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  
  if (sysmd->mask & DPL_SYSMD_MASK_CANNED_ACL)
    {
      switch (sysmd->canned_acl)
        {
        case DPL_CANNED_ACL_UNDEF:
          n_aces = 0;
          break ;
        case DPL_CANNED_ACL_PRIVATE:
          aces[0].type = DPL_ACE_TYPE_ALLOW;
          aces[0].flag = 0;
          aces[0].access_mask = DPL_ACE_MASK_ALL;
          aces[0].who = DPL_ACE_WHO_OWNER;
          n_aces = 1;
          break ;
        case DPL_CANNED_ACL_PUBLIC_READ:
          aces[0].type = DPL_ACE_TYPE_ALLOW;
          aces[0].flag = 0;
          aces[0].access_mask = DPL_ACE_MASK_READ_OBJECT|
            DPL_ACE_MASK_EXECUTE;
          aces[0].who = DPL_ACE_WHO_EVERYONE;
          n_aces = 1;
          break ;
        case DPL_CANNED_ACL_PUBLIC_READ_WRITE:
          aces[0].type = DPL_ACE_TYPE_ALLOW;
          aces[0].flag = 0;
          aces[0].access_mask = DPL_ACE_MASK_RW_ALL;
          aces[0].who = DPL_ACE_WHO_EVERYONE;
          n_aces = 1;
          break ;
        case DPL_CANNED_ACL_AUTHENTICATED_READ:
          aces[0].type = DPL_ACE_TYPE_ALLOW;
          aces[0].flag = 0;
          aces[0].access_mask = DPL_ACE_MASK_READ_ALL;
          aces[0].who = DPL_ACE_WHO_AUTHENTICATED;
          n_aces = 1;
          break ;
        case DPL_CANNED_ACL_BUCKET_OWNER_READ:
          //XXX ?
          break ;
        case DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL:
          //XXX ?
          break ;
        }

      acesp = aces;
      do_acl = 1;
    }

  if (sysmd->mask & DPL_SYSMD_MASK_ACL)
    {
      n_aces = sysmd->n_aces;
      acesp = (dpl_ace_t *) sysmd->aces;
      do_acl = 1;
    }

  if (do_acl)
    {
      int i;
      dpl_value_t value;

      tmp_vec = dpl_vec_new(2, 2);
      if (NULL == tmp_vec)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      for (i = 0;i < n_aces;i++)
        {
          const char *str;
          char buf[256];
          dpl_value_t row_value;

          tmp_dict2 = dpl_dict_new(13);
          if (NULL == tmp_dict2)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
  
          str = dpl_cdmi_who_str(acesp[i].who);
          if (NULL == str)
            {
              ret = DPL_EINVAL;
              goto end;
            }

          ret2 = dpl_dict_add(tmp_dict2, "identifier", str, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          snprintf(buf, sizeof (buf), "0x%08x", acesp[i].type);

          ret2 = dpl_dict_add(tmp_dict2, "acetype", buf, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          snprintf(buf, sizeof (buf), "0x%08x", acesp[i].flag);

          ret2 = dpl_dict_add(tmp_dict2, "aceflags", buf, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          snprintf(buf, sizeof (buf), "0x%08x", acesp[i].access_mask);

          ret2 = dpl_dict_add(tmp_dict2, "acemask", buf, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          row_value.type = DPL_VALUE_SUBDICT;
          row_value.subdict = tmp_dict2;
          ret2 = dpl_vec_add_value(tmp_vec, &row_value);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          dpl_dict_free(tmp_dict2);
          tmp_dict2 = NULL;
        }

      value.type = DPL_VALUE_VECTOR;
      value.vector = tmp_vec;
      ret2 = dpl_dict_add_value(tmp_dict, "cdmi_acl", &value, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      dpl_vec_free(tmp_vec);
      tmp_vec = NULL;
    }

  if (sysmd->mask & DPL_SYSMD_MASK_ID)
    {
      //XXX extension ???
    }

  //dpl_dict_print(tmp_dict, stderr, 0);

  ret2 = dpl_req_add_metadata(req, tmp_dict);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
                            
  ret = 0;

 end:

  if (NULL != tmp_vec)
    dpl_vec_free(tmp_vec);

  if (NULL != tmp_dict2)
    dpl_dict_free(tmp_dict2);

  if (NULL != tmp_dict)
    dpl_dict_free(tmp_dict);

  return ret;
}

dpl_status_t
dpl_cdmi_req_set_resource(dpl_req_t *req,
                          const char *resource)
{
  char *nstr;
  int len;
  dpl_status_t ret;

  nstr = strdup(resource);
  if (NULL == nstr)
    return DPL_ENOMEM;

  len = strlen(nstr);

  //suppress special chars
  if ('?' == nstr[len - 1])
    nstr[len - 1] = 0;

  ret = dpl_req_set_resource(req, nstr);

  free(nstr);

  return ret;
}

dpl_status_t
dpl_cdmi_make_bucket(dpl_ctx_t *ctx,
                     const char *bucket,
                     const dpl_sysmd_t *sysmd,
                     char **locationp)
{
  return dpl_mkdir(ctx, bucket);
}

dpl_status_t
dpl_cdmi_list_bucket(dpl_ctx_t *ctx,
                     const char *bucket,
                     const char *prefix,
                     const char *delimiter,
                     dpl_vec_t **objectsp,
                     dpl_vec_t **common_prefixesp,
                     char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  char          *data_buf = NULL;
  u_int         data_len;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_vec_t     *common_prefixes = NULL;
  dpl_vec_t     *objects = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  ret2 = dpl_cdmi_req_set_resource(req, NULL != prefix ? prefix : "/");
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_set_object_type(req, DPL_FTYPE_DIR);

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, &data_buf, &data_len, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  (void) dpl_log_event(ctx, "DATA", "OUT", data_len);

  objects = dpl_vec_new(2, 2);
  if (NULL == objects)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  common_prefixes = dpl_vec_new(2, 2);
  if (NULL == common_prefixes)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_cdmi_parse_list_bucket(ctx, data_buf, data_len, prefix, objects, common_prefixes);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != objectsp)
    {
      *objectsp = objects;
      objects = NULL; //consume it
    }

  if (NULL != common_prefixesp)
    {
      *common_prefixesp = common_prefixes;
      common_prefixes = NULL; //consume it
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != objects)
    dpl_vec_objects_free(objects);

  if (NULL != common_prefixes)
    dpl_vec_common_prefixes_free(common_prefixes);

  if (NULL != data_buf)
    free(data_buf);

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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_post(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              dpl_option_t *option,
              dpl_ftype_t object_type,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd,
              const char *data_buf,
              unsigned int data_len,
              const dpl_dict_t *query_params,
              char **resource_idp,
              char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;
  char          *body_str = NULL;
  int           body_len = 0;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_POST);

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_set_object_type(req, object_type);

  if (NULL != data_buf)
    {
      chunk.buf = data_buf;
      chunk.len = data_len;
      dpl_req_set_chunk(req, &chunk);
    }

  dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);

  if (NULL != sysmd)
    {
      ret2 = add_sysmd_to_req(sysmd, req);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != metadata)
    {
      ret2 = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, &body_str, &body_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
  iov[n_iov].iov_base = body_str;
  iov[n_iov].iov_len = body_len;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  (void) dpl_log_event(ctx, "DATA", "IN", data_len);

  ret = DPL_SUCCESS;

 end:

  if (NULL != body_str)
    free(body_str);

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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_post_buffered(dpl_ctx_t *ctx,
                       const char *bucket,
                       const char *resource,
                       const char *subresource,
                       dpl_option_t *option,
                       dpl_ftype_t object_type,
                       const dpl_dict_t *metadata,
                       const dpl_sysmd_t *sysmd,
                       unsigned int data_len,
                       const dpl_dict_t *query_params,
                       dpl_conn_t **connp,
                       char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t    *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_POST);

  if (NULL != bucket)
    {
      ret2 = dpl_req_set_bucket(req, bucket);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  chunk.buf = NULL;
  chunk.len = data_len;
  dpl_req_set_chunk(req, &chunk);

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;
    }

  dpl_req_set_object_type(req, object_type);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_EXPECT);

  if (NULL != sysmd)
    {
      ret2 = add_sysmd_to_req(sysmd, req);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != metadata)
    {
      ret2 = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_build(req, req_mask, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  (void) dpl_log_event(ctx, "DATA", "IN", data_len);

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

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_put(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             dpl_option_t *option,
             dpl_ftype_t object_type,
             const dpl_dict_t *metadata,
             const dpl_sysmd_t *sysmd,
             const char *data_buf,
             unsigned int data_len,
             char **locationp)
{
  return dpl_cdmi_put_range(ctx, bucket, resource, subresource, option,
                            object_type, NULL, 
                            metadata, sysmd,
                            data_buf, data_len, 
                            locationp);
}

dpl_status_t
dpl_cdmi_put_range(dpl_ctx_t *ctx,
                   const char *bucket,
                   const char *resource,
                   const char *subresource,
                   dpl_option_t *option,
                   dpl_ftype_t object_type,
                   const dpl_range_t *range,
                   const dpl_dict_t *metadata,
                   const dpl_sysmd_t *sysmd,
                   const char *data_buf,
                   unsigned int data_len,
                   char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;
  char          *body_str = NULL;
  int           body_len = 0;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_set_object_type(req, object_type);

  if (NULL != data_buf)
    {
      chunk.buf = data_buf;
      chunk.len = data_len;
      dpl_req_set_chunk(req, &chunk);
    }

  dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);

  if (NULL != sysmd)
    {
      ret2 = add_sysmd_to_req(sysmd, req);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != metadata)
    {
      ret2 = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, &body_str, &body_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
  iov[n_iov].iov_base = body_str;
  iov[n_iov].iov_len = body_len;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  (void) dpl_log_event(ctx, "DATA", "IN", data_len);

  ret = DPL_SUCCESS;

 end:

  if (NULL != body_str)
    free(body_str);

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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_put_buffered(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *resource,
                      const char *subresource,
                      dpl_option_t *option,
                      dpl_ftype_t object_type,
                      const dpl_dict_t *metadata,
                      const dpl_sysmd_t *sysmd,
                      unsigned int data_len,
                      dpl_conn_t **connp,
                      char **locationp)
{
  return dpl_cdmi_put_range_buffered(ctx, bucket, resource, subresource, option,
                                     object_type, NULL,
                                     metadata, sysmd, data_len,
                                     connp, locationp);
}

dpl_status_t
dpl_cdmi_put_range_buffered(dpl_ctx_t *ctx,
                            const char *bucket,
                            const char *resource,
                            const char *subresource,
                            dpl_option_t *option,
                            dpl_ftype_t object_type,
                            const dpl_range_t *range,
                            const dpl_dict_t *metadata,
                            const dpl_sysmd_t *sysmd,
                            unsigned int data_len,
                            dpl_conn_t **connp,
                            char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t    *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  if (NULL != bucket)
    {
      ret2 = dpl_req_set_bucket(req, bucket);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  chunk.buf = NULL;
  chunk.len = data_len;
  dpl_req_set_chunk(req, &chunk);

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;
      else
        {
          ret = DPL_ENOTSUPP;
          goto end;
        }
    }

  dpl_req_set_object_type(req, object_type);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_EXPECT);

  if (NULL != sysmd)
    {
      ret2 = add_sysmd_to_req(sysmd, req);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != metadata)
    {
      ret2 = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  (void) dpl_log_event(ctx, "DATA", "IN", data_len);

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

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

/**/

struct mdparse_data
{
  dpl_dict_t *metadata;
  dpl_metadatum_func_t metadatum_func;
  void *cb_arg;
};

dpl_status_t 
cb_metadata_list(dpl_dict_var_t *var,
                 void *cb_arg)
{
  struct mdparse_data *arg = (struct mdparse_data *) cb_arg;
  dpl_status_t ret, ret2;

  if (DPL_VALUE_STRING != var->val->type)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  if (arg->metadatum_func)
    {
      dpl_value_t val;
      
      val.type = DPL_VALUE_STRING;
      val.string = var->val->string;
      ret2 = arg->metadatum_func(arg->cb_arg, var->key, &val);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  
  ret2 = dpl_dict_add(arg->metadata, var->key, var->val->string, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

/** 
 * parse a value into a suitable metadata or sysmd
 * 
 * @param key 
 * @param val 
 * @param metadatum_func
 * @param cb_arg
 * @param metadata 
 * @param sysmdp 
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_get_metadatum_from_value(const char *key,
                                  dpl_value_t *val,
                                  dpl_metadatum_func_t metadatum_func,
                                  void *cb_arg,
                                  dpl_dict_t *metadata,
                                  dpl_sysmd_t *sysmdp)
{
  dpl_status_t ret, ret2;
  dpl_dict_var_t *var;
  dpl_cdmi_object_id_t obj_id;

  if (sysmdp)
    {
      if (!strcmp(key, "objectID"))
        {
          if (DPL_VALUE_STRING != val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          ret2 = dpl_cdmi_string_to_object_id(val->string, &obj_id);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          ret2 = dpl_cdmi_opaque_to_string(&obj_id, sysmdp->id);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          sysmdp->mask |= DPL_SYSMD_MASK_ID;
          
          sysmdp->enterprise_number = obj_id.enterprise_number;
          sysmdp->mask |= DPL_SYSMD_MASK_ENTERPRISE_NUMBER;
        }
      else if (!strcmp(key, "parentID"))
        {
          if (DPL_VALUE_STRING != val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          ret2 = dpl_cdmi_string_to_object_id(val->string, &obj_id);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          ret2 = dpl_cdmi_opaque_to_string(&obj_id, sysmdp->parent_id);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          sysmdp->mask |= DPL_SYSMD_MASK_PARENT_ID;
        }
      else if (!strcmp(key, "objectType"))
        {
          if (DPL_VALUE_STRING != val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          sysmdp->mask |= DPL_SYSMD_MASK_FTYPE;
          sysmdp->ftype = dpl_cdmi_content_type_to_ftype(val->string);
        }
    }

  if (!strcmp(key, "metadata"))
    {
      //this is the metadata object
      if (DPL_VALUE_SUBDICT != val->type)
        {
          ret = DPL_EINVAL;
          goto end;
        }

      if (sysmdp)
        {
          //some sysmds are stored in metadata
          
          var = dpl_dict_get(val->subdict, "cdmi_mtime");
          if (NULL != var)
            {
              if (DPL_VALUE_STRING != var->val->type)
                {
                  ret = DPL_EINVAL;
                  goto end;
                }
              
              sysmdp->mask |= DPL_SYSMD_MASK_MTIME;
              sysmdp->mtime = dpl_iso8601totime(var->val->string);
            }
          
          var = dpl_dict_get(val->subdict, "cdmi_atime");
          if (NULL != var)
            {
              if (DPL_VALUE_STRING != var->val->type)
                {
                  ret = DPL_EINVAL;
                  goto end;
                }
              
              sysmdp->mask |= DPL_SYSMD_MASK_ATIME;
              sysmdp->atime = dpl_iso8601totime(var->val->string);
            }
          
          var = dpl_dict_get(val->subdict, "cdmi_size");
          if (NULL != var)
            {
              if (DPL_VALUE_STRING != var->val->type)
                {
                  ret = DPL_EINVAL;
                  goto end;
                }
              
              sysmdp->mask |= DPL_SYSMD_MASK_SIZE;
              sysmdp->size = strtoull(var->val->string, NULL, 0);
            }
        }

      if (metadata)
        {
          struct mdparse_data arg;
          
          arg.metadatum_func = metadatum_func;
          arg.metadata = metadata;
          arg.cb_arg = cb_arg;

          //iterate metadata object
          ret2 = dpl_dict_iterate(val->subdict, cb_metadata_list, &arg);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
    }
  
  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

/** 
 * common routine for x-object-meta-* and x-container-meta-*
 * 
 * @param string 
 * @param value 
 * @param metadatum_func 
 * @param cb_arg 
 * @param metadata 
 * @param sysmdp 
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_get_metadatum_from_string(const char *key,
                                   const char *value,
                                   dpl_metadatum_func_t metadatum_func,
                                   void *cb_arg,
                                   dpl_dict_t *metadata,
                                   dpl_sysmd_t *sysmdp)
{
  dpl_status_t ret, ret2;
  dpl_value_t *val = NULL;
  
  //XXX convert

  ret2 = dpl_cdmi_get_metadatum_from_value(key, val, 
                                           metadatum_func, cb_arg,
                                           metadata, sysmdp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != val)
    dpl_value_free(val);

  return ret;
}

/** 
 * parse a HTTP header into a suitable metadata or sysmd
 * 
 * @param header 
 * @param value 
 * @param metadatum_func optional
 * @param cb_arg for metadatum_func
 * @param metadata optional
 * @param sysmdp optional
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_get_metadatum_from_header(const char *header,
                                   const char *value,
                                   dpl_metadatum_func_t metadatum_func,
                                   void *cb_arg,
                                   dpl_dict_t *metadata,
                                   dpl_sysmd_t *sysmdp)
{
  dpl_status_t ret, ret2;

  if (!strncmp(header, X_OBJECT_META_PREFIX, strlen(X_OBJECT_META_PREFIX)))
    {
      char *key;

      key = (char *) header + strlen(X_OBJECT_META_PREFIX);

      ret2 = dpl_cdmi_get_metadatum_from_string(key, value, 
                                                metadatum_func, cb_arg,
                                                metadata, sysmdp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else if (!strncmp(header, X_CONTAINER_META_PREFIX, strlen(X_CONTAINER_META_PREFIX)))
    {
      char *key;

      key = (char *) header + strlen(X_CONTAINER_META_PREFIX);

      ret2 = dpl_cdmi_get_metadatum_from_string(key, value, 
                                                metadatum_func, cb_arg,
                                                metadata, sysmdp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else
    {
      if (sysmdp)
        {
          if (!strcmp(header, "content-length"))
            {
              sysmdp->mask |= DPL_SYSMD_MASK_SIZE;
              sysmdp->size = atoi(value);
            }
          
          if (!strcmp(header, "last-modified"))
            {
              sysmdp->mask |= DPL_SYSMD_MASK_MTIME;
              sysmdp->mtime = dpl_get_date(value, NULL);
            }
          
          if (!strcmp(header, "etag"))
            {
              int value_len = strlen(value);
              
              if (value_len < DPL_SYSMD_ETAG_SIZE && value_len >= 2)
                {
                  sysmdp->mask |= DPL_SYSMD_MASK_ETAG;
                  //supress double quotes
                  strncpy(sysmdp->etag, value + 1, DPL_SYSMD_ETAG_SIZE);
                  sysmdp->etag[value_len-2] = 0;
                }
            }
        }
    }
    
  ret = DPL_SUCCESS;

 end:

  return ret;
}

struct metadata_conven
{
  dpl_dict_t *metadata;
  dpl_sysmd_t *sysmdp;
};

static dpl_status_t
cb_headers_iterate(dpl_dict_var_t *var,
                   void *cb_arg)
{
  struct metadata_conven *mc = (struct metadata_conven *) cb_arg;
  
  assert(var->val->type == DPL_VALUE_STRING);
  return dpl_cdmi_get_metadatum_from_header(var->key,
                                            var->val->string,
                                            NULL,
                                            NULL,
                                            mc->metadata,
                                            mc->sysmdp);
}

/** 
 * get metadata from headers
 * 
 * @param headers 
 * @param metadatap 
 * @param sysmdp 
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_get_metadata_from_headers(const dpl_dict_t *headers,
                                   dpl_dict_t **metadatap,
                                   dpl_sysmd_t *sysmdp)
{
  dpl_dict_t *metadata = NULL;
  dpl_status_t ret, ret2;
  struct metadata_conven mc;

  if (metadatap)
    {
      metadata = dpl_dict_new(13);
      if (NULL == metadata)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  memset(&mc, 0, sizeof (mc));
  mc.metadata = metadata;
  mc.sysmdp = sysmdp;

  if (sysmdp)
    sysmdp->mask = 0;
      
  ret2 = dpl_dict_iterate(headers, cb_headers_iterate, &mc);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return ret;
}

static dpl_status_t
cb_values_iterate(dpl_dict_var_t *var,
                  void *cb_arg)
{
  struct metadata_conven *mc = (struct metadata_conven *) cb_arg;
  
  return dpl_cdmi_get_metadatum_from_value(var->key,
                                           var->val,
                                           NULL,
                                           NULL,
                                           mc->metadata,
                                           mc->sysmdp);
}

/** 
 * get metadata from values
 * 
 * @param values 
 * @param metadatap 
 * @param sysmdp 
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_get_metadata_from_values(const dpl_dict_t *values,
                                  dpl_dict_t **metadatap,
                                  dpl_sysmd_t *sysmdp)
{
  dpl_dict_t *metadata = NULL;
  dpl_status_t ret, ret2;
  struct metadata_conven mc;

  if (metadatap)
    {
      metadata = dpl_dict_new(13);
      if (NULL == metadata)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  memset(&mc, 0, sizeof (mc));
  mc.metadata = metadata;
  mc.sysmdp = sysmdp;

  if (sysmdp)
    sysmdp->mask = 0;
      
  ret2 = dpl_dict_iterate(values, cb_values_iterate, &mc);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return ret;
}

dpl_status_t
dpl_cdmi_get(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             dpl_option_t *option,
             dpl_ftype_t object_type,
             const dpl_condition_t *condition,
             char **data_bufp,
             unsigned int *data_lenp,
             dpl_dict_t **metadatap,
             dpl_sysmd_t *sysmdp,
             char **locationp)
{
  return dpl_cdmi_get_range(ctx, bucket, resource, subresource, option,
                            object_type, condition, NULL,
                            data_bufp, data_lenp,
                            metadatap, sysmdp,
                            locationp);
}

dpl_status_t
dpl_cdmi_get_range(dpl_ctx_t *ctx,
                   const char *bucket,
                   const char *resource,
                   const char *subresource,
                   dpl_option_t *option,
                   dpl_ftype_t object_type,
                   const dpl_condition_t *condition,
                   const dpl_range_t *range,
                   char **data_bufp,
                   unsigned int *data_lenp,
                   dpl_dict_t **metadatap,
                   dpl_sysmd_t *sysmdp,
                   char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  char          *data_buf = NULL;
  u_int         data_len;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_dict_t    *metadata = NULL;
  dpl_req_t     *req = NULL;
  int raw = 0;
  dpl_value_t *val = NULL;
  dpl_dict_var_t *var = NULL;
  int value_len;
  int orig_len;
  char *orig_buf = NULL;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  if (NULL != bucket)
    {
      ret2 = dpl_req_set_bucket(req, bucket);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != condition)
    {
      dpl_req_set_condition(req, condition);
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;

      if (option->mask & DPL_OPTION_RAW)
        raw = 1;
    }

  dpl_req_set_object_type(req, object_type);

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, &data_buf, &data_len, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (req_mask & DPL_CDMI_REQ_HTTP_COMPAT)
    {
      //metadata are in headers
      ret2 = dpl_cdmi_get_metadata_from_headers(headers_reply, metadatap, sysmdp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else
    {
      if (!raw)
        {
          char *tmp;

          //extract data+metadata from json
          ret2 = dpl_cdmi_parse_json_buffer(ctx, data_buf, data_len, &val);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          if (DPL_VALUE_SUBDICT != val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          //find the value object
          ret2 = dpl_dict_get_lowered(val->subdict, "value", &var);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          if (DPL_VALUE_STRING != var->val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          //decode base64
          value_len = strlen(var->val->string);
          if (value_len == 0)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          orig_len = DPL_BASE64_ORIG_LENGTH(value_len);
          orig_buf = malloc(orig_len);
          if (NULL == orig_buf)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          
          orig_len = dpl_base64_decode((u_char *) var->val->string, value_len, (u_char *) orig_buf);
          
          //swap pointers
          tmp = data_buf;
          data_buf = orig_buf;
          orig_buf = tmp;
          data_len = orig_len;
        }
    }

  (void) dpl_log_event(ctx, "DATA", "OUT", data_len);

  if (NULL != data_bufp)
    {
      *data_bufp = data_buf;
      data_buf = NULL; //consume it
    }

  if (NULL != data_lenp)
    *data_lenp = data_len;

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL; //consume it
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != orig_buf)
    free(orig_buf);

  if (NULL != val)
    dpl_value_free(val);

  if (NULL != data_buf)
    free(data_buf);

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != metadata)
    dpl_dict_free(metadata);

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

struct get_conven
{
  dpl_dict_t *metadata;
  dpl_sysmd_t *sysmdp;
  dpl_metadatum_func_t metadatum_func;
  dpl_buffer_func_t buffer_func;
  void *cb_arg;
  int http_compat;
  int connection_close;
};

static dpl_status_t
cb_get_header(void *cb_arg,
              const char *header,
              const char *value)
{
  struct get_conven *gc = (struct get_conven *) cb_arg;
  dpl_status_t ret, ret2;

  if (!strcasecmp(header, "connection"))
    {
      if (!strcasecmp(value, "close"))
        gc->connection_close = 1;
    }
  else
    {
      if (gc->http_compat)
        {
          ret2 = dpl_cdmi_get_metadatum_from_header(header,
                                                    value,
                                                    gc->metadatum_func,
                                                    gc->cb_arg,
                                                    gc->metadata,
                                                    gc->sysmdp);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

static dpl_status_t
cb_get_buffer(void *cb_arg,
              char *buf,
              unsigned int len)
{
  struct get_conven *gc = (struct get_conven *) cb_arg;
  int ret;

  if (NULL != gc->buffer_func)
    {
      //XXX transform buffer if JSON

      ret = gc->buffer_func(gc->cb_arg, buf, len);
      if (DPL_SUCCESS != ret)
        return ret;
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_cdmi_get_buffered(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *resource,
                      const char *subresource,
                      dpl_option_t *option,
                      dpl_ftype_t object_type,
                      const dpl_condition_t *condition,
                      dpl_metadatum_func_t metadatum_func,
                      dpl_dict_t **metadatap,
                      dpl_sysmd_t *sysmdp,
                      dpl_buffer_func_t buffer_func,
                      void *cb_arg,
                      char **locationp)
{
  return dpl_cdmi_get_range_buffered(ctx, bucket, resource, subresource, option,
                                     object_type, condition, NULL, 
                                     metadatum_func, metadatap, sysmdp, buffer_func,
                                     cb_arg, locationp);
}

dpl_status_t
dpl_cdmi_get_range_buffered(dpl_ctx_t *ctx,
                            const char *bucket,
                            const char *resource,
                            const char *subresource,
                            dpl_option_t *option,
                            dpl_ftype_t object_type,
                            const dpl_condition_t *condition,
                            const dpl_range_t *range,
                            dpl_metadatum_func_t metadatum_func,
                            dpl_dict_t **metadatap,
                            dpl_sysmd_t *sysmdp,
                            dpl_buffer_func_t buffer_func,
                            void *cb_arg,
                            char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_req_t     *req = NULL;
  struct get_conven gc;
  int http_status;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  memset(&gc, 0, sizeof (gc));
  gc.metadata = dpl_dict_new(13);
  if (NULL == gc.metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }
  gc.sysmdp = sysmdp;
  gc.metadatum_func = metadatum_func;
  gc.buffer_func = buffer_func;
  gc.cb_arg = cb_arg;

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  if (NULL != bucket)
    {
      ret2 = dpl_req_set_bucket(req, bucket);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != condition)
    {
      dpl_req_set_condition(req, condition);
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        {
          req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;
          gc.http_compat = 1;
        }
    }

  if (!(req_mask & DPL_CDMI_REQ_HTTP_COMPAT))
    {
      //XXX dont known how to desencapsulate data from buffered json yet
      ret = DPL_EINVAL;
      goto end;
    }

  //build request
  ret2 = dpl_cdmi_req_build(req, req_mask, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
      gc.connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply_buffered(conn, 1, &http_status, cb_get_header, cb_get_buffer, &gc);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "read http answer failed");
      gc.connection_close = 1;
      ret = ret2;
      goto end;
    }

  if (!conn->ctx->keep_alive)
    gc.connection_close = 1;

  if (!gc.http_compat)
    {
      //XXX fetch metadata from overall buffer
      ret = DPL_ENOTSUPP;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = gc.metadata;
      gc.metadata = NULL;
    }
  
  //map http_status to relevant value
  ret = dpl_map_http_status(http_status);

  //caller is responsible for logging the event

  ret = DPL_SUCCESS;

 end:

  if (NULL != conn)
    {
      if (1 == gc.connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != gc.metadata)
    dpl_dict_free(gc.metadata);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_head_all(dpl_ctx_t *ctx,
                  const char *bucket,
                  const char *resource,
                  const char *subresource,
                  dpl_option_t *option,
                  dpl_ftype_t object_type,
                  const dpl_condition_t *condition,
                  dpl_dict_t **metadatap,
                  char **locationp)
{
  int ret, ret2;
  char *md_buf = NULL;
  u_int md_len;
  dpl_value_t *val = NULL;
  dpl_option_t option2;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");
  
  //fetch metadata from JSON content
  option2.mask |= DPL_OPTION_RAW;
  ret2 = dpl_cdmi_get(ctx, bucket, resource, NULL != subresource ? subresource : "metadata;objectID;parentID;objectType", &option2, object_type, condition, &md_buf, &md_len, NULL, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  ret2 = dpl_cdmi_parse_json_buffer(ctx, md_buf, md_len, &val);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_VALUE_SUBDICT != val->type)
    {
      ret = DPL_EINVAL;
      goto end;
    }
  
  if (NULL != metadatap)
    {
      *metadatap = val->subdict;
      val->subdict = NULL;
    }
  
  ret = DPL_SUCCESS;
  
 end:

  if (NULL != val)
    dpl_value_free(val);

  if (NULL != md_buf)
    free(md_buf);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_cdmi_head(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              dpl_option_t *option,
              dpl_ftype_t object_type,
              const dpl_condition_t *condition,
              dpl_dict_t **metadatap,
              dpl_sysmd_t *sysmdp,
              char **locationp)
{
  int ret, ret2;
  dpl_dict_t *all_mds = NULL;
  dpl_dict_t *metadata = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  ret2 = dpl_cdmi_head_all(ctx, bucket, resource, subresource, option, object_type, condition, &all_mds, locationp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_get_metadata_from_values(all_mds, &metadata, sysmdp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  if (NULL != all_mds)
    dpl_dict_free(all_mds);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_delete(dpl_ctx_t *ctx,
                const char *bucket,
                const char *resource,
                const char *subresource,
                dpl_option_t *option,
                dpl_ftype_t object_type,
                char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_DELETE);

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;

  //build request
  ret2 = dpl_cdmi_req_build(req, req_mask, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  (void) dpl_log_event(ctx, "REQUEST", "DELETE", 0);

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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_copy(dpl_ctx_t *ctx,
              const char *src_bucket,
              const char *src_resource,
              const char *src_subresource,
              const char *dst_bucket,
              const char *dst_resource,
              const char *dst_subresource,
              dpl_option_t *option,
              dpl_ftype_t object_type,
              dpl_copy_directive_t copy_directive,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd,
              const dpl_condition_t *condition,
              char **locationp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  char          *body_str = NULL;
  int           body_len = 0;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (DPL_COPY_DIRECTIVE_METADATA_REPLACE == copy_directive)
    return dpl_cdmi_put(ctx, dst_bucket, dst_resource,
                        NULL != dst_subresource ? dst_subresource : "metadata", NULL,
                        object_type, metadata, DPL_CANNED_ACL_UNDEF, NULL, 0, locationp);

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  ret2 = dpl_cdmi_req_set_resource(req, dst_resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != dst_subresource)
    {
      ret2 = dpl_req_set_subresource(req, dst_subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_req_set_src_resource(req, src_resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != src_subresource)
    {
      ret2 = dpl_req_set_src_subresource(req, src_subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  dpl_req_set_copy_directive(req, copy_directive);

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_set_object_type(req, object_type);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);

  if (NULL != sysmd)
    {
      ret2 = add_sysmd_to_req(sysmd, req);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != metadata)
    {
      ret2 = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, &body_str, &body_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_ECONNECT;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
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
  iov[n_iov].iov_base = body_str;
  iov[n_iov].iov_len = body_len;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != body_str)
    free(body_str);

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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_get_id_path(dpl_ctx_t *ctx,
                     const char *bucket,
                     char **id_pathp)
{
  char *id_path;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  id_path = strdup("/cdmi_objectid/");
  if (NULL == id_path)
    return DPL_ENOMEM;
  
  *id_pathp = id_path;

  return DPL_SUCCESS;
}

dpl_status_t
dpl_cdmi_convert_id_to_native(dpl_ctx_t *ctx, 
                              const char *id,
                              uint32_t enterprise_number,
                              char **native_idp)
{
  dpl_cdmi_object_id_t obj_id;
  dpl_status_t ret, ret2;
  char opaque[DPL_CDMI_OBJECT_ID_LEN];
  int opaque_len;
  char native_id[DPL_CDMI_OBJECT_ID_LEN];
  char *str = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  ret2 = dpl_cdmi_string_to_opaque(id, opaque, &opaque_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_object_id_init(&obj_id, enterprise_number, opaque, opaque_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_object_id_to_string(&obj_id, native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  str = strdup(native_id);
  if (NULL == str)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != native_idp)
    {
      *native_idp = str;
      str = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != str)
    free(str);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_convert_native_to_id(dpl_ctx_t *ctx, 
                              const char *native_id,
                              char **idp, 
                              uint32_t *enterprise_numberp)
{
  dpl_status_t ret, ret2;
  dpl_cdmi_object_id_t obj_id;
  char id[DPL_CDMI_OBJECT_ID_LEN]; //at least
  char *str = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  ret2 = dpl_cdmi_string_to_object_id(native_id, &obj_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_opaque_to_string(&obj_id, id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  str = strdup(id);
  if (NULL == str)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != idp)
    {
      *idp = str;
      str = NULL;
    }

  if (NULL != enterprise_numberp)
    *enterprise_numberp = obj_id.enterprise_number;
  
  ret = DPL_SUCCESS;

 end:

  if (NULL != str)
    free(str);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}
