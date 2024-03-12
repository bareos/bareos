/*
 * Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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
#include <droplet/cdmi/reqbuilder.h>
#include <droplet/cdmi/replyparser.h>
#include <droplet/cdmi/backend.h>

/** @file */

// #define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt, ...)

const char* dpl_cdmi_who_str(dpl_ace_who_t who)
{
  switch (who) {
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

dpl_status_t dpl_cdmi_add_sysmd_to_req(const dpl_sysmd_t* sysmd, dpl_req_t* req)
{
  dpl_status_t ret, ret2;
  char buf[256];
  dpl_dict_t* tmp_dict = NULL;
  dpl_dict_t* tmp_dict2 = NULL;
  dpl_vec_t* tmp_vec = NULL;
  int do_acl = 0;
  uint32_t n_aces = 0;
  dpl_ace_t aces[DPL_SYSMD_ACE_MAX], *acesp;

  tmp_dict = dpl_dict_new(13);
  if (NULL == tmp_dict) {
    ret = DPL_ENOMEM;
    goto end;
  }

  if (sysmd->mask & DPL_SYSMD_MASK_SIZE) {
    /* optional (computed remotely by storage) */
    snprintf(buf, sizeof(buf), "%ld", sysmd->size);

    ret2 = dpl_dict_add(tmp_dict, "cdmi_size", buf, 0);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  }

  if (sysmd->mask & DPL_SYSMD_MASK_ATIME) {
    /* optional */
    ret2 = dpl_timetoiso8601(sysmd->atime, buf, sizeof(buf));
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }

    ret2 = dpl_dict_add(tmp_dict, "cdmi_atime", buf, 0);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  }

  if (sysmd->mask & DPL_SYSMD_MASK_MTIME) {
    /* optional */
    ret2 = dpl_timetoiso8601(sysmd->mtime, buf, sizeof(buf));
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }

    ret2 = dpl_dict_add(tmp_dict, "cdmi_mtime", buf, 0);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  }

  if (sysmd->mask & DPL_SYSMD_MASK_CTIME) {
    /* optional */
    ret2 = dpl_timetoiso8601(sysmd->ctime, buf, sizeof(buf));
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }

    ret2 = dpl_dict_add(tmp_dict, "cdmi_ctime", buf, 0);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  }

  if (sysmd->mask & DPL_SYSMD_MASK_OWNER) {
    ret2 = dpl_dict_add(tmp_dict, "cdmi_owner", sysmd->owner, 0);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  }

  if (sysmd->mask & DPL_SYSMD_MASK_CANNED_ACL) {
    switch (sysmd->canned_acl) {
      case DPL_CANNED_ACL_ERROR:
      case DPL_CANNED_ACL_UNDEF:
        n_aces = 0;
        break;
      case DPL_CANNED_ACL_PRIVATE:
        aces[0].type = DPL_ACE_TYPE_ALLOW;
        aces[0].flag = 0;
        aces[0].access_mask = DPL_ACE_MASK_ALL;
        aces[0].who = DPL_ACE_WHO_OWNER;
        n_aces = 1;
        break;
      case DPL_CANNED_ACL_PUBLIC_READ:
        aces[0].type = DPL_ACE_TYPE_ALLOW;
        aces[0].flag = 0;
        aces[0].access_mask = DPL_ACE_MASK_READ_OBJECT | DPL_ACE_MASK_EXECUTE;
        aces[0].who = DPL_ACE_WHO_EVERYONE;
        n_aces = 1;
        break;
      case DPL_CANNED_ACL_PUBLIC_READ_WRITE:
        aces[0].type = DPL_ACE_TYPE_ALLOW;
        aces[0].flag = 0;
        aces[0].access_mask = DPL_ACE_MASK_RW_ALL;
        aces[0].who = DPL_ACE_WHO_EVERYONE;
        n_aces = 1;
        break;
      case DPL_CANNED_ACL_AUTHENTICATED_READ:
        aces[0].type = DPL_ACE_TYPE_ALLOW;
        aces[0].flag = 0;
        aces[0].access_mask = DPL_ACE_MASK_READ_ALL;
        aces[0].who = DPL_ACE_WHO_AUTHENTICATED;
        n_aces = 1;
        break;
      case DPL_CANNED_ACL_BUCKET_OWNER_READ:
        // XXX ?
        break;
      case DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL:
        // XXX ?
        break;
    }

    acesp = aces;
    do_acl = 1;
  }

  if (sysmd->mask & DPL_SYSMD_MASK_ACL) {
    n_aces = sysmd->n_aces;
    acesp = (dpl_ace_t*)sysmd->aces;
    do_acl = 1;
  }

  if (do_acl) {
    int i;
    dpl_value_t value;

    tmp_vec = dpl_vec_new(2, 2);
    if (NULL == tmp_vec) {
      ret = DPL_ENOMEM;
      goto end;
    }

    for (i = 0; i < n_aces; i++) {
      const char* str;
      char buf2[256];
      dpl_value_t row_value;

      tmp_dict2 = dpl_dict_new(13);
      if (NULL == tmp_dict2) {
        ret = DPL_ENOMEM;
        goto end;
      }

      str = dpl_cdmi_who_str(acesp[i].who);
      if (NULL == str) {
        ret = DPL_EINVAL;
        goto end;
      }

      ret2 = dpl_dict_add(tmp_dict2, "identifier", str, 0);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      snprintf(buf2, sizeof(buf2), "0x%08x", acesp[i].type);

      ret2 = dpl_dict_add(tmp_dict2, "acetype", buf2, 0);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      snprintf(buf2, sizeof(buf2), "0x%08x", acesp[i].flag);

      ret2 = dpl_dict_add(tmp_dict2, "aceflags", buf2, 0);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      snprintf(buf2, sizeof(buf2), "0x%08x", acesp[i].access_mask);

      ret2 = dpl_dict_add(tmp_dict2, "acemask", buf2, 0);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      row_value.type = DPL_VALUE_SUBDICT;
      row_value.subdict = tmp_dict2;
      ret2 = dpl_vec_add_value(tmp_vec, &row_value);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      dpl_dict_free(tmp_dict2);
      tmp_dict2 = NULL;
    }

    value.type = DPL_VALUE_VECTOR;
    value.vector = tmp_vec;
    ret2 = dpl_dict_add_value(tmp_dict, "cdmi_acl", &value, 0);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }

    dpl_vec_free(tmp_vec);
    tmp_vec = NULL;
  }

  if (sysmd->mask & DPL_SYSMD_MASK_ID) {
    // XXX extension ???
  }

  // dpl_dict_print(tmp_dict, stderr, 0);

  ret2 = dpl_req_add_metadata(req, tmp_dict);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = 0;

