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

/*
 * vdir
 */
typedef struct dpl_fqn
{
  char path[DPL_MAXPATHLEN];
} dpl_fqn_t;

#define DPL_ROOT_FQN  ((dpl_fqn_t) {.path = ""})

typedef struct
{
  dpl_fqn_t fqn;
  dpl_ctx_t *ctx;
  dpl_vec_t *files;
  dpl_vec_t *directories;
  int files_cursor;
  int directories_cursor;
} dpl_dir_t;

typedef struct
{
  char name[DPL_MAXNAMLEN];
  dpl_fqn_t fqn;
  dpl_ftype_t type;
  time_t last_modified;
  size_t size;
} dpl_dirent_t;

/*
 * vfile
 */

typedef enum
  {
    DPL_VFILE_FLAG_CREAT =   (1u<<0),     /*!< create file if it doesnt exist */
    DPL_VFILE_FLAG_EXCL =    (1u<<1),     /*!< exclusive creation */
    DPL_VFILE_FLAG_MD5 =     (1u<<2),     /*!< check MD5 */
    DPL_VFILE_FLAG_ENCRYPT = (1u<<3),     /*!< encrypt on the fly */
    DPL_VFILE_FLAG_POST =    (1u<<4),     /*!< use POST to write/creat file */
    DPL_VFILE_FLAG_RANGE =   (1u<<5),     /*!< use specified range (get only) */
    DPL_VFILE_FLAG_BLOB =    (1u<<6),     /*!< get/put file in one-shot */
    DPL_VFILE_FLAG_RDONLY =  (1u<<7),     /*!< open in read-only mode */
    DPL_VFILE_FLAG_WRONLY =  (1u<<8),     /*!< open in write-only mode */
    DPL_VFILE_FLAG_RDWR =    (1u<<9),     /*!< open in read-write mode */
  } dpl_vfile_flag_t;

typedef struct
{
  dpl_ctx_t *ctx;

  unsigned int flags;

  dpl_conn_t *conn;

  /*
   * MD5
   */
  MD5_CTX md5_ctx;

  /*
   * encrypt
   */
  unsigned char salt[PKCS5_SALT_LEN];
  EVP_CIPHER_CTX *cipher_ctx;
  int header_done;

  /*
   * read
   */
  dpl_buffer_func_t buffer_func;
  void *cb_arg;

  /*
   * for one shot
   */
  char *bucket;
  char *resource;
  dpl_fqn_t obj_fqn;
  dpl_condition_t *condition;
  dpl_ftype_t obj_type;
  dpl_dict_t *metadata;
  dpl_sysmd_t *sysmd;
  dpl_dict_t *query_params;

} dpl_vfile_t;

#define DPL_ENCRYPT_MAGIC "Salted__"

#define DPL_CIPHER      "dpl_cipher"
#define DPL_CIPHER_TYPE "dpl_cipher_type"

/* PROTO vfs.c */
dpl_fqn_t dpl_cwd(dpl_ctx_t *ctx, const char *bucket);
dpl_status_t dpl_opendir(dpl_ctx_t *ctx, const char *locator, void **dir_hdlp);
dpl_status_t dpl_readdir(void *dir_hdl, dpl_dirent_t *dirent);
dpl_status_t dpl_iterate(dpl_ctx_t *ctx, const char *locator, int (* cb)(dpl_dirent_t *dirent, void *ctx), void *cb_ctx);
int dpl_eof(void *dir_hdl);
void dpl_closedir(void *dir_hdl);
dpl_status_t dpl_chdir(dpl_ctx_t *ctx, const char *locator);
dpl_status_t dpl_mkdir(dpl_ctx_t *ctx, const char *locator, dpl_dict_t *metadata, dpl_sysmd_t *sysmd);
dpl_status_t dpl_mknod(dpl_ctx_t *ctx, const char *locator, dpl_ftype_t object_type, dpl_dict_t *metadata, dpl_sysmd_t *sysmd);
dpl_status_t dpl_rmdir(dpl_ctx_t *ctx, const char *locator);
dpl_status_t dpl_close_ex(dpl_vfile_t *vfile, dpl_sysmd_t *returned_sysmdp, char **locationp);
dpl_status_t dpl_close(dpl_vfile_t *vfile);

dpl_status_t dpl_open(dpl_ctx_t *ctx,const char *locator, dpl_vfile_flag_t flag, dpl_condition_t *condition, dpl_dict_t *metadata, dpl_sysmd_t *sysmd, dpl_dict_t *query_params, dpl_vfile_t **vfilep);

dpl_status_t dpl_openwrite(dpl_ctx_t *ctx, const char *locator, dpl_ftype_t obj_type, dpl_vfile_flag_t flags, dpl_range_t *range, dpl_dict_t *metadata, dpl_sysmd_t *sysmd, unsigned int data_len, dpl_dict_t *query_params, dpl_vfile_t **vfilep);
dpl_status_t dpl_write(dpl_vfile_t *vfile, char *buf, unsigned int len);
dpl_status_t dpl_pwrite(dpl_vfile_t *vfile, char *buf, unsigned int len, unsigned long long offset, dpl_dict_t *metadata, dpl_sysmd_t *sysmd);

dpl_status_t dpl_openread(dpl_ctx_t *ctx, const char *locator, dpl_vfile_flag_t flags, dpl_condition_t *condition, dpl_range_t *range, dpl_buffer_func_t buffer_func, void *cb_arg, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_read(dpl_vfile_t *vfile, size_t nbytes, char **bufp, int *buf_lenp, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_pread(dpl_vfile_t *vfile, unsigned int len, unsigned long long offset, char **bufp, int *buf_lenp, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp);

dpl_status_t dpl_unlink(dpl_ctx_t *ctx, const char *locator);
dpl_status_t dpl_getattr(dpl_ctx_t *ctx, const char *locator, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_getattr_raw(dpl_ctx_t *ctx, const char *locator, dpl_dict_t **metadatap);
dpl_status_t dpl_setattr(dpl_ctx_t *ctx, const char *locator, dpl_dict_t *metadata, dpl_sysmd_t *sysmd);
dpl_status_t dpl_fgenurl(dpl_ctx_t *ctx, const char *locator, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
dpl_status_t dpl_fcopy(dpl_ctx_t *ctx, const char *src_locator, const char *dst_locator);
dpl_status_t dpl_rename(dpl_ctx_t *ctx, const char *src_locator, const char *dst_locator, dpl_ftype_t object_type);
dpl_status_t dpl_symlink(dpl_ctx_t *ctx, const char *src_locator, const char *dst_locator);
dpl_status_t dpl_link(dpl_ctx_t *ctx, const char *src_locator, const char *dst_locator);
dpl_status_t dpl_mkdent(dpl_ctx_t *ctx, const char *src_id, const char *dst_locator, dpl_ftype_t object_type);
dpl_status_t dpl_rmdent(dpl_ctx_t *ctx, const char *src_name, const char *dst_locator);
dpl_status_t dpl_mvdent(dpl_ctx_t *ctx, const char *src_locator, const char *dst_locator);
#endif
