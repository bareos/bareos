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
#include <json.h>
#include <droplet/cdmi/reqbuilder.h>
#include <droplet/cdmi/replyparser.h>
#include <droplet/cdmi/object_id.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

struct cdmi_req_add_md_arg
{
  dpl_sbuf_t *field;
  int comma;
};

dpl_status_t 
cb_cdmi_req_add_metadata(dpl_dict_var_t *var,
                         void *cb_arg)
{
  struct cdmi_req_add_md_arg *arg = (struct cdmi_req_add_md_arg *) cb_arg;
  dpl_status_t ret, ret2;
  char buf[256];

  snprintf(buf, sizeof (buf), "%smetadata:%s", arg->comma ? "," : "", var->key);

  ret2 = dpl_sbuf_add(arg->field, buf, strlen(buf));
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  arg->comma = 1;

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

dpl_status_t
dpl_cdmi_req_add_metadata(dpl_req_t *req,
                          const dpl_dict_t *metadata,
                          int append)
{
  dpl_status_t ret, ret2;
  struct cdmi_req_add_md_arg arg;

  memset(&arg, 0, sizeof (arg));

  if (append)
    {
      //iterate metadata object
      arg.field = dpl_sbuf_new(30);
      if (NULL == arg.field)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      ret2 = dpl_dict_iterate(metadata, cb_cdmi_req_add_metadata, &arg);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_sbuf_add(arg.field, "", 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_req_set_subresource(req, arg.field->buf);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_req_add_metadata(req, metadata);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != arg.field)
    dpl_sbuf_free(arg.field);

  return ret;
}

struct metadata_list_arg
{
  dpl_dict_t *metadata;
  dpl_metadatum_func_t metadatum_func;
  void *cb_arg;
};

dpl_status_t 
cb_metadata_list(dpl_dict_var_t *var,
                 void *cb_arg)
{
  struct metadata_list_arg *arg = (struct metadata_list_arg *) cb_arg;
  dpl_status_t ret, ret2;

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
  
  ret2 = dpl_dict_add_value(arg->metadata, var->key, var->val, 0);
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

  DPRINTF("key=%s val.type=%d\n", key, val->type);

  if (sysmdp)
    {
      if (!strcmp(key, "objectID"))
        {
          if (DPL_VALUE_STRING != val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          ret2 = dpl_cdmi_string_to_object_id(dpl_sbuf_get_str(val->string),
                                              &obj_id);
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

          if (strcmp(dpl_sbuf_get_str(val->string), ""))
            {
              ret2 = dpl_cdmi_string_to_object_id(dpl_sbuf_get_str(val->string), &obj_id);
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
        }
      else if (!strcmp(key, "objectType"))
        {
          if (DPL_VALUE_STRING != val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          sysmdp->mask |= DPL_SYSMD_MASK_FTYPE;
          sysmdp->ftype = dpl_cdmi_content_type_to_ftype(dpl_sbuf_get_str(val->string));
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
              sysmdp->mtime = dpl_iso8601totime(dpl_sbuf_get_str(var->val->string));
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
              sysmdp->atime = dpl_iso8601totime(dpl_sbuf_get_str(var->val->string));
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
              sysmdp->size = strtoull(dpl_sbuf_get_str(var->val->string), NULL, 0);
            }
        }

      if (metadata)
        {
          struct metadata_list_arg arg;
          
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

  if (!strncmp(header, DPL_X_OBJECT_META_PREFIX, strlen(DPL_X_OBJECT_META_PREFIX)))
    {
      char *key;

      key = (char *) header + strlen(DPL_X_OBJECT_META_PREFIX);

      ret2 = dpl_cdmi_get_metadatum_from_string(key, value, 
                                                metadatum_func, cb_arg,
                                                metadata, sysmdp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else if (!strncmp(header, DPL_X_CONTAINER_META_PREFIX, strlen(DPL_X_CONTAINER_META_PREFIX)))
    {
      char *key;

      key = (char *) header + strlen(DPL_X_CONTAINER_META_PREFIX);

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
                                            dpl_sbuf_get_str(var->val->string),
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

  for (i = -1;i < n_children;i++)
    {
      char name[1024];
      int name_len;

      if (-1 == i)
        {
          // add the directory itself to the list
          snprintf(name, sizeof (name), "%s", NULL != prefix ? prefix : "/");
        }
      else
        {
          json_object *child = json_object_array_get_idx(children, i);
      
          if (json_type_string != json_object_get_type(child))
            {
              ret = DPL_FAILURE;
              goto end;
            }

          snprintf(name, sizeof (name), "%s%s", NULL != prefix ? prefix : "", json_object_get_string(child));
        }

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
          object->path = strdup(name);
          if (NULL == object->path)
            {
              ret = DPL_ENOMEM;
              goto end;
            }

          if (name_len > 0 && name[name_len-1] == '?')
            {
              //this is a symbolic link: remove final '?'

              object->path[name_len - 1] = '\0';
            }

          object->type = DPL_FTYPE_UNDEF;

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

static dpl_status_t
convert_obj_to_value(dpl_ctx_t *ctx,
                     struct json_object *obj,
                     int level,
                     dpl_value_t **valp)
{
  int ret, ret2;
  dpl_value_t *val = NULL;
  char *key; 
  struct lh_entry *entry;
  json_object *child;
  dpl_dict_t *subdict = NULL;
  dpl_vec_t *vector = NULL;

  DPRINTF("convert_obj_to_value level=%d type=%d\n", level, json_object_get_type(obj));

  val = malloc(sizeof (*val));
  if (NULL == val)
    {
      ret = DPL_ENOMEM;
      goto end;
    }
  memset(val, 0, sizeof (*val));

  switch (json_object_get_type(obj))
    {
    case json_type_null:
	  {
        ret = DPL_ENOTSUPP;
	  	goto end;
	  }
    case json_type_array:
      {
        int n_items = json_object_array_length(obj);
        int i;

        vector = dpl_vec_new(2, 2);
        if (NULL == vector)
          {
            ret = DPL_ENOMEM;
            goto end;
          }

        for (i = 0;i < n_items;i++)
          {
            child = json_object_array_get_idx(obj, i);
            dpl_value_t *subval;

            ret2 = convert_obj_to_value(ctx, child, level+1, &subval);
            if (DPL_SUCCESS != ret2)
              {
                ret = ret2;
                goto end;
              }
            
            ret2 = dpl_vec_add_value(vector, subval);

            dpl_value_free(subval);
            
            if (DPL_SUCCESS != ret2)
              {
                ret = ret2;
                goto end;
              }
          }

        val->type = DPL_VALUE_VECTOR;
        val->vector = vector;
        vector = NULL;
        break ;
      }
    case json_type_object:
      {
        subdict = dpl_dict_new(13);
        if (NULL == subdict)
          {
            ret = DPL_ENOMEM;
            goto end;
          }
        
        for (entry = json_object_get_object(obj)->head; (entry ? (key = (char*)entry->k, child = (struct json_object*)entry->v, entry) : 0); entry = entry->next)
          {
            dpl_value_t *subval;

            DPRINTF("key='%s'\n", key);

            ret2 = convert_obj_to_value(ctx, child, level+1, &subval);
            if (DPL_SUCCESS != ret2)
              {
                ret = ret2;
                goto end;
              }
            
            ret2 = dpl_dict_add_value(subdict, key, subval, 0);

            dpl_value_free(subval);

            if (DPL_SUCCESS != ret2)
              {
                ret = ret2;
                goto end;
              }
          }

        val->type = DPL_VALUE_SUBDICT;
        val->subdict = subdict;
        subdict = NULL;
        break ;
      }
    case json_type_boolean:
    case json_type_double:
    case json_type_int:
      {
        pthread_mutex_lock(&ctx->lock); //lock for objects other than string
        val->string = dpl_sbuf_new(16);
        if (NULL == val->string)
          {
            ret = DPL_ENOMEM;
            goto end;
          }
        dpl_sbuf_add_str(val->string, json_object_get_string(obj));
        pthread_mutex_unlock(&ctx->lock);

        val->type = DPL_VALUE_STRING;
        break ;
      }
    case json_type_string:
      {
        pthread_mutex_lock(&ctx->lock); //lock for objects other than string
	val->string = dpl_sbuf_new(json_object_get_string_length(obj) + 1);
        if (NULL == val->string)
          {
            ret = DPL_ENOMEM;
            goto end;
          }
        dpl_sbuf_add(val->string, json_object_get_string(obj),
                     json_object_get_string_length(obj));
        pthread_mutex_unlock(&ctx->lock);

        val->type = DPL_VALUE_STRING;
        break ;
      }
    }

  if (NULL != valp)
    {
      *valp = val;
      val = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != vector)
    dpl_vec_free(vector);

  if (NULL != subdict)
    dpl_dict_free(subdict);

  if (NULL != val)
    dpl_value_free(val);

  DPRINTF("level=%d ret=%d\n", level, ret);

  return ret;
}

/** 
 * parse a JSON buffer into a value
 * 
 * @param ctx 
 * @param buf 
 * @param len 
 * @param valp 
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_parse_json_buffer(dpl_ctx_t *ctx,
                           const char *buf,
                           int len,
                           dpl_value_t **valp)
{
  int ret, ret2;
  json_tokener *tok = NULL;
  json_object *obj = NULL;
  dpl_value_t *val = NULL;

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

  ret2 = convert_obj_to_value(ctx, obj, 0, &val);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  if (NULL != valp)
    {
      *valp = val;
      val = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != val)
    dpl_value_free(val);

  if (NULL != obj)
    json_object_put(obj);

  if (NULL != tok)
    json_tokener_free(tok);

  return ret;
}

dpl_ftype_t
dpl_cdmi_content_type_to_ftype(const char *str)
{
  if (!strcmp(DPL_CDMI_CONTENT_TYPE_OBJECT, str))
    return DPL_FTYPE_REG;
  else if (!strcmp(DPL_CDMI_CONTENT_TYPE_CONTAINER, str))
    return DPL_FTYPE_DIR;
  else if (!strcmp(DPL_CDMI_CONTENT_TYPE_CAPABILITY, str))
    return DPL_FTYPE_CAP;
  else if (!strcmp(DPL_CDMI_CONTENT_TYPE_DOMAIN, str))
    return DPL_FTYPE_DOM;

  return DPL_FTYPE_UNDEF;
}
