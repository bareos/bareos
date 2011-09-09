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
typedef dpl_status_t (*dpl_put_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, char *data_buf, unsigned int data_len);
typedef dpl_status_t (*dpl_get_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_head_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_head_all_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_get_metadata_from_headers_t)(dpl_dict_t *headers, dpl_dict_t *metadata);
typedef dpl_status_t (*dpl_delete_t)(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource);
/* vdir */
typedef dpl_status_t (*dpl_namei_t)(dpl_ctx_t *ctx, char *path, char *bucket, dpl_ino_t ino, dpl_ino_t *parent_inop, dpl_ino_t *obj_inop, dpl_ftype_t *obj_typep);
typedef dpl_ino_t (*dpl_cwd_t)(dpl_ctx_t *ctx, char *bucket);
typedef dpl_status_t (*dpl_opendir_t)(dpl_ctx_t *ctx, char *locator, void **dir_hdlp);
typedef dpl_status_t (*dpl_readdir_t)(void *dir_hdl, dpl_dirent_t *dirent);
typedef int (*dpl_eof_t)(void *dir_hdl);
typedef void (*dpl_closedir_t)(void *dir_hdl);
typedef dpl_status_t (*dpl_chdir_t)(dpl_ctx_t *ctx, char *locator);
typedef dpl_status_t (*dpl_mkdir_t)(dpl_ctx_t *ctx, char *locator);
typedef dpl_status_t (*dpl_mknod_t)(dpl_ctx_t *ctx, char *locator);
typedef dpl_status_t (*dpl_rmdir_t)(dpl_ctx_t *ctx, char *locator);
/* vfile */
typedef dpl_status_t (*dpl_close_t)(dpl_vfile_t *vfile);
typedef dpl_status_t (*dpl_openwrite_t)(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, unsigned int data_len, dpl_vfile_t **vfilep);
typedef dpl_status_t (*dpl_write_t)(dpl_vfile_t *vfile, char *buf, unsigned int len);
typedef dpl_status_t (*dpl_openread_t)(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_condition_t *condition, dpl_buffer_func_t buffer_func, void *cb_arg, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_openread_range_t)(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_unlink_t)(dpl_ctx_t *ctx, char *locator);
typedef dpl_status_t (*dpl_getattr_t)(dpl_ctx_t *ctx, char *locator, dpl_dict_t **metadatap);
typedef dpl_status_t (*dpl_setattr_t)(dpl_ctx_t *ctx, char *locator, dpl_dict_t *metadata);
typedef dpl_status_t (*dpl_fgenurl_t)(dpl_ctx_t *ctx, char *locator, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
typedef dpl_status_t (*dpl_fcopy_t)(dpl_ctx_t *ctx, char *src_locator, char *dst_locator);

typedef struct dpl_backend_s
{
  char *name; /*!< name of the backend */
  /* general */
  dpl_list_all_my_buckets_t list_all_my_buckets;
  dpl_list_bucket_t list_bucket;
  dpl_make_bucket_t make_bucket;
  dpl_delete_bucket_t delete_bucket;
  dpl_put_t put;
  dpl_get_t get;
  dpl_head_t head;
  dpl_head_all_t head_all;
  dpl_get_metadata_from_headers_t get_metadata_from_headers; /*!< broken by design since not all backends have metadata in headers */
  dpl_delete_t delete;
  /* vdir */
  dpl_namei_t namei;
  dpl_cwd_t cwd;
  dpl_opendir_t opendir;
  dpl_readdir_t readdir;
  dpl_eof_t eof;
  dpl_closedir_t closedir;
  dpl_chdir_t chdir;
  dpl_mkdir_t mkdir;
  dpl_mknod_t mknod;
  dpl_rmdir_t rmdir;
  /* vfile */
  dpl_close_t close;
  dpl_openwrite_t openwrite;
  dpl_write_t write;
  dpl_openread_t openread;
  dpl_openread_range_t openread_range;
  dpl_unlink_t unlink;
  dpl_getattr_t getattr;
  dpl_setattr_t setattr;
  dpl_fgenurl_t fgenurl;
  dpl_fcopy_t fcopy;
} dpl_backend_t;

/* PROTO backend.c */
#endif
