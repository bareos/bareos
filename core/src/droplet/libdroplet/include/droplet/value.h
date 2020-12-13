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
#ifndef __DROPLET_VAR_H__
#define __DROPLET_VAR_H__ 1

#include <droplet/sbuf.h>

struct dpl_vec;
struct dpl_dict;

typedef enum
  {
    DPL_VALUE_STRING = 0,
    DPL_VALUE_SUBDICT,
    DPL_VALUE_VECTOR,
    DPL_VALUE_VOIDPTR,
  } dpl_value_type_t;

typedef struct dpl_value
{
  union
  {
    dpl_sbuf_t *string;
    struct dpl_dict *subdict;
    struct dpl_vec *vector;
    void *ptr;
  };

  dpl_value_type_t type;
} dpl_value_t;

typedef int (*dpl_value_cmp_func_t)(const void *p1, const void *p2);

/* PROTO value.c */
void dpl_value_free(dpl_value_t *value);
dpl_value_t *dpl_value_dup(dpl_value_t *src);
void dpl_value_print(dpl_value_t *val, FILE *f, int level, int indent);
#endif
