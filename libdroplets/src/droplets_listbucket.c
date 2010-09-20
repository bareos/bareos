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

void
dpl_object_free(dpl_object_t *object)
{
  if (NULL != object->key)
    free(object->key);
  
  free(object);
}

void 
dpl_vec_objects_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_object_free((dpl_object_t *) vec->array[i]);
  dpl_vec_free(vec);
}

void
dpl_common_prefix_free(dpl_common_prefix_t *common_prefix)
{
  if (NULL != common_prefix->prefix)
    free(common_prefix->prefix);

  free(common_prefix);
}

void 
dpl_vec_common_prefixes_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_common_prefix_free((dpl_common_prefix_t *) vec->array[i]);
  dpl_vec_free(vec);
}

static dpl_status_t
parse_list_bucket_content(xmlNode *node,
                          dpl_vec_t *vec)
{
  xmlNode *tmp;
  dpl_object_t *object = NULL;
  int ret;

  object = malloc(sizeof (*object));
  if (NULL == object)
    goto bad;

  memset(object, 0, sizeof (*object));

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE) 
        {
          DPRINTF("name: %s\n", tmp->name);
          if (!strcmp((char *) tmp->name, "Key"))
            {
              object->key = strdup((char *) tmp->children->content);
              if (NULL == object->key)
                goto bad;
            }
          else if (!strcmp((char *) tmp->name, "LastModified"))
            {
              object->last_modified = dpl_iso8601totime((char *) tmp->children->content);
            }
          else if (!strcmp((char *) tmp->name, "Size"))
            {
              object->size = strtoull((char *) tmp->children->content, NULL, 0);
            }
          
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = dpl_vec_add(vec, object);
  if (DPL_SUCCESS != ret)
    goto bad;

  return DPL_SUCCESS;

 bad:

  if (NULL != object)
    dpl_object_free(object);

  return DPL_FAILURE;
}

static dpl_status_t
parse_list_bucket_common_prefixes(xmlNode *node,
                                  dpl_vec_t *vec)
{
  xmlNode *tmp;
  dpl_common_prefix_t *common_prefix = NULL;
  int ret;

  common_prefix = malloc(sizeof (*common_prefix));
  if (NULL == common_prefix)
    goto bad;

  memset(common_prefix, 0, sizeof (*common_prefix));

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE) 
        {
          DPRINTF("name: %s\n", tmp->name);
          if (!strcmp((char *) tmp->name, "Prefix"))
            {
              common_prefix->prefix = strdup((char *) tmp->children->content);
              if (NULL == common_prefix->prefix)
                goto bad;
            }
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = dpl_vec_add(vec, common_prefix);
  if (DPL_SUCCESS != ret)
    goto bad;

  return DPL_SUCCESS;

 bad:

  if (NULL != common_prefix)
    dpl_common_prefix_free(common_prefix);

  return DPL_FAILURE;
}

static dpl_status_t
parse_list_bucket_children(xmlNode *node,
                           dpl_vec_t *objects,
                           dpl_vec_t *common_prefixes)
{
  xmlNode *tmp;
  int ret;

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE) 
        {
          DPRINTF("name: %s\n", tmp->name);
          
          if (!strcmp((char *) tmp->name, "Contents"))
            {
              ret = parse_list_bucket_content(tmp->children, objects);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }
          else if (!strcmp((char *) tmp->name, "CommonPrefixes"))
            {
              ret = parse_list_bucket_common_prefixes(tmp->children, common_prefixes);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_parse_list_bucket(dpl_ctx_t *ctx,
                      char *buf,
                      int len,
                      dpl_vec_t *objects,
                      dpl_vec_t *common_prefixes)
{
  xmlParserCtxtPtr ctxt = NULL;
  xmlDocPtr doc = NULL;
  int ret;
  xmlNode *tmp;
  //ssize_t cc;

  //cc = write(1, buf, len);

  if ((ctxt = xmlNewParserCtxt()) == NULL)
    {
      ret = DPL_FAILURE;
      goto end;
    }
  
  doc = xmlCtxtReadMemory(ctxt, buf, len, NULL, NULL, 0u);
  if (NULL == doc)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  for (tmp = xmlDocGetRootElement(doc); NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE) 
        {
          DPRINTF("name: %s\n", tmp->name);
          
          if (!strcmp((char *) tmp->name, "ListBucketResult"))
            {
              ret = parse_list_bucket_children(tmp->children, objects, common_prefixes);
              if (DPL_SUCCESS != ret)
                return DPL_FAILURE;
            }
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != doc)
    xmlFreeDoc(doc);

  if (NULL != ctxt)
    xmlFreeParserCtxt(ctxt);

  return ret;
}

/** 
 * list bucket
 * 
 * @param ctx 
 * @param bucket 
 * @param prefix can be NULL
 * @param delimiter can be NULL
 * @param objectsp 
 * @param prefixesp
 * 
 * @return 
 */
dpl_status_t
dpl_list_bucket(dpl_ctx_t *ctx,
                char *bucket,
                char *prefix,
                char *delimiter,
                dpl_vec_t **objectsp,
                dpl_vec_t **common_prefixesp)
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
  char          *data_buf = NULL;
  u_int         data_len;
  dpl_vec_t     *objects = NULL;
  dpl_vec_t     *common_prefixes = NULL;
  dpl_dict_t    *headers = NULL;
  dpl_dict_t    *query_params = NULL;
  dpl_dict_t    *headers_returned = NULL;

  snprintf(host, sizeof (host), "%s.%s", bucket, ctx->host);

  DPRINTF("dpl_list_bucket: host=%s:%d\n", host, ctx->port);

  DPL_TRACE(ctx, DPL_TRACE_S3, "listbucket bucket=%s prefix=%s delimiter=%s", bucket, prefix, delimiter);

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

  query_params = dpl_dict_new(13);
  if (NULL == query_params)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != prefix)
    {
      ret2 = dpl_dict_add(query_params, "prefix", prefix, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  if (NULL != delimiter)
    {
      ret2 = dpl_dict_add(query_params, "delimiter", delimiter, 0);
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
                              "GET",
                              bucket,
                              "/",
                              NULL,
                              query_params,
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

  ret2 = dpl_read_http_reply(conn, &data_buf, &data_len, &headers_returned);
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

  (void) dpl_log_charged_event(ctx, "REQUEST", "LIST", 0);

  objects = dpl_vec_new(2, 2);
  if (NULL == objects)
    {
      ret = DPL_FAILURE;
      goto end;
    }
  
  common_prefixes = dpl_vec_new(2, 2);
  if (NULL == common_prefixes)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = dpl_parse_list_bucket(ctx, data_buf, data_len, objects, common_prefixes);
  if (DPL_SUCCESS != ret)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (NULL != objectsp)
    {
      *objectsp = objects;
      objects = NULL; //consume i
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

  if (NULL != query_params)
    dpl_dict_free(query_params);

  if (NULL != headers)
    dpl_dict_free(headers);

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);
  
  DPRINTF("ret=%d\n", ret);

  return ret;
}
