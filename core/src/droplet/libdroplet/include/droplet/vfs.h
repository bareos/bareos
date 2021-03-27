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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_VFS_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_VFS_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * vdir
 */
typedef struct dpl_fqn {
  char path[DPL_MAXPATHLEN];
} dpl_fqn_t;

#define DPL_ROOT_FQN ((dpl_fqn_t){.path = ""})

typedef struct {
  dpl_fqn_t fqn;
  dpl_ctx_t* ctx;
  dpl_vec_t* files;
  dpl_vec_t* directories;
  int files_cursor;
  int directories_cursor;
} dpl_dir_t;

typedef struct {
  char name[DPL_MAXNAMLEN];
  dpl_fqn_t fqn;
  dpl_ftype_t type;
  time_t last_modified;
  size_t size;
} dpl_dirent_t;

/*
 * vfile
 */
#ifndef __cplusplus
typedef enum
#else
enum
#endif
{
  DPL_VFILE_FLAG_CREAT = (1u << 0),  /*!< create file if it doesnt exist */
  DPL_VFILE_FLAG_EXCL = (1u << 1),   /*!< exclusive creation */
  DPL_VFILE_FLAG_RDONLY = (1u << 2), /*!< open in read-only mode */
  DPL_VFILE_FLAG_WRONLY = (1u << 3), /*!< open in write-only mode */
  DPL_VFILE_FLAG_RDWR = (1u << 4),   /*!< open in read-write mode */
  DPL_VFILE_FLAG_STREAM
  = (1u << 5), /*!< open in stream mode (required for read/append) */
#ifndef __cplusplus
} dpl_vfile_flag_t;
#else
};

typedef unsigned int dpl_vfile_flag_t;
#endif

typedef struct {
  dpl_ctx_t* ctx;
  dpl_vfile_flag_t flags;
  char* bucket;
  dpl_fqn_t obj_fqn;
  dpl_option_t* option;
  dpl_condition_t* condition;
  dpl_dict_t* metadata;
  dpl_sysmd_t* sysmd;
  dpl_dict_t* query_params;
  dpl_stream_t* stream;
} dpl_vfile_t;

/* PROTO vfs.c */
dpl_fqn_t dpl_cwd(dpl_ctx_t* ctx, const char* bucket);
dpl_status_t dpl_opendir(dpl_ctx_t* ctx, const char* locator, void** dir_hdlp);
dpl_status_t dpl_opendir_attrs(dpl_ctx_t* ctx,
                               const char* locator,
                               dpl_dict_t** metadatap,
                               dpl_sysmd_t* sysmdp,
                               void** dir_hdlp);
dpl_status_t dpl_readdir(void* dir_hdl, dpl_dirent_t* dirent);
dpl_status_t dpl_iterate(dpl_ctx_t* ctx,
                         const char* locator,
                         int (*cb)(dpl_dirent_t* dirent, void* ctx),
                         void* cb_ctx);
int dpl_eof(void* dir_hdl);
void dpl_closedir(void* dir_hdl);
dpl_status_t dpl_chdir(dpl_ctx_t* ctx, const char* locator);
dpl_status_t dpl_close(dpl_vfile_t* vfile);
dpl_status_t dpl_pwrite(dpl_vfile_t* vfile,
                        char* buf,
                        unsigned int len,
                        unsigned long long offset);
dpl_status_t dpl_pread(dpl_vfile_t* vfile,
                       unsigned int len,
                       unsigned long long offset,
                       char** bufp,
                       unsigned int* buf_lenp);
dpl_status_t dpl_fstream_putmd(dpl_vfile_t* vfile,
                               dpl_dict_t* metadata,
                               dpl_sysmd_t* sysmd);
dpl_status_t dpl_fstream_put(dpl_vfile_t* vfile,
                             char* buf,
                             unsigned int len,
                             struct json_object** statusp);
dpl_status_t dpl_fstream_getmd(dpl_vfile_t* vfile,
                               dpl_dict_t** metadatap,
                               dpl_sysmd_t** sysmdp);
dpl_status_t dpl_fstream_get(dpl_vfile_t* vfile,
                             unsigned int len,
                             char** bufp,
                             unsigned int* buf_lenp,
                             struct json_object** statusp);
