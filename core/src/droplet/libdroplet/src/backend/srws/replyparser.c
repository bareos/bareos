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
#include <droplet/srws/replyparser.h>

// #define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
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

dpl_status_t dpl_srws_get_metadatum_from_header(
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

  if (!strcmp(header, DPL_SRWS_X_BIZ_USERMD)) {
    DPRINTF("val=%s\n", value);

    // decode base64
    int value_len = strlen(value);
    if (value_len == 0) return DPL_EINVAL;

    orig_len = DPL_BASE64_ORIG_LENGTH(value_len);
    orig = alloca(orig_len);

    orig_len = dpl_base64_decode((u_char*)value, value_len, (u_char*)orig);

    // dpl_dump_simple(orig, orig_len);

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
  } else {
    if (sysmdp) {
      if (!strcmp(header, "content-length")) {
        sysmdp->mask |= DPL_SYSMD_MASK_SIZE;
        sysmdp->size = atoi(value);
      } else if (!strcmp(header, "last-modified")) {
        sysmdp->mask |= DPL_SYSMD_MASK_MTIME;
        sysmdp->mtime = dpl_get_date(value, NULL);
      } else if (!strcmp(header, "etag")) {
        int value_len = strlen(value);

        if (value_len < DPL_SYSMD_ETAG_SIZE && value_len >= 2) {
          sysmdp->mask |= DPL_SYSMD_MASK_ETAG;
          // supress double quotes
          strncpy(sysmdp->etag, value + 1, DPL_SYSMD_ETAG_SIZE);
          sysmdp->etag[value_len - 2] = 0;
        }
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
  return dpl_srws_get_metadatum_from_header(
      var->key, dpl_sbuf_get_str(var->val->string), NULL, NULL, mc->metadata,
      mc->sysmdp);
}

dpl_status_t dpl_srws_get_metadata_from_headers(const dpl_dict_t* headers,
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
