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
#include <dropletp.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

//#define DEBUG

/*
 * port
 */

#ifndef HAVE_DRAND48_R
int
srand48_r(long int seedval,
          struct drand48_data *buffer)
{
  buffer->seed = seedval;
  return 0;
}

int
lrand48_r(struct drand48_data *buffer,
          long int *result)
{
  return rand_r(&buffer->seed);
}
#endif

#ifndef HAVE_GETTID
pid_t
dpl_gettid()
{
  return -1; //XXX
}
#elif defined(__APPLE__) && defined(__MACH__) || defined(__ellcc__ )
#include <sys/syscall.h>
pid_t
dpl_gettid()
{
  return syscall(SYS_gettid);
}

#else
#include <syscall.h>
pid_t
dpl_gettid()
{
  return syscall(SYS_gettid);
}
#endif

#ifndef HAVE_CANONICALIZE_FILE_NAME
char *
canonicalize_file_name(const char *path)
{
  return realpath(path, NULL);
}
#endif

int
dpl_gethostbyname_r(const char *name,
                      struct hostent *ret,
  	              char *buf,
                      size_t buflen,
                      struct hostent **result,
                      int *h_errnop)
{
#if defined(SOLARIS) || defined(__sun__)
  struct hostent *resultp;

  resultp = gethostbyname_r(name, ret, buf, buflen, h_errnop);
  if (NULL == resultp)
    return 1;

  *result = resultp;

  return 0;

#elif defined(__APPLE__) && defined(__MACH__) || defined(__ellcc__ )

  struct hostent *resultp;

  resultp = gethostbyname(name);
  if (NULL == resultp)
    return 1;

  *result = resultp;

  return 0;
#else
  //linux
  return gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
#endif
}

/*
 * debug
 */

void
dpl_dump_init(struct dpl_dump_ctx *ctx,
              int binary)
{
  ctx->file = stderr;
  ctx->prevb_inited = 0;
  ctx->star_displayed = 0;
  ctx->global_off = 0;
  ctx->binary = binary;
}

void
dpl_dump_line(struct dpl_dump_ctx *ctx,
              unsigned int off,
              unsigned char *b,
              unsigned int l)
{
  u_int i;

  //printf("l=%d prevb_inited=%d star_displayed=%d\n", l, ctx->prevb_inited, ctx->star_displayed);

  if (1 == ctx->prevb_inited &&
      DPL_DUMP_LINE_SIZE == l &&
      !memcmp(ctx->prevb, b, DPL_DUMP_LINE_SIZE))
    {
      if (0 == ctx->star_displayed)
        {
          if (ctx->binary)
            fprintf(ctx->file, "*\n");
          else
            fprintf(ctx->file, "...");
          ctx->star_displayed = 1;
        }

      return ;
    }
  else
    {
      ctx->star_displayed = 0;
    }

  if (ctx->binary)
    {
      fprintf(ctx->file, "%08d  ", (int) ctx->global_off + off - l);
      
      i = 0;
      while (i < DPL_DUMP_LINE_SIZE)
        {
          if (i < l)
            fprintf(ctx->file, "%02x", b[i]);
          else
            fprintf(ctx->file, "  ");
          if (i % 2)
            fprintf(ctx->file, " ");
          i++;
        }
      
      fprintf(ctx->file, "  ");
      
      i = 0;
      while (i < l)
        {
          if (isprint(b[i]))
            fprintf(ctx->file, "%c", b[i]);
          else
            fprintf(ctx->file, ".");
          i++;
        }
      
      fprintf(ctx->file, "\n");
    }
  else
    {
      i = 0;
      while (i < l)
        {
          fprintf(ctx->file, "%c", b[i]);
          i++;
        }
    }
}

void
dpl_dump(struct dpl_dump_ctx *ctx,
         char *buf,
         int len)
{
  int i;
  u_char b[DPL_DUMP_LINE_SIZE];
  u_int l;

  l = 0;
  i = 0;
  while (i < len)
    {
      if (l < DPL_DUMP_LINE_SIZE)
        {
          b[l++] = buf[i];
        }
      else
        {
          dpl_dump_line(ctx, i, b, DPL_DUMP_LINE_SIZE);

          memcpy(ctx->prevb, b, DPL_DUMP_LINE_SIZE);
          ctx->prevb_inited = 1;

          l = 0;
          b[l++] = buf[i];
        }
      i++;
    }

  if (l > 0)
    dpl_dump_line(ctx, i, b, l);

  ctx->global_off += i;

  if (!ctx->binary)
    {
      if (len > 1 && buf[len-1] != '\n')
        fprintf(ctx->file, "\n");
    }
}

