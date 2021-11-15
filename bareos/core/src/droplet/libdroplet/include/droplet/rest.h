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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_REST_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_REST_H_

const char* dpl_get_backend_name(dpl_ctx_t* ctx);
dpl_status_t dpl_get_capabilities(dpl_ctx_t* ctx, dpl_capability_t* maskp);
dpl_status_t dpl_login(dpl_ctx_t* ctx);
dpl_status_t dpl_list_all_my_buckets(dpl_ctx_t* ctx, dpl_vec_t** vecp);
dpl_status_t dpl_list_bucket(dpl_ctx_t* ctx,
                             const char* bucket,
                             const char* prefix,
                             const char* delimiter,
                             const int max_keys,
                             dpl_vec_t** objectsp,
                             dpl_vec_t** common_prefixesp);
dpl_status_t dpl_list_bucket_attrs(dpl_ctx_t* ctx,
                                   const char* bucket,
                                   const char* prefix,
                                   const char* delimiter,
                                   const int max_keys,
                                   dpl_dict_t** metadatap,
                                   dpl_sysmd_t* sysmdp,
                                   dpl_vec_t** objectsp,
                                   dpl_vec_t** common_prefixesp);
dpl_status_t dpl_make_bucket(dpl_ctx_t* ctx,
                             const char* bucket,
                             dpl_location_constraint_t location_constraint,
                             dpl_canned_acl_t canned_acl);
dpl_status_t dpl_delete_bucket(dpl_ctx_t* ctx, const char* bucket);
dpl_status_t dpl_post(dpl_ctx_t* ctx,
                      const char* bucket,
                      const char* path,
                      const dpl_option_t* option,
                      dpl_ftype_t object_type,
                      const dpl_condition_t* condition,
                      const dpl_range_t* range,
                      const dpl_dict_t* metadata,
                      const dpl_sysmd_t* sysmd,
                      const char* data_buf,
                      unsigned int data_len,
                      const dpl_dict_t* query_params,
                      dpl_sysmd_t* returned_sysmdp);
dpl_status_t dpl_put(dpl_ctx_t* ctx,
                     const char* bucket,
                     const char* path,
                     const dpl_option_t* option,
                     dpl_ftype_t object_type,
                     const dpl_condition_t* condition,
                     const dpl_range_t* range,
                     const dpl_dict_t* metadata,
                     const dpl_sysmd_t* sysmd,
                     const char* data_buf,
                     unsigned int data_len);
dpl_status_t dpl_get(dpl_ctx_t* ctx,
                     const char* bucket,
                     const char* path,
                     const dpl_option_t* option,
                     dpl_ftype_t object_type,
                     const dpl_condition_t* condition,
                     const dpl_range_t* range,
                     char** data_bufp,
                     unsigned int* data_lenp,
                     dpl_dict_t** metadatap,
                     dpl_sysmd_t* sysmdp);
dpl_status_t dpl_get_noredirect(dpl_ctx_t* ctx,
                                const char* bucket,
                                const char* path,
                                dpl_ftype_t object_type,
                                char** locationp);
dpl_status_t dpl_head(dpl_ctx_t* ctx,
                      const char* bucket,
                      const char* path,
                      const dpl_option_t* option,
                      dpl_ftype_t object_type,
                      const dpl_condition_t* condition,
                      dpl_dict_t** metadatap,
                      dpl_sysmd_t* sysmdp);
dpl_status_t dpl_head_raw(dpl_ctx_t* ctx,
                          const char* bucket,
                          const char* path,
                          const dpl_option_t* option,
                          dpl_ftype_t object_type,
                          const dpl_condition_t* condition,
                          dpl_dict_t** metadatap);
dpl_status_t dpl_delete(dpl_ctx_t* ctx,
                        const char* bucket,
                        const char* path,
                        const dpl_option_t* option,
                        dpl_ftype_t object_type,
                        const dpl_condition_t* condition);
dpl_status_t dpl_delete_all(dpl_ctx_t* ctx,
                            const char* bucket,
                            dpl_locators_t* locators,
                            const dpl_option_t* option,
                            const dpl_condition_t* condition,
                            dpl_vec_t** objects);
dpl_status_t dpl_copy(dpl_ctx_t* ctx,
                      const char* src_bucket,
                      const char* src_path,
                      const char* dst_bucket,
                      const char* dst_path,
                      const dpl_option_t* option,
                      dpl_ftype_t object_type,
                      dpl_copy_directive_t copy_directive,
                      const dpl_dict_t* metadata,
                      const dpl_sysmd_t* sysmd,
                      const dpl_condition_t* condition);
