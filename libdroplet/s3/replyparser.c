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
#include <droplet/s3/s3.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

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
dpl_s3_get_metadatum_from_header(const char *header,
                                 const char *value,
                                 dpl_metadatum_func_t metadatum_func,
                                 void *cb_arg,
                                 dpl_dict_t *metadata,
                                 dpl_sysmd_t *sysmdp)
{
  dpl_status_t ret, ret2;

  if (!strncmp(header, DPL_X_AMZ_META_PREFIX, strlen(DPL_X_AMZ_META_PREFIX)))
    {
      char *key;

      key = (char *) header + strlen(DPL_X_AMZ_META_PREFIX);

      if (metadatum_func)
        {
          dpl_sbuf_t sbuf;
          dpl_value_t val;
          
          sbuf.allocated = 0;
          sbuf.buf = (char *) value;
          sbuf.len = strlen(value);
          val.type = DPL_VALUE_STRING;
          val.string = &sbuf;
          ret2 = metadatum_func(cb_arg, key, &val);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      if (metadata)
        {
          ret2 = dpl_dict_add(metadata, key, value, 1);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
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
          else if (!strcmp(header, "last-modified"))
            {
              sysmdp->mask |= DPL_SYSMD_MASK_MTIME;
              sysmdp->mtime = dpl_get_date(value, NULL);
            }
          else if (!strcmp(header, "etag"))
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
  return dpl_s3_get_metadatum_from_header(var->key,
                                          dpl_sbuf_get_str(var->val->string),
                                          NULL,
                                          NULL,
                                          mc->metadata,
                                          mc->sysmdp);
}

dpl_status_t
dpl_s3_get_metadata_from_headers(const dpl_dict_t *headers,
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
dpl_s3_parse_list_all_my_buckets(const dpl_ctx_t *ctx,
                                 const char *buf,
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

/**/

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
              object->path = strdup((char *) tmp->children->content);
              if (NULL == object->path)
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
dpl_s3_parse_list_bucket(const dpl_ctx_t *ctx,
                         const char *buf,
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

/**/


