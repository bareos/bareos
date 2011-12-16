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
#ifndef __DROPLET_S3_BACKEND_H__
#define __DROPLET_S3_BACKEND_H__ 1

/* PROTO backend.c */
/* src/backend.c */
dpl_status_t dpl_s3_list_all_my_buckets(dpl_ctx_t *ctx, dpl_vec_t **vecp);
dpl_status_t dpl_s3_list_bucket(dpl_ctx_t *ctx, const char *bucket, const char *prefix, const char *delimiter, dpl_vec_t **objectsp, dpl_vec_t **common_prefixesp);
dpl_status_t dpl_s3_make_bucket(dpl_ctx_t *ctx, const char *bucket, const dpl_sysmd_t *sysmd);
dpl_status_t dpl_s3_deletebucket(dpl_ctx_t *ctx, const char *bucket);
dpl_status_t dpl_s3_put(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const char *data_buf, unsigned int data_len);
dpl_status_t dpl_s3_put_buffered(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, unsigned int data_len, dpl_conn_t **connp);
dpl_status_t dpl_s3_get(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_s3_get_range(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_s3_get_buffered(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_header_func_t header_func, dpl_buffer_func_t buffer_func, void *cb_arg);
dpl_status_t dpl_s3_head(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_dict_t **metadatap);
dpl_status_t dpl_s3_head_all(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type, const dpl_condition_t *condition, dpl_dict_t **metadatap);
dpl_status_t dpl_s3_delete(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, dpl_ftype_t object_type);
dpl_status_t dpl_s3_genurl(dpl_ctx_t *ctx, const char *bucket, const char *resource, const char *subresource, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
dpl_status_t dpl_s3_copy(dpl_ctx_t *ctx, const char *src_bucket, const char *src_resource, const char *src_subresource, const char *dst_bucket, const char *dst_resource, const char *dst_subresource, dpl_ftype_t object_type, dpl_metadata_directive_t metadata_directive, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const dpl_condition_t *condition);
#endif
