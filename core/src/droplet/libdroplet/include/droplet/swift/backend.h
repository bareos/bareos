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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SWIFT_BACKEND_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SWIFT_BACKEND_H_

/* PROTO backend.c */
/* src/backend.c */
dpl_status_t dpl_swift_get_capabilities(dpl_ctx_t* ctx,
                                        dpl_capability_t* maskp);
/* dpl_status_t dpl_swift_list_bucket(dpl_ctx_t *ctx, const char *bucket, const
 * char *prefix, const char *delimiter, const int max_keys, dpl_vec_t
 * **objectsp, dpl_vec_t **common_prefixesp, char **locationp); */

dpl_status_t dpl_swift_login(dpl_ctx_t* ctx);
dpl_status_t dpl_swift_get(dpl_ctx_t* ctx,
                           const char* bucket,
                           const char* resource,
                           const char* subresource,
                           const dpl_option_t* option,
                           dpl_ftype_t object_type,
                           const dpl_condition_t* condition,
                           const dpl_range_t* range,
                           char** data_bufp,
                           unsigned int* data_lenp,
                           dpl_dict_t** metadatap,
                           dpl_sysmd_t* sysmdp,
                           char** locationp);
dpl_status_t dpl_swift_put(dpl_ctx_t* ctx,
                           const char* bucket,
                           const char* resource,
                           const char* subresource,
                           const dpl_option_t* option,
                           dpl_ftype_t object_type,
                           const dpl_condition_t* condition,
                           const dpl_range_t* range,
                           const dpl_dict_t* metadata,
                           const dpl_sysmd_t* sysmd,
                           const char* data_buf,
                           unsigned int data_len,
                           const dpl_dict_t* query_params,
                           dpl_sysmd_t* returned_sysmdp,
                           char** locationp);
dpl_status_t dpl_swift_delete(dpl_ctx_t* ctx,
                              const char* bucket,
                              const char* resource,
                              const char* subresource,
                              const dpl_option_t* option,
                              dpl_ftype_t object_type,
                              const dpl_condition_t* condition,
                              char** locationp);


#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SWIFT_BACKEND_H_
