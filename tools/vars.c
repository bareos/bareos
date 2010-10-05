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

#include "dplsh.h"

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
  tvar *var, *prev;

  for (bucket = 0;bucket < N_VAR_BUCKETS;bucket++)
    {
      for (var = var_buckets[bucket];var;var = prev)
        {
          prev = var->prev;
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

void
free_cb(tvar *var,
        void *cb_arg)
{
  free(var->key);
  free(var->value);
  free(var);
}

int
vars_free()
{
  vars_iterate(free_cb, NULL);

  return 0;
}

/**/

void
save_cb(tvar *var,
        void *cb_arg)
{
  FILE *f = (FILE *) cb_arg;

  fprintf(f, "%s=%s\n", var->key, var->value);
}

int
vars_save()
{
  char *home;
  char buf[1024];
  FILE *f;

  home = getenv("HOME");
  if (NULL == home)
    {
      fprintf(stderr, "no HOME\n");
      return -1;
    }

  snprintf(buf, sizeof (buf), "%s/%s", home, DPLSHRC);

  f = fopen(buf, "w");
  if (NULL == f)
    {
      fprintf(stderr, "open %s failed\n", buf);
      return -1;
    }

  vars_iterate(save_cb, f);

  fflush(f);
  fclose(f);

  return 0;
}

int
vars_load()
{
  char *home;
  char buf[1024];
  FILE *f;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;

  home = getenv("HOME");
  if (NULL == home)
    {
      fprintf(stderr, "no HOME\n");
      return -1;
    }

  snprintf(buf, sizeof (buf), "%s/%s", home, DPLSHRC);

  f = fopen(buf, "r");
  if (NULL == f)
    return -1;

  while ((read = getline(&line, &len, f)) != -1) 
    {
      enum shell_error shell_err;
      int ret;
      
      //printf("%s", line);

      ret = shell_parse(cmd_defs, line, &shell_err);
      if (ret == SHELL_EPARSE)
        {
          fprintf(stderr, 
                  "parsing: %s\n", shell_error_str(shell_err));
        }
    }

  if (NULL != line)
    free(line);
  
  fclose(f);
  
  return 0;
}
