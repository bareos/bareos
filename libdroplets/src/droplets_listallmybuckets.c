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
dpl_bucket_free(dpl_bucket_t *bucket)
{
  free(bucket->name);
  free(bucket);  
}

void 
dpl_vec_buckets_free(dpl_vec_t *vec)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    dpl_bucket_free((dpl_bucket_t *) vec->array[i]);
  dpl_vec_free(vec);
}

static dpl_status_t
parse_list_all_my_buckets_bucket(xmlNode *node,
                                 dpl_vec_t *vec)
{
  xmlNode *tmp;
  dpl_bucket_t *bucket = NULL;
  int ret;

  bucket = malloc(sizeof (*bucket));
  if (NULL == bucket)
    goto bad;

  memset(bucket, 0, sizeof (*bucket));

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE) 
        {
          DPRINTF("name: %s\n", tmp->name);
          if (!strcmp((char *) tmp->name, "Name"))
            {
              bucket->name = strdup((char *) tmp->children->content);
              if (NULL == bucket->name)
                goto bad;
            }

          if (!strcmp((char *) tmp->name, "CreationDate"))
            {
              bucket->creation_time = dpl_iso8601totime((char *) tmp->children->content);
            }
          
        }
      else if (tmp->type == XML_TEXT_NODE)
        {
          DPRINTF("content: %s\n", tmp->content);
        }
    }

  ret = dpl_vec_add(vec, bucket);
  if (DPL_SUCCESS != ret)
    goto bad;

  return DPL_SUCCESS;

 bad:

  if (NULL != bucket)
    dpl_bucket_free(bucket);

  return DPL_FAILURE;
}

static dpl_status_t
parse_list_all_my_buckets_buckets(xmlNode *node,
                                  dpl_vec_t *vec)
{
  xmlNode *tmp;
  int ret;

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE) 
        {
          DPRINTF("name: %s\n", tmp->name);

          if (!strcmp((char *) tmp->name, "Bucket"))
            {
              ret = parse_list_all_my_buckets_bucket(tmp->children, vec);
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

static dpl_status_t
parse_list_all_my_buckets_children(xmlNode *node,
                                   dpl_vec_t *vec)
{
  xmlNode *tmp;
  int ret;

  for (tmp = node; NULL != tmp; tmp = tmp->next)
    {
      if (tmp->type == XML_ELEMENT_NODE) 
        {
          DPRINTF("name: %s\n", tmp->name);
          
          if (!strcmp((char *) tmp->name, "Buckets"))
            {
              ret = parse_list_all_my_buckets_buckets(tmp->children, vec);
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
dpl_parse_list_all_my_buckets(dpl_ctx_t *ctx,
                              char *buf,
                              int len,
                              dpl_vec_t *vec)
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
          
          if (!strcmp((char *) tmp->name, "ListAllMyBucketsResult"))
            {
              ret = parse_list_all_my_buckets_children(tmp->children, vec);
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
 * list all buckets
 * 
 * @param ctx 
 * @param vecp 
 * 
 * @return 
 */
dpl_status_t
dpl_list_all_my_buckets(dpl_ctx_t *ctx,
                        dpl_vec_t **vecp)
{
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
  dpl_vec_t     *vec = NULL;
  dpl_dict_t    *headers = NULL;
  dpl_dict_t    *headers_returned = NULL;

  DPL_TRACE(ctx, DPL_TRACE_S3, "listallmybuckets");

  headers = dpl_dict_new(13);
  if (NULL == headers)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "Host", ctx->host, 0);
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

  ret2 = linux_gethostbyname_r(ctx->host, &hret, hbuf, sizeof (hbuf), &hresult, &herr); 
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
                              NULL,
                              "/",
                              NULL,
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
  
  vec = dpl_vec_new(2, 2);
  if (NULL == vec)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = dpl_parse_list_all_my_buckets(ctx, data_buf, data_len, vec);
  if (DPL_SUCCESS != ret)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (NULL != vecp)
    {
      *vecp = vec;
      vec = NULL; //consume vec
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != vec)
    dpl_vec_buckets_free(vec);

  if (NULL != data_buf)
    free(data_buf);

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

