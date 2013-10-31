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
 * @defgroup dict Dictionaries
 * @addtogroup dict
 * @{
 * Nestable dictionary keyed on strings.
 *
 * The `dpl_dict_t` structure is a simple key/value dictionary.  Keys
 * are strings, and values may be strings, vectors, or nested dicts.
 * Entries are unique for a given key; adding a second entry with a
 * matching key silently replaces the original.
 *
 * Dicts are implemented with a chained hash table for efficiency.
 *
 * Dicts are used within Droplet to store all kinds of data, but in
 * particular collections of user metadata strings (which map naturally
 * to key/value string pairs).
 */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/*
 * compute a simple hash code
 *
 * note: bad dispersion, good for hash tables
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

/**
 * Create a new dict.
 *
 * Creates and returns a new `dpl_dict_t` object.  The @a n_buckets
 * parameter controls how many hash buckets the dict will use, which
 * is fixed for the lifetime of the dict but does not affect the dict's
 * capacity, only it's performance.  Typically a prime number like 13
 * is a good choice for small dicts.  You should call `dpl_dict_free()`
 * to free the dict when you have finished using it.
 *
 * @param n_buckets specifies how many buckets the dict will use
 * @return a new dict, or NULL on failure
 */
dpl_dict_t *
dpl_dict_new(int n_buckets)
{
  dpl_dict_t *dict;

  if (0 == n_buckets)
      return NULL;

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


/**
 * Lookup an entry.
 *
 * Looks up and returns an entry in the dict matching key @a key exactly,
 * or `NULL` if no matching entry is found.  The `val` field in the returned
 * `dpl_dict_var_t` structure points to a `dpl_value_t` which contains
 * the stored value.  If you know beforehand that the value will be a
 * string, you should call `dpl_dict_get_value()` instead, as it's a
 * more convenient interface.
 *
 * @param dict the dict to look up
 * @param key the key to look up
 * @return the entry matching @a key if found, or NULL
 */
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

/**
 * Lookup an entry using a lowered key
 *
 * Looks up and returns an entry in the dict matching a lower case
 * version of @a key.
 *
 * @param dict the dict to look up in
 * @param key the key to look up
 * @param[out] varp if a matching entry is found, `*varp` will be
 * changed to point at it
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_ENOENT if no matching entry is found, or
 * @retval DPL_* other Droplet error codes on failure
 */
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
 * Iterate the entries of a dict.
 *
 * Calls the callback function @a cb_func once for each entry in the
 * dict.  The callback may safely add or delete entries from the dict;
 * newly added entries may or may not be seen during the iteration.
 *
 * The callback returns a Droplet error code; if it returns any code
 * other than `DPL_SUCCESS` iteration ceases immediately and that code
 * is returned from `dpl_dict_iterate()`.  The callback function should
 * be declared like this
 *
 * @code
 * dpl_status_t
 * my_callback(dpl_dict_var_t *var, void *cb_arg)
 * {
 *    const char *key = var->key;
 *    dpl_value_t *val = var->val;
 *    return DPL_SUCCESS;  // keep iterating
 * }
 * @endcode
 *
 * @param dict the dict whose entries will be iterated
 * @param cb_func callback function to call
 * @param cb_arg opaque pointer passed to callback function
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code returned by the callback function
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

/**
 * Count entries in a dict.
 *
 * Counts and returns the number of entries in the dict.
 *
 * @param dict the dict whose entries are to be counted
 * @return the number of entries in the dict
 */
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

/**
 * Free a dict.
 *
 * Free the dict and all its entries.
 *
 * @param dict the dict to free
 */
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

/**
 * Print the contents of a dict.
 *
 * Prints the keys and values of all entries in the dict in a human
 * readable format with indenting.  Handles non-string values; in
 * particular vectors and nested dicts.  This function is useful only
 * for debugging, as there is no ability to read this information back
 * into a dict.
 *
 * @param dict the dict whose entries are to be printed
 * @param f a stdio file to which to print the entries
 * @param level you should pass 0 for this argument
 */
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
 * Add or replace an entry.
 *
 * Adds to the dict a new entry keyed by @a key with value `value`.
 * If the dict already contains an entry with the same key, the old
 * entry is replaced.  Both @a key and @a value are copied.
 * If the value is a string, you should use `dpl_dict_add()` which is
 * a more convenient interface.
 *
 * @param dict the dict to add an entry to
 * @param key the key for the new
 * @param value the new value to be entered
 * @param lowered if nonzero, @a key is converted to lower case
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
dpl_status_t
dpl_dict_add_value(dpl_dict_t *dict,
                   const char *key,
                   dpl_value_t *value,
                   int lowered)
{
  dpl_dict_var_t *var = NULL;

  if (lowered)
    dpl_dict_get_lowered(dict, key, &var);
  else
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
          return DPL_ENOMEM;

      dpl_value_free(var->val);
      var->val = nval;
    }

  return DPL_SUCCESS;
}

/**
 * Add or replace a string entry.
 *
 * Adds to the dict a new entry keyed by @a key with a string
 * value @a string.  If the dict already contains an entry with
 * the same key, the old entry is replaced.  Both @a key and
 * @a string are copied.
 *
 * @param dict the dict to add an entry to
 * @param key the key for the new entry
 * @param string the new value to be entered
 * @param lowered if nonzero, @a key is converted to lower case
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
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

/**
 * Copy all entries from one dict to another.
 *
 * Copies all the entries from dict @a src to dict @a dst.  If @a dst
 * contains entries which match entries in @a src, those entries will
 * be replaced.
 *
 * @param dst the dict to copy entries to
 * @param src the dict to copy entries from
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
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

/**
 * Duplicate a dict.
 *
 * Creates and returns a new dict containing copies of all the entries
 * from the dict @a src.  You should call `dpl_dict_free()` to free the
 * dict when you have finished using it.
 *
 * @param src the dict to copy entries from
 * @returns a new dict or NULL on failure
 */
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

/**
 * Copy entries matching a prefix from one dict to another
 *
 * Copies to dict @a dst all the entries from dict @a src whose keys
 * match the string prefix @a prefix.  The comparison is case
 * sensitive.  If @a dst contains entries which match entries in
 * @a src, those entries will be replaced.
 *
 * @note if @a src is `NULL`, @a dst will be freed.  This may be
 * surprising.
 *
 * @param dst the dict to copy entries to
 * @param src the dict to copy entries from
 * @param prefix a string prefix for comparing to keys
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
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

/**
 * Copy entries not matching a prefix from one dict to another
 *
 * Copies to dict @a dst all the entries from dict @a src whose keys
 * do @b NOT match the string prefix @a prefix.  The comparison is case
 * sensitive.  If @a dst contains entries which match entries in
 * @a src, those entries will be replaced.
 *
 * @note if @a src is `NULL`, @a dst will be freed.  This may be
 * surprising.
 *
 * @param dst the dict to copy entries to
 * @param src the dict to copy entries from
 * @param prefix a string prefix for comparing to keys
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
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

/**
 * Lookup a string-valued entry.
 *
 * Looks up an entry in the dict matching key @a key exactly, and returns
 * the entry's value string or `NULL` if no matching entry is found.
 * The returned string is owned by the dict, do not free it.
 *
 * A found entry must be string valued, or this function will fail an
 * assert.  Use `dpl_dict_get()` for the more general case where the
 * entry can be other types than a string.
 *
 * @param dict the dict to look up
 * @param key the key to look up
 * @return the matching entry's value if found, or `NULL`
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

/** @} */
