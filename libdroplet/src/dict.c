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

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

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

  dict->buckets = malloc(n_buckets * sizeof (dpl_dict_var_t *));
  if (NULL == dict->buckets)
    {
      free(dict);
      return NULL;
    }
  memset(dict->buckets, 0, n_buckets * sizeof (dpl_dict_var_t *));

  dict->n_buckets = n_buckets;

  return dict;
}

dpl_dict_var_t *
dpl_dict_get(const dpl_dict_t *dict,
             const char *key)
{
  int bucket;
  dpl_dict_var_t *var;

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
                     dpl_dict_var_t **varp)
{
  dpl_dict_var_t *var;
  char *nkey;

  nkey = strdup(key);
  if (NULL == nkey)
    return DPL_ENOMEM;

  dpl_strlower(nkey);

  var = dpl_dict_get(dict, nkey);

  free(nkey);

  if (NULL == var)
    return DPL_ENOENT;

  if (NULL != varp)
    *varp = var;

  return DPL_SUCCESS;
}

/** 
 * stop if cb_func status != DPL_SUCCESS
 * 
 * @param dict 
 * @param cb_func 
 * @param cb_arg 
 * 
 * @return the last failed status or DPL_SUCCESS
 */
dpl_status_t
dpl_dict_iterate(const dpl_dict_t *dict,
                 dpl_dict_func_t cb_func,
                 void *cb_arg)
{
  int bucket;
  dpl_dict_var_t *var, *prev;
  dpl_status_t ret2;

  for (bucket = 0;bucket < dict->n_buckets;bucket++)
    {
      for (var = dict->buckets[bucket];var;var = prev)
        {
          prev = var->prev;
          ret2 = cb_func(var, cb_arg);
          if (DPL_SUCCESS != ret2)
            return ret2;
        }
    }

  return DPL_SUCCESS;
}

static dpl_status_t
cb_var_count(dpl_dict_var_t *var,
             void *cb_arg)
{
  int *count = (int *) cb_arg;

  (*count)++;
  return DPL_SUCCESS;
}

int
dpl_dict_count(const dpl_dict_t *dict)
{
  int count;

  count = 0;
  dpl_dict_iterate(dict, cb_var_count, &count);

  return count;
}

void
dpl_dict_var_free(dpl_dict_var_t *var)
{
  dpl_value_free(var->val);
  free(var);
}

static dpl_status_t
cb_var_free(dpl_dict_var_t *var,
            void *arg)
{
  free(var->key);
  dpl_dict_var_free(var);
  return DPL_SUCCESS;
}

void
dpl_dict_free(dpl_dict_t *dict)
{
  dpl_dict_iterate(dict, cb_var_free, NULL);
  free(dict->buckets);
  free(dict);
}

struct print_data
{
  FILE *f;
  int level;
};

static dpl_status_t
cb_var_print(dpl_dict_var_t *var,
             void *arg)
{
  struct print_data *pd = (struct print_data *) arg;
  int i;
  
  for (i = 0;i < pd->level;i++)
    fprintf(pd->f, " ");

  fprintf(pd->f, "%s=", var->key);
  dpl_value_print(var->val, pd->f, pd->level + 1, 0);
  fprintf(pd->f, "\n");

  return DPL_SUCCESS;
}

void
dpl_dict_print(const dpl_dict_t *dict,
               FILE *f,
               int level)
{
  struct print_data pd;

  pd.f = f;
  pd.level = level;
  dpl_dict_iterate(dict, cb_var_print, &pd);
}

/** 
 * make a copy of the variable
 * 
 * @param dict 
 * @param key 
 * @param var 
 * @param lowered 
 * 
 * @return 
 */
dpl_status_t
dpl_dict_add_value(dpl_dict_t *dict,
                   const char *key,
                   dpl_value_t *value,
                   int lowered)
{
  dpl_dict_var_t *var;

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

      var->val = dpl_value_dup(value);
      if (NULL == var->val)
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
      dpl_value_t *nval;

      nval = dpl_value_dup(value);
      if (NULL == nval)
        {
          free(var->key);
          free(var);
          return DPL_ENOMEM;
        }

      dpl_value_free(var->val);
      var->val = nval;
    }

  return DPL_SUCCESS;
}