end:

  if (NULL != tmp_vec) dpl_vec_free(tmp_vec);

  if (NULL != tmp_dict2) dpl_dict_free(tmp_dict2);

  if (NULL != tmp_dict) dpl_dict_free(tmp_dict);

  return ret;
}

dpl_status_t dpl_cdmi_req_set_resource(dpl_req_t* req, const char* resource)
{
  char* nstr;
  int len;
  dpl_status_t ret;

  nstr = strdup(resource);
  if (NULL == nstr) return DPL_ENOMEM;

  len = strlen(nstr);

  // suppress special chars
  if (len > 0u && '?' == nstr[len - 1]) nstr[len - 1] = 0;

  ret = dpl_req_set_resource(req, nstr);

  free(nstr);

  return ret;
}

dpl_status_t dpl_cdmi_req_add_range(dpl_req_t* req,
                                    dpl_cdmi_req_mask_t req_mask,
                                    const dpl_range_t* range)
{
  dpl_status_t ret, ret2;

  if (req_mask & DPL_CDMI_REQ_HTTP_COMPAT) {
    ret2 = dpl_req_add_range(req, range->start, range->end);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  } else {
    char buf[256];

    snprintf(buf, sizeof(buf), "value:%lu-%lu", range->start, range->end);
    ret2 = dpl_req_set_subresource(req, buf);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  }

  ret = DPL_SUCCESS;

end:

  return ret;
}