void
dpl_dump_simple(char *buf,
                int len,
                int dump_binary)
{
  struct dpl_dump_ctx ctx;

  dpl_dump_init(&ctx, dump_binary);
  dpl_dump(&ctx, buf, len);
}

/**/

void
dpl_trace(dpl_ctx_t *ctx,
          unsigned int level,
          const char *file,
          const char *func,
          int lineno,
          const char *fmt,
          ...)
{
  va_list args;
  char buf[256];

  va_start(args, fmt);
  vsnprintf(buf, sizeof (buf), fmt, args);
  va_end(args);

  if (NULL != ctx->trace_func)
    ctx->trace_func(dpl_gettid(), level, file, func, lineno, buf);
  else
    fprintf(stderr, "trace: %ld: [%x] %s:%s:%d: %s\n", (long int) dpl_gettid(), level, file, func, lineno, buf);
}

/**/

size_t
dpl_iov_size(struct iovec *iov,
             int n_iov)
{
  int i;
  size_t size = 0;

  for (i = 0;i < n_iov;i++)
    size += iov[i].iov_len;

  return size;
}

void
dpl_iov_dump(struct iovec *iov,
             int n_iov,
             size_t n_bytes,
             int binary)
{
  __attribute__((unused)) size_t total;
  ssize_t dump_size;
  int i;
  struct dpl_dump_ctx dump_ctx;

  dpl_dump_init(&dump_ctx, binary);

  total = 0;
  for (i = 0;i < n_iov && n_bytes > 0;i++)
    {
      if (binary)
        fprintf(dump_ctx.file, "%d: len=%lu\n", i, (long unsigned) iov[i].iov_len);
      dump_size = MIN(n_bytes, iov[i].iov_len);
      dump_ctx.global_off = 0;
      dpl_dump(&dump_ctx, iov[i].iov_base, dump_size);
      n_bytes -= dump_size;
    }
}

/*
 * utils
 */

static int
check_string(const char *str,
             const char *format)
{
  while (*format)
    {
      if (*format == 'd')
        {
          if (!isdigit(*str))
            return 0;
        }
      else if (*str != *format)
        return 0;

      str++, format++;
    }

  return 1;
}

time_t
dpl_iso8601totime(const char *str)
{
  struct tm tm_buf;
  time_t t;

#define nextnum() (((*str - '0') * 10) + (*(str + 1) - '0'))

  if (!check_string(str, "dddd-dd-ddTdd:dd:dd"))
    return -1;

  memset(&tm_buf, 0, sizeof(tm_buf));

  tm_buf.tm_year = (nextnum() - 19) * 100;
  str += 2;
  tm_buf.tm_year += nextnum();
  str += 3;

  tm_buf.tm_mon = nextnum() - 1;
  str += 3;

  tm_buf.tm_mday = nextnum();
  str += 3;

  tm_buf.tm_hour = nextnum();
  str += 3;

  tm_buf.tm_min = nextnum();
  str += 3;

  tm_buf.tm_sec = nextnum();
  str += 2;

  tm_buf.tm_isdst = -1;

  tm_buf.tm_zone = "UTC";

  t = mktime(&tm_buf);

  if (*str == '.')
    {
      str++;
      while (isdigit(*str))
        str++;
    }

  if (check_string(str, "-dd:dd") || check_string(str, "+dd:dd"))
    {
      int sign = (*str++ == '-') ? -1 : 1;
      int hours = nextnum();
      str += 3;
      int minutes = nextnum();
      t += (-sign * (((hours * 60) + minutes) * 60));
    }

  return t;
}

dpl_status_t
dpl_timetoiso8601(time_t t, 
                  char *buf,
                  int buf_size)
{
  struct tm *tm_ptr, tm_buf;

  tm_ptr = gmtime_r(&t, &tm_buf);

  snprintf(buf, buf_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
           1900 + tm_ptr->tm_year,
           tm_ptr->tm_mon + 1,
           tm_ptr->tm_mday,
           tm_ptr->tm_hour,
           tm_ptr->tm_min,
           tm_ptr->tm_sec);

  return DPL_SUCCESS;
}

/**/

