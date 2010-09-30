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

#ifndef __DROPLET_H__
#define __DROPLET_H__ 1

/*
 * dependencies
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <openssl/ssl.h>

/*
 * return codes
 */
typedef int dpl_status_t;

#define DPL_SUCCESS               0    /*!< Success */
#define DPL_FAILURE               (-1) /*!< General failure */
#define DPL_ENOENT                (-2) /*!< No such entry */
#define DPL_EINVAL                (-3) /*!< Invalid argument */
#define DPL_ETIMEOUT              (-4) /*!< Operation timeouted */
#define DPL_ENOMEM                (-5) /*!< No memory available */
#define DPL_ESYS                  (-6) /*!< System error */
#define DPL_EIO                   (-7) /*!< I/O error */
#define DPL_ELIMIT                (-8) /*!< Limit has been reached */
#define DPL_ENAMETOOLONG          (-9)  /*!< Name is too long */
#define DPL_ENOTDIR               (-10) /*!< Not a directory */
#define DPL_ENOTEMPTY             (-11) /*!< Directory is not empty */
#define DPL_EISDIR                (-12) /*!< Is a directory */
#define DPL_EEXIST                (-13) /*!< Object already exists */

/**/

typedef void (*dpl_trace_func_t)(pid_t tid, u_int level, char *file, int lineno, char *buf);

#include <droplet_vec.h>
#include <droplet_dict.h>

enum dpl_data_type
  {
    DPL_DATA_TYPE_IN,
    DPL_DATA_TYPE_OUT,
    DPL_DATA_TYPE_STORAGE,
  };
#define DPL_N_DATA_TYPE (DPL_DATA_TYPE_STORAGE+1)

#define DPL_DEFAULT_N_CONN_BUCKETS      1021
#define DPL_DEFAULT_N_CONN_MAX          900
#define DPL_DEFAULT_N_CONN_MAX_HITS     50
#define DPL_DEFAULT_CONN_IDLE_TIME      100
#define DPL_DEFAULT_CONN_TIMEOUT        5
#define DPL_DEFAULT_READ_TIMEOUT        30
#define DPL_DEFAULT_WRITE_TIMEOUT       30
#define DPL_DEFAULT_READ_BUF_SIZE       8192

#define DPL_TRACE_CONN  (1u<<0) /*!< trace connection */
#define DPL_TRACE_IO    (1u<<1) /*!< trace I/O */
#define DPL_TRACE_HTTP  (1u<<2) /*!< trace HTTP */
#define DPL_TRACE_SSL   (1u<<3) /*!< trace SSL */
#define DPL_TRACE_S3    (1u<<4) /*!< trace S3 */
#define DPL_TRACE_VDIR  (1u<<5) /*!< trace vdir calls */
#define DPL_TRACE_VFILE (1u<<6) /*!< trace vfile calls */
#define DPL_TRACE_BUF   (1u<<7) /*!< trace buffers */

#define DPL_MAXPATHLEN 1024
#define DPL_MAXNAMLEN  255

#define DPL_DEFAULT_DELIM "/"

typedef struct dpl_ino
{
  char key[DPL_MAXPATHLEN];
} dpl_ino_t;

typedef struct dpl_ctx
{
  /*
   * profile
   */
  int n_conn_buckets;         /*!< number of buckets         */
  int n_conn_max;             /*!< max connexions            */
  int n_conn_max_hits;        /*!< before auto-close         */
  int conn_idle_time;         /*!< auto-close after (sec)    */
  int conn_timeout;           /*!< connection timeout (sec)  */
  int read_timeout;           /*!< read timeout (sec)        */
  int write_timeout;          /*!< write timeout (sec)       */
  int use_https;
  char *host;
  int port;
  char *access_key;
  char *secret_key;
  char *ssl_cert_file;
  char *ssl_key_file;
  char *ssl_password;
  char *ssl_ca_list;
  u_int trace_level;
  char *pricing;              /*!< might be NULL */
  u_int read_buf_size;
  char *encrypt_key;
  char *delim;                /*!< vdir delimiter */
 
  /*
   * pricing
   */
  dpl_vec_t *request_pricing;
  dpl_vec_t *data_pricing[DPL_N_DATA_TYPE];

  /*
   * ssl
   */
  BIO *ssl_bio_err;
  SSL_CTX *ssl_ctx;
  BIO *ssl_bio;

  /*
   * conn pool
   */
  struct dpl_conn **conn_buckets;  /*!< idle connections buckets  */
  int n_conn_fds;                  /*!< number of active fds      */

  /*
   * vfile
   */
  char *cur_bucket;
  dpl_ino_t cur_ino;

  /*
   * common
   */
  char *droplet_dir;
  char *profile_name;

  FILE *event_log; /*!< log charged events */

  dpl_trace_func_t trace_func;

  pthread_mutex_t lock;

} dpl_ctx_t;

#include <droplet_conn.h>
#include <droplet_httpreq.h>
#include <droplet_httpreply.h>
#include <droplet_requests.h>
#include <droplet_vdir.h>
#include <droplet_vfile.h>

/* PROTO droplet.c */
/* src/droplet.c */
const char *dpl_status_str(dpl_status_t status);
dpl_status_t dpl_init(void);
void dpl_free(void);
dpl_ctx_t *dpl_ctx_new(char *droplet_dir, char *profile_name);
void dpl_ctx_free(dpl_ctx_t *ctx);
double dpl_price_storage(dpl_ctx_t *ctx, size_t size);
char *dpl_price_storage_str(dpl_ctx_t *ctx, size_t size);
char *dpl_size_str(uint64_t size);
#endif
