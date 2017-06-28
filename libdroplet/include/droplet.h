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

#ifdef __cplusplus
extern "C" {
#endif

#define DPL_VERSION_MAJOR 3
#define DPL_VERSION_MINOR 0

/*
 * dependencies
 */
#include <sys/types.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/md5.h>
#include <pthread.h>

/* NOTE: For fix build issue on Ubuntu 10.04 and Centos 5 */
#ifndef SSL_OP_NO_COMPRESSION
#define SSL_OP_NO_COMPRESSION 0
#endif

/*
 * default values
 */
#define DPL_DEFAULT_HEADER_SIZE         8192
#define DPL_DEFAULT_N_CONN_BUCKETS      1021
#define DPL_DEFAULT_N_CONN_MAX          900
#define DPL_DEFAULT_N_CONN_MAX_HITS     50
#define DPL_DEFAULT_CONN_IDLE_TIME      100
#define DPL_DEFAULT_CONN_TIMEOUT        5
#define DPL_DEFAULT_READ_TIMEOUT        30
#define DPL_DEFAULT_WRITE_TIMEOUT       30
#define DPL_DEFAULT_READ_BUF_SIZE       8192
#define DPL_DEFAULT_MAX_REDIRECTS       10
#define DPL_DEFAULT_AWS_AUTH_SIGN_VERSION        4
#define DPL_DEFAULT_AWS_REGION          "us-east-1"
#define DPL_DEFAULT_SSL_METHOD          SSLv23_method()
#define DPL_DEFAULT_SSL_CIPHER_LIST     "ALL:-aNULL:!LOW:!MEDIUM:!RC2:!3DES:!MD5:!DSS:!SEED:!RC4:@STRENGTH"
#define DPL_DEFAULT_SSL_COMP_NONE       0
#define DPL_DEFAULT_SSL_CERT_VERIF      1

extern int dpl_header_size;

#define DPL_MAXPATHLEN 4096
#define DPL_MAXNAMLEN  255

#define DPL_DEFAULT_BASE_PATH "/"

/*
 * types
 */
typedef enum
  {
    DPL_SUCCESS              =  0,   /*!< Success */
    DPL_FAILURE              = (-1), /*!< General failure */
    DPL_ENOENT               = (-2), /*!< No such entry */
    DPL_EINVAL               = (-3), /*!< Invalid argument */
    DPL_ETIMEOUT             = (-4), /*!< Operation timeouted */
    DPL_ENOMEM               = (-5), /*!< No memory available */
    DPL_ESYS                 = (-6), /*!< System error */
    DPL_EIO                  = (-7), /*!< I/O error */
    DPL_ELIMIT               = (-8), /*!< Limit has been reached */
    DPL_ENAMETOOLONG         = (-9), /*!< Name is too long */
    DPL_ENOTDIR              = (-10),/*!< Not a directory */
    DPL_ENOTEMPTY            = (-11),/*!< Directory is not empty */
    DPL_EISDIR               = (-12),/*!< Is a directory */
    DPL_EEXIST               = (-13),/*!< Object already exists */
    DPL_ENOTSUPP             = (-14),/*!< Method not supported */
    DPL_EREDIRECT            = (-15),/*!< Redirection */
    DPL_ETOOMANYREDIR        = (-16),/*!< Too many redirects */
    DPL_ECONNECT             = (-17),/*!< Connect error */
    DPL_EPERM                = (-18),/*!< Permission denied */
    DPL_EPRECOND             = (-19),/*!< Precondition failed */
    DPL_ECONFLICT            = (-20),/*!< Conflict */
    DPL_ERANGEUNAVAIL        = (-21),/*!< Range Unavailable */
  } dpl_status_t;

#include <droplet/queue.h>
#include <droplet/addrlist.h>

typedef enum
  {
    DPL_TRACE_ERR       = (1u <<  0), /*!< trace errors */
    DPL_TRACE_WARN      = (1u <<  1), /*!< trace warnings */
    DPL_TRACE_CONN      = (1u <<  2), /*!< trace connections */
    DPL_TRACE_IO        = (1u <<  3), /*!< trace I/O */
    DPL_TRACE_HTTP      = (1u <<  4), /*!< trace HTTP */
    DPL_TRACE_SSL       = (1u <<  5), /*!< trace SSL */
    DPL_TRACE_REQ       = (1u <<  6), /*!< trace request builder */
    DPL_TRACE_REST      = (1u <<  7), /*!< trace REST based calls */
    DPL_TRACE_ID        = (1u <<  8), /*!< trace ID based calls */
    DPL_TRACE_VFS       = (1u <<  9), /*!< trace VFS based calls */
    DPL_TRACE_BACKEND   = (1u << 10), /*!< trace backend calls */
    DPL_TRACE_ID_SCHEME = (1u << 11), /*!< trace ID scheme calls */
  } dpl_trace_t;

typedef void (*dpl_trace_func_t)(pid_t tid, dpl_trace_t, const char *file, const char *func, int lineno, char *buf);

typedef enum
  {
    DPL_CAP_BUCKETS         = (1ULL <<  0), /*!< ability to manipulate buckets */
    DPL_CAP_FNAMES          = (1ULL <<  1), /*!< ability to manipulate files by name */
    DPL_CAP_IDS             = (1ULL <<  2), /*!< ability to manipulate files by ID */
    DPL_CAP_LAZY            = (1ULL <<  3), /*!< support lazy option */
    DPL_CAP_HTTP_COMPAT     = (1ULL <<  4), /*!< support HTTP compat option */
    DPL_CAP_RAW             = (1ULL <<  5), /*!< support raw operations */
    DPL_CAP_APPEND_METADATA = (1ULL <<  6), /*!< support append_metadata option */
    DPL_CAP_CONSISTENCY     = (1ULL <<  7), /*!< support consistency options */
    DPL_CAP_VERSIONING      = (1ULL <<  8), /*!< support versioning options */
    DPL_CAP_CONDITIONS      = (1ULL <<  9), /*!< support conditions */
    DPL_CAP_PUT_RANGE       = (1ULL << 10), /*!< support PUT range */
    DPL_CAP_BATCH_DELETE    = (1ULL << 11), /*!< support batch DELETE */
  } dpl_capability_t;

#include <droplet/value.h>
#include <droplet/vec.h>
#include <droplet/dict.h>

typedef enum
  {
    DPL_METHOD_GET,
    DPL_METHOD_PUT,
    DPL_METHOD_DELETE,
    DPL_METHOD_HEAD,
    DPL_METHOD_POST,
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
    DPL_LOCATION_CONSTRAINT_EU_WEST_1,
    DPL_LOCATION_CONSTRAINT_EU_CENTRAL_1,
    DPL_LOCATION_CONSTRAINT_US_EAST_1,
    DPL_LOCATION_CONSTRAINT_US_WEST_1,
    DPL_LOCATION_CONSTRAINT_US_WEST_2,
    DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1,
    DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_2,
    DPL_LOCATION_CONSTRAINT_AP_NORTHEAST_1,
    DPL_LOCATION_CONSTRAINT_SA_EAST_1,

    /* Double values here */
    DPL_LOCATION_CONSTRAINT_EU = DPL_LOCATION_CONSTRAINT_EU_WEST_1,
    DPL_LOCATION_CONSTRAINT_US_STANDARD = DPL_LOCATION_CONSTRAINT_US_EAST_1,

  } dpl_location_constraint_t;
#define DPL_N_LOCATION_CONSTRAINT (DPL_LOCATION_CONSTRAINT_SA_EAST_1+1)

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

/*
 * ACL mgmt (follow RFC3530)
 */

typedef enum
  {
    DPL_ACE_TYPE_ALLOW = 0x00000001,
    DPL_ACE_TYPE_DENY  = 0x00000002,
    DPL_ACE_TPE_AUDIT  = 0x00000003,
    DPL_ACE_TPE_ALARM  = 0x00000004
  } dpl_ace_type_t;

typedef enum
  {
    DPL_ACE_FLAG_OBJECT_INHERIT =       0x00000001,
    DPL_ACE_FLAG_CONTAINER_INHERIT =    0x00000002,
    DPL_ACE_FLAG_NO_PROPAGATE =         0x00000004,
    DPL_ACE_FLAG_INHERIT_ONLY =         0x00000008,
    DPL_ACE_FLAG_SUCCESSFUL_ACCESS =    0x00000010,
    DPL_ACE_FLAG_FAILED_ACCESS =        0x00000020,
    DPL_ACE_FLAG_IDENTIFIER_GROUP =     0x00000040,
    DPL_ACE_FLAG_INHERITED =            0x00000080,
  } dpl_ace_flag_t;

typedef enum
  {
    DPL_ACE_MASK_READ_OBJECT =          0x00000001,
    DPL_ACE_MASK_LIST_CONTAINER =       0x00000001,
    DPL_ACE_MASK_WRITE_OBJECT =         0x00000002,
    DPL_ACE_MASK_ADD_OBJECT =           0x00000002,
    DPL_ACE_MASK_APPEND_DATA =          0x00000004,
    DPL_ACE_MASK_ADD_SUBCONTAINER =     0x00000004,
    DPL_ACE_MASK_READ_METADATA =        0x00000008,
    DPL_ACE_MASK_WRITE_METADATA =       0x00000010,
    DPL_ACE_MASK_EXECUTE =              0x00000020,
    DPL_ACE_MASK_DELETE_OBJECT =        0x00000040,
    DPL_ACE_MASK_DELETE_SUBCONTAINER =  0x00000040,
    DPL_ACE_MASK_READ_ATTRIBUTES =      0x00000080,
    DPL_ACE_MASK_WRITE_ATTRIBUTES =     0x00000100,
    DPL_ACE_MASK_WRITE_RETENTION =      0x00000200,
    DPL_ACE_MASK_WRITE_RETENTION_HOLD = 0x00000400,
    DPL_ACE_MASK_DELETE =               0x00010000,
    DPL_ACE_MASK_READ_ACL =             0x00020000,
    DPL_ACE_MASK_WRITE_ACL =            0x00040000,
    DPL_ACE_MASK_WRITE_OWNER =          0x00080000,
    DPL_ACE_MASK_SYNCHRONIZE =          0x00190000
  } dpl_ace_mask_t;

#define DPL_ACE_MASK_READ               (DPL_ACE_MASK_READ_OBJECT|\
                                         DPL_ACE_MASK_READ_METADATA|    \
                                         DPL_ACE_MASK_EXECUTE|          \
                                         DPL_ACE_MASK_READ_ATTRIBUTES)
