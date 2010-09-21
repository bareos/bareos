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

#include <dropletp.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

//#define DEBUG

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
gettid()
{
  return -1; //XXX
}
#else
#include <syscall.h>
pid_t 
gettid()
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
linux_gethostbyname_r(const char *name,
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
#else
  //linux
  return gethostbyname_r(name, ret, buf, buflen, result, h_errnop);
#endif
}

/**/

void
dpl_dump_init(struct dpl_dump_ctx *ctx)
{
  ctx->file = stderr;
  ctx->prevb_inited = 0;
  ctx->star_displayed = 0;
  ctx->global_off = 0;
}

void 
dpl_dump_line(struct dpl_dump_ctx *ctx,
              u_int off,
              u_char *b,
              u_int l)
{
  u_int i;

  //printf("l=%d prevb_inited=%d star_displayed=%d\n", l, ctx->prevb_inited, ctx->star_displayed);
  
  if (1 == ctx->prevb_inited && 
      DPL_DUMP_LINE_SIZE == l &&
      !memcmp(ctx->prevb, b, DPL_DUMP_LINE_SIZE))
    {
      if (0 == ctx->star_displayed)
        {
          fprintf(ctx->file, "*\n");
          ctx->star_displayed = 1;
        }
      
      return ;
    }
  else
    {
      ctx->star_displayed = 0;
    }
   
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
}

void
dpl_dump_simple(char *buf,
                int len)
{
  struct dpl_dump_ctx ctx;

  dpl_dump_init(&ctx);
  dpl_dump(&ctx, buf, len);
}

/**/

void
dpl_trace(dpl_ctx_t *ctx,
          u32 level,
          char *file,
          int lineno,
          char *fmt,
          ...)
{
  va_list args;
  char buf[256];
  
  va_start(args, fmt);
  vsnprintf(buf, sizeof (buf), fmt, args);
  va_end(args);
  
  if (NULL != ctx->trace_func)
    ctx->trace_func(gettid(), level, file, lineno, buf);
  else
    fprintf(stderr, "trace: %ld: [%x] file %s, line %d: %s\n", (long int) gettid(), level, file, lineno, buf);
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
             size_t n_bytes)
{
  size_t total;
  ssize_t dump_size;
  int i;
  struct dpl_dump_ctx dump_ctx;
  
  dpl_dump_init(&dump_ctx);

  total = 0;
  for (i = 0;i < n_iov && n_bytes > 0;i++)
    {
      printf("vec=%d len=%lu\n", i, (long unsigned) iov[i].iov_len);

      dump_size = MIN(n_bytes, iov[i].iov_len);
      dump_ctx.global_off = 0;
      dpl_dump(&dump_ctx, iov[i].iov_base, dump_size);
      n_bytes -= dump_size;
    }
}

/**/

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

/**/

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
          *str++ = strtoul(buf, NULL, 16);
          state = 0;
          break ;
        }
    }
  *str = 0;
}
