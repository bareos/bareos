/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

/* PROTO droplet_vdir.c */
/* src/droplet_vdir.c */
dpl_status_t dpl_iname(dpl_ctx_t *ctx, char *bucket, dpl_ino_t ino, char *path, u_int path_len);
dpl_status_t dpl_namei(dpl_ctx_t *ctx, char *path, char *bucket, dpl_ino_t ino, dpl_ino_t *parent_inop, dpl_ino_t *obj_inop, dpl_ftype_t *obj_typep);
dpl_ino_t dpl_cwd(dpl_ctx_t *ctx, char *bucket);
dpl_status_t dpl_opendir(dpl_ctx_t *ctx, char *locator, void **dir_hdlp);
dpl_status_t dpl_readdir(void *dir_hdl, dpl_dirent_t *dirent);
int dpl_eof(void *dir_hdl);
void dpl_closedir(void *dir_hdl);
dpl_status_t dpl_chdir(dpl_ctx_t *ctx, char *locator);
dpl_status_t dpl_mkdir(dpl_ctx_t *ctx, char *locator);
dpl_status_t dpl_rmdir(dpl_ctx_t *ctx, char *locator);
#endif