#define DPL_ACE_MASK_READ_ALL           (DPL_ACE_MASK_READ|             \
                                         DPL_ACE_MASK_READ_ACL)
#define DPL_ACE_MASK_WRITE              (DPL_ACE_MASK_WRITE_OBJECT|     \
                                         DPL_ACE_MASK_ADD_SUBCONTAINER| \
                                         DPL_ACE_MASK_WRITE_METADATA)
#define DPL_ACE_MASK_WRITE_ALL          (DPL_ACE_MASK_WRITE|            \
                                         DPL_ACE_MASK_DELETE_OBJECT|    \
                                         DPL_ACE_MASK_DELETE|           \
                                         DPL_ACE_MASK_WRITE_ATTRIBUTES| \
                                         DPL_ACE_MASK_WRITE_RETENTION|  \
                                         DPL_ACE_MASK_WRITE_RETENTION_HOLD| \
                                         DPL_ACE_MASK_WRITE_ACL|        \
                                         DPL_ACE_MASK_WRITE_OWNER)
#define DPL_ACE_MASK_RW                 (DPL_ACE_MASK_READ|DPL_ACE_MASK_WRITE)
#define DPL_ACE_MASK_RW_ALL             (DPL_ACE_MASK_READ_ALL|DPL_ACE_MASK_WRITE_ALL)
#define DPL_ACE_MASK_ALL                (DPL_ACE_MASK_RW_ALL|DPL_ACE_MASK_SYNCHRONIZE)