static dpl_status_t convert_value_to_obj(dpl_value_t* val, json_object** objp)
{
  dpl_status_t ret, ret2;
  json_object* obj = NULL;
  json_object* tmp = NULL;

  switch (val->type) {
    case DPL_VALUE_STRING: {
      obj = json_object_new_string_len(val->string->buf, val->string->len);
      if (NULL == obj) {
        ret = DPL_ENOMEM;
        goto end;
      }

      break;
    }
    case DPL_VALUE_SUBDICT: {
      int bucket;
      dpl_dict_t* dict = val->subdict;
      dpl_dict_var_t* var;

      obj = json_object_new_object();
      if (NULL == obj) {
        ret = DPL_ENOMEM;
        goto end;
      }

      for (bucket = 0; bucket < dict->n_buckets; bucket++) {
        for (var = dict->buckets[bucket]; var; var = var->prev) {
          ret2 = convert_value_to_obj(var->val, &tmp);
          if (DPL_SUCCESS != ret2) {
            ret = ret2;
            goto end;
          }

          json_object_object_add(obj, var->key, tmp);
          tmp = NULL;
        }
      }

      break;
    }
    case DPL_VALUE_VECTOR: {
      int i;
      dpl_vec_t* vec = val->vector;

      obj = json_object_new_array();
      if (NULL == obj) {
        ret = DPL_ENOMEM;
        goto end;
      }

      for (i = 0; i < vec->n_items; i++) {
        ret2 = convert_value_to_obj(vec->items[i], &tmp);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }

        json_object_array_add(obj, tmp);
        tmp = NULL;
      }

      break;
    }
    case DPL_VALUE_VOIDPTR:
      assert(0);
      break;
  }

  if (NULL != objp) {
    *objp = obj;
    obj = NULL;
  }

  ret = DPL_SUCCESS;

end:

  if (NULL != tmp) json_object_put(tmp);

  if (NULL != obj) json_object_put(obj);

  return ret;
}

static dpl_status_t add_metadata_to_json_body(const dpl_req_t* req,
                                              json_object* body_obj)

{
  dpl_value_t value;
  dpl_status_t ret, ret2;
  json_object* md_obj = NULL;

  if (0 == dpl_dict_count(req->metadata)) {
    ret = DPL_SUCCESS;
    goto end;
  }

  // fake value
  value.type = DPL_VALUE_SUBDICT;
  value.subdict = req->metadata;

  ret2 = convert_value_to_obj(&value, &md_obj);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  json_object_object_add(body_obj, "metadata", md_obj);
  md_obj = NULL;

  ret = DPL_SUCCESS;

end:

  if (NULL != md_obj) json_object_put(md_obj);

  return ret;
}

static dpl_status_t add_metadata_to_headers(const dpl_req_t* req,
                                            dpl_dict_t* headers,
                                            dpl_ftype_t object_type)

{
  int bucket;
  dpl_dict_var_t* var;
  char header[dpl_header_size];
  dpl_status_t ret, ret2;
  json_object* obj = NULL;

  for (bucket = 0; bucket < req->metadata->n_buckets; bucket++) {
    for (var = req->metadata->buckets[bucket]; var; var = var->prev) {
      switch (object_type) {
        case DPL_FTYPE_DIR:
          snprintf(header, sizeof(header), "%s%s", DPL_X_CONTAINER_META_PREFIX,
                   var->key);
          break;
        default:
          snprintf(header, sizeof(header), "%s%s", DPL_X_OBJECT_META_PREFIX,
                   var->key);
          break;
      }

      ret2 = convert_value_to_obj(var->val, &obj);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      pthread_mutex_lock(&req->ctx->lock);
      ret2 = dpl_dict_add(headers, header, json_object_get_string(obj), 0);
      pthread_mutex_unlock(&req->ctx->lock);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }
    }
  }

  ret = DPL_SUCCESS;