/**
 * find the last occurence of needle in haystack
 *
 * @param haystack
 * @param needle
 *
 * @return
 */
char *
dpl_strrstr(const char *haystack,
            const char *needle)
{
  int haystack_len = strlen(haystack);
  int needle_len = strlen(needle);
  int i;

  for (i = haystack_len - needle_len;i >= 0;i--)
    {
      //printf("%s\n", haystack + i);
      if (!strncmp(haystack + i, needle, needle_len))
        {
          //printf("found '%s'\n", haystack + i);
          return (char *) (haystack + i);
        }
    }

  return NULL;
}

#if 1
void
test_strrstr()
{
  printf("%s\n", dpl_strrstr("lev1DIRlev2DIRlev3", "DIR"));
  printf("%s\n", dpl_strrstr("foo/bar/", "/"));
}
#endif

void
dpl_strlower(char *str)
{
  for (;*str;str++)
    {
      if (isupper(*str))
        *str = tolower(*str);
    }
}

/**
 * compute HMAC-SHA1
 *
 * @param key_buf
 * @param key_len
 * @param data_buf
 * @param data_len
 * @param digest_buf
 * @param digest_lenp
 *
 * @return digest_len
 */
unsigned int
dpl_hmac_sha1(const char *key_buf,
              unsigned int key_len,
              const char *data_buf,
              unsigned int data_len,
              char *digest_buf)
{
  HMAC_CTX ctx;
  u_int digest_len;

  HMAC_CTX_init(&ctx);
  HMAC_Init_ex(&ctx, key_buf, key_len, EVP_sha1(), NULL);
  HMAC_Update(&ctx, (u_char *) data_buf, data_len);
  HMAC_Final(&ctx, (u_char *) digest_buf, &digest_len);
  HMAC_CTX_cleanup(&ctx);

  return digest_len;
}

/**/

static const char *base = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int valueof[256] = {0};

static void create_valueof_table(void)
{
  int i;

  for (i = 0; i < 256; ++i)
    {
      valueof[i] = -1;
    }

  for (i = 0; i < 64; ++i)
    {
      valueof[(int)base[i]] = i;
    }
}

int dpl_base64_init(void)
{
  create_valueof_table();

  return 0;
}


#define isbase64(c) (-1 != valueof[(c)])
#define value(c) valueof[(c)]


/**
 * base64 encode
 *
 * @brief encode a binary buffer in base64.
 *
 * @param in_buf input buffer to encode
 * @param in_len input buffer size
 * @param out_buf base64 output buffer that must be at least of size
 * DPL_BASE64_LENGTH(in_len)
 *
 * @return out_len length of base64 output
 */
u_int
dpl_base64_encode(const u_char *in_buf,
                   u_int in_len,
                   u_char *out_buf)
{
  u_char *saved_out_buf = out_buf;

  while (in_len)
    {
      *out_buf++ = base[*in_buf >> 2];
      if (!--in_len)
        {
          *out_buf++ = base[(*in_buf & 0x3) << 4];
          *out_buf++ = '=';
          *out_buf++ = '=';
          break;
        }

      *out_buf++ = base[((*in_buf & 0x3) << 4) | (*(in_buf + 1) >> 4)];
      in_buf++;
      if (!--in_len)
        {
          *out_buf++ = base[(*in_buf & 0xF) << 2];
          *out_buf++ = '=';
          break;
        }

      *out_buf++ = base[((*in_buf & 0xF) << 2) | (*(in_buf + 1) >> 6)];
      in_buf++;

      *out_buf++ = base[*in_buf & 0x3F];
      in_buf++, in_len--;
    }

  return (out_buf - saved_out_buf);
}

/**
 * base64 decode
 *
 * @brief decode a base64-encoded buffer to the original binary data.
 *
 * @param in_buf input buffer to decode
 * @param in_len input buffer size
 * @param out_buf base64 output buffer (must be at least of size
 * DPL_BASE64_ORIG_LENGTH(in_len))
 *
 * @return out_len length of decoded output
 */