typedef enum
  {
    DPL_ACE_WHO_OWNER,
    DPL_ACE_WHO_GROUP,
    DPL_ACE_WHO_EVERYONE,
    DPL_ACE_WHO_INTERACTIVE,
    DPL_ACE_WHO_NETWORK,
    DPL_ACE_WHO_DIALUP,
    DPL_ACE_WHO_BATCH,
    DPL_ACE_WHO_ANONYMOUS,
    DPL_ACE_WHO_AUTHENTICATED,
    DPL_ACE_WHO_SERVICE,
    DPL_ACE_WHO_ADMINISTRATOR,
    DPL_ACE_WHO_ADMINUSERS,
  } dpl_ace_who_t;

typedef struct
{
  dpl_ace_type_t type;
  dpl_ace_flag_t flag;
  dpl_ace_mask_t access_mask;
  dpl_ace_who_t who;
} dpl_ace_t;

typedef enum
  {
    DPL_STORAGE_CLASS_UNDEF,
    DPL_STORAGE_CLASS_STANDARD,
    DPL_STORAGE_CLASS_REDUCED_REDUNDANCY,
    DPL_STORAGE_CLASS_CUSTOM,
    DPL_STORAGE_CLASS_STANDARD_IA
  } dpl_storage_class_t;
#define DPL_N_STORAGE_CLASS (DPL_STORAGE_CLASS_STANDARD_IA+1)

