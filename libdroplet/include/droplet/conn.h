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

#ifndef __DROPLET_CONN_H__
#define __DROPLET_CONN_H__ 1

struct dpl_hash_info
{
  struct in_addr addr;
  u_short port;
};

typedef struct dpl_conn
{
  struct dpl_ctx *ctx;

  struct dpl_conn *next;
  struct dpl_conn *prev;

  struct dpl_hash_info hash_info;

  int fd;
  time_t start_time;
  time_t close_time;
  u_int	n_hits;

  /*
   * buffer
   */
  size_t read_buf_size;
  char *read_buf;
  int read_buf_pos;

  /*
   * read line state
   */
  int block_size;
  int max_blocks;
  ssize_t cc;
  dpl_status_t status;
  int eof;           /*!< set to 1 at EOF            */

  /*
   * ssl
   */
  SSL *ssl;
  BIO *bio;
} dpl_conn_t;

/* PROTO droplet_conn.c */
/* src/droplet_conn.c */
dpl_conn_t *dpl_conn_open(dpl_ctx_t *ctx, struct in_addr addr, u_int port);
dpl_conn_t *dpl_conn_open_host(dpl_ctx_t *ctx, char *host, u_int port);
void dpl_conn_release(dpl_conn_t *conn);
void dpl_conn_terminate(dpl_conn_t *conn);
dpl_status_t dpl_conn_pool_init(dpl_ctx_t *ctx);
void dpl_conn_pool_destroy(dpl_ctx_t *ctx);
dpl_status_t dpl_conn_writev_all(dpl_conn_t *conn, struct iovec *iov, int n_iov, int timeout);
#endif
