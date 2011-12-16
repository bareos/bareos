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

/**
 * compute a simple hash code
 *
 * note: bad dispersion, good for hash tables
 *
 * @param s
 *
 * @return
 */
static int
dict_hashcode(const char *s)
{
  const char *p;
  unsigned h, g;

  h = g = 0;
  for (p=s;*p!='\0';p=p+1)
    {
      h = (h<<4)+(*p);
      if ((g=h&0xf0000000))
        {
          h=h^(g>>24);
          h=h^g;
        }
    }

  return h;
}

dpl_dict_t *
dpl_dict_new(int n_buckets)
{
  dpl_dict_t *dict;

  dict = malloc(sizeof (*dict));
  if (NULL == dict)
    return NULL;

  memset(dict, 0, sizeof (*dict));

  dict->buckets = malloc(n_buckets * sizeof (dpl_var_t *));
  if (NULL == dict->buckets)
    {
      free(dict);
      return NULL;
    }
  memset(dict->buckets, 0, n_buckets * sizeof (dpl_var_t *));

  dict->n_buckets = n_buckets;

  return dict;
}

dpl_var_t *
dpl_dict_get(const dpl_dict_t *dict,
             const char *key)
{
  int bucket;
  dpl_var_t *var;

  bucket = dict_hashcode(key) % dict->n_buckets;

  for (var = dict->buckets[bucket];var;var = var->prev)
    {
      if (!strcmp(var->key, key))
        return var;
    }

  return NULL;
}

dpl_status_t
dpl_dict_get_lowered(const dpl_dict_t *dict,
                     const char *key,
                     dpl_var_t **varp)
{
  dpl_var_t *var;
  char *nkey;

  nkey = strdup(key);
  if (NULL == nkey)
    return DPL_ENOMEM;

  dpl_strlower(nkey);

  var = dpl_dict_get(dict, nkey);

  free(nkey);

  if (NULL != varp)
    *varp = var;

  return DPL_SUCCESS;
}

char *
dpl_dict_get_value(const dpl_dict_t *dict,
                   const char *key)
{
  dpl_var_t *var;

  var = dpl_dict_get(dict, key);
  if (NULL == var)
    return NULL;

  return var->value;
}

void
dpl_dict_iterate(const dpl_dict_t *dict,
                 dpl_dict_func_t cb_func,
                 void *cb_arg)
{
  int bucket;
  dpl_var_t *var, *prev;

  for (bucket = 0;bucket < dict->n_buckets;bucket++)
    {
      for (var = dict->buckets[bucket];var;var = prev)
        {
          prev = var->prev;
          cb_func(var, cb_arg);
        }
    }
}

static void
cb_var_count(dpl_var_t *var,
             void *cb_arg)
{
  int *count = (int *) cb_arg;

  (*count)++;
}

int
dpl_dict_count(const dpl_dict_t *dict)
{
  int count;

  count = 0;
  dpl_dict_iterate(dict, cb_var_count, &count);

  return count;
}

static void
cb_var_free(dpl_var_t *var,
            void *arg)
{
  free(var->key);
  free(var->value);
  free(var);
}

void
dpl_dict_free(dpl_dict_t *dict)
{
  dpl_dict_iterate(dict, cb_var_free, NULL);
  free(dict->buckets);
  free(dict);
}

static void
cb_var_print(dpl_var_t *var,
             void *arg)
{
  fprintf(stderr, "%s=%s\n", var->key, var->value);
}

void
dpl_dict_print(const dpl_dict_t *dict)
{
  dpl_dict_iterate(dict, cb_var_print, NULL);
}

