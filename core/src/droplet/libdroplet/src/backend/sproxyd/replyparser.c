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
#include <droplet/sproxyd/replyparser.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt, ...)

struct mdparse_data {
  const char* orig;
  int orig_len;
  dpl_dict_t* metadata;
  dpl_metadatum_func_t metadatum_func;
  void* cb_arg;
};

static int cb_ntinydb(const char* key_ptr, int key_len, void* cb_arg)
{
  struct mdparse_data* arg = (struct mdparse_data*)cb_arg;
  int ret, ret2;
  const char* value_returned = NULL;
  int value_len_returned;
  char* key_str;
  char* value_str;

  DPRINTF("%.*s\n", key_len, key_ptr);

  key_str = alloca(key_len + 1);
  memcpy(key_str, key_ptr, key_len);
  key_str[key_len] = 0;

  ret2 = dpl_ntinydb_get(arg->orig, arg->orig_len, key_str, &value_returned,
                         &value_len_returned);
  if (DPL_SUCCESS != ret2) {
    ret = -1;
    goto end;
  }

  DPRINTF("%.*s\n", value_len_returned, value_returned);

  value_str = alloca(value_len_returned + 1);
  memcpy(value_str, value_returned, value_len_returned);
  value_str[value_len_returned] = 0;

  if (arg->metadatum_func) {
    dpl_sbuf_t sbuf;
    dpl_value_t val;

    sbuf.allocated = 0;
    sbuf.buf = value_str;
    sbuf.len = strlen(value_str);
    val.type = DPL_VALUE_STRING;
    val.string = &sbuf;
    ret2 = arg->metadatum_func(arg->cb_arg, key_str, &val);
    if (DPL_SUCCESS != ret2) {
      ret = ret2;
      goto end;
    }
  }

  ret2 = dpl_dict_add(arg->metadata, key_str, value_str, 0);
  if (DPL_SUCCESS != ret2) {
    ret = -1;
    goto end;
  }

  ret = 0;

end:

  return ret;
}

dpl_status_t dpl_sproxyd_get_metadatum_from_header(
    const char* header,
    const char* value,
    dpl_metadatum_func_t metadatum_func,
    void* cb_arg,
    dpl_dict_t* metadata,
    dpl_sysmd_t* sysmdp)
{
  dpl_status_t ret, ret2;
  char* orig;
  int orig_len;
  struct mdparse_data arg;
  int value_len;

  if (!strcasecmp(header, DPL_SPROXYD_X_SCAL_USERMD)) {
    DPRINTF("val=%s\n", value);

    // decode base64
    value_len = strlen(value);
    if (value_len == 0) return DPL_SUCCESS;

    orig_len = DPL_BASE64_ORIG_LENGTH(value_len);
    orig = alloca(orig_len);

    orig_len = dpl_base64_decode((u_char*)value, value_len, (u_char*)orig);

    // dpl_dump_simple(orig, orig_len);

    if (metadata) {
      arg.metadata = metadata;
      arg.orig = orig;
      arg.orig_len = orig_len;
      arg.metadatum_func = metadatum_func;
      arg.cb_arg = cb_arg;

      ret2 = dpl_ntinydb_list(orig, orig_len, cb_ntinydb, &arg);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }
    }
  } else {
    if (sysmdp) {
      if (!strcasecmp(header, DPL_SPROXYD_X_SCAL_SIZE)) {
        sysmdp->mask |= DPL_SYSMD_MASK_SIZE;
        sysmdp->size = atoi(value);
      } else if (!strcasecmp(header, DPL_SPROXYD_X_SCAL_ATIME)) {
        sysmdp->mask |= DPL_SYSMD_MASK_ATIME;
        sysmdp->atime = dpl_get_date(value, NULL);
      } else if (!strcasecmp(header, DPL_SPROXYD_X_SCAL_MTIME)) {
        sysmdp->mask |= DPL_SYSMD_MASK_MTIME;
        sysmdp->mtime = dpl_get_date(value, NULL);
      } else if (!strcasecmp(header, DPL_SPROXYD_X_SCAL_CTIME)) {
        sysmdp->mask |= DPL_SYSMD_MASK_CTIME;
        sysmdp->ctime = dpl_get_date(value, NULL);
      } else if (!strcasecmp(header, DPL_SPROXYD_X_SCAL_VERSION)) {
        sysmdp->mask |= DPL_SYSMD_MASK_VERSION;
        snprintf(sysmdp->version, sizeof(sysmdp->version), "%s", value);
      } else if (!strcasecmp(header, DPL_SPROXYD_X_SCAL_CRC32)) {
        sysmdp->mask |= DPL_SYSMD_MASK_ETAG;
        snprintf(sysmdp->etag, sizeof(sysmdp->etag), "%s", value);
      }
    }
  }

  ret = DPL_SUCCESS;

