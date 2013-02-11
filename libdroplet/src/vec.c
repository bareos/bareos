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

void
dpl_vec_free(dpl_vec_t *vec)
{
  int i;

  if (NULL != vec->items)
    {
      for (i = 0;i < vec->n_items;i++)
        dpl_value_free(vec->items[i]);
      free(vec->items);
    }
  free(vec);
}

dpl_vec_t *
dpl_vec_new(int init_size,
            int incr_size)
{
  dpl_vec_t *vec = NULL;

  if (init_size < 1)
    return NULL;

  vec = malloc(sizeof (*vec));
  if (NULL == vec)
    goto bad;

  vec->size = init_size;
  vec->incr_size = incr_size;
  vec->n_items = 0;

  vec->items = malloc(sizeof (dpl_value_t *) * init_size);
  if (NULL == vec->items)
    goto bad;

  memset(vec->items, 0, sizeof (dpl_value_t *) * init_size);

  return vec;

 bad:

  if (NULL != vec)
    dpl_vec_free(vec);

  return NULL;
}

/** 
 * add a value to vector
 * 
 * @param vec 
 * @param val
 * 
 * @return 
 */
dpl_status_t 
dpl_vec_add_value(dpl_vec_t *vec, 
                  dpl_value_t *val)
{
  dpl_value_t *nval = NULL;
  dpl_status_t ret;

  nval = dpl_value_dup(val);
  if (NULL == nval)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (vec->n_items == vec->size)
    {
      // Need to allocate more space
      void *new_items;
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

      new_items = realloc(vec->items, sizeof(dpl_value_t *) * new_size);
      if (new_items == (void*) NULL)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      vec->items = new_items;
      vec->size = new_size;
    }

  vec->items[vec->n_items] = nval;
  nval = NULL;
  vec->n_items++;

  ret = DPL_SUCCESS;

 end:

  if (NULL != nval)
    dpl_value_free(nval);

  return ret;
}

dpl_status_t
dpl_vec_add(dpl_vec_t *vec,
            void *item)
{
  dpl_value_t *nval = NULL;
  dpl_status_t ret, ret2;
 
  nval = malloc(sizeof (*nval));
  if (NULL == nval)
    {
      ret = DPL_ENOMEM;
      goto end;
    }
  memset(nval, 0, sizeof (*nval));

  nval->type = DPL_VALUE_VOIDPTR;
  nval->ptr = item;

  ret2 = dpl_vec_add_value(vec, nval);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  ret = DPL_SUCCESS;

 end:

  if (NULL != nval)
    dpl_value_free(nval);

  return ret;
}

/** 
 * return the void ptr bound to ith elt
 * 
 * @param vec 
 * @param i 
 * 
 * @return 
 */
void *
dpl_vec_get(dpl_vec_t *vec,
            int i)
{
  assert(i < vec->n_items);
  assert(vec->items[i]->type == DPL_VALUE_VOIDPTR);
  return vec->items[i]->ptr;
}

dpl_vec_t *
dpl_vec_dup(dpl_vec_t *vec)
{
  dpl_vec_t *nvec = NULL;
  int i;
  dpl_status_t ret2;

  assert(NULL != vec);

  nvec = dpl_vec_new(vec->size, vec->incr_size);
  if (NULL == nvec)
    goto bad;

  for (i = 0;i < vec->n_items;i++)
    {
      ret2 = dpl_vec_add_value(nvec, vec->items[i]);
      if (DPL_SUCCESS != ret2)
        goto bad;
    }
  
  return nvec;

 bad:

  if (NULL != nvec)
    dpl_vec_free(nvec);

  return NULL;
}

void
dpl_vec_sort(dpl_vec_t *vec,
             dpl_value_cmp_func_t cmp_func)
{
  qsort(vec->items, vec->n_items, sizeof (dpl_value_t *), cmp_func);
}

void
dpl_vec_print(dpl_vec_t *vec, 
              FILE *f,
              int level)
{
  int i;

  for (i = 0;i < vec->n_items;i++)
    {
      int last = (i == (vec->n_items - 1));

      dpl_value_print(vec->items[i], f, level, 0);
      if (!last)
        fprintf(f, ",");
    }
}
