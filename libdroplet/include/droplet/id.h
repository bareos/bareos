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
#ifndef __DROPLET_ID_H__
#define __DROPLET_ID_H__ 1

/* PROTO id.c */
/* src/id.c */
dpl_status_t dpl_post_id(dpl_ctx_t *ctx, const char *bucket, const char *subresource, dpl_ftype_t object_type, dpl_dict_t *metadata, dpl_sysmd_t *sysmd, char *data_buf, unsigned int data_len, dpl_dict_t *query_params, char **resource_idp);
dpl_status_t dpl_post_buffered_id(dpl_ctx_t *ctx, const char *bucket, const char *subresource, dpl_ftype_t object_type, dpl_dict_t *metadata, dpl_sysmd_t *sysmd, unsigned int data_len, dpl_dict_t *query_params, dpl_conn_t **connp);
dpl_status_t dpl_put_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource, dpl_ftype_t object_type, dpl_dict_t *metadata, dpl_sysmd_t *sysmd, char *data_buf, unsigned int data_len);
dpl_status_t dpl_put_buffered_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource, dpl_ftype_t object_type, dpl_dict_t *metadata, dpl_sysmd_t *sysmd, unsigned int data_len, dpl_conn_t **connp);
dpl_status_t dpl_get_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_get_range_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_get_buffered_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource, dpl_ftype_t object_type, dpl_condition_t *condition, dpl_header_func_t header_func, dpl_buffer_func_t buffer_func, void *cb_arg);
dpl_status_t dpl_head_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
dpl_status_t dpl_head_all_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
dpl_status_t dpl_head_sysmd_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource, dpl_condition_t *condition, dpl_sysmd_t *sysmdp, dpl_dict_t **metadatap);
dpl_status_t dpl_delete_id(dpl_ctx_t *ctx, const char *bucket, const char *id, const char *subresource);
dpl_status_t dpl_copy_id(dpl_ctx_t *ctx, const char *src_bucket, const char *src_id, const char *src_subresource, const char *dst_bucket, const char *dst_id, const char *dst_subresource, dpl_ftype_t object_type, dpl_metadata_directive_t metadata_directive, dpl_dict_t *metadata, dpl_sysmd_t *sysmd, dpl_condition_t *condition);
dpl_status_t dpl_gen_id_from_oid(dpl_ctx_t *ctx, uint64_t oid, dpl_storage_class_t storage_class, char **resource_idp);
#endif