end:

  return ret;
}

struct metadata_conven {
  dpl_dict_t* metadata;
  dpl_sysmd_t* sysmdp;
};

static dpl_status_t cb_headers_iterate(dpl_dict_var_t* var, void* cb_arg)
{
  struct metadata_conven* mc = (struct metadata_conven*)cb_arg;

  assert(var->val->type == DPL_VALUE_STRING);
  return dpl_sproxyd_get_metadatum_from_header(
      var->key, dpl_sbuf_get_str(var->val->string), NULL, NULL, mc->metadata,
      mc->sysmdp);
}

dpl_status_t dpl_sproxyd_get_metadata_from_headers(const dpl_dict_t* headers,
                                                   dpl_dict_t** metadatap,
                                                   dpl_sysmd_t* sysmdp)
{
  dpl_dict_t* metadata = NULL;
  dpl_status_t ret, ret2;
  struct metadata_conven mc;

  if (metadatap) {
    metadata = dpl_dict_new(13);
    if (NULL == metadata) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  memset(&mc, 0, sizeof(mc));
  mc.metadata = metadata;
  mc.sysmdp = sysmdp;

  if (sysmdp) sysmdp->mask = 0;

  ret2 = dpl_dict_iterate(headers, cb_headers_iterate, &mc);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  if (NULL != metadatap) {
    *metadatap = metadata;
    metadata = NULL;
  }

  ret = DPL_SUCCESS;

end:

  if (NULL != metadata) dpl_dict_free(metadata);

  return ret;
}

static dpl_status_t parse_delete_all_deleted(const dpl_ctx_t* ctx,
                                             xmlNode* elem,
                                             dpl_vec_t* objects)
{
  dpl_status_t ret = DPL_SUCCESS;
  dpl_delete_object_t* object;

  object = (dpl_delete_object_t*)malloc(sizeof(dpl_delete_object_t));
  if (object == NULL) return DPL_ENOMEM;

  object->status = DPL_SUCCESS;
  object->name = NULL;
  object->version_id = NULL;
  object->error = NULL;

  while (elem != NULL && ret == DPL_SUCCESS) {
    if (elem->type == XML_ELEMENT_NODE) {
      if (!strcmp((char*)elem->name, "Key")) {
        object->name = strdup((char*)elem->children->content);
        if (object->name == NULL) ret = DPL_ENOMEM;
      } else if (!strcmp((char*)elem->name, "VersionId")) {
        object->version_id = strdup((char*)elem->children->content);
        if (object->version_id == NULL) ret = DPL_ENOMEM;
      }
    }
    elem = elem->next;
  }

  if (ret == DPL_SUCCESS) ret = dpl_vec_add(objects, object);

  if (ret != DPL_SUCCESS) dpl_delete_object_free(object);

  return ret;
}

static dpl_status_t parse_delete_all_delete_error(const dpl_ctx_t* ctx,
                                                  xmlNode* elem,
                                                  dpl_vec_t* objects)
{
  dpl_status_t ret = DPL_SUCCESS;
  dpl_delete_object_t* object;

  object = (dpl_delete_object_t*)malloc(sizeof(dpl_delete_object_t));
  if (object == NULL) return DPL_ENOMEM;

  object->status = DPL_FAILURE;
  object->name = NULL;
  object->version_id = NULL;
  object->error = NULL;

  while (elem != NULL && ret == DPL_SUCCESS) {
    if (elem->type == XML_ELEMENT_NODE) {
      if (!strcmp((char*)elem->name, "Key")) {
        object->name = strdup((char*)elem->children->content);
        if (object->name == NULL) ret = DPL_ENOMEM;
      } else if (!strcmp((char*)elem->name, "VersionId")) {
        object->version_id = strdup((char*)elem->children->content);
        if (object->version_id == NULL) ret = DPL_ENOMEM;
      } else if (!strcmp((char*)elem->name, "Message")) {
        object->error = strdup((char*)elem->children->content);
        if (object->error == NULL) ret = DPL_ENOMEM;
      }
    }
    elem = elem->next;
  }

  if (ret == DPL_SUCCESS) ret = dpl_vec_add(objects, object);

  if (ret != DPL_SUCCESS) dpl_delete_object_free(object);

  return ret;
}

static dpl_status_t parse_delete_all_children(const dpl_ctx_t* ctx,
                                              xmlNode* elem,
                                              dpl_vec_t* objects)
{
  dpl_status_t ret = DPL_SUCCESS;

  while (elem != NULL) {
    if (elem->type == XML_ELEMENT_NODE) {
      if (!strcmp((char*)elem->name, "Deleted"))
        ret = parse_delete_all_deleted(ctx, elem->children, objects);
      else if (!strcmp((char*)elem->name, "Error"))
        ret = parse_delete_all_delete_error(ctx, elem->children, objects);

      if (ret != DPL_SUCCESS) break;
    }
    elem = elem->next;
  }

  return ret;
}

static void parse_delete_all_error(const dpl_ctx_t* ctx, xmlNode* elem)
{
  char *code = NULL, *message = NULL;

  while (elem != NULL) {
    if (elem->type == XML_ELEMENT_NODE) {
      if (!strcmp((char*)elem->name, "Code"))
        code = (char*)elem->children->content;
      else if (!strcmp((char*)elem->name, "Message"))
        message = (char*)elem->children->content;
    }
    elem = elem->next;
  }

  if (message == NULL) return;

  if (code != NULL)
    DPL_LOG((dpl_ctx_t*)ctx, DPL_ERROR, "Error: %s (%s)", message, code);
  else
    DPL_LOG((dpl_ctx_t*)ctx, DPL_ERROR, "Error: %s", message);
}

dpl_status_t dpl_sproxyd_parse_delete_all(const dpl_ctx_t* ctx,
                                          const char* buf,
                                          int len,
                                          dpl_vec_t* objects)
{
  int ret = DPL_SUCCESS;
  xmlParserCtxtPtr ctxt;
  xmlDocPtr doc;
  xmlNode* elem;

  ctxt = xmlNewParserCtxt();
  if (ctxt == NULL) return DPL_FAILURE;

  doc = xmlCtxtReadMemory(ctxt, buf, len, NULL, NULL, 0u);
  if (doc == NULL) {
    xmlFreeParserCtxt(ctxt);
    return DPL_FAILURE;
  }

  elem = xmlDocGetRootElement(doc);
  while (elem != NULL) {
    if (elem->type == XML_ELEMENT_NODE) {
      if (!strcmp((char*)elem->name, "DeleteResult")) {
        ret = parse_delete_all_children(ctx, elem->children, objects);
        if (ret != DPL_SUCCESS) break;
      } else if (!strcmp((char*)elem->name, "Error"))
        parse_delete_all_error(ctx, elem->children);
    }
    elem = elem->next;
  }

  xmlFreeDoc(doc);
  xmlFreeParserCtxt(ctxt);

  return ret;
}
