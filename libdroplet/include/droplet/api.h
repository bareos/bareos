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
#ifndef __DROPLET_API_H__
#define __DROPLET_API_H__ 1

/* general */
dpl_status_t dpl_list_all_my_buckets(dpl_ctx_t *ctx, dpl_vec_t **vecp);
dpl_status_t dpl_list_bucket(dpl_ctx_t *ctx, char *bucket, char *prefix, char *delimiter, dpl_vec_t **objectsp, dpl_vec_t **common_prefixesp);
dpl_status_t dpl_make_bucket(dpl_ctx_t *ctx, char *bucket, dpl_location_constraint_t location_constraint, dpl_canned_acl_t canned_acl);
dpl_status_t dpl_deletebucket(dpl_ctx_t *ctx, char *bucket);
dpl_status_t dpl_put(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, char *data_buf, unsigned int data_len);
dpl_status_t dpl_get(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_head(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
dpl_status_t dpl_head_all(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
dpl_status_t dpl_get_metadata_from_headers(dpl_ctx_t *ctx, dpl_dict_t *headers, dpl_dict_t *metadata);
dpl_status_t dpl_delete(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource);
/* vdir */
dpl_status_t dpl_namei(dpl_ctx_t *ctx, char *path, char *bucket, dpl_ino_t ino, dpl_ino_t *parent_inop, dpl_ino_t *obj_inop, dpl_ftype_t *obj_typep);
dpl_ino_t dpl_cwd(dpl_ctx_t *ctx, char *bucket);
dpl_status_t dpl_opendir(dpl_ctx_t *ctx, char *locator, void **dir_hdlp);
dpl_status_t dpl_readdir(void *dir_hdl, dpl_dirent_t *dirent);
int dpl_eof(void *dir_hdl);
void dpl_closedir(void *dir_hdl);
dpl_status_t dpl_chdir(dpl_ctx_t *ctx, char *locator);
dpl_status_t dpl_mkdir(dpl_ctx_t *ctx, char *locator);
dpl_status_t dpl_mknod(dpl_ctx_t *ctx, char *locator);
dpl_status_t dpl_rmdir(dpl_ctx_t *ctx, char *locator);
/* vfile */
dpl_status_t dpl_close(dpl_vfile_t *vfile);
dpl_status_t dpl_openwrite(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, unsigned int data_len, dpl_vfile_t **vfilep);
dpl_status_t dpl_write(dpl_vfile_t *vfile, char *buf, unsigned int len);
dpl_status_t dpl_openread(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_condition_t *condition, dpl_buffer_func_t buffer_func, void *cb_arg, dpl_dict_t **metadatap);
dpl_status_t dpl_openread_range(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_unlink(dpl_ctx_t *ctx, char *locator);
dpl_status_t dpl_getattr(dpl_ctx_t *ctx, char *locator, dpl_dict_t **metadatap);
dpl_status_t dpl_setattr(dpl_ctx_t *ctx, char *locator, dpl_dict_t *metadata);
dpl_status_t dpl_fgenurl(dpl_ctx_t *ctx, char *locator, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
dpl_status_t dpl_fcopy(dpl_ctx_t *ctx, char *src_locator, char *dst_locator);

#endif
