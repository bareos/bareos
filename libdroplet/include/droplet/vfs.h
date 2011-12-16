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
#ifndef __DROPLET_VFS_H__
#define __DROPLET_VFS_H__ 1

/* PROTO vfs.c */
dpl_status_t dpl_iname(dpl_ctx_t *ctx, const char *bucket, dpl_ino_t ino, char *path, unsigned int path_len);
dpl_status_t dpl_namei(dpl_ctx_t *ctx, const char *path, const char *bucket, dpl_ino_t ino, dpl_ino_t *parent_inop, dpl_ino_t *obj_inop, dpl_ftype_t *obj_typep);
dpl_ino_t dpl_cwd(dpl_ctx_t *ctx, const char *bucket);
dpl_status_t dpl_opendir(dpl_ctx_t *ctx, const char *locator, void **dir_hdlp);
dpl_status_t dpl_readdir(void *dir_hdl, dpl_dirent_t *dirent);
int dpl_eof(void *dir_hdl);
void dpl_closedir(void *dir_hdl);
dpl_status_t dpl_chdir(dpl_ctx_t *ctx, const char *locator);
dpl_status_t dpl_mkdir(dpl_ctx_t *ctx, const char *locator);
dpl_status_t dpl_mknod(dpl_ctx_t *ctx, const char *locator);
dpl_status_t dpl_rmdir(dpl_ctx_t *ctx, const char *locator);
dpl_status_t dpl_close_ex(dpl_vfile_t *vfile, char **resource_idp);
dpl_status_t dpl_close(dpl_vfile_t *vfile);
dpl_status_t dpl_openwrite_ex(dpl_ctx_t *ctx, const char *locator, dpl_ftype_t object_type, dpl_vfile_flag_t flags, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, unsigned int data_len, dpl_dict_t *query_params, dpl_vfile_t **vfilep);
dpl_status_t dpl_openwrite(dpl_ctx_t *ctx, const char *locator, dpl_vfile_flag_t flags, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, unsigned int data_len, dpl_vfile_t **vfilep);
dpl_status_t dpl_write(dpl_vfile_t *vfile, char *buf, unsigned int len);
dpl_status_t dpl_openread(dpl_ctx_t *ctx, const char *locator, dpl_vfile_flag_t flags, dpl_condition_t *condition, dpl_buffer_func_t buffer_func, void *cb_arg, dpl_dict_t **metadatap);
dpl_status_t dpl_openread_range(dpl_ctx_t *ctx, const char *locator, dpl_vfile_flag_t flags, dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_unlink(dpl_ctx_t *ctx, const char *locator);
dpl_status_t dpl_getattr(dpl_ctx_t *ctx, const char *locator, dpl_dict_t **metadatap);
dpl_status_t dpl_getattr_raw(dpl_ctx_t *ctx, const char *locator, dpl_dict_t **metadatap);
dpl_status_t dpl_setattr(dpl_ctx_t *ctx, const char *locator, dpl_dict_t *metadata);
dpl_status_t dpl_fgenurl(dpl_ctx_t *ctx, const char *locator, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
dpl_status_t dpl_fcopy(dpl_ctx_t *ctx, const char *src_locator, const char *dst_locator);
#endif
