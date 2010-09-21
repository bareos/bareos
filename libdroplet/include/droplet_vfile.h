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

#ifndef __DROPLET_VFILE_H__
#define __DROPLET_VFILE_H__ 1

#define DPL_VFILE_FLAG_CREAT   (1u<<0)
#define DPL_VFILE_FLAG_EXCL    (1u<<1)
#define DPL_VFILE_FLAG_ENCRYPT (1u<<2)

typedef struct
{
  dpl_ctx_t *ctx;

  u_int flags;

  dpl_conn_t *conn;

  /*
   * encrypt
   */
  u_char salt[PKCS5_SALT_LEN];
  EVP_CIPHER_CTX *cipher_ctx;
  int header_done;

  /*
   * read
   */
  dpl_buffer_func_t buffer_func;
  void *cb_arg;

} dpl_vfile_t;

#define DPL_ENCRYPT_MAGIC "Salted__"

/* PROTO droplet_vfile.c */
/* src/droplet_vfile.c */
dpl_status_t dpl_close(dpl_vfile_t *vfile);
dpl_status_t dpl_openwrite(dpl_ctx_t *ctx, char *path, u_int flags, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, u_int data_len, dpl_vfile_t **vfilep);
dpl_status_t dpl_write(dpl_vfile_t *vfile, char *buf, u_int len);
dpl_status_t dpl_openread(dpl_ctx_t *ctx, char *path, u_int flags, dpl_condition_t *condition, dpl_buffer_func_t buffer_func, void *cb_arg, dpl_dict_t **metadatap);
dpl_status_t dpl_unlink(dpl_ctx_t *ctx, char *path);
dpl_status_t dpl_getattr(dpl_ctx_t *ctx, char *path, dpl_condition_t *condition, dpl_dict_t **metadatap);
#endif
