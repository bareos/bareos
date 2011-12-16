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
#include <json/json.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_cdmi_get_metadata_from_headers(const dpl_dict_t *headers,
                                   dpl_dict_t *metadata)
{
  //metadata are not stored in headers
  return DPL_FAILURE;
}

/**/

dpl_status_t
dpl_cdmi_parse_list_bucket(dpl_ctx_t *ctx,
                           const char *buf,
                           int len,
                           const char *prefix,
                           dpl_vec_t *objects,
                           dpl_vec_t *common_prefixes)
{
  int ret, ret2;
  json_tokener *tok = NULL;
  json_object *obj = NULL;
  json_object *children = NULL;
  int n_children, i;
  dpl_common_prefix_t *common_prefix = NULL;
  dpl_object_t *object = NULL;

  //  write(1, buf, len);

  tok = json_tokener_new();
  if (NULL == tok)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  obj = json_tokener_parse_ex(tok, buf, len);
  if (NULL == obj)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  children = json_object_object_get(obj, "children");
  if (NULL == children)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (json_type_array != json_object_get_type(children))
    {
      ret = DPL_FAILURE;
      goto end;
    }

  n_children = json_object_array_length(children);

  for (i = 0;i < n_children;i++)
    {
      json_object *child = json_object_array_get_idx(children, i);
      char name[1024];
      int name_len;
      
      if (json_type_string != json_object_get_type(child))
        {
          ret = DPL_FAILURE;
          goto end;
        }

      snprintf(name, sizeof (name), "%s%s", NULL != prefix ? prefix : "", json_object_get_string(child));
      name_len = strlen(name);

      if (name_len > 0 && name[name_len-1] == '/')
        {
          //this is a directory
          common_prefix = malloc(sizeof (*common_prefix));
          if (NULL == common_prefix)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          memset(common_prefix, 0, sizeof (*common_prefix));
          common_prefix->prefix = strdup(name);
          if (NULL == common_prefix->prefix)
            {
              ret = DPL_ENOMEM;
              goto end;
            }

          ret2 = dpl_vec_add(common_prefixes, common_prefix);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          common_prefix = NULL;
        }
      else
        {
          object = malloc(sizeof (*object));
          if (NULL == object)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          memset(object, 0, sizeof (*object));
          object->key = strdup(name);
          if (NULL == object->key)
            {
              ret = DPL_ENOMEM;
              goto end;
            }

          ret2 = dpl_vec_add(objects, object);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          object = NULL;
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != common_prefix)
    dpl_common_prefix_free(common_prefix);

  if (NULL != object)
    dpl_object_free(object);

  if (NULL != obj)
    json_object_put(obj);

  if (NULL != tok)
    json_tokener_free(tok);

  return ret;
}

/**/

dpl_status_t
dpl_cdmi_parse_metadata(dpl_ctx_t *ctx,
                        const char *buf,
                        int len,
                        dpl_dict_t *metadata)
{
  int ret, ret2;
  json_tokener *tok = NULL;
  json_object *obj = NULL;
  json_object *md_obj = NULL;
  char *key; 
  struct json_object *val_obj;
  struct lh_entry *entry;

  //  write(1, buf, len);

  tok = json_tokener_new();
  if (NULL == tok)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  obj = json_tokener_parse_ex(tok, buf, len);
  if (NULL == obj)
    {
      ret = DPL_FAILURE;
      goto end;
    }
  
  md_obj = json_object_object_get(obj, "metadata");
  if (NULL == metadata)
    {
      ret = DPL_FAILURE;
      goto end;
    }


  for (entry = json_object_get_object(md_obj)->head; (entry ? (key = (char*)entry->k, val_obj = (struct json_object*)entry->v, entry) : 0); entry = entry->next)
    {
      char *val, *valp;
      int val_len;

      pthread_mutex_lock(&ctx->lock);
      val = strdup((char *) json_object_to_json_string(val_obj));
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == val)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      val_len = strlen(val);

      if (val[0] == '"')
        valp = val + 1;
      else
        valp = val;
          
      if (val_len > 0)
        {
          if (val[val_len-1] == '"')
            val[val_len-1] = 0;
        }

      ret2 = dpl_dict_add(metadata, key, valp, 0);

      free(val);

      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != obj)
    json_object_put(obj);

  if (NULL != tok)
    json_tokener_free(tok);

  return ret;
}

dpl_status_t
dpl_cdmi_get_metadata_from_json_metadata(dpl_dict_t *json_metadata,
                                         dpl_dict_t *metadata)
{
  return dpl_dict_filter_no_prefix(metadata, json_metadata, "cdmi_");
}
