/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
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

#ifndef __DROPLETS_VFILE_H__
#define __DROPLETS_VFILE_H__ 1

#define DPL_VFILE_MODE_READ    (1u<<0)
#define DPL_VFILE_MODE_WRITE   (1u<<1)
#define DPL_VFILE_MODE_CREAT   (1u<<2)
#define DPL_VFILE_MODE_EXCL    (1u<<3)
#define DPL_VFILE_MODE_ENCRYPT (1u<<4)

typedef struct
{
  u_int flags;

  dpl_conn_t *conn;

  /*
   * encrypt
   */
  EVP_CIPHER_CTX *cipher_ctx;

} dpl_vfile_t;

/* PROTO droplets_vfile.c */
/* src/droplets_vfile.c */
void dpl_vfile_free(dpl_vfile_t *vfile);
dpl_vfile_t *dpl_vfile_new(dpl_conn_t *conn);
dpl_status_t dpl_vfile_put(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, u_int data_len, dpl_vfile_t **vfilep);
dpl_status_t dpl_vfile_write_all(dpl_vfile_t *vfile, char *buf, u_int len);
dpl_status_t dpl_vfile_open(dpl_ctx_t *ctx, char *bucket, dpl_ino_t ino, char *path, u_int mode);
dpl_status_t dpl_vfile_close(void);
dpl_status_t dpl_vfile_read(void);
dpl_status_t dpl_vfile_write(void);
dpl_status_t dpl_vfile_pread(void);
#endif