typedef enum
  {
    DPL_FTYPE_UNDEF,  /*!< undefined object type */
    DPL_FTYPE_ANY,    /*!< any object type */
    DPL_FTYPE_REG,    /*!< regular file */
    DPL_FTYPE_DIR,    /*!< directory */
    DPL_FTYPE_CAP,    /*!< capability */
    DPL_FTYPE_DOM,    /*!< domain */
    DPL_FTYPE_CHRDEV, /*!< character device */
    DPL_FTYPE_BLKDEV, /*!< block device */
    DPL_FTYPE_FIFO,   /*!< named pipe */
    DPL_FTYPE_SOCKET, /*!< named socket */
    DPL_FTYPE_SYMLINK, /*!< symbolic link */
  } dpl_ftype_t;

#define DPL_DEFAULT_ENTERPRISE_NUMBER 37489 /*!< scality */

/**/

#ifndef __cplusplus
typedef enum
#else
enum
#endif
  {
    DPL_SYSMD_MASK_CANNED_ACL    = (1u<<0),
    DPL_SYSMD_MASK_STORAGE_CLASS = (1u<<1),
    DPL_SYSMD_MASK_SIZE          = (1u<<2),
    DPL_SYSMD_MASK_ATIME         = (1u<<3),
    DPL_SYSMD_MASK_MTIME         = (1u<<4),
    DPL_SYSMD_MASK_CTIME         = (1u<<5),
    DPL_SYSMD_MASK_ETAG          = (1u<<6),
    DPL_SYSMD_MASK_LOCATION_CONSTRAINT = (1u<<8),
    DPL_SYSMD_MASK_OWNER         = (1u<<9),
    DPL_SYSMD_MASK_GROUP         = (1u<<10),
    DPL_SYSMD_MASK_ACL           = (1u<<11),
    DPL_SYSMD_MASK_ID            = (1u<<12),
    DPL_SYSMD_MASK_PARENT_ID     = (1u<<13),
    DPL_SYSMD_MASK_FTYPE         = (1u<<14),
    DPL_SYSMD_MASK_ENTERPRISE_NUMBER  = (1u<<15),
    DPL_SYSMD_MASK_PATH          = (1u<<16),
    DPL_SYSMD_MASK_VERSION       = (1u<<17),
#ifndef __cplusplus
  } dpl_sysmd_mask_t;
#else
  };
typedef unsigned int dpl_sysmd_mask_t;
#endif

typedef struct
{
  char          *name;
  char          *version_id;
  dpl_ftype_t   type;
} dpl_locator_t;

typedef struct
{
  dpl_locator_t *tab;
  unsigned int  size;
} dpl_locators_t;

