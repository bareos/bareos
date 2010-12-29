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

#include "dropletp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

//#define DEBUG

dpl_ctx_t *dpl_default_conn_ctx = NULL;

static u_int
conn_hashcode(char *buf,
              int len)
{
  u_int h, g, i;

  h = g = 0;

  for (i=0;i < len;buf++, i--)
    {
      h = (h<<4)+(*buf);
      if ((g=h&0xf0000000))
        {
          h=h^(g>>24);
          h=h^g;
        }
    }

  return h;
}

static dpl_conn_t *
dpl_conn_get_nolock(dpl_ctx_t *ctx,
                    struct in_addr addr,
                    u_short port)
{
  u_int bucket;
  struct dpl_hash_info hash_info;
  dpl_conn_t *conn;

  memset(&hash_info, 0, sizeof (hash_info));

  hash_info.addr.s_addr = addr.s_addr;
  hash_info.port = port;

  bucket = conn_hashcode((char *) &hash_info, sizeof (hash_info)) % ctx->n_conn_buckets;

  for (conn = ctx->conn_buckets[bucket];conn;conn = conn->prev)
    {
      if (!memcmp(&conn->hash_info, &hash_info, sizeof (hash_info)))
        return conn;
    }

  return NULL;
}

static void
dpl_conn_add_nolock(dpl_conn_t *conn)
{
  u_int bucket;

  bucket = conn_hashcode((char *) &conn->hash_info, sizeof (conn->hash_info)) % conn->ctx->n_conn_buckets;

  conn->next = NULL;
  conn->prev = conn->ctx->conn_buckets[bucket];

  if (NULL != conn->ctx->conn_buckets[bucket])
    conn->ctx->conn_buckets[bucket]->next = conn;

  conn->ctx->conn_buckets[bucket] = conn;

}

static void
dpl_conn_remove_nolock(dpl_ctx_t *ctx,
                       dpl_conn_t *conn)
{
  u_int bucket;

  bucket = conn_hashcode((char *) &conn->hash_info, sizeof (conn->hash_info)) % ctx->n_conn_buckets;

  if (conn->prev)
    conn->prev->next = conn->next;

  if (conn->next)
    conn->next->prev = conn->prev;

  if (ctx->conn_buckets[bucket] == conn)
    ctx->conn_buckets[bucket] = conn->prev;

}

static int
safe_close(int fd)
{
  int ret;

  DPRINTF("closing fd=%d\n", fd);

 retry:
  ret = close(fd);
  if (-1 == ret)
    {
      if (EINTR == errno)
        goto retry;

      DPLERR(1, "close failed");
      return -1;
    }

  return 0;
}

static void
dpl_conn_free(dpl_conn_t *conn)
{
  if (NULL != conn->ssl)
    SSL_free(conn->ssl);

  if (-1 != conn->fd)
    (void) safe_close(conn->fd);

  if (NULL != conn->read_buf)
    free(conn->read_buf);

  free(conn);
}

static void
dpl_conn_terminate_nolock(dpl_conn_t *conn)
{
  DPL_TRACE(conn->ctx, DPL_TRACE_CONN, "conn_terminate conn=%p", conn);

  conn->ctx->n_conn_fds--;
  dpl_conn_free(conn);
}

static int
do_connect(dpl_ctx_t *ctx,
           struct in_addr addr,
           u_int port)
{
  struct sockaddr_in sin;
  int   fd = -1;
  int ret;
  struct pollfd fds;
  int on;

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == fd)
    {
      DPLERR(1, "socket failed");
      fd = -1;
      goto end;
    }

  sin.sin_family = AF_INET;
  sin.sin_port = htons(port);
  sin.sin_addr.s_addr = addr.s_addr;

  on = 1;
  ret = ioctl(fd, FIONBIO, &on);
  if (-1 == ret)
    {
      (void) safe_close(fd);
      fd = -1;
      goto end;
    }

  DPL_TRACE(ctx, DPL_TRACE_CONN, "connect %s:%d", inet_ntoa(addr), port);

  ret = connect(fd, (struct sockaddr *)&sin, sizeof (sin));

  if (-1 == ret)
    {
      if (EINPROGRESS != errno)
        {
          DPLERR(1, "connect failed");
          (void) safe_close(fd);
          fd = -1;
          goto end;
        }
    }

 retry:
  memset(&fds, 0, sizeof (fds));
  fds.fd = fd;
  fds.events = POLLOUT;

  ret = poll(&fds, 1, ctx->conn_timeout*1000);
  if (-1 == ret)
    {
      if (errno == EINTR)
        goto retry;
      fd = -1;
      goto end;
    }

  if (0 == ret)
    return -1;
  else if (!(fds.revents & POLLOUT))
    {
      fd = -1;
      goto end;
    }

  on = 0;
  ret = ioctl(fd, FIONBIO, &on);
  if (-1 == ret)
    {
      (void) safe_close(fd);
      fd = -1;
      goto end;
    }

 end:

  DPL_TRACE(ctx, DPL_TRACE_CONN, "connect fd=%d", fd);

  return fd;
}

