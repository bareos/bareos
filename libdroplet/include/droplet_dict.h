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

#ifndef __DROPLET_DICT_H__
#define __DROPLET_DICT_H__ 1

typedef struct dpl_var
{
  struct dpl_var *prev;
  struct dpl_var *next;

  char *key;
  char *value;
} dpl_var_t;

typedef void (*dpl_dict_func_t)(dpl_var_t *var, void *cb_arg);

typedef struct
{
  dpl_var_t **buckets;
  u_int n_buckets;
} dpl_dict_t;

/* PROTO droplet_dict.c */
/* src/droplet_dict.c */
dpl_dict_t *dpl_dict_new(int n_buckets);
dpl_var_t *dpl_dict_get(dpl_dict_t *dict, char *key);
dpl_status_t dpl_dict_get_lowered(dpl_dict_t *dict, char *key, dpl_var_t **varp);
char *dpl_dict_get_value(dpl_dict_t *dict, char *key);
void dpl_dict_iterate(dpl_dict_t *dict, dpl_dict_func_t cb_func, void *cb_arg);
int dpl_dict_count(dpl_dict_t *dict);
void dpl_dict_free(dpl_dict_t *dict);
void dpl_dict_print(dpl_dict_t *dict);
dpl_status_t dpl_dict_add(dpl_dict_t *dict, char *key, char *value, int lowered);
void dpl_dict_remove(dpl_dict_t *dict, dpl_var_t *var);
#endif
