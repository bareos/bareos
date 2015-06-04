/*
 * Copyright (C) 2014 SCALITY SA. All rights reserved.
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

#include <dropletp.h>
#include <droplet/s3/s3.h>

static dpl_status_t
_multipart_parse_init(const dpl_ctx_t *ctx,
                      const char *buf, int len,
                      const char **uploadidp)
{
  dpl_status_t          ret = DPL_SUCCESS;
  xmlParserCtxtPtr      ctxt;
  xmlDocPtr             doc;
  xmlNode               *elem;

  ctxt = xmlNewParserCtxt();
  if (ctxt == NULL)
    return DPL_FAILURE;

  doc = xmlCtxtReadMemory(ctxt, buf, len, NULL, NULL, 0u);
  if (doc == NULL) {
    xmlFreeParserCtxt(ctxt);
    return DPL_FAILURE;
  }

  elem = xmlDocGetRootElement(doc);
  while (elem != NULL)
    {
      if (elem->type == XML_ELEMENT_NODE)
        {
          if (!strcmp((char *) elem->name, "InitiateMultipartUploadResult"))
            {
	      elem = elem->children;
              ret = DPL_FAILURE;
              while (elem != NULL)
                {
                  if (elem->type == XML_ELEMENT_NODE)
                    {
                      if (!strcmp((char *) elem->name, "UploadId"))
                        {
			  ret = DPL_SUCCESS;
                          *uploadidp = strdup((char *) elem->children->content);
                          if (NULL == *uploadidp)
                            ret = DPL_ENOMEM;
                          break;
                        }
                    }
                  elem = elem->next;
                }
              // Reaching here means that we already browsed
              // the InitiateMultipartUploadResult xml subtree,
	      // whether we found UploadId or not.
              break;
            }
        }
      elem = elem->next;
    }

  xmlFreeDoc(doc);
  xmlFreeParserCtxt(ctxt);

  return ret;
}

static dpl_status_t
_multipart_parse_complete(const dpl_ctx_t *ctx,
                          const char *buf, int len)
{
  dpl_status_t          ret = DPL_SUCCESS;
  xmlParserCtxtPtr      ctxt;
  xmlDocPtr             doc;
  xmlNode               *elem;

  if (len == 0)
    return DPL_SUCCESS;

  ctxt = xmlNewParserCtxt();
  if (ctxt == NULL)
    return DPL_FAILURE;

  doc = xmlCtxtReadMemory(ctxt, buf, len, NULL, NULL, 0u);
  if (doc == NULL) {
    xmlFreeParserCtxt(ctxt);
    return DPL_FAILURE;
  }

  elem = xmlDocGetRootElement(doc);
  while (elem != NULL)
    {
      if (elem->type == XML_ELEMENT_NODE)
        {
          if (!strcmp((char *)(elem->name), "Error"))
            {
              ret = DPL_FAILURE;
              break ;
            }
        }
      elem = elem->next;
    }

  xmlFreeDoc(doc);
  xmlFreeParserCtxt(ctxt);

  return ret;
}


static dpl_status_t
multipart_complete_gen_body(const dpl_ctx_t *ctx,
			    struct json_object *parts, unsigned int n_parts,
                            char **bufp, unsigned int *lenp)
{
  dpl_status_t        ret = DPL_FAILURE;
  struct json_object  *part = NULL;
  unsigned int        part_idx = 0;
  const char          *etag;
  char                *buf;
  unsigned int        bufsize = 2048; // Size of allocated buf
  unsigned int        bufcount = 0; // Amount of data in buf
  unsigned int        bufavail = 0;
  unsigned int        written;

/*
 * Use this macro as an expression returning the number of bytes written
 * to the buffer. It manages reallocating the buffer if we are lacking space.
 */
#define ENSURE_SNPRINTF(format, ...)                                          \
      ({                                                                      \
        do {                                                                  \
        bufavail = bufsize - bufcount;                                        \
        written = snprintf(&buf[bufcount], bufavail, format, ## __VA_ARGS__); \
        if (written >= bufavail)                                              \
          {                                                                   \
            bufsize += 512;                                                   \
            char *tmp = realloc(buf, bufsize);                                \
            if (NULL == tmp)                                                  \
              {                                                               \
                ret = DPL_ENOMEM;                                             \
                goto end;                                                     \
              }                                                               \
            buf = tmp;                                                        \
          }                                                                   \
        } while (written >= bufavail);                                        \
      written;})

  buf = malloc(bufsize);
  if (NULL == buf)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  written = ENSURE_SNPRINTF("<CompleteMultipartUpload>\n");
  bufcount += written;

  for (part_idx=0; part_idx < n_parts; ++part_idx)
    {
      part = json_object_array_get_idx(parts, part_idx);
      if (NULL == part || !json_object_is_type(part, json_type_string))
        {
          ret = DPL_FAILURE;
          goto end;
        }

      etag = json_object_get_string(part);
      written = ENSURE_SNPRINTF("  <Part>\n"
                                "    <PartNumber>%u</PartNumber>\n"
                                "    <ETag>\"%s\"</ETag>\n"
                                "  </Part>\n",
                                part_idx + 1, etag);
      bufcount += written;
    }

    written = ENSURE_SNPRINTF("</CompleteMultipartUpload>\n");
    bufcount += written;

    *lenp = bufcount;
    *bufp = buf;
    buf = NULL;

    ret = DPL_SUCCESS;

end:

  if (buf)
    free(buf);

#undef ENSURE_SNPRINTF

  return ret;
}