#define DPL_ETAG_SIZE 64
typedef struct
{
  dpl_sysmd_mask_t mask;
  dpl_canned_acl_t canned_acl;
  dpl_storage_class_t storage_class;
  uint64_t size;
  time_t atime;
  time_t mtime;
  time_t ctime;
#define DPL_SYSMD_ETAG_SIZE DPL_ETAG_SIZE
  char etag[DPL_SYSMD_ETAG_SIZE+1];
  dpl_location_constraint_t location_constraint;
#define DPL_SYSMD_OWNER_SIZE 32
  char owner[DPL_SYSMD_OWNER_SIZE+1];
#define DPL_SYSMD_GROUP_SIZE 32
  char group[DPL_SYSMD_OWNER_SIZE+1];
#define DPL_SYSMD_ACE_MAX 10
  uint32_t n_aces;
  dpl_ace_t aces[DPL_SYSMD_ACE_MAX];
#define DPL_SYSMD_ID_SIZE 64
  char id[DPL_SYSMD_ID_SIZE+1];
  char parent_id[DPL_SYSMD_ID_SIZE+1];
  dpl_ftype_t ftype;
  uint32_t enterprise_number;
  char path[DPL_MAXPATHLEN+1];
  char version[DPL_SYSMD_ID_SIZE+1];
} dpl_sysmd_t;

/**/

typedef enum
  {
    DPL_CONDITION_IF_MODIFIED_SINCE,
    DPL_CONDITION_IF_UNMODIFIED_SINCE,
    DPL_CONDITION_IF_MATCH,
    DPL_CONDITION_IF_NONE_MATCH,
  } dpl_condition_type_t;

typedef struct
{
  dpl_condition_type_t type;
  union
  {
    time_t time;
    char etag[DPL_ETAG_SIZE+1];
  };
} dpl_condition_one_t;

typedef struct
{
  int n_conds;
#define DPL_COND_MAX 10
  dpl_condition_one_t conds[DPL_COND_MAX];
} dpl_condition_t;

#ifndef __cplusplus
typedef enum
#else
enum
#endif
  {
    DPL_OPTION_LAZY                = (1u<<0), /*!< perform a lazy operation */
    DPL_OPTION_HTTP_COMPAT         = (1u<<1), /*!< use HTTP compat mode */
    DPL_OPTION_RAW                 = (1u<<2), /*!< put/get RAW buffer */
    DPL_OPTION_APPEND_METADATA     = (1u<<3), /*!< append metadata */
    DPL_OPTION_CONSISTENT          = (1u<<4), /*!< perform a consistent operation */
    DPL_OPTION_EXPECT_VERSION      = (1u<<5), /*!< expect version */
    DPL_OPTION_FORCE_VERSION       = (1u<<6), /*!< force version */
    DPL_OPTION_NOALLOC             = (1u<<7), /*!< caller provides buffer for GETs */
#ifndef __cplusplus
  } dpl_option_mask_t;
#else
  };
typedef unsigned int dpl_option_mask_t;
#endif

typedef struct
{
  dpl_option_mask_t mask;
  char expect_version[DPL_SYSMD_ID_SIZE+1];
  char force_version[DPL_SYSMD_ID_SIZE+1];
} dpl_option_t;

typedef struct
{
#define DPL_UNDEF -1LL
  uint64_t start;
  uint64_t end;
} dpl_range_t;

typedef enum
  {
    DPL_COPY_DIRECTIVE_UNDEF,
    DPL_COPY_DIRECTIVE_COPY,
    DPL_COPY_DIRECTIVE_METADATA_REPLACE,
    DPL_COPY_DIRECTIVE_LINK,
    DPL_COPY_DIRECTIVE_SYMLINK,
    DPL_COPY_DIRECTIVE_MOVE,
    DPL_COPY_DIRECTIVE_MKDENT,
    DPL_COPY_DIRECTIVE_RMDENT,
    DPL_COPY_DIRECTIVE_MVDENT,
  } dpl_copy_directive_t;

typedef struct
{
  char *name;
  time_t creation_time;
} dpl_bucket_t;

typedef struct
{
  char *path;
  time_t last_modified;
  size_t size;
  char *etags;
  dpl_ftype_t type;
} dpl_object_t;

