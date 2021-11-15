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

#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_BACKEND_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_BACKEND_H_

/**
 * All backend fn return a @e dpl_status_t and get a @e dpl_ctx_t* as first
 * argument.
 */
#define DCL_BACKEND_FN(fn, args...) dpl_status_t(fn)(dpl_ctx_t*, ##args)

#define DCL_BACKEND_GET_CAPABILITIES_FN(fn) \
  DCL_BACKEND_FN(fn, dpl_capability_t*)
#define DCL_BACKEND_LOGIN_FN(fn) DCL_BACKEND_FN(fn)
#define DCL_BACKEND_LIST_ALL_MY_BUCKETS_FN(fn) \
  DCL_BACKEND_FN(fn, dpl_vec_t**, char**)
#define DCL_BACKEND_LIST_BUCKET_FN(fn)                                 \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*, const int, \
                 dpl_vec_t**, dpl_vec_t**, char**)
#define DCL_BACKEND_LIST_BUCKET_ATTRS_FN(fn)                           \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*, const int, \
                 dpl_dict_t**, dpl_sysmd_t*, dpl_vec_t**, dpl_vec_t**, char**)
#define DCL_BACKEND_MAKE_BUCKET_FN(fn) \
  DCL_BACKEND_FN(fn, const char*, const dpl_sysmd_t*, char**)
#define DCL_BACKEND_DELETE_BUCKET_FN(fn) DCL_BACKEND_FN(fn, const char*, char**)
#define DCL_BACKEND_PUT_FN(fn)                                               \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*,                  \
                 const dpl_option_t*, dpl_ftype_t, const dpl_condition_t*,   \
                 const dpl_range_t*, const dpl_dict_t*, const dpl_sysmd_t*,  \
                 const char*, unsigned int, const dpl_dict_t*, dpl_sysmd_t*, \
                 char**)
#define DCL_BACKEND_GET_FN(fn)                                             \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*,                \
                 const dpl_option_t*, dpl_ftype_t, const dpl_condition_t*, \
                 const dpl_range_t*, char**, unsigned int*, dpl_dict_t**,  \
                 dpl_sysmd_t*, char**)
#define DCL_BACKEND_HEAD_FN(fn)                                            \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*,                \
                 const dpl_option_t*, dpl_ftype_t, const dpl_condition_t*, \
                 dpl_dict_t**, dpl_sysmd_t*, char**)
#define DCL_BACKEND_HEAD_RAW_FN(fn)                                        \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*,                \
                 const dpl_option_t*, dpl_ftype_t, const dpl_condition_t*, \
                 dpl_dict_t**, char**)
#define DCL_BACKEND_DELETE_FN(fn)                                          \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*,                \
                 const dpl_option_t*, dpl_ftype_t, const dpl_condition_t*, \
                 char**)
#define DCL_BACKEND_DELETE_ALL_FN(fn)                                   \
  DCL_BACKEND_FN(fn, const char*, dpl_locators_t*, const dpl_option_t*, \
                 const dpl_condition_t*, dpl_vec_t**)
#define DCL_BACKEND_DELETE_ALL_ID_FN(fn)                        \
  DCL_BACKEND_FN(fn, const char*, const char*, dpl_locators_t*, \
                 const dpl_option_t*, const dpl_condition_t*, dpl_vec_t**)
#define DCL_BACKEND_GENURL_FN(fn)                                  \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*,        \
                 const dpl_option_t*, time_t, char*, unsigned int, \
                 unsigned int*, char**)
#define DCL_BACKEND_COPY_FN(fn)                                               \
  DCL_BACKEND_FN(fn, const char*, const char*, const char*, const char*,      \
                 const char*, const char*, const dpl_option_t*, dpl_ftype_t,  \
                 dpl_copy_directive_t, const dpl_dict_t*, const dpl_sysmd_t*, \
                 const dpl_condition_t*, char**)
#define DCL_BACKEND_GET_ID_SCHEME_FN(fn) DCL_BACKEND_FN(fn, dpl_id_scheme_t**)

struct json_object;
#define DCL_BACKEND_STREAM_RESUME_FN(fn) \
  DCL_BACKEND_FN(fn, dpl_stream_t*, struct json_object*)
#define DCL_BACKEND_STREAM_GETMD_FN(fn) \
  DCL_BACKEND_FN(fn, dpl_stream_t*, dpl_dict_t**, dpl_sysmd_t**)