/**
 * check an existing connection bound on (addr,port). if none is found
 * a new connection is created.
 *
 * @param addr
 * @param port
 *
 * @return
 */
dpl_conn_t *
dpl_conn_open(dpl_ctx_t *ctx,
              struct in_addr addr,
              u_int port)
{
  dpl_conn_t *conn = NULL;
  time_t now = time(0);
  int ret;

  pthread_mutex_lock(&ctx->lock);

  DPL_TRACE(ctx, DPL_TRACE_CONN, "conn_open %s:%d", inet_ntoa(addr), port);

  conn = dpl_conn_get_nolock(ctx, addr, port);

  if (NULL != conn)
    {
      dpl_conn_remove_nolock(ctx, conn);
      if (conn->n_hits >= ctx->n_conn_max_hits ||
          (now - conn->close_time) >= ctx->conn_idle_time)
        {
          DPRINTF("auto-close\n");
          dpl_conn_terminate_nolock(conn);
          conn = NULL;
        }
      else
        {
          //OK reuse
          conn->n_hits++;
          goto end;
        }
    }

  if (ctx->n_conn_fds >= ctx->n_conn_max)
    {
      DPLERR(0, "reaching limit %d", ctx->n_conn_fds);
      conn = NULL;
      goto end;
    }

  DPRINTF("allocate new conn\n");

  conn = malloc(sizeof (*conn));
  if (NULL == conn)
    {
      DPLERR(1, "malloc failed");
      conn = NULL;
      goto end;
    }

  memset(conn, 0, sizeof (*conn));

  conn->ctx = ctx;
  conn->read_buf_size = ctx->read_buf_size;
  conn->fd = -1;

  if ((conn->read_buf = malloc(conn->read_buf_size)) == NULL)
    {
      dpl_conn_free(conn);
      conn = NULL;
      goto end;
    }

  conn->hash_info.addr.s_addr = addr.s_addr;
  conn->hash_info.port = port;

  conn->fd = do_connect(ctx, addr, port);
  if (-1 == conn->fd)
    {
      dpl_conn_free(conn);
      conn = NULL;
      goto end;
    }

  conn->start_time = now;
  conn->n_hits = 0;

  if (1 == ctx->use_https)
    {
      conn->ssl = SSL_new(ctx->ssl_ctx);
      if (NULL == conn->ssl)
        {
          dpl_conn_free(conn);
          conn = NULL;
          goto end;
        }

      conn->bio = BIO_new_socket(conn->fd, BIO_NOCLOSE);
      if (NULL == conn->bio)
        {
          dpl_conn_free(conn);
          conn = NULL;
          goto end;
        }

      SSL_set_bio(conn->ssl, conn->bio, conn->bio);

      DPL_TRACE(conn->ctx, DPL_TRACE_SSL, "SSL_connect conn=%p", conn);

      ret = SSL_connect(conn->ssl);
      if (ret <= 0)
        {
          fprintf(stderr, "SSL_connect: %d\n", SSL_get_error(conn->ssl, ret));
          dpl_conn_free(conn);
          conn = NULL;
          goto end;
        }

    }

  ctx->n_conn_fds++;

 end:

  pthread_mutex_unlock(&ctx->lock);

  DPL_TRACE(ctx, DPL_TRACE_CONN, "conn_open conn=%p", conn);

  return conn;
}

dpl_conn_t *
dpl_conn_open_host(dpl_ctx_t *ctx,
                   char *host,
                   u_int port)
{
  int           ret2;
  struct hostent hret, *hresult;
  char          hbuf[1024];
  int           herr;
  struct in_addr addr;
  dpl_conn_t    *conn;

  ret2 = linux_gethostbyname_r(host, &hret, hbuf, sizeof (hbuf), &hresult, &herr);
  if (0 != ret2)
    {
      DPLERR(0, "gethostbyname failed");
      goto bad;
    }

  if (!hresult)
    {
      DPLERR(0, "Invalid hostname");
      goto bad;
    }

  if (AF_INET != hresult->h_addrtype)
    {
      DPLERR(0, "bad addr family");
      goto bad;
    }

  memcpy(&addr, hresult->h_addr_list[0], hresult->h_length);

  conn = dpl_conn_open(ctx, addr, ctx->port);
  if (NULL == conn)
    {
      DPLERR(0, "connect failed");
      goto bad;
    }

  return conn;

 bad:

  return NULL;
}

/**
 * release the connection
 *
 * @param conn
 */
void
dpl_conn_release(dpl_conn_t *conn)
{
  pthread_mutex_lock(&conn->ctx->lock);

  DPL_TRACE(conn->ctx, DPL_TRACE_CONN, "conn_release conn=%p", conn);

  conn->close_time = time(0);
  dpl_conn_add_nolock(conn);

  pthread_mutex_unlock(&conn->ctx->lock);
}