/** 
 * assume value is DPL_VALUE_STRING
 * 
 * @param dict 
 * @param key 
 * @param string
 * @param lowered 
 * 
 * @return 
 */
dpl_status_t
dpl_dict_add(dpl_dict_t *dict,
             const char *key,
             const char *string,
             int lowered)
{
  dpl_sbuf_t sbuf;
  dpl_value_t value;

  sbuf.allocated = 0;
  sbuf.buf = (char *)string;
  sbuf.len = strlen(string);
  value.type = DPL_VALUE_STRING;
  value.string = &sbuf;
  return dpl_dict_add_value(dict, key, &value, lowered);
}

void
dpl_dict_remove(dpl_dict_t *dict,
                dpl_dict_var_t *var)
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
  dpl_dict_var_free(var);
}

static dpl_status_t
cb_var_copy(dpl_dict_var_t *var,
            void *arg)
{
  return dpl_dict_add_value((dpl_dict_t *)arg, var->key, var->val, 0);
}

dpl_status_t
dpl_dict_copy(dpl_dict_t *dst,
              const dpl_dict_t *src)
{
  dpl_status_t ret;

  if (! dst)
    return DPL_FAILURE;

  if (src)
    ret = dpl_dict_iterate(src, cb_var_copy, dst);
  else
    {
      dpl_dict_free(dst);
      ret = DPL_SUCCESS;
    }

  return ret;
}

dpl_dict_t *
dpl_dict_dup(const dpl_dict_t *src)
{
  dpl_dict_t *dst;
  dpl_status_t status;

  assert(NULL != src);

  dst = dpl_dict_new(src->n_buckets);
  if (NULL == dst)
    return NULL;

  status = dpl_dict_copy(dst, src);
  if (DPL_SUCCESS != status)
    {
      dpl_dict_free(dst);
      return NULL;
    }
  
  return dst;
}

struct conviterate 
{
  dpl_dict_t *dict;
  const char *prefix;
  int reverse_logic;
};

static dpl_status_t
cb_var_filter_string(dpl_dict_var_t *var,
                     void *cb_arg)
{
  struct conviterate *conv = cb_arg;
  size_t prefix_len;
  dpl_status_t ret = DPL_SUCCESS;

  if (! conv->prefix)
    return DPL_SUCCESS;

  prefix_len = strlen(conv->prefix);

  if (0 == strncmp(var->key, conv->prefix, prefix_len))
    {
      if (!conv->reverse_logic)
        ret = dpl_dict_add_value(conv->dict, var->key + prefix_len, var->val, 0);
    }
  else
    {
      if (conv->reverse_logic)
        ret = dpl_dict_add_value(conv->dict, var->key, var->val, 0);
    }

  return ret;
}

dpl_status_t
dpl_dict_filter_prefix(dpl_dict_t *dst,
                       const dpl_dict_t *src,
                       const char *prefix)
{
  dpl_status_t ret;

  if (! dst)
    return DPL_FAILURE;

  if (src) {
    struct conviterate conv = { .dict = dst, .prefix = prefix, .reverse_logic = 0 };
    ret = dpl_dict_iterate(src, cb_var_filter_string, &conv);
  } else {
    dpl_dict_free(dst);
    ret = DPL_SUCCESS;
  }

  return ret;
}

dpl_status_t
dpl_dict_filter_no_prefix(dpl_dict_t *dst,
                          const dpl_dict_t *src,
                          const char *prefix)
{
  dpl_status_t ret;

  if (! dst)
    return DPL_FAILURE;

  if (src) {
    struct conviterate conv = { .dict = dst, .prefix = prefix, .reverse_logic = 1 };
    ret = dpl_dict_iterate(src, cb_var_filter_string, &conv);
  } else {
    dpl_dict_free(dst);
    ret = DPL_SUCCESS;
  }

  return ret;
}

/*
 * helpers for DPL_VALUE_STRING
 */

char *
dpl_dict_get_value(const dpl_dict_t *dict,
                   const char *key)
{
  dpl_dict_var_t *var;

  var = dpl_dict_get(dict, key);
  if (NULL == var)
    return NULL;

  assert(var->val->type == DPL_VALUE_STRING);
  return dpl_sbuf_get_str(var->val->string);
}