dpl_status_t dpl_fstream_resume(dpl_vfile_t* vfile, struct json_object* status);
dpl_status_t dpl_fstream_flush(dpl_vfile_t* vfile);
dpl_status_t dpl_open(dpl_ctx_t* ctx,
                      const char* locator,
                      dpl_vfile_flag_t flag,
                      dpl_option_t* option,
                      dpl_condition_t* condition,
                      dpl_dict_t* metadata,
                      dpl_sysmd_t* sysmd,
                      dpl_dict_t* query_params,
                      struct json_object* stream_status,
                      dpl_vfile_t** vfilep);
dpl_status_t dpl_fput(dpl_ctx_t* ctx,
                      const char* locator,
                      dpl_option_t* option,
                      dpl_condition_t* condition,
                      dpl_range_t* range,
                      dpl_dict_t* metadata,
                      dpl_sysmd_t* sysmd,
                      char* data_buf,
                      unsigned int data_len);
dpl_status_t dpl_fget(dpl_ctx_t* ctx,
                      const char* locator,
                      const dpl_option_t* option,
                      const dpl_condition_t* condition,
                      const dpl_range_t* range,
                      char** data_bufp,
                      unsigned int* data_lenp,
                      dpl_dict_t** metadatap,
                      dpl_sysmd_t* sysmdp);
dpl_status_t dpl_mkdir(dpl_ctx_t* ctx,
                       const char* locator,
                       dpl_dict_t* metadata,
                       dpl_sysmd_t* sysmd);
dpl_status_t dpl_mknod(dpl_ctx_t* ctx,
                       const char* locator,
                       dpl_ftype_t object_type,
                       dpl_dict_t* metadata,
                       dpl_sysmd_t* sysmd);
dpl_status_t dpl_rmdir(dpl_ctx_t* ctx, const char* locator);
dpl_status_t dpl_unlink(dpl_ctx_t* ctx, const char* locator);
dpl_status_t dpl_unlink_all(dpl_ctx_t* ctx,
                            const char* bucket,
                            dpl_locators_t* locators,
                            dpl_vec_t** objectsp);
dpl_status_t dpl_getattr(dpl_ctx_t* ctx,
                         const char* locator,
                         dpl_dict_t** metadatap,
                         dpl_sysmd_t* sysmdp);
dpl_status_t dpl_getattr_raw(dpl_ctx_t* ctx,
                             const char* locator,
                             dpl_dict_t** metadatap);
dpl_status_t dpl_setattr(dpl_ctx_t* ctx,
                         const char* locator,
                         dpl_dict_t* metadata,
                         dpl_sysmd_t* sysmd);
dpl_status_t dpl_fgenurl(dpl_ctx_t* ctx,
                         const char* locator,
                         time_t expires,
                         char* buf,
                         unsigned int len,
                         unsigned int* lenp);
dpl_status_t dpl_fcopy(dpl_ctx_t* ctx,
                       const char* src_locator,
                       const char* dst_locator);
dpl_status_t dpl_rename(dpl_ctx_t* ctx,
                        const char* src_locator,
                        const char* dst_locator,
                        dpl_ftype_t object_type);
dpl_status_t dpl_readlink(dpl_ctx_t* ctx,
                          const char* src_locator,
                          char** target_locatorp);
dpl_status_t dpl_symlink(dpl_ctx_t* ctx,
                         const char* locator,
                         const char* dst_locator);
dpl_status_t dpl_link(dpl_ctx_t* ctx,
                      const char* src_locator,
                      const char* dst_locator);
dpl_status_t dpl_mkdent(dpl_ctx_t* ctx,
                        const char* src_id,
                        const char* dst_locator,
                        dpl_ftype_t object_type);
dpl_status_t dpl_rmdent(dpl_ctx_t* ctx,
                        const char* src_name,
                        const char* dst_locator);
dpl_status_t dpl_mvdent(dpl_ctx_t* ctx,
                        const char* src_locator,
                        const char* dst_locator);

#ifdef __cplusplus
}
#endif

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_VFS_H_