/**
 * call this if you encounter an error on conn fd
 *
 * @param conn
 */
void
dpl_conn_terminate(dpl_conn_t *conn)
{
  dpl_ctx_t *ctx;

  DPRINTF("explicit termination n_hits=%d\n", conn->n_hits);

  ctx = conn->ctx;

  pthread_mutex_lock(&ctx->lock);

  dpl_conn_terminate_nolock(conn);

  pthread_mutex_unlock(&ctx->lock);
}

dpl_status_t
dpl_conn_pool_init(dpl_ctx_t *ctx)
{
  ctx->conn_buckets = malloc(ctx->n_conn_buckets * sizeof (dpl_conn_t *));
  if (NULL == ctx->conn_buckets)
    return DPL_FAILURE;

  memset(ctx->conn_buckets, 0, ctx->n_conn_buckets * sizeof (dpl_conn_t *));

  return DPL_SUCCESS;
}

void
dpl_conn_pool_destroy(dpl_ctx_t *ctx)
{
  int bucket;
  dpl_conn_t *conn, *prev;

  if (NULL != ctx->conn_buckets)
    {
      for (bucket = 0;bucket < ctx->n_conn_buckets;bucket++)
        {
          for (conn = ctx->conn_buckets[bucket];conn;conn = prev)
            {
              prev = conn->prev;
              dpl_conn_terminate_nolock(conn);
            }
        }

      free(ctx->conn_buckets);
    }
}

/*
 * I/O
 */

/**
 * write a vector with retry
 *
 * @note: modify iov content
 *
 * @param fd
 * @param iov
 * @param n_iov
 * @param timeout in secs or -1
 *
 * @return biznet_status
 */
static dpl_status_t
writev_all_plaintext(dpl_conn_t *conn,
                     struct iovec *iov,
                     int n_iov,
                     int timeout)
{
  ssize_t cc = 0;
  int i, ret;

  DPRINTF("writev n_iov=%d\n", n_iov);

  while (1)
    {
      if (-1 != timeout)
        {
          struct pollfd fds;

        retry:
          memset(&fds, 0, sizeof (fds));
          fds.fd = conn->fd;
          fds.events = POLLOUT;

          ret = poll(&fds, 1, timeout*1000);
          if (-1 == ret)
            {
              if (errno == EINTR)
                goto retry;
              return DPL_FAILURE;
            }

          if (0 == ret)
            return DPL_ETIMEOUT;
          else if (!(fds.revents & POLLOUT))
            {
              return DPL_FAILURE;
            }
        }

      cc = writev(conn->fd, iov, n_iov);
      if (-1 == cc)
        {
          if (EINTR == errno)
            continue ;

          return DPL_FAILURE;
        }

      for (i = 0;i < n_iov;i++)
        {
          if (iov[i].iov_len > cc)
            {
              iov[i].iov_base = (char *) iov[i].iov_base + cc;
              iov[i].iov_len -= cc;
              break ;
            }
          cc -= iov[i].iov_len;
          iov[i].iov_len = 0;
        }

      if (n_iov == i)
        return DPL_SUCCESS;
    }

  return DPL_SUCCESS;
}

/**
 *
 *
 * @param conn
 * @param iov
 * @param n_iov
 * @param timeout ignored for now
 *
 * @return
 */
static dpl_status_t
writev_all_ssl(dpl_conn_t *conn,
               struct iovec *iov,
               int n_iov,
               int timeout)
{
  int i, ret;
  u_int total_size, off;
  char *ptr = NULL;

  total_size = 0;

  for (i = 0;i < n_iov;i++)
    total_size += iov[i].iov_len;

  ptr = malloc(total_size);
  if (NULL == ptr)
    return DPL_FAILURE;

  off = 0;
  for (i = 0;i < n_iov;i++)
    {
      memcpy(ptr + off, iov[i].iov_base, iov[i].iov_len);
      off += iov[i].iov_len;
    }

  ret = SSL_write(conn->ssl, ptr, total_size);
  if (ret <= 0)
    {
      fprintf(stderr, "SSL_write: %d\n", SSL_get_error(conn->ssl, ret));
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != ptr)
    free(ptr);

  return ret;
}

dpl_status_t
dpl_conn_writev_all(dpl_conn_t *conn,
                    struct iovec *iov,
                    int n_iov,
                    int timeout)
{
  DPL_TRACE(conn->ctx, DPL_TRACE_IO, "writev conn=%p https=%d size=%ld", conn, conn->ctx->use_https, dpl_iov_size(iov, n_iov));

  if (conn->ctx->trace_level & DPL_TRACE_BUF)
    dpl_iov_dump(iov, n_iov, dpl_iov_size(iov, n_iov));

  if (0 == conn->ctx->use_https)
    return writev_all_plaintext(conn, iov, n_iov, timeout);
  else
    return writev_all_ssl(conn, iov, n_iov, timeout);
}
