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

#ifndef __VARS_H__
#define __VARS_H__ 1

typedef char * (*tvar_special_func)(char *value);

typedef struct svar
{
  struct svar *prev;
  struct svar *next;

  char *key;
  char *value;

  tvar_special_func special_func;
} tvar;

typedef void (*tvar_iterate_func)(tvar *var, void *cb_arg);

#define VAR_CMD_SET         0
#define VAR_CMD_SET_SPECIAL 1

/* PROTO vars.c */
/* ./vars.c */
tvar *var_get(char *key);
char *var_get_value(char *key);
void vars_iterate(tvar_iterate_func cb_func, void *cb_arg);
void var_set(char *key, char *value, int cmd, tvar_special_func special_func);
void var_remove(tvar *var);
void save_cb(tvar *var, void *cb_arg);
int vars_save(void);
int vars_load(void);
#endif