u_int 
dpl_base64_decode(const u_char *in_buf,
                   u_int in_len,
                   u_char *out_buf)
{
  unsigned char *p = out_buf;
  char a, b, c, d;

  while (in_len > 0)
    {
      if (in_len < 4)
        return (u_int)-1;

      if (!isbase64(in_buf[0]) ||
          !isbase64(in_buf[1]))
        return (u_int)-1;

      a = value(*in_buf);
      ++in_buf;
      b = value(*in_buf);
      ++in_buf;

      *p++ = (a << 2) | (b >> 4);

      if (!isbase64(*in_buf))
        {
          if ('=' == *in_buf)
            {
              if ('=' == in_buf[1])
                break ;
            }
          return (u_int)-1;
        }

      c = value(*in_buf);
      ++in_buf;

      *p++ = (b << 4) | (c >> 2);

      if (!isbase64(*in_buf))
        {
          if ('=' == *in_buf)
            break ;
          return (u_int)-1;
        }

      d = value(*in_buf);
      ++in_buf;

      *p++ = (c << 6) | d;

      in_len -= 4;

      while (in_len > 0 && (*in_buf == '\r' || *in_buf == '\n'))
        {
          ++in_buf;
          --in_len;
        }
    }

  return p - out_buf;
}


/**/

/**
 * encode str into URL form. str_ue length must be at least strlen(str)*3+1
 *
 * @param str
 * @param str_ue
 *
 * @return
 */
void
dpl_url_encode(const char *str,
               char *str_ue)
{
  int   i;

  for (i = 0;*str;str++)
    {
      if (isalnum(*str))
        str_ue[i++] = *str;
      else
        {
          sprintf(str_ue + i, "%%%02X", (unsigned char)*str);
          i+=3;
        }
    }
  str_ue[i] = 0;
}

void
dpl_url_encode_no_slashes(const char *str,
                          char *str_ue)
{
  int   i;

  for (i = 0;*str;str++)
    {
      if (isalnum(*str) || *str == '/')
        str_ue[i++] = *str;
      else
        {
          sprintf(str_ue + i, "%%%02X", (unsigned char)*str);
          i+=3;
        }
    }
  str_ue[i] = 0;
}

/**
 * decode an URL
 *
 * @param str
 */
void
dpl_url_decode(char *str)
{
  char buf[3], *p;
  int   state;

  state = 0;
  for (p = str;*p;p++)
    {
      switch (state)
        {
        case 0:
          if (*p == '%')
            {
              state = 1;
              break ;
            }
          else
            *str++ = *p;
          break ;
        case 1:
          buf[0] = *p;
          state++;
          break ;
        case 2:
          buf[1] = *p;
          buf[2] = 0;
          *str++ = (char)strtoul(buf, NULL, 16);
          state = 0;
          break ;
        }
    }
  *str = 0;
}

/**/

unsigned int
dpl_bcd_encode(unsigned char *in_buf,
               unsigned int in_len,
               char *out_buf)
{
  int i;
  u_int out_len = 0;

  for (i = 0;i < in_len;i++)
    {
      int       c1, c2;

      c1 = (in_buf[i] & 0xf0) >> 4;
      c2 = in_buf[i] & 0xf;
      out_buf[i*2] = c1 >= 10 ? c1 - 10 + 'a' : c1 + '0';
      out_buf[i*2+1] = c2 >= 10 ? c2 - 10 + 'a' : c2 + '0';
      out_len += 2;
    }

  return out_len;
}

/**/

dpl_status_t
dpl_rand(char *buf, int len)
{
  int ret;

  ret = RAND_pseudo_bytes((u_char *) buf, len);
  if (-1 == ret)
    {
      return DPL_FAILURE;
    }

  return DPL_SUCCESS;
}

uint64_t
dpl_rand_u64(void)
{
  uint64_t random_val = 0xDEADBEEFBAADF00DLL;

  dpl_rand((char *)&random_val, sizeof(random_val));

  return random_val;
}

uint32_t
dpl_rand_u32(void)
{
  uint32_t random_val = 0xDEADBEEF;

  dpl_rand((char *)&random_val, sizeof(random_val));

  return random_val;
}

/**/

dpl_status_t
dpl_uuid_rand(dpl_uuid_t *uuid)
{
  int ret2, ret;

  memset(uuid, 0, sizeof(*uuid));
  ret2 = dpl_rand((char *) uuid, sizeof (*uuid));
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }
  
  uuid->time_hi_and_version = (uuid->time_hi_and_version & 0x0fff) |  0x4;
  uuid->clock_seq_hi_and_reserved = (uuid->clock_seq_hi_and_reserved & 0x0fff) | 0xa;

  ret = DPL_SUCCESS;
  
 end:
  
  return ret;
}

/**/

