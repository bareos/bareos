/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "xfuncs.h"
#include "vars.h"

#define N_VAR_BUCKETS 13 

static tvar *var_buckets[N_VAR_BUCKETS];

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
hashcode(char *s)
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

tvar *
var_get(char *key)
{
  int bucket;
  tvar *var;

  bucket = hashcode(key) % N_VAR_BUCKETS;

  for (var = var_buckets[bucket];var;var = var->prev)
    {
      if (!strcmp(var->key, key))
        return var;
    }

  return NULL;
}

char *
var_get_value(char *key)
{
  tvar *var;

  var = var_get(key);
  if (NULL == var)
    return NULL;

  return var->value;
}

void
vars_iterate(tvar_iterate_func cb_func,
             void *cb_arg)
{
  int bucket;
  tvar *var;

  for (bucket = 0;bucket < N_VAR_BUCKETS;bucket++)
    {
      for (var = var_buckets[bucket];var;var = var->prev)
        {
          cb_func(var, cb_arg);
        }
    }
}

void
var_set(char *key,
        char *value,
        int cmd,
        tvar_special_func special_func)
{
  tvar *var;

  var = var_get(key);
  if (NULL == var)
    {
      int bucket; 

      var = xmalloc(sizeof (*var));

      memset(var, 0, sizeof (*var));

      var->key = xstrdup(key);
      var->value = xstrdup("");
      
      bucket = hashcode(key) % N_VAR_BUCKETS;
      
      var->next = NULL;
      var->prev = var_buckets[bucket];
      if (NULL != var_buckets[bucket])
        var_buckets[bucket]->next = var;
      var_buckets[bucket] = var;
    }

  if (VAR_CMD_SET_SPECIAL == cmd)
    var->special_func = special_func;
  else if (VAR_CMD_SET == cmd)
    {
      if (NULL != var->special_func)
        {
          char *nvalue;

          nvalue = var->special_func(value);
          if (NULL == nvalue)
            {
              fprintf(stderr, "error in special func\n");
              return ;
            }
          free(var->value);
          var->value = nvalue;
        }
      else
        {
          free(var->value);
          var->value = xstrdup(value);
        }
    }
}

void
var_remove(tvar *var)
{
  int bucket;

  bucket = hashcode(var->key) % N_VAR_BUCKETS;

  if (var->prev)
    var->prev->next = var->next;
  if (var->next)
    var->next->prev = var->prev;
  if (var_buckets[bucket] == var)
    var_buckets[bucket] = var->prev;

  free(var->key);
  free(var->value);
  free(var);
}