typedef struct
{
  char *prefix;
} dpl_common_prefix_t;

typedef struct
{
  char          *name;
  char          *version_id;
  dpl_status_t  status;
  char          *error;
} dpl_delete_object_t;

typedef struct 
{
  uint32_t time_low;
  uint16_t time_mid;
  uint16_t time_hi_and_version;
  uint8_t clock_seq_hi_and_reserved;
  uint8_t clock_seq_low;
  uint8_t node_addr[6];
} dpl_uuid_t;

struct dpl_backend_s;

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
  dpl_addrlist_t *addrlist;   /*!< list of addresses to contact */
  int cur_host;               /*!< current host beeing used in addrlist */
  int blacklist_expiretime;   /*!< expiration time of blacklisting */
  char *base_path;            /*!< or RootURI */
  char *access_key;
  char *secret_key;
  unsigned char aws_auth_sign_version; /*!< S3 Auth signature version */
  char aws_region[32];        /*!< AWS Region */
  /* SSL */
  char *ssl_cert_file;        /*!< SSL certificate of the client*/
  char *ssl_key_file;         /*!< SSL private key of the client*/
  char *ssl_password;         /*!< password for the SSL private key*/
  char *ssl_ca_list;          /*!< SSL certificate authority (CA) list*/
  char *ssl_crl_list;          /*!< SSL certificate revocation list (CRL) list*/
  int cert_verif;             /*!< SSL certificate verification (default to true) */
/*!< SSL method among SSLv3,TLSv1,TLSv1.1,TLSv1.2 and SSLv23 */
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
  const SSL_METHOD *ssl_method;
#else
  SSL_METHOD *ssl_method;
#endif
  char *ssl_cipher_list;
  int ssl_comp;               /*!< SSL compression support (default to false) */
  /* log */
  unsigned int trace_level;
  int trace_buffers;
  int trace_binary;          /*!< default is trace ascii */
  char *pricing;             /*!< might be NULL */
  unsigned int read_buf_size;
  char *encrypt_key;
  int encode_slashes;        /*!< client wants slashes encoded */
  int empty_folder_emulation;/*!< folders are represented as empty objects (otherwise, they're purely virtual) */
  int keep_alive;            /*!< client supports keep-alive */
  int url_encoding;          /*!< some servers does not handle url encoding */
  int max_redirects;         /*!< maximum number of redirects */
  int preserve_root_path;    /*!< Preserve "/" for root path access in HTTP requests */
  uint32_t enterprise_number; /*!< for generating native IDs */
  struct dpl_backend_s *backend;

  /*
   * http
   */
  int header_size;

  /*
   * pricing
   */
  dpl_vec_t *request_pricing;
  dpl_vec_t *data_pricing[DPL_N_DATA_TYPE];

  /*
   * ssl
   */
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

  char *pricing_dir; /*!< where charged events should be logged, empty string to disable logging */
  FILE *event_log; /*!< log charged events */

  dpl_trace_func_t trace_func;

  pthread_mutex_t lock;
  int canary; /*!< for debugging lock */

  /*
  * backend ctx (for session, etc...)
  */
  void *backend_ctx;
} dpl_ctx_t;

/**/

typedef struct
{
    char                *bucket;
    int                 locator_is_id;
    char                *locator;
    dpl_option_t        *options;
    dpl_condition_t     *condition;

    struct json_object  *status;

    dpl_dict_t          *md;
    dpl_sysmd_t         *sysmd;
} dpl_stream_t;

/**/

typedef enum
  {
    DPL_BEHAVIOR_MD5 =         (1u<<0),     /*!< MD5 is computed for object */
    DPL_BEHAVIOR_EXPECT =      (1u<<1),     /*!< Use the Expect: 100-Continue */
    DPL_BEHAVIOR_VIRTUAL_HOSTING = (1u<<2), /*!< Use virtual hosting instead of path-style access */
    DPL_BEHAVIOR_KEEP_ALIVE =  (1u<<3),     /*!< Reuse connections */
  } dpl_behavior_flag_t;