end:

  if (NULL != obj) json_object_put(obj);

  return ret;
}

static dpl_status_t add_copy_directive_to_json_body(const dpl_req_t* req,
                                                    json_object* body_obj)

{
  // dpl_ctx_t *ctx = (dpl_ctx_t *) req->ctx;
  dpl_status_t ret;
  json_object* tmp = NULL;
  const char* field = NULL;
  char* src_resource;

  if (DPL_COPY_DIRECTIVE_UNDEF == req->copy_directive) {
    ret = DPL_SUCCESS;
    goto end;
  }

  if (NULL == req->src_resource) {
    ret = DPL_EINVAL;
    goto end;
  }

  switch (req->copy_directive) {
    case DPL_COPY_DIRECTIVE_UNDEF:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_COPY_DIRECTIVE_COPY:
      field = "copy";
      break;
    case DPL_COPY_DIRECTIVE_METADATA_REPLACE:
      ret = DPL_EINVAL;
      goto end;
    case DPL_COPY_DIRECTIVE_LINK:
      field = "link";
      break;
    case DPL_COPY_DIRECTIVE_SYMLINK:
      field = "reference";
      break;
    case DPL_COPY_DIRECTIVE_MOVE:
      field = "move";
      break;
    case DPL_COPY_DIRECTIVE_MKDENT:
      field = "mkdent";
      break;
    case DPL_COPY_DIRECTIVE_RMDENT:
      field = "rmdent";
      break;
    case DPL_COPY_DIRECTIVE_MVDENT:
      field = "mvdent";
      break;
  }

  src_resource = req->src_resource;

  tmp = json_object_new_string(src_resource);
  if (NULL == tmp) {
    ret = DPL_ENOMEM;
    goto end;
  }

  assert(NULL != field);
  json_object_object_add(body_obj, field, tmp);

  tmp = NULL;

  ret = DPL_SUCCESS;

end:

  if (NULL != tmp) json_object_put(tmp);

  return ret;
}

static dpl_status_t add_data_to_json_body(const dpl_req_t* req,
                                          json_object* body_obj)

{
  int ret;
  json_object* value_obj = NULL;
  json_object* valuetransferencoding_obj = NULL;
  char* base64_str = NULL;
  int base64_len;

  // encode body to base64
  base64_str = malloc(DPL_BASE64_LENGTH(req->data_len) + 1);
  if (NULL == base64_str) {
    ret = DPL_ENOMEM;
    goto end;
  }

  base64_len = dpl_base64_encode((const u_char*)req->data_buf, req->data_len,
                                 (u_char*)base64_str);
  base64_str[base64_len] = 0;

  value_obj = json_object_new_string(base64_str);
  if (NULL == value_obj) {
    ret = DPL_ENOMEM;
    goto end;
  }

  json_object_object_add(body_obj, "value", value_obj);

  value_obj = NULL;

  /**/
  valuetransferencoding_obj = json_object_new_string("base64");
  if (NULL == valuetransferencoding_obj) {
    ret = DPL_ENOMEM;
    goto end;
  }

  json_object_object_add(body_obj, "valuetransferencoding",
                         valuetransferencoding_obj);

  valuetransferencoding_obj = NULL;

  ret = DPL_SUCCESS;

end:

  if (NULL != base64_str) free(base64_str);

  if (NULL != valuetransferencoding_obj)
    json_object_put(valuetransferencoding_obj);

  if (NULL != value_obj) json_object_put(value_obj);

  return ret;
}

/**
 * build headers from request
 *
 * @param req
 * @param headersp
 *
 * @return
 */