void
dpl_uuid_tostr(dpl_uuid_t *uuid,
               char *ostr)
{
  (void) sprintf(ostr,
                 "%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
                 uuid->time_low,
                 uuid->time_mid,
                 uuid->time_hi_and_version,
                 uuid->clock_seq_hi_and_reserved,
                 uuid->clock_seq_low,
                 uuid->node_addr[0],
                 uuid->node_addr[1],
                 uuid->node_addr[2],
                 uuid->node_addr[3],
                 uuid->node_addr[4],
                 uuid->node_addr[5]);
}

char *
dpl_ftype_to_str(dpl_ftype_t type)
{
  switch (type)
    {
#define MAP(x) case DPL_FTYPE_##x : return #x
      MAP(UNDEF);
      MAP(ANY);
      MAP(REG);
      MAP(DIR);
      MAP(CAP);
      MAP(DOM);
      MAP(CHRDEV);
      MAP(BLKDEV);
      MAP(FIFO);
      MAP(SOCKET);
      MAP(SYMLINK);
    }
#undef MAP

  return "impossible case";
}

char *
dpl_copy_directive_to_str(dpl_copy_directive_t directive)
{
  switch (directive)
    {
#define MAP(x) case DPL_COPY_DIRECTIVE_##x : return #x
      MAP(UNDEF);
      MAP(COPY);
      MAP(METADATA_REPLACE);
      MAP(LINK);
      MAP(SYMLINK);
      MAP(MOVE);
      MAP(MKDENT);
      MAP(RMDENT);
      MAP(MVDENT);
#undef MAP
    }


  return "impossible case";
}

u_int 
dpl_pow2_next(u_int v)
{
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  
  return v;
}

static void
dpl_default_logfunc(dpl_ctx_t *ctx, dpl_log_level_t level, const char *message)
{
  fputs(message, stderr);
  fputc('\n', stderr);
}

static dpl_log_func_t dpl_logfunc = dpl_default_logfunc;

dpl_status_t
dpl_log(dpl_ctx_t *ctx,
	dpl_log_level_t level,
        const char *file,
        const char *func,
        int lineno,
	const char *fmt,
	...)
{
  /* TODO: there is no dpl_dbuf_add_vprintf() */
  va_list args;
  const char *level_name = NULL;
  int n;
  char linebuf[32];
  char message[2048] = "";
  /* the following two are magic names used in DPL_APPEND_* macros */
  size_t len = sizeof(message);
  char *p = message;

  switch (level) {
  case DPL_DEBUG: level_name = "DEBUG"; break;
  case DPL_INFO: level_name = "info"; break;
  case DPL_WARNING: level_name = "warning"; break;
  case DPL_ERROR: level_name = "error"; break;
  }
  if (level_name) {
    DPL_APPEND_STR(level_name);
    DPL_APPEND_STR(": ");
  }

  if (file) {
    DPL_APPEND_STR(file);
    DPL_APPEND_CHAR(':');
    snprintf(linebuf, sizeof(linebuf), "%d", lineno);
    DPL_APPEND_STR(linebuf);
    DPL_APPEND_STR(": ");
  }

  if (func) {
    DPL_APPEND_STR(func);
    DPL_APPEND_STR(": ");
  }

  va_start(args, fmt);
  n = vsnprintf(p, len, fmt, args);
  va_end(args);
  if (n > len)
    return DPL_FAILURE;	/* sprintf output was truncated */

  dpl_logfunc(ctx, level, message);
}

/**
 * @addtogroup init
 * @{
 */

/**
 * Set logging callback function.
 *
 * Set a function that will be called to handle every log message
 * emitted by the Droplet library.  Passing `NULL` for @a logfunc
 * resets the Droplet library to using it's default internal logging
 * which emits log messages to the process' stderr.  The @a logfunc
 * is called with a pre-formatted string representing a single line
 * of log output with no newline.  It is also passed a `dpl_ctx_t*`
 * which may be NULL or may point to a context associated with the
 * error.
 *
 * This function is not thread-safe.  Do not call it while other threads
 * are running Droplet library code.  It is recommended you call the
 * function just after calling `dpl_init()`.
 *
 * @param logfunc the callback function or NULL for default behaviour
 */
void
dpl_set_log_func(dpl_log_func_t logfunc)
{
  dpl_logfunc = (logfunc == NULL ? dpl_default_logfunc : logfunc);
}

/** @} */
