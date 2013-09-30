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
#include <droplet/swift/reqbuilder.h>
#include <droplet/swift/replyparser.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

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
dpl_swift_get_metadatum_from_value(const char *key,
				   dpl_value_t *val,
				   dpl_metadatum_func_t metadatum_func,
				   void *cb_arg,
				   dpl_dict_t *metadata,
				   dpl_sysmd_t *sysmdp)
{
  dpl_status_t ret, ret2;
  dpl_dict_var_t *var;
  /* dpl_swift_object_id_t obj_id; */

  DPRINTF("key=%s val.type=%d\n", key, val->type);

  if (sysmdp)
    {
      /* if (!strcmp(key, "objectID")) */
      /*   { */
      /*     if (DPL_VALUE_STRING != val->type) */
      /*       { */
      /*         ret = DPL_EINVAL; */
      /*         goto end; */
      /*       } */
          
      /*     ret2 = dpl_swift_string_to_object_id(dpl_sbuf_get_str(val->string), */
      /*                                         &obj_id); */
      /*     if (DPL_SUCCESS != ret2) */
      /*       { */
      /*         ret = ret2; */
      /*         goto end; */
      /*       } */
          
      /*     ret2 = dpl_swift_opaque_to_string(&obj_id, sysmdp->id); */
      /*     if (DPL_SUCCESS != ret2) */
      /*       { */
      /*         ret = ret2; */
      /*         goto end; */
      /*       } */
          
      /*     sysmdp->mask |= DPL_SYSMD_MASK_ID; */
          
      /*     sysmdp->enterprise_number = obj_id.enterprise_number; */
      /*     sysmdp->mask |= DPL_SYSMD_MASK_ENTERPRISE_NUMBER; */
      /*   } */
      /* else if (!strcmp(key, "parentID")) */
      /*   { */
      /*     if (DPL_VALUE_STRING != val->type) */
      /*       { */
      /*         ret = DPL_EINVAL; */
      /*         goto end; */
      /*       } */

      /*     if (strcmp(dpl_sbuf_get_str(val->string), "")) */
      /*       { */
      /*         ret2 = dpl_swift_string_to_object_id(dpl_sbuf_get_str(val->string), &obj_id); */
      /*         if (DPL_SUCCESS != ret2) */
      /*           { */
      /*             ret = ret2; */
      /*             goto end; */
      /*           } */
              
      /*         ret2 = dpl_swift_opaque_to_string(&obj_id, sysmdp->parent_id); */
      /*         if (DPL_SUCCESS != ret2) */
      /*           { */
      /*             ret = ret2; */
      /*             goto end; */
      /*           } */
              
      /*         sysmdp->mask |= DPL_SYSMD_MASK_PARENT_ID; */
      /*       } */
      /*   } */
      /* else if (!strcmp(key, "objectType")) */
      /*   { */
      /*     if (DPL_VALUE_STRING != val->type) */
      /*       { */
      /*         ret = DPL_EINVAL; */
      /*         goto end; */
      /*       } */
          
      /*     sysmdp->mask |= DPL_SYSMD_MASK_FTYPE; */
      /*     sysmdp->ftype = dpl_swift_content_type_to_ftype(dpl_sbuf_get_str(val->string)); */
      /*   } */
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
          
          var = dpl_dict_get(val->subdict, "swift_mtime");
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
          
          var = dpl_dict_get(val->subdict, "swift_atime");
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
          
          var = dpl_dict_get(val->subdict, "swift_size");
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

      /* if (metadata) */
      /*   { */
      /*     struct metadata_list_arg arg; */
          
      /*     arg.metadatum_func = metadatum_func; */
      /*     arg.metadata = metadata; */
      /*     arg.cb_arg = cb_arg; */

      /*     //iterate metadata object */
      /*     ret2 = dpl_dict_iterate(val->subdict, cb_metadata_list, &arg); */
      /*     if (DPL_SUCCESS != ret2) */
      /*       { */
      /*         ret = ret2; */
      /*         goto end; */
      /*       } */
      /*   } */
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
dpl_swift_get_metadatum_from_string(const char *key,
                                   const char *value,
                                   dpl_metadatum_func_t metadatum_func,
                                   void *cb_arg,
                                   dpl_dict_t *metadata,
                                   dpl_sysmd_t *sysmdp)
{
  dpl_status_t ret, ret2;
  dpl_value_t *val = NULL;
  
  //XXX convert

  ret2 = dpl_swift_get_metadatum_from_value(key, val, 
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
dpl_swift_get_metadatum_from_header(const char *header,
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

      ret2 = dpl_swift_get_metadatum_from_string(key, value, 
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

      ret2 = dpl_swift_get_metadatum_from_string(key, value, 
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
  return dpl_swift_get_metadatum_from_header(var->key,
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
dpl_swift_get_metadata_from_headers(const dpl_dict_t *headers,
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
  
  return dpl_swift_get_metadatum_from_value(var->key,
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
dpl_swift_get_metadata_from_values(const dpl_dict_t *values,
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
