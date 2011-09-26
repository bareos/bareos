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
typedef dpl_status_t (*dpl_list_bucket_t)(dpl_ctx_t *ctx, char *bucket, char *prefix, char *delimiter, dpl_vec_t **objectsp, dpl_vec_t **common_prefixesp);
typedef dpl_status_t (*dpl_make_bucket_t)(dpl_ctx_t *ctx, char *bucket, dpl_location_constraint_t location_constraint, dpl_canned_acl_t canned_acl);
typedef dpl_status_t (*dpl_delete_bucket_t)(dpl_ctx_t *ctx, char *bucket);
typedef dpl_status_t (*dpl_put_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, char *data_buf, unsigned int data_len);
typedef dpl_status_t (*dpl_put_buffered_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, unsigned int data_len, dpl_conn_t **connp);
typedef dpl_status_t (*dpl_get_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_get_range_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_get_buffered_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, dpl_header_func_t header_func, dpl_buffer_func_t buffer_func, void *cb_arg);
typedef dpl_status_t (*dpl_head_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_head_all_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_get_metadata_from_headers_t)(dpl_dict_t *headers, dpl_dict_t *metadata);
typedef dpl_status_t (*dpl_delete_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type);
typedef dpl_status_t (*dpl_genurl_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
typedef dpl_status_t (*dpl_copy_t)(dpl_ctx_t *ctx, char *src_bucket, char *src_resource, char *src_subresource, char *dst_bucket, char *dst_resource, char *dst_subresource, dpl_ftype_t object_type, dpl_metadata_directive_t metadata_directive, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, dpl_condition_t *condition);

typedef struct dpl_backend_s
{
  char *name; /*!< name of the backend */
  dpl_list_all_my_buckets_t list_all_my_buckets;
  dpl_list_bucket_t list_bucket;
  dpl_make_bucket_t make_bucket;
  dpl_delete_bucket_t delete_bucket;
  dpl_put_t put;
  dpl_put_buffered_t put_buffered;
  dpl_get_t get;
  dpl_get_range_t get_range;
  dpl_get_buffered_t get_buffered;
  dpl_head_t head;
  dpl_head_all_t head_all;
  dpl_get_metadata_from_headers_t get_metadata_from_headers; /*!< broken by design since not all backends have metadata in headers */
  dpl_delete_t delete;
  dpl_genurl_t genurl;
  dpl_copy_t copy;
} dpl_backend_t;

/* PROTO backend.c */
dpl_status_t dpl_list_all_my_buckets(dpl_ctx_t *ctx, dpl_vec_t **vecp);
dpl_status_t dpl_list_bucket(dpl_ctx_t *ctx, char *bucket, char *prefix, char *delimiter, dpl_vec_t **objectsp, dpl_vec_t **common_prefixesp);
dpl_status_t dpl_make_bucket(dpl_ctx_t *ctx, char *bucket, dpl_location_constraint_t location_constraint, dpl_canned_acl_t canned_acl);
dpl_status_t dpl_deletebucket(dpl_ctx_t *ctx, char *bucket);
dpl_status_t dpl_put(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, char *data_buf, unsigned int data_len);
dpl_status_t dpl_put_buffered(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, unsigned int data_len, dpl_conn_t **connp);
dpl_status_t dpl_get(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_get_range(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_get_buffered(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, dpl_header_func_t header_func, dpl_buffer_func_t buffer_func, void *cb_arg);
dpl_status_t dpl_head(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
dpl_status_t dpl_head_all(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
dpl_status_t dpl_get_metadata_from_headers(dpl_ctx_t *ctx, dpl_dict_t *headers, dpl_dict_t *metadata);
dpl_status_t dpl_delete(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource);
dpl_status_t dpl_genurl(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
dpl_status_t dpl_copy(dpl_ctx_t *ctx, char *src_bucket, char *src_resource, char *src_subresource, char *dst_bucket, char *dst_resource, char *dst_subresource, dpl_ftype_t object_type, dpl_metadata_directive_t metadata_directive, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, dpl_condition_t *condition);
#endif
