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
