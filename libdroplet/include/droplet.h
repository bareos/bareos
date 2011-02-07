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
#ifndef __DROPLET_H__
#define __DROPLET_H__ 1

/*
 * dependencies
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/md5.h>

/*
 * default values
 */
#define DPL_DEFAULT_N_CONN_BUCKETS      1021
#define DPL_DEFAULT_N_CONN_MAX          900
#define DPL_DEFAULT_N_CONN_MAX_HITS     50
#define DPL_DEFAULT_CONN_IDLE_TIME      100
#define DPL_DEFAULT_CONN_TIMEOUT        5
#define DPL_DEFAULT_READ_TIMEOUT        30
#define DPL_DEFAULT_WRITE_TIMEOUT       30
#define DPL_DEFAULT_READ_BUF_SIZE       8192

#define DPL_MAXPATHLEN 1024
#define DPL_MAXNAMLEN  255

#define DPL_DEFAULT_DELIM "/"

/*
 * types
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

#define DPL_TRACE_CONN  (1u<<0) /*!< trace connection */
#define DPL_TRACE_IO    (1u<<1) /*!< trace I/O */
#define DPL_TRACE_HTTP  (1u<<2) /*!< trace HTTP */
#define DPL_TRACE_SSL   (1u<<3) /*!< trace SSL */
#define DPL_TRACE_REQ   (1u<<5) /*!< trace request builder */
#define DPL_TRACE_CONV  (1u<<6) /*!< trace convenience functions */
#define DPL_TRACE_VDIR  (1u<<7) /*!< trace vdir */
#define DPL_TRACE_VFILE (1u<<8) /*!< trace vfile */
#define DPL_TRACE_BUF   (1u<<31) /*!< trace buffers */

typedef void (*dpl_trace_func_t)(pid_t tid, unsigned int level, char *file, int lineno, char *buf);

#include <droplet/vec.h>
#include <droplet/dict.h>

typedef enum
  {
    DPL_METHOD_GET,
    DPL_METHOD_PUT,
    DPL_METHOD_DELETE,
    DPL_METHOD_HEAD,
  } dpl_method_t;

enum dpl_data_type
  {
    DPL_DATA_TYPE_IN,
    DPL_DATA_TYPE_OUT,
    DPL_DATA_TYPE_STORAGE,
  };
#define DPL_N_DATA_TYPE (DPL_DATA_TYPE_STORAGE+1)

typedef enum
  {
    DPL_LOCATION_CONSTRAINT_UNDEF,
    DPL_LOCATION_CONSTRAINT_US_STANDARD,
    DPL_LOCATION_CONSTRAINT_EU,
    DPL_LOCATION_CONSTRAINT_US_WEST_1,
    DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1
  } dpl_location_constraint_t;
#define DPL_N_LOCATION_CONSTRAINT (DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1+1)

typedef enum
  {
    DPL_CANNED_ACL_UNDEF,
    DPL_CANNED_ACL_PRIVATE,
    DPL_CANNED_ACL_PUBLIC_READ,
    DPL_CANNED_ACL_PUBLIC_READ_WRITE,
    DPL_CANNED_ACL_AUTHENTICATED_READ,
    DPL_CANNED_ACL_BUCKET_OWNER_READ,
    DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL,
  } dpl_canned_acl_t;
#define DPL_N_CANNED_ACL (DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL+1)

typedef enum
  {
    DPL_STORAGE_CLASS_UNDEF,
    DPL_STORAGE_CLASS_STANDARD,
    DPL_STORAGE_CLASS_REDUCED_REDUNDANCY,
  } dpl_storage_class_t;
#define DPL_N_STORAGE_CLASS (DPL_STORAGE_CLASS_REDUCED_REDUNDANCY+1)

typedef struct
{
#define DPL_CONDITION_IF_MODIFIED_SINCE   (1u<<0)
#define DPL_CONDITION_IF_UNMODIFIED_SINCE (1u<<1)
#define DPL_CONDITION_IF_MATCH            (1u<<2)
#define DPL_CONDITION_IF_NONE_MATCH       (1u<<3)
  unsigned int mask;
  time_t time;
  char etag[MD5_DIGEST_LENGTH];
} dpl_condition_t;

typedef struct dpl_ino
{
  char key[DPL_MAXPATHLEN];
} dpl_ino_t;

typedef struct
{
  char *buf;
  unsigned int len;
} dpl_chunk_t;

typedef struct
{
#define DPL_UNDEF -1
  int start;
  int end;
} dpl_range_t;

typedef enum
  {
    DPL_METADATA_DIRECTIVE_UNDEF,
    DPL_METADATA_DIRECTIVE_COPY,
    DPL_METADATA_DIRECTIVE_REPLACE,
  } dpl_metadata_directive_t;

typedef struct
{
  char *name;
  time_t creation_time;
} dpl_bucket_t;

typedef struct
{
  char *key;
  time_t last_modified;
  size_t size;
  char *etags;
} dpl_object_t;

typedef struct
{
  char *prefix;
} dpl_common_prefix_t;

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
  unsigned int trace_level;
  char *pricing;              /*!< might be NULL */
  unsigned int read_buf_size;
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
   * vdir
   */
  dpl_dict_t *cwds;
  char *cur_bucket;

  /*
   * common
   */
  char *droplet_dir;
  char *profile_name;

  FILE *event_log; /*!< log charged events */

  dpl_trace_func_t trace_func;

  pthread_mutex_t lock;

} dpl_ctx_t;

/**/

typedef struct
{
  dpl_ctx_t *ctx;

#define DPL_BEHAVIOR_MD5          (1u<<0)
#define DPL_BEHAVIOR_EXPECT       (1u<<1)
#define DPL_BEHAVIOR_VIRTUAL_HOSTING (1u<<2)
#define DPL_BEHAVIOR_KEEP_ALIVE   (1u<<3)
#define DPL_BEHAVIOR_QUERY_STRING (1u<<4)
#define DPL_BEHAVIOR_COPY         (1u<<5)
  unsigned int behavior_flags;

  dpl_method_t method;
  char *bucket;
  char *resource;
  char *subresource;

  char *cache_control;
  char *content_disposition;
  char *content_encoding;
  char *content_type;
  dpl_location_constraint_t location_constraint;
  dpl_canned_acl_t canned_acl;
  dpl_condition_t condition;
  dpl_storage_class_t storage_class;

  dpl_dict_t *metadata;
  dpl_chunk_t *chunk;

#define DPL_RANGE_MAX 10
  dpl_range_t ranges[DPL_RANGE_MAX];
  int n_ranges;

  time_t expires; /*!< for query strings */

  /*
   * server side copy
   */
  char *src_bucket;
  char *src_resource;
  char *src_subresource;
  dpl_metadata_directive_t metadata_directive;
  dpl_condition_t copy_source_condition;

} dpl_req_t;

/*
 * public functions
 */
#include <droplet/conn.h>
#include <droplet/converters.h>
#include <droplet/reqbuilder.h>
#include <droplet/httpreply.h>
#include <droplet/replyparser.h>
#include <droplet/convapi.h>
#include <droplet/vdir.h>
#include <droplet/vfile.h>

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
