/*
 * Copyright (C) 2020-2021 Bareos GmbH & Co. KG
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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_DICT_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_DICT_H_

struct dpl_dict;

typedef struct dpl_dict_var {
  struct dpl_dict_var* prev;
  struct dpl_dict_var* next;
  char* key;
  dpl_value_t* val;
} dpl_dict_var_t;

typedef dpl_status_t (*dpl_dict_func_t)(dpl_dict_var_t* var, void* cb_arg);

typedef struct dpl_dict {
  dpl_dict_var_t** buckets;
  unsigned int n_buckets;
} dpl_dict_t;

/* PROTO dict.c */
/* src/dict.c */
dpl_dict_t* dpl_dict_new(int n_buckets);
dpl_dict_var_t* dpl_dict_get(const dpl_dict_t* dict, const char* key);
dpl_status_t dpl_dict_get_lowered(const dpl_dict_t* dict,
                                  const char* key,
                                  dpl_dict_var_t** varp);
char* dpl_dict_get_value(const dpl_dict_t* dict, const char* key);
dpl_status_t dpl_dict_iterate(const dpl_dict_t* dict,
                              dpl_dict_func_t cb_func,
                              void* cb_arg);
int dpl_dict_count(const dpl_dict_t* dict);
void dpl_dict_var_free(dpl_dict_var_t* var);
void dpl_dict_free(dpl_dict_t* dict);
void dpl_dict_print(const dpl_dict_t* dict, FILE* f, int level);
dpl_status_t dpl_dict_add_value(dpl_dict_t* dict,
                                const char* key,
                                dpl_value_t* value,
                                int lowered);
dpl_status_t dpl_dict_add(dpl_dict_t* dict,
                          const char* key,
                          const char* string,
                          int lowered);
void dpl_dict_remove(dpl_dict_t* dict, dpl_dict_var_t* var);
dpl_status_t dpl_dict_copy(dpl_dict_t* dst, const dpl_dict_t* src);
dpl_dict_t* dpl_dict_dup(const dpl_dict_t* src);
dpl_status_t dpl_dict_filter_prefix(dpl_dict_t* dst,
                                    const dpl_dict_t* src,
                                    const char* prefix);
dpl_status_t dpl_dict_filter_no_prefix(dpl_dict_t* dst,
                                       const dpl_dict_t* src,
                                       const char* prefix);
#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_DICT_H_
