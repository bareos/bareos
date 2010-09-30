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

#ifndef __DROPLET_UTILS_H__
#define __DROPLET_UTILS_H__ 1

/*
 * general
 */
#if defined(SOLARIS) || defined(__sun__)
# include <sys/filio.h> //FIONBIO
# include <strings.h>
# include <alloca.h>
#else
//linux
# define HAVE_DRAND48_R 1
# define HAVE_GETTID 1
# define HAVE_CANONICALIZE_FILE_NAME
#endif

/*
 * endianness
 */
#if defined(SOLARIS) || defined(__sun__)
# include <sys/isa_defs.h>
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321

#ifdef _LITTLE_ENDIAN
#define __BYTE_ORDER __LITTLE_ENDIAN

#else // default to big endian
#define __BYTE_ORDER __BIG_ENDIAN
#endif

#else
// linux case
# include <endian.h>
#endif

#if defined(SOLARIS) || defined(__sun__)
# include <asm/byteorder.h>
//defines htonll() and ntohll() natively
#else
# include <byteswap.h>

#ifndef __BYTE_ORDER
# error "byte order is not defined"
#endif

# if __BYTE_ORDER == __BIG_ENDIAN
#  define ntohll(x)       (x)
#  define htonll(x)       (x)
#  else
#   if __BYTE_ORDER == __LITTLE_ENDIAN
#    define ntohll(x)     bswap_64 (x)
#    define htonll(x)     bswap_64 (x)
#   endif
# endif
#endif

#ifndef HAVE_DRAND48_R
//emulate drand48_r with rand_r()
struct drand48_data
{
  int seed;
};
int srand48_r(long int seedval, struct drand48_data *buffer);
int lrand48_r(struct drand48_data *buffer, long int *result);
#endif

/**/

#include <sys/types.h>
pid_t gettid();

/**/

#ifndef HAVE_CANONICALIZE_FILE_NAME
char *canonicalize_file_name(const char *path);
#endif

/**/

//one fits all
int linux_gethostbyname_r(const char *name,
                          struct hostent *ret,
                          char *buf, 
                          size_t buflen,
                          struct hostent **result, 
                          int *h_errnop);

/*
 * std types
 */

typedef signed char s8;
typedef unsigned char u8;
typedef signed short s16;
typedef unsigned short u16;
typedef signed int s32;
typedef unsigned int u32;
typedef signed long long s64;
typedef unsigned long long u64;

#if __WORDSIZE == 64

#define PTR_TO_U64(p) ((u64)(p))
#define PTR_TO_U32(p) ((u32)((u64)(p) % UINT_MAX))
#define PTR_TO_INT(p) ((int)((u64)(p) % INT_MAX))
#define U64_TO_PTR(p) ((void*)(p))
#define U32_TO_PTR(p) ((void*)(u64)(p))
#define INT_TO_PTR(p) ((void*)(u64)(p))

#else

#define PTR_TO_U64(p) ((u64)(u32)(p))
#define PTR_TO_U32(p) ((u32)(p))
#define PTR_TO_INT(p) ((int)(p))
#define U64_TO_PTR(p) ((void*)(u32)(p))
#define U32_TO_PTR(p) ((void*)(p))
#define INT_TO_PTR(p) ((void*)(p))

#endif // __WORDSIZE == 64

/**/

#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/**/

#ifdef DROPLET_DEBUG
//XXX not atomic
# define DPLERR(sys, format, ...) do {fprintf(stderr, "dpl error (%s:%d): ",__FILE__,__LINE__);fprintf(stderr, format, ##__VA_ARGS__  ); if (sys) fprintf(stderr, ": %s\n", strerror(errno)); else fprintf(stderr, "\n");} while (0)
#else
# define DPLERR(sys, format, ...)
#endif

/**/

#define DPL_DUMP_LINE_SIZE    16

struct dpl_dump_ctx
{
  FILE *file;
  char prevb[DPL_DUMP_LINE_SIZE];
  int prevb_inited;
  int star_displayed;
  size_t global_off;
};

/**/

typedef int 
(*dpl_conf_cb_func_t)(void *cb_arg, 
                      char *var,
                      char *value);

struct dpl_conf_buf
{
#define DPL_CONF_MAX_BUF 256
  char buf[DPL_CONF_MAX_BUF+1];
  int pos;
};

struct dpl_conf_ctx
{
  int backslash;
  int comment;
  int quote;
  struct dpl_conf_buf var_cbuf;
  struct dpl_conf_buf value_cbuf;
  struct dpl_conf_buf *cur_cbuf;
  dpl_conf_cb_func_t cb_func;
  void *cb_arg;
};

/**/

#define DPL_TRACE(ctx, level, format, ...) do {if (ctx->trace_level & level) dpl_trace(ctx, level, __FILE__, __LINE__, format, ##__VA_ARGS__  );} while (0)

/* PROTO droplet_utils.c */
/* src/droplet_utils.c */
pid_t gettid(void);
int linux_gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
void dpl_dump_init(struct dpl_dump_ctx *ctx);
void dpl_dump_line(struct dpl_dump_ctx *ctx, u_int off, u_char *b, u_int l);
void dpl_dump(struct dpl_dump_ctx *ctx, char *buf, int len);
void dpl_dump_simple(char *buf, int len);
void dpl_trace(dpl_ctx_t *ctx, u32 level, char *file, int lineno, char *fmt, ...);
size_t dpl_iov_size(struct iovec *iov, int n_iov);
void dpl_iov_dump(struct iovec *iov, int n_iov, size_t n_bytes);
time_t dpl_iso8601totime(const char *str);
void dpl_url_decode(char *str);
char *dpl_strrstr(const char *haystack, const char *needle);
#endif