dpl_status_t
dpl_s3_stream_multipart_init(dpl_ctx_t *ctx,
                             const char *bucket,
                             const char *resource,
                             const char **uploadidp)
{
  dpl_status_t  ret;
  dpl_conn_t    *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  char          *replybuf = NULL;
  unsigned int  replybuflen = 0;

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_POST);

  if (NULL == bucket)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  ret = dpl_req_set_bucket(req, bucket);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_set_subresource(req, "uploads");
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_s3_req_build(req, 0u, &headers_request);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_s3_add_authorization_to_headers(req, headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_gen_http_request(ctx, req, headers_request, NULL,
                                  header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret)
    goto end;

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  ret = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      goto end;
    }

  ret = dpl_read_http_reply_ext(conn, 1, 0, &replybuf, &replybuflen,
                                &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = _multipart_parse_init(ctx, replybuf, replybuflen, uploadidp);
  if (DPL_SUCCESS != ret)
    goto end;

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

  return ret;
}

dpl_status_t
dpl_s3_stream_multipart_complete(dpl_ctx_t *ctx,
                                 const char *bucket,
                                 const char *resource,
                                 const char *uploadid,
                                 struct json_object *parts,
                                 unsigned int n_parts,
                                 const dpl_dict_t *metadata,
                                 const dpl_sysmd_t *sysmd)
{
  dpl_status_t  ret;
  dpl_conn_t    *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  char          subresource[strlen(uploadid) + 10 /* for 'uploadId=' */];
  char          *databuf = NULL;
  unsigned int  databuflen = 0;
  char          *replybuf = NULL;
  unsigned int  replybuflen = 0;

  snprintf(subresource, sizeof(subresource), "uploadId=%s", uploadid);

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_POST);

  if (NULL == bucket)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  ret = dpl_req_set_bucket(req, bucket);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_set_subresource(req, subresource);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = multipart_complete_gen_body(ctx, parts, n_parts,
                                    &databuf, &databuflen);
  if (DPL_SUCCESS != ret)
    goto end;

  dpl_req_set_data(req, databuf, databuflen);
  dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);

  if (sysmd)
    {
      if (sysmd->mask & DPL_SYSMD_MASK_CANNED_ACL)
        dpl_req_set_canned_acl(req, sysmd->canned_acl);

      if (sysmd->mask & DPL_SYSMD_MASK_STORAGE_CLASS)
        dpl_req_set_storage_class(req, sysmd->storage_class);
    }

  if (NULL != metadata)
    {
      ret = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret)
        goto end;
    }
  ret = dpl_s3_req_build(req, 0u, &headers_request);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_s3_add_authorization_to_headers(req, headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_gen_http_request(ctx, req, headers_request, NULL,
                                  header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret)
    goto end;

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  //buffer
  iov[n_iov].iov_base = (void *)databuf;
  iov[n_iov].iov_len = databuflen;
  n_iov++;

  ret = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      goto end;
    }

  ret = dpl_read_http_reply_ext(conn, 1, 0, &replybuf, &replybuflen,
                                 &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = _multipart_parse_complete(ctx, replybuf, replybuflen);
  if (DPL_SUCCESS != ret)
      goto end;

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

  return ret;
}

dpl_status_t
dpl_s3_stream_multipart_put(dpl_ctx_t *ctx,
                            const char *bucket,
                            const char *resource,
                            const char *uploadid,
			    unsigned int partnb,
                            char *buf, unsigned int len,
                            const char **etagp)
{
  dpl_status_t  ret;
  dpl_conn_t    *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_s3_req_mask_t req_mask = 0u;
  dpl_req_t           *req = NULL;
  char          subresource[  11 /* for 'partNumber=' */ + 18 /* max len for %lu */
			    + 10 /* for 'uploadId=' */   + strlen(uploadid)];


  snprintf(subresource, sizeof(subresource), "partNumber=%u&uploadId=%s", partnb, uploadid);

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  if (NULL == bucket)
    {
      ret = DPL_EINVAL;
      goto end;
    }

  ret = dpl_req_set_bucket(req, bucket);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_set_subresource(req, subresource);
  if (DPL_SUCCESS != ret)
    goto end;

  dpl_req_set_data(req, buf, len);
  dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);

  ret = dpl_s3_req_build(req, req_mask, &headers_request);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_s3_add_authorization_to_headers(req, headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret)
    goto end;

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  //buffer
  iov[n_iov].iov_base = (void *)buf;
  iov[n_iov].iov_len = len;
  n_iov++;

  ret = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      goto end;
    }

  ret = dpl_read_http_reply(conn, 1, NULL, NULL,
                             &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret)
    goto end;

  if (etagp)
  {
    /*
     * Etag of the part is returned as a header, as a string value.
     * Thus, it should be surrounded by double-quotes that we need
     * to remove
     */
    dpl_dict_var_t *var = NULL;
    const char *ro_etag = NULL;
    const char *start_etag = NULL;
    const char *end_etag = NULL;

    ret = dpl_dict_get_lowered(headers_reply, "Etag", &var);
    if (ret != DPL_SUCCESS || var == NULL)
      {
        ret = DPL_FAILURE;
        goto end;
      }

    assert(var->val->type == DPL_VALUE_STRING);
    ro_etag = dpl_sbuf_get_str(var->val->string);
    start_etag = strchr(ro_etag, '"');
    if (start_etag == NULL)
      {
	start_etag = ro_etag;
	end_etag = start_etag + strlen(start_etag);
      }
    else
      {
        start_etag += 1; // go after the " char
        end_etag = strchr(start_etag, '"');
      }

    *etagp = strndup(start_etag, end_etag - start_etag);
    if (*etagp == NULL)
      {
        ret = DPL_ENOMEM;
        goto end;
      }
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

  return ret;
}

