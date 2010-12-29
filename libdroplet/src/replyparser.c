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

dpl_status_t
dpl_get_metadata_from_headers(dpl_dict_t *headers,
                              dpl_dict_t *metadata)
{
  int bucket;
  dpl_var_t *var;
  int ret;

  for (bucket = 0;bucket < headers->n_buckets;bucket++)
    {
      for (var = headers->buckets[bucket];var;var = var->prev)
        {
          int x_amz_meta_len;

#define X_AMZ_META "x-amz-meta-"
          x_amz_meta_len = strlen(X_AMZ_META);
          if (!strncmp(var->key, X_AMZ_META, x_amz_meta_len))
            {
              char *p;

              p = var->key + x_amz_meta_len;

              ret = dpl_dict_add(metadata, p, var->value, 0);
              if (DPL_SUCCESS != ret)
                {
                  return DPL_FAILURE;
                }
            }
        }
    }

  return DPL_SUCCESS;
}

/**/

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

/**/

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

/**/