dpl_status_t
dpl_dict_add(dpl_dict_t *dict,
             const char *key,
             const char *value,
             int lowered)
{
  dpl_var_t *var;

  var = dpl_dict_get(dict, key);
  if (NULL == var)
    {
      int bucket;

      var = malloc(sizeof (*var));
      if (NULL == var)
        return DPL_ENOMEM;

      memset(var, 0, sizeof (*var));

      var->key = strdup(key);
      if (NULL == var->key)
        {
          free(var);
          return DPL_ENOMEM;
        }

      if (1 == lowered)
        dpl_strlower(var->key);

      var->value = strdup(value);
      if (NULL == var->value)
        {
          free(var->key);
          free(var);
          return DPL_ENOMEM;
        }

      bucket = dict_hashcode(var->key) % dict->n_buckets;

      var->next = NULL;
      var->prev = dict->buckets[bucket];
      if (NULL != dict->buckets[bucket])
        dict->buckets[bucket]->next = var;
      dict->buckets[bucket] = var;
    }
  else
    {
      char *nval;

      nval = strdup(value);
      if (NULL == nval)
        return DPL_ENOMEM;
      free(var->value);
      var->value = nval;
    }

  return DPL_SUCCESS;
}

void
dpl_dict_remove(dpl_dict_t *dict,
                dpl_var_t *var)
{
  int bucket;

  bucket = dict_hashcode(var->key) % dict->n_buckets;

  if (var->prev)
    var->prev->next = var->next;
  if (var->next)
    var->next->prev = var->prev;
  if (dict->buckets[bucket] == var)
    dict->buckets[bucket] = var->prev;

  free(var->key);
  free(var->value);
  free(var);
}



static void
cb_var_copy(dpl_var_t *var,
            void *arg)
{
  dpl_dict_add((dpl_dict_t *)arg, var->key, var->value, 0);
}

dpl_status_t
dpl_dict_copy(dpl_dict_t *dst,
              const dpl_dict_t *src)
{
  if (! dst)
    return DPL_FAILURE;

  if (src)
    dpl_dict_iterate(src, cb_var_copy, dst);
  else
    dpl_dict_free(dst);

  return DPL_SUCCESS;
}


struct conviterate {
  dpl_dict_t *dict;
  const char *prefix;
  int reverse_logic;
};

static void
cb_var_filter_string(dpl_var_t *var,
                     void *cb_arg)
{
  struct conviterate *conv = cb_arg;
  size_t prefix_len;

  if (! conv->prefix)
    return;

  prefix_len = strlen(conv->prefix);

  if (0 == strncmp(var->key, conv->prefix, prefix_len))
    {
      if (!conv->reverse_logic)
        dpl_dict_add(conv->dict, var->key + prefix_len, var->value, 0);
    }
  else
    {
      if (conv->reverse_logic)
        dpl_dict_add(conv->dict, var->key, var->value, 0);
    }
}

dpl_status_t
dpl_dict_filter_prefix(dpl_dict_t *dst,
                       const dpl_dict_t *src,
                       const char *prefix)
{
  if (! dst)
    return DPL_FAILURE;

  if (src) {
    struct conviterate conv = { .dict = dst, .prefix = prefix, .reverse_logic = 0 };
    dpl_dict_iterate(src, cb_var_filter_string, &conv);
  } else {
    dpl_dict_free(dst);
  }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_dict_filter_no_prefix(dpl_dict_t *dst,
                          const dpl_dict_t *src,
                          const char *prefix)
{
  if (! dst)
    return DPL_FAILURE;

  if (src) {
    struct conviterate conv = { .dict = dst, .prefix = prefix, .reverse_logic = 1 };
    dpl_dict_iterate(src, cb_var_filter_string, &conv);
  } else {
    dpl_dict_free(dst);
  }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_dict_update_value(dpl_dict_t *dict,
                      const char *key,
                      const char *value)
{
  dpl_var_t *var = NULL;

  if (! dict || ! value || ! key)
    return DPL_FAILURE;

  var = dpl_dict_get(dict, key);

  if (! var)
    return dpl_dict_add(dict, key, value, 0);

  free(var->value);
  var->value = strdup(value);

  if (! var->value)
    return DPL_ENOMEM;

  return DPL_SUCCESS;
}