dpl_status_t dpl_post_id(dpl_ctx_t* ctx,
                         const char* bucket,
                         const char* id,
                         const dpl_option_t* option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t* condition,
                         const dpl_range_t* range,
                         const dpl_dict_t* metadata,
                         const dpl_sysmd_t* sysmd,
                         const char* data_buf,
                         unsigned int data_len,
                         const dpl_dict_t* query_params,
                         dpl_sysmd_t* returned_sysmdp);
dpl_status_t dpl_put_id(dpl_ctx_t* ctx,
                        const char* bucket,
                        const char* id,
                        const dpl_option_t* option,
                        dpl_ftype_t object_type,
                        const dpl_condition_t* condition,
                        const dpl_range_t* range,
                        const dpl_dict_t* metadata,
                        const dpl_sysmd_t* sysmd,
                        const char* data_buf,
                        unsigned int data_len);
dpl_status_t dpl_get_id(dpl_ctx_t* ctx,
                        const char* bucket,
                        const char* id,
                        const dpl_option_t* option,
                        dpl_ftype_t object_type,
                        const dpl_condition_t* condition,
                        const dpl_range_t* range,
                        char** data_bufp,
                        unsigned int* data_lenp,
                        dpl_dict_t** metadatap,
                        dpl_sysmd_t* sysmdp);
dpl_status_t dpl_head_id(dpl_ctx_t* ctx,
                         const char* bucket,
                         const char* id,
                         const dpl_option_t* option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t* condition,
                         dpl_dict_t** metadatap,
                         dpl_sysmd_t* sysmdp);
dpl_status_t dpl_head_raw_id(dpl_ctx_t* ctx,
                             const char* bucket,
                             const char* id,
                             const dpl_option_t* option,
                             dpl_ftype_t object_type,
                             const dpl_condition_t* condition,
                             dpl_dict_t** metadatap);
dpl_status_t dpl_delete_id(dpl_ctx_t* ctx,
                           const char* bucket,
                           const char* id,
                           const dpl_option_t* option,
                           dpl_ftype_t object_type,
                           const dpl_condition_t* condition);
dpl_status_t dpl_delete_all_id(dpl_ctx_t* ctx,
                               const char* bucket,
                               const char* ressource,
                               dpl_locators_t* locators,
                               const dpl_option_t* option,
                               const dpl_condition_t* condition,
                               dpl_vec_t** objects);
dpl_status_t dpl_copy_id(dpl_ctx_t* ctx,
                         const char* src_bucket,
                         const char* src_id,
                         const char* dst_bucket,
                         const char* dst_path,
                         const dpl_option_t* option,
                         dpl_ftype_t object_type,
                         dpl_copy_directive_t copy_directive,
                         const dpl_dict_t* metadata,
                         const dpl_sysmd_t* sysmd,
                         const dpl_condition_t* condition);
dpl_status_t dpl_genurl(dpl_ctx_t* ctx,
                        const char* bucket,
                        const char* path,
                        const dpl_option_t* option,
                        time_t expires,
                        char* buf,
                        unsigned int len,
                        unsigned int* lenp);

struct json_object;
dpl_status_t dpl_stream_open(dpl_ctx_t* ctx,
                             const char* bucket,
                             const char* path,
                             const dpl_option_t* option,
                             const dpl_condition_t* condition,
                             dpl_dict_t* metadata,
                             dpl_sysmd_t* sysmd,
                             dpl_stream_t** streamp);
dpl_status_t dpl_stream_openid(dpl_ctx_t* ctx,
                               const char* bucket,
                               const char* id,
                               const dpl_option_t* option,
                               const dpl_condition_t* condition,
                               dpl_dict_t* metadata,
                               dpl_sysmd_t* sysmd,
                               dpl_stream_t** streamp);
dpl_status_t dpl_stream_resume(dpl_ctx_t* ctx,
                               dpl_stream_t* stream,
                               struct json_object* status);
dpl_status_t dpl_stream_getmd(dpl_ctx_t* ctx,
                              dpl_stream_t* stream,
                              dpl_dict_t** metadatap,
                              dpl_sysmd_t** sysmdp);
dpl_status_t dpl_stream_get(dpl_ctx_t* ctx,
                            dpl_stream_t* stream,
                            unsigned int len,
                            char** data_bufp,
                            unsigned int* data_lenp,
                            struct json_object** statusp);
dpl_status_t dpl_stream_putmd(dpl_ctx_t* ctx,
                              dpl_stream_t* stream,
                              dpl_dict_t* metadata,
                              dpl_sysmd_t* sysmd);
dpl_status_t dpl_stream_put(dpl_ctx_t* ctx,
                            dpl_stream_t* stream,
                            char* buf,
                            unsigned int len,
                            struct json_object** statusp);
dpl_status_t dpl_stream_flush(dpl_ctx_t* ctx, dpl_stream_t* stream);
void dpl_stream_close(dpl_ctx_t* ctx, dpl_stream_t* stream);

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_REST_H_