dpl_status_t dpl_cdmi_req_build(const dpl_req_t* req,
                                dpl_cdmi_req_mask_t req_mask,
                                dpl_dict_t** headersp,
                                char** body_strp,
                                int* body_lenp)
{
  dpl_dict_t* headers = NULL;
  int ret, ret2;
  char* method = dpl_method_str(req->method);
  json_object* body_obj = NULL;
  char* body_str = NULL;
  int body_len = 0;
  char buf[256];

  DPL_TRACE(req->ctx, DPL_TRACE_REQ,
            "req_build method=%s bucket=%s resource=%s subresource=%s", method,
            req->bucket, req->resource, req->subresource);

  headers = dpl_dict_new(13);
  if (NULL == headers) {
    ret = DPL_ENOMEM;
    goto end;
  }

  // per method headers
  if (DPL_METHOD_GET == req->method) {
    if (req->range_enabled) {
      ret2 = dpl_add_range_to_headers(&req->range, headers);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }
    }

    switch (req->object_type) {
      case DPL_FTYPE_UNDEF:
        // do nothing
        break;
      case DPL_FTYPE_ANY:
        ret2 = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_ANY, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_REG:
        ret2 = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_OBJECT, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_DIR:
        ret2 = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_CONTAINER,
                            0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_CAP:
        ret2 = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_CAPABILITY,
                            0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_DOM:
        ret2 = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_DOMAIN, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_CHRDEV:
        ret2 = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_CHARDEVICE,
                            0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_BLKDEV:
        ret2 = dpl_dict_add(headers, "Accept",
                            DPL_CDMI_CONTENT_TYPE_BLOCKDEVICE, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_FIFO:
        ret2 = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_FIFO, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_SOCKET:
        ret2 = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_SOCKET, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_SYMLINK:
        ret2
            = dpl_dict_add(headers, "Accept", DPL_CDMI_CONTENT_TYPE_SYMLINK, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
    }
  } else if (DPL_METHOD_PUT == req->method || DPL_METHOD_POST == req->method) {
    if (req->range_enabled) {
      ret2 = dpl_add_content_range_to_headers(&req->range, headers);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }
    }

    body_obj = json_object_new_object();
    if (NULL == body_obj) {
      ret = DPL_ENOMEM;
      goto end;
    }

    if (NULL != req->cache_control) {
      ret2 = dpl_dict_add(headers, "Cache-Control", req->cache_control, 0);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }
    }

    if (NULL != req->content_disposition) {
      ret2 = dpl_dict_add(headers, "Content-Disposition",
                          req->content_disposition, 0);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }
    }

    if (NULL != req->content_encoding) {
      ret2
          = dpl_dict_add(headers, "Content-Encoding", req->content_encoding, 0);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }
    }

    if (!(req_mask & DPL_CDMI_REQ_HTTP_COMPAT)) {
      ret2 = add_metadata_to_json_body(req, body_obj);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      ret2 = add_copy_directive_to_json_body(req, body_obj);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      if (req->data_enabled && DPL_FTYPE_DIR != req->object_type) {
        ret2 = add_data_to_json_body(req, body_obj);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
      }

      pthread_mutex_lock(&req->ctx->lock);
      body_str = (char*)json_object_to_json_string(body_obj);
      pthread_mutex_unlock(&req->ctx->lock);
      if (NULL == body_str) {
        ret = DPL_ENOMEM;
        goto end;
      }

      body_len = strlen(body_str);

      snprintf(buf, sizeof(buf), "%u", body_len);
      ret2 = dpl_dict_add(headers, "Content-Length", buf, 0);
      if (DPL_SUCCESS != ret2) {
        ret = DPL_ENOMEM;
        goto end;
      }
    } else {
      ret2 = add_metadata_to_headers(req, headers, req->object_type);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      if (req->data_enabled) {
        snprintf(buf, sizeof(buf), "%u", req->data_len);
        ret2 = dpl_dict_add(headers, "Content-Length", buf, 0);
        if (DPL_SUCCESS != ret2) {
          ret = DPL_ENOMEM;
          goto end;
        }
      }
    }

    if (req->behavior_flags & DPL_BEHAVIOR_EXPECT) {
      ret2 = dpl_dict_add(headers, "Expect", "100-continue", 0);
      if (DPL_SUCCESS != ret2) {
        ret = DPL_ENOMEM;
        goto end;
      }
    }

    switch (req->object_type) {
      case DPL_FTYPE_UNDEF:
        // do nothing
        break;
      case DPL_FTYPE_ANY:
        // error ?
        break;
      case DPL_FTYPE_REG:
        ret2 = dpl_dict_add(headers, "Content-Type",
                            DPL_CDMI_CONTENT_TYPE_OBJECT, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_DIR:
        ret2 = dpl_dict_add(headers, "Content-Type",
                            DPL_CDMI_CONTENT_TYPE_CONTAINER, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_CAP:
        ret2 = dpl_dict_add(headers, "Content-Type",
                            DPL_CDMI_CONTENT_TYPE_CAPABILITY, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_DOM:
        ret2 = dpl_dict_add(headers, "Content-Type",
                            DPL_CDMI_CONTENT_TYPE_DOMAIN, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_CHRDEV:
        ret2 = dpl_dict_add(headers, "Content-Type",
                            DPL_CDMI_CONTENT_TYPE_CHARDEVICE, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_BLKDEV:
        ret2 = dpl_dict_add(headers, "Content-Type",
                            DPL_CDMI_CONTENT_TYPE_BLOCKDEVICE, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_FIFO:
        ret2 = dpl_dict_add(headers, "Content-Type", DPL_CDMI_CONTENT_TYPE_FIFO,
                            0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_SOCKET:
        ret2 = dpl_dict_add(headers, "Content-Type",
                            DPL_CDMI_CONTENT_TYPE_SOCKET, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
      case DPL_FTYPE_SYMLINK:
        ret2 = dpl_dict_add(headers, "Content-Type",
                            DPL_CDMI_CONTENT_TYPE_SYMLINK, 0);
        if (DPL_SUCCESS != ret2) {
          ret = ret2;
          goto end;
        }
        break;
    }
  } else if (DPL_METHOD_DELETE == req->method) {
  } else {
    ret = DPL_EINVAL;
    goto end;
  }

  // common headers
  ret2 = dpl_add_condition_to_headers(&req->condition, headers);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  if (!(req_mask & DPL_CDMI_REQ_HTTP_COMPAT)) {
    ret2 = dpl_dict_add(headers, "X-CDMI-Specification-Version", "1.0.1", 0);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  }

  if (req->behavior_flags & DPL_BEHAVIOR_KEEP_ALIVE) {
    ret2 = dpl_dict_add(headers, "Connection", "keep-alive", 0);
    if (DPL_SUCCESS != ret2) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  ret2 = dpl_add_basic_authorization_to_headers(req, headers);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  if (NULL != body_strp) {
    if (NULL == body_str) {
      *body_strp = NULL;
    } else {
      *body_strp = strdup(body_str);
      if (NULL == body_strp) {
        ret = DPL_ENOMEM;
        goto end;
      }
    }
  }

  if (NULL != body_lenp) *body_lenp = body_len;

  if (NULL != headersp) {
    *headersp = headers;
    headers = NULL;  // consume it
  }

  ret = DPL_SUCCESS;

end:

  if (NULL != body_obj) json_object_put(body_obj);

  if (NULL != headers) dpl_dict_free(headers);

  return ret;
}

dpl_status_t dpl_cdmi_req_gen_url(const dpl_req_t* req,
                                  dpl_dict_t* headers,
                                  char* buf,
                                  int len,
                                  unsigned int* lenp)
{
  // XXX no spec in CDMI

  return DPL_ENOTSUPP;
}
