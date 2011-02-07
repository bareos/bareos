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
#ifndef __DROPLET_VDIR_H__
#define __DROPLET_VDIR_H__ 1

#define DPL_ROOT_INO  ((dpl_ino_t) {.key = ""})

typedef enum
  {
    DPL_FTYPE_REG,
    DPL_FTYPE_DIR
  } dpl_ftype_t;

typedef struct
{
  dpl_ino_t ino;
  dpl_ctx_t *ctx;
  dpl_vec_t *files;
  dpl_vec_t *directories;
  int files_cursor;
  int directories_cursor;
} dpl_dir_t;

typedef struct
{
  char name[DPL_MAXNAMLEN];
  dpl_ino_t ino;
  dpl_ftype_t type;
  time_t last_modified;
  size_t size;
} dpl_dirent_t;

/* PROTO vdir.c */
/* src/vdir.c */
dpl_status_t dpl_iname(dpl_ctx_t *ctx, char *bucket, dpl_ino_t ino, char *path, unsigned int path_len);
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
#endif
