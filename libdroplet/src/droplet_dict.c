/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
dict_hashcode(char *s)
{
  char *p;
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
dpl_dict_get(dpl_dict_t *dict,
             char *key)
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
dpl_dict_get_lowered(dpl_dict_t *dict,
                     char *key,
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
dpl_dict_get_value(dpl_dict_t *dict,
                   char *key)
{
  dpl_var_t *var;

  var = dpl_dict_get(dict, key);
  if (NULL == var)
    return NULL;

  return var->value;
}

void
dpl_dict_iterate(dpl_dict_t *dict,
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

  count++;
}

int
dpl_dict_count(dpl_dict_t *dict)
{
  int count;
  
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

dpl_status_t
dpl_dict_add(dpl_dict_t *dict,
             char *key,
             char *value,
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
