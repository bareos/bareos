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

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/** 
 * allow null contained values
 * 
 * @note a value of DPL_VALUE_VOIDPTR will not be freed
 *
 * @param value must not be null
 */
void
dpl_value_free(dpl_value_t *value)
{
  switch (value->type)
    {
    case DPL_VALUE_STRING:
      if (NULL != value->string)
        dpl_sbuf_free(value->string);
      break ;
    case DPL_VALUE_SUBDICT:
      if (NULL != value->subdict)
        dpl_dict_free(value->subdict);
      break ;
    case DPL_VALUE_VECTOR:
      if (NULL != value->vector)
        dpl_vec_free(value->vector);
      break ;
    case DPL_VALUE_VOIDPTR:
      //cannot take decision
      break ;
    }
  free(value);
}

/** 
 * duplicate a value
 * 
 * @note duping a VOIDPTR causes a pointer stealing
 *
 * @param src 
 * 
 * @return 
 */
dpl_value_t *
dpl_value_dup(dpl_value_t *src)
{
  dpl_value_t *dst = NULL;

  assert(NULL != src);

  dst = malloc(sizeof (*dst));
  if (NULL == dst)
    return NULL;
  
  memset(dst, 0, sizeof (*dst));

  dst->type = src->type;

  switch (src->type)
    {
    case DPL_VALUE_STRING:
      if (NULL != src->string)
        {
          dst->string = dpl_sbuf_dup(src->string);
          if (NULL == dst->string)
            goto bad;
        }
      break ;
    case DPL_VALUE_SUBDICT:
      if (NULL != src->subdict)
        {
          dst->subdict = dpl_dict_dup(src->subdict);
          if (NULL == dst->subdict)
            goto bad;
        }
      break ;
    case DPL_VALUE_VECTOR:
      if (NULL != src->vector)
        {
          dst->vector = dpl_vec_dup(src->vector);
          if (NULL == dst->vector)
            goto bad;
        }
      break ;
    case DPL_VALUE_VOIDPTR:
      dst->ptr = src->ptr;
      break ;
    }

  return dst;

 bad:

  if (NULL != dst)
    free(dst);

  return NULL;
}

void
dpl_value_print(dpl_value_t *val,
                FILE *f,
                int level,
                int indent)
{
  int i;

  switch (val->type)
    {
    case DPL_VALUE_STRING:
      dpl_sbuf_print(f, val->string);
      break ;
    case DPL_VALUE_SUBDICT:
      if (indent)
        for (i = 0;i < level;i++)
          fprintf(f, " ");
      fprintf(f, "{\n");
      dpl_dict_print(val->subdict, f, level+1);
      for (i = 0;i < level;i++)
        fprintf(f, " ");
      fprintf(f, "}");
      break ;
    case DPL_VALUE_VECTOR:
      if (indent)
        for (i = 0;i < level;i++)
          fprintf(f, " ");
      fprintf(f, "[");
      dpl_vec_print(val->vector, f, level+1);
      fprintf(f, "]");
      break ;
    case DPL_VALUE_VOIDPTR:
      fprintf(f, "%p", val->ptr);
      break ;
    }
}
