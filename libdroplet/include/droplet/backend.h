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
typedef dpl_status_t (*dpl_list_all_my_buckets_t)(dpl_ctx_t *ctx, dpl_vec_t **vecp);
typedef dpl_status_t (*dpl_list_bucket_t)(dpl_ctx_t *ctx, const char *bucket, const char *prefix, const char *delimiter, dpl_vec_t **objectsp, dpl_vec_t **common_prefixesp);
typedef dpl_status_t (*dpl_make_bucket_t)(dpl_ctx_t *ctx, const char *bucket, const dpl_sysmd_t *sysmd);
typedef dpl_status_t (*dpl_delete_bucket_t)(dpl_ctx_t *ctx, const char *bucket);
typedef dpl_status_t (*dpl_post_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const char *data_buf, unsigned int data_len, const dpl_dict_t *query_params, char **resource_idp);
typedef dpl_status_t (*dpl_post_buffered_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, unsigned int data_len, const dpl_dict_t *query_params, dpl_conn_t **connp);
typedef dpl_status_t (*dpl_put_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const char *data_buf, unsigned int data_len);
typedef dpl_status_t (*dpl_put_buffered_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, unsigned int data_len, dpl_conn_t **connp);
typedef dpl_status_t (*dpl_get_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_get_range_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_get_buffered_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_header_func_t header_func, dpl_buffer_func_t buffer_func, void *cb_arg);
typedef dpl_status_t (*dpl_head_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_head_all_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_head_sysmd_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_sysmd_t *sysmdp, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_get_metadata_from_headers_t)(const dpl_dict_t *headers, dpl_dict_t *metadata);
typedef dpl_status_t (*dpl_delete_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type);
typedef dpl_status_t (*dpl_genurl_t)(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
typedef dpl_status_t (*dpl_copy_t)(dpl_ctx_t *ctx, const char *src_bucket, const char *src_resource, const char *src_subresource, const char *dst_bucket, const char *dst_resource, const char *dst_subresource, dpl_ftype_t object_type, dpl_metadata_directive_t metadata_directive, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const dpl_condition_t *condition);
typedef dpl_status_t (*dpl_get_id_path_t)(dpl_ctx_t *ctx, const char *bucket, char **id_pathp);
typedef dpl_status_t (*dpl_gen_id_from_oid_t)(dpl_ctx_t *ctx, uint64_t oid, dpl_storage_class_t storage_class, char **resource_idp);

typedef struct dpl_backend_s
{
  const char *name; /*!< name of the backend */
  dpl_list_all_my_buckets_t list_all_my_buckets;
  dpl_list_bucket_t list_bucket;
  dpl_make_bucket_t make_bucket;
  dpl_delete_bucket_t delete_bucket;
  dpl_post_t post;
  dpl_post_buffered_t post_buffered;
  dpl_put_t put;
  dpl_put_buffered_t put_buffered;
  dpl_get_t get;
  dpl_get_range_t get_range;
  dpl_get_buffered_t get_buffered;
  dpl_head_t head;
  dpl_head_all_t head_all;
  dpl_get_metadata_from_headers_t get_metadata_from_headers; /*!< broken by design since not all backends have metadata in headers */
  dpl_head_sysmd_t head_sysmd;
  dpl_delete_t deletef;
  dpl_genurl_t genurl;
  dpl_copy_t copy;
  dpl_get_id_path_t get_id_path;
  dpl_gen_id_from_oid_t gen_id_from_oid;
} dpl_backend_t;

/* PROTO backend.c */
#endif