#define DCL_BACKEND_STREAM_GET_FN(fn)                                    \
  DCL_BACKEND_FN(fn, dpl_stream_t*, unsigned int, char**, unsigned int*, \
                 struct json_object**)
#define DCL_BACKEND_STREAM_PUTMD_FN(fn) \
  DCL_BACKEND_FN(fn, dpl_stream_t*, dpl_dict_t*, dpl_sysmd_t*)
#define DCL_BACKEND_STREAM_PUT_FN(fn) \
  DCL_BACKEND_FN(fn, dpl_stream_t*, char*, unsigned int, struct json_object**)
#define DCL_BACKEND_STREAM_FLUSH_FN(fn) DCL_BACKEND_FN(fn, dpl_stream_t*)

typedef DCL_BACKEND_GET_CAPABILITIES_FN(*dpl_get_capabilities_t);
typedef DCL_BACKEND_LOGIN_FN(*dpl_login_t);
typedef DCL_BACKEND_LIST_ALL_MY_BUCKETS_FN(*dpl_list_all_my_buckets_t);
typedef DCL_BACKEND_LIST_BUCKET_FN(*dpl_list_bucket_t);
typedef DCL_BACKEND_LIST_BUCKET_ATTRS_FN(*dpl_list_bucket_attrs_t);
typedef DCL_BACKEND_MAKE_BUCKET_FN(*dpl_make_bucket_t);
typedef DCL_BACKEND_DELETE_BUCKET_FN(*dpl_delete_bucket_t);
typedef DCL_BACKEND_PUT_FN(*dpl_put_t);
typedef DCL_BACKEND_GET_FN(*dpl_get_t);
typedef DCL_BACKEND_HEAD_FN(*dpl_head_t);
typedef DCL_BACKEND_HEAD_RAW_FN(*dpl_head_raw_t);
typedef DCL_BACKEND_DELETE_FN(*dpl_delete_t);
typedef DCL_BACKEND_DELETE_ALL_FN(*dpl_delete_all_t);
typedef DCL_BACKEND_DELETE_ALL_ID_FN(*dpl_delete_all_id_t);
typedef DCL_BACKEND_GENURL_FN(*dpl_genurl_t);
typedef DCL_BACKEND_COPY_FN(*dpl_copy_t);
typedef DCL_BACKEND_GET_ID_SCHEME_FN(*dpl_get_id_scheme_t);
typedef DCL_BACKEND_STREAM_RESUME_FN(*dpl_stream_resume_t);
typedef DCL_BACKEND_STREAM_GETMD_FN(*dpl_stream_getmd_t);
typedef DCL_BACKEND_STREAM_GET_FN(*dpl_stream_get_t);
typedef DCL_BACKEND_STREAM_PUTMD_FN(*dpl_stream_putmd_t);
typedef DCL_BACKEND_STREAM_PUT_FN(*dpl_stream_put_t);
typedef DCL_BACKEND_STREAM_FLUSH_FN(*dpl_stream_flush_t);

typedef struct dpl_backend_s {
  const char* name; /*!< name of the backend */
  dpl_get_capabilities_t get_capabilities;
  dpl_login_t login;
  dpl_list_all_my_buckets_t list_all_my_buckets;
  dpl_list_bucket_t list_bucket;
  dpl_list_bucket_attrs_t list_bucket_attrs;
  dpl_make_bucket_t make_bucket;
  dpl_delete_bucket_t delete_bucket;
  dpl_put_t post;
  dpl_put_t put;
  dpl_get_t get;
  dpl_head_t head;
  dpl_head_raw_t head_raw;
  dpl_delete_t deletef;
  dpl_delete_all_t delete_all;
  dpl_delete_all_id_t delete_all_id;
  dpl_get_id_scheme_t get_id_scheme;
  dpl_put_t post_id;
  dpl_put_t put_id;
  dpl_get_t get_id;
  dpl_head_t head_id;
  dpl_head_raw_t head_id_raw;
  dpl_delete_t delete_id;
  dpl_genurl_t genurl;
  dpl_copy_t copy;
  dpl_copy_t copy_id;
  dpl_stream_resume_t stream_resume;
  dpl_stream_getmd_t stream_getmd;
  dpl_stream_get_t stream_get;
  dpl_stream_putmd_t stream_putmd;
  dpl_stream_put_t stream_put;
  dpl_stream_flush_t stream_flush;
} dpl_backend_t;

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_BACKEND_H_
