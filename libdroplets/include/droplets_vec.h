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

#ifndef __DROPLETS_VEC_H__
#define __DROPLETS_VEC_H__ 1

typedef struct
{
  void **array;
  int size;
  int incr_size;
  int n_items;
} dpl_vec_t;

/* PROTO droplets_vec.c */
/* src/droplets_vec.c */
dpl_vec_t *dpl_vec_new(int init_size, int incr_size);
dpl_status_t dpl_vec_add(dpl_vec_t *vec, void *item);
void dpl_vec_free(dpl_vec_t *vec);
#endif