typedef struct
{
  dpl_ctx_t *ctx;

  char *host;
  char *port;
  dpl_behavior_flag_t behavior_flags;
  
  dpl_method_t method;
  char *bucket;
  char *resource;
  char *subresource;

  char *cache_control;
  char *content_disposition;
  char *content_encoding;
  dpl_ftype_t object_type;
  char *content_type;
  dpl_location_constraint_t location_constraint;
  dpl_canned_acl_t canned_acl;
  dpl_condition_t condition;
  dpl_storage_class_t storage_class;

  dpl_dict_t *metadata;

  const char *data_buf;
  u_int data_len;
  int data_enabled;

  dpl_range_t range;
  int range_enabled;

  time_t expires; /*!< for query strings */

  /*
   * server side copy
   */
  char *src_bucket;
  char *src_resource;
  char *src_subresource;
  dpl_copy_directive_t copy_directive;
  dpl_condition_t copy_source_condition;

} dpl_req_t;

/** @addtogroup init
 * @{ */
/** Message severity levels for Droplet log messages. */
typedef enum
  {
    DPL_DEBUG,	    /*!< debug message (lowest) */
    DPL_INFO,	    /*!< informational message */
    DPL_WARNING,    /*!< warning message */
    DPL_ERROR,	    /*!< error message (highest) */
  } dpl_log_level_t;
/** @} */

/*
 * public functions
 */

typedef dpl_status_t (*dpl_metadatum_func_t)(void *cb_arg,
                                             const char *key,
                                             dpl_value_t *val);
/** @addtogroup init
 * @{ */
/** Callback function for log messages, see `dpl_set_log_func()` */
typedef void (*dpl_log_func_t)(dpl_ctx_t *, dpl_log_level_t, const char *message);
/** @} */


#include <droplet/converters.h>
#include <droplet/conn.h>
#include <droplet/httprequest.h>
#include <droplet/httpreply.h>
#include <droplet/req.h>
#include <droplet/rest.h>
#include <droplet/sysmd.h>
#include <droplet/id_scheme.h>

/* PROTO droplet.c */
/* src/droplet.c */
const char *dpl_status_str(dpl_status_t status);
dpl_status_t dpl_init(void);
void dpl_free(void);
void dpl_set_log_func(dpl_log_func_t);
void dpl_ctx_lock(dpl_ctx_t *ctx);
void dpl_ctx_unlock(dpl_ctx_t *ctx);
dpl_ctx_t *dpl_ctx_new(const char *droplet_dir, const char *profile_name);
dpl_ctx_t *dpl_ctx_new_from_dict(const dpl_dict_t *profile);
void dpl_ctx_free(dpl_ctx_t *ctx);
double dpl_price_storage(dpl_ctx_t *ctx, size_t size);
char *dpl_price_storage_str(dpl_ctx_t *ctx, size_t size);
char *dpl_size_str(uint64_t size);
int dpl_backend_set(dpl_ctx_t *ctx, const char *name);
dpl_status_t dpl_print_capabilities(dpl_ctx_t *ctx);
void dpl_bucket_free(dpl_bucket_t *bucket);
void dpl_vec_buckets_free(dpl_vec_t *vec);
void dpl_object_free(dpl_object_t *object);
void dpl_vec_objects_free(dpl_vec_t *vec);
void dpl_common_prefix_free(dpl_common_prefix_t *common_prefix);
void dpl_vec_common_prefixes_free(dpl_vec_t *vec);
void dpl_delete_object_free(dpl_delete_object_t *object);
void dpl_vec_delete_objects_free(dpl_vec_t *vec);
dpl_option_t *dpl_option_dup(const dpl_option_t *src);
void dpl_option_free(dpl_option_t *option);
dpl_condition_t *dpl_condition_dup(const dpl_condition_t *src);
void dpl_condition_free(dpl_condition_t *condition);
dpl_range_t *dpl_range_dup(const dpl_range_t *src);
void dpl_range_free(dpl_range_t *range);
char *dpl_ftype_to_str(dpl_ftype_t type);
char *dpl_copy_directive_to_str(dpl_copy_directive_t directive);

#ifdef __cplusplus
}
#endif

#endif
