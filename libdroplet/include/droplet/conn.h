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
#ifndef __DROPLET_CONN_H__
#define __DROPLET_CONN_H__ 1

struct dpl_hash_info
{
  struct in_addr addr;
  u_short port;
};

typedef enum
  {
    DPL_CONN_TYPE_HTTP,
    DPL_CONN_TYPE_FILE,
  } dpl_conn_type_t;

typedef struct dpl_conn
{
  dpl_conn_type_t type;

  struct dpl_ctx *ctx;

  struct dpl_conn *next;
  struct dpl_conn *prev;

  struct dpl_hash_info hash_info;

  char *host; //string used to resolve ip addr
  char *port; //string used to resolve port

  int fd;
  time_t start_time;
  time_t close_time;
  unsigned int	n_hits;

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

/* PROTO conn.c */
/* src/conn.c */
dpl_conn_t *dpl_conn_open_host(dpl_ctx_t *ctx, const char *host, const char *portstr);
void dpl_blacklist_host(dpl_ctx_t *ctx, const char *host, const char *portstr);
dpl_status_t dpl_try_connect(dpl_ctx_t *ctx, dpl_req_t *req, dpl_conn_t **connp);
void dpl_conn_release(dpl_conn_t *conn);
void dpl_conn_terminate(dpl_conn_t *conn);
dpl_status_t dpl_conn_pool_init(dpl_ctx_t *ctx);
void dpl_conn_pool_destroy(dpl_ctx_t *ctx);
dpl_status_t dpl_conn_writev_all(dpl_conn_t *conn, struct iovec *iov, int n_iov, int timeout);
dpl_conn_t *dpl_conn_open_file(dpl_ctx_t *ctx, int fd);
#endif
