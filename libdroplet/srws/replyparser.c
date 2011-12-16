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
#include <droplet/srws/replyparser.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

struct mdparse_data
{
  const char *orig;
  int orig_len;
  dpl_dict_t *metadata;
};

static int
cb_ntinydb(const char *key_ptr,
           int key_len,
           void *cb_arg)
{
  struct mdparse_data *arg = (struct mdparse_data *) cb_arg;
  int ret, ret2;
  const char *value_returned = NULL;
  int value_len_returned;
  char *key_str;
  char *value_str;

  DPRINTF("%.*s\n", key_len, key_ptr);
  
  key_str = alloca(key_len + 1);
  memcpy(key_str, key_ptr, key_len);
  key_str[key_len] = 0;

  ret2 = dpl_ntinydb_get(arg->orig, arg->orig_len, key_str, &value_returned, &value_len_returned);
  if (DPL_SUCCESS != ret2)
    {
      ret = -1;
      goto end;
    }

  DPRINTF("%.*s\n", value_len_returned, value_returned);

  value_str = alloca(value_len_returned + 1);
  memcpy(value_str, value_returned, value_len_returned);
  value_str[value_len_returned] = 0;
  
  ret2 = dpl_dict_add(arg->metadata, key_str, value_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = -1;
      goto end;
    }

  ret = 0;
  
 end:

  return ret;
}

dpl_status_t
dpl_srws_get_metadata_from_headers(const dpl_dict_t *headers,
                                   dpl_dict_t *metadata)
{
  int ret;
  dpl_var_t *var;
  int value_len;
  char *orig;
  int orig_len;
  struct mdparse_data arg;

  DPRINTF("srws_get_metadata_from_headers\n");

  memset(&arg, 0, sizeof (arg));

  var = dpl_dict_get(headers, DPL_SRWS_X_BIZ_USERMD);
  if (NULL == var)
    {
      //no metadata;
      ret = DPL_SUCCESS;
      goto end;
    }

  DPRINTF("val=%s\n", var->value);

  //decode base64
  value_len = strlen(var->value);
  if (value_len == 0)
    return DPL_EINVAL;

  orig_len = DPL_BASE64_ORIG_LENGTH(value_len);
  orig = alloca(orig_len);

  orig_len = dpl_base64_decode((u_char *) var->value, value_len, (u_char *) orig);
  
  //dpl_dump_simple(orig, orig_len);

  arg.metadata = metadata;
  arg.orig = orig;
  arg.orig_len = orig_len;
  ret = dpl_ntinydb_list(orig, orig_len, cb_ntinydb, &arg);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}
