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
#ifndef __DROPLET_UTILS_H__
#define __DROPLET_UTILS_H__ 1

#include <stdarg.h>
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

/* Mac OSX has __BIG_ENDIAN__ or __LITTLE_ENDIAN__ automatically set by the compiler (at least with GCC) */
#elif defined(__APPLE__) && defined(__MACH__) || defined(__ellcc__ )

    #ifdef _LITTLE_ENDIAN__
    #define __BYTE_ORDER __LITTLE_ENDIAN__
    #else // default to big endian
    #define __BYTE_ORDER __BIG_ENDIAN__
    #endif

#else
// linux case
# include <endian.h>
#endif

#if defined(SOLARIS) || defined(__sun__)
# include <asm/byteorder.h>

#elif defined(__APPLE__) && defined(__MACH__) || defined(__ellcc__ )
    #include <libkern/OSByteOrder.h>
    #define le32toh OSSwapLittleToHostInt32
    #define htole32 OSSwapHostToLittleInt32
    #define bswap_32 OSSwapInt32
#else
//defines htonll() and ntohll() natively
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
pid_t dpl_gettid();

/**/

#ifndef HAVE_CANONICALIZE_FILE_NAME
char *canonicalize_file_name(const char *path);
#endif

/**/

//one fits all
int dpl_gethostbyname_r(const char *name,
                          struct hostent *ret,
                          char *buf,
                          size_t buflen,
                          struct hostent **result,
                          int *h_errnop);

/**/

#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
# define MAX(a,b) ((a) > (b) ? (a) : (b))
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
  int binary;
};

/**/

typedef int
(*dpl_conf_cb_func_t)(void *cb_arg,
                      const char *var,
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

int dpl_base64_init(void);
#define DPL_BASE64_LENGTH(len) (((len) + 2) / 3 * 4)
#define DPL_BASE64_ORIG_LENGTH(len) (((len) + 3) / 4 * 3)

#define DPL_URL_LENGTH(len) ((len)*3+1)
#define DPL_BCD_LENGTH(len) (2*(len))

/**/

#define DPL_APPEND_CHAR(Char)                                   \
  do {                                                          \
    if (len < 1)                                                \
      return DPL_FAILURE;                                       \
    *p = (Char);p++;len--;                                      \
  } while (0);

#define DPL_APPEND_BUF(Buf, Len)                                \
  do {                                                          \
    if (len < (Len))                                            \
      return DPL_FAILURE;                                       \
    memcpy(p, (Buf), (Len)); p += (Len); len -= (Len);          \
  } while (0);

#define DPL_APPEND_STR(Str) DPL_APPEND_BUF((Str), strlen(Str))

/**/

#define DPL_TRACE(ctx, level, format, ...) do {if (ctx->trace_level & level) dpl_trace(ctx, level, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__  );} while (0)
#define DPL_LOG(ctx, level, format, ...) do { dpl_log(ctx, level, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__); } while(0)

/* PROTO utils.c */
/* src/utils.c */
pid_t dpl_gettid(void);
int dpl_gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop);
void dpl_dump_init(struct dpl_dump_ctx *ctx, int binary);
void dpl_dump_line(struct dpl_dump_ctx *ctx, unsigned int off, unsigned char *b, unsigned int l);
void dpl_dump(struct dpl_dump_ctx *ctx, char *buf, int len);
void dpl_dump_simple(char *buf, int len, int binary);
void dpl_trace(dpl_ctx_t *ctx, unsigned int level, const char *file, const char *func, int lineno, const char *fmt, ...);
size_t dpl_iov_size(struct iovec *iov, int n_iov);
void dpl_iov_dump(struct iovec *iov, int n_iov, size_t n_bytes, int binary);
time_t dpl_iso8601totime(const char *str);
dpl_status_t dpl_timetoiso8601(time_t t, char *buf, int buf_size);
char *dpl_strrstr(const char *haystack, const char *needle);
void test_strrstr(void);
void dpl_strlower(char *str);
unsigned int dpl_hmac_sha1(const char *key_buf, unsigned int key_len, const char *data_buf, unsigned int data_len, char *digest_buf);
u_int dpl_base64_encode(const u_char *in_buf, u_int in_len, u_char *out_buf);
u_int dpl_base64_decode(const u_char *in_buf, u_int in_len, u_char *out_buf);
void dpl_url_encode(const char *str, char *str_ue);
void dpl_url_encode_no_slashes(const char *str, char *str_ue);
void dpl_url_decode(char *str);
unsigned int dpl_bcd_encode(unsigned char *in_buf, unsigned int in_len, char *out_buf);
dpl_status_t dpl_rand(char *buf, int len);
uint64_t dpl_rand_u64(void);
uint32_t dpl_rand_u32(void);
dpl_status_t dpl_uuid_rand(dpl_uuid_t *uuid);
void dpl_uuid_tostr(dpl_uuid_t *uuid, char *ostr);
time_t dpl_get_date(const char *p, const time_t *now);
u_int dpl_pow2_next(u_int v);
dpl_status_t dpl_log(dpl_ctx_t *ctx, dpl_log_level_t level, const char *file,
		     const char *func, int lineno, const char *fmt, ...);
#endif
