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
#ifndef __DROPLET_BACKEND_H__
#define __DROPLET_BACKEND_H__ 1

/* general */
typedef dpl_status_t (*dpl_get_capabilities_t)(dpl_ctx_t *ctx, dpl_capability_t *maskp);
typedef dpl_status_t (*dpl_list_all_my_buckets_t)(dpl_ctx_t *ctx, dpl_vec_t **vecp, char **locationp);
typedef dpl_status_t (*dpl_list_bucket_t)(dpl_ctx_t *ctx, const char *bucket, const char *prefix, const char *delimiter, const int max_keys, dpl_vec_t **objectsp, dpl_vec_t **common_prefixesp, char **locationp);
typedef dpl_status_t (*dpl_make_bucket_t)(dpl_ctx_t *ctx, const char *bucket, const dpl_sysmd_t *sysmd, char **locationp);
typedef dpl_status_t (*dpl_delete_bucket_t)(dpl_ctx_t *ctx, const char *bucket, char **locationp);
typedef dpl_status_t (*dpl_put_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const char *data_buf, unsigned int data_len, const dpl_dict_t *query_params, dpl_sysmd_t *returned_sysmdp, char **locationp);
typedef dpl_status_t (*dpl_put_buffered_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, unsigned int data_len, const dpl_dict_t *query_params, dpl_conn_t **connp, char **locationp);
typedef dpl_status_t (*dpl_get_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp, char **locationp);
typedef dpl_status_t (*dpl_get_buffered_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range, dpl_metadatum_func_t metadatum_func, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp, dpl_buffer_func_t buffer_func, void *cb_arg, char **locationp);
typedef dpl_status_t (*dpl_head_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp, char **locationp);
typedef dpl_status_t (*dpl_head_raw_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_dict_t **metadatap, char **locationp);
typedef dpl_status_t (*dpl_delete_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, char **locationp);
typedef dpl_status_t (*dpl_genurl_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, const dpl_option_t *option, time_t expires, char *buf, unsigned int len, unsigned int *lenp, char **locationp);
typedef dpl_status_t (*dpl_copy_t)(dpl_ctx_t *ctx, const char *src_bucket, const char *src_resource, const char *src_subresource, const char *dst_bucket, const char *dst_resource, const char *dst_subresource, const dpl_option_t *option, dpl_ftype_t object_type, dpl_copy_directive_t copy_directive, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const dpl_condition_t *condition, char **locationp);

typedef dpl_status_t (*dpl_get_id_scheme_t)(dpl_ctx_t *ctx, dpl_id_scheme_t **id_schemep);

typedef struct dpl_backend_s
{
  const char *name; /*!< name of the backend */
  dpl_get_capabilities_t get_capabilities;
  dpl_list_all_my_buckets_t list_all_my_buckets;
  dpl_list_bucket_t list_bucket;
  dpl_make_bucket_t make_bucket;
  dpl_delete_bucket_t delete_bucket;
  dpl_put_t post;
  dpl_put_buffered_t post_buffered;
  dpl_put_t put;
  dpl_put_buffered_t put_buffered;
  dpl_get_t get;
  dpl_get_buffered_t get_buffered;
  dpl_head_t head;
  dpl_head_raw_t head_raw;
  dpl_delete_t deletef;
  dpl_get_id_scheme_t get_id_scheme;
  dpl_put_t post_id;
  dpl_put_buffered_t post_id_buffered;
  dpl_put_t put_id;
  dpl_put_buffered_t put_id_buffered;
  dpl_get_t get_id;
  dpl_get_buffered_t get_id_buffered;
  dpl_head_t head_id;
  dpl_head_raw_t head_id_raw;
  dpl_delete_t delete_id;
  dpl_genurl_t genurl;
  dpl_copy_t copy;
  dpl_copy_t copy_id;
} dpl_backend_t;

/* PROTO backend.c */
#endif
