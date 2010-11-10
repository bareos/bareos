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

dpl_vec_t *
dpl_vec_new(int init_size,
            int incr_size)
{
  dpl_vec_t *vec;

  if (init_size < 1)
    return NULL;

  vec = malloc(sizeof (*vec));
  if (NULL == vec)
    return NULL;

  vec->size = init_size;
  vec->incr_size = incr_size;
  vec->n_items = 0;

  vec->array = malloc(sizeof (void *) * init_size);
  if (NULL == vec->array)
    {
      free(vec);
      return NULL;
    }

  memset(vec->array, 0, sizeof (void *) * init_size);

  return vec;
}

dpl_status_t 
dpl_vec_add(dpl_vec_t *vec,
            void *item)
{
  if (vec->n_items == vec->size)
    {
      // Need to allocate more space
      void *new_array;
      int new_size;
      
      // Compute new size
      if (vec->incr_size <= 0)
        {
          // Grow exponentially
          if (1 < vec->size)
            {
              new_size = vec->size + (vec->size / 2);
            }
          else
            {
              new_size = 2;
            }
        }
      else
        {
          // Linear (slow) growth
          new_size = vec->size + vec->incr_size;
        }
      
      new_array = realloc(vec->array, sizeof(void *) * new_size);
      if (new_array == (void*) NULL)
        return -1;
      
      vec->array = new_array;
      vec->size = new_size;
    }

  vec->array[vec->n_items] = item;
  vec->n_items++;

  return 0;
}

void 
dpl_vec_free(dpl_vec_t *vec)
{
  free(vec->array);
  free(vec);
}
