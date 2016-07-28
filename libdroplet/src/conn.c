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
#include "dropletp.h"

/** @file */

/* #define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__) */
#define DPRINTF(fmt,...)

//#define DEBUG

dpl_ctx_t *dpl_default_conn_ctx = NULL;

static u_int
conn_hashcode(const unsigned char *data,
              size_t len)
{
  const unsigned char *p;
  u_int h, g;
  int i = 0;

  h = g = 0;

  for (p=data;i < len;p=p+1,i++)
    {
      h = (h<<4)+(*p);
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
                    struct hostent *host, u_short port)
{
  u_int                 bucket;
  struct dpl_hash_info  hash_info;
  dpl_conn_t            *conn;

  memset(&hash_info, 0, sizeof (hash_info));

  memcpy(&hash_info.addr, host->h_addr, host->h_length);
  hash_info.port = port;

  bucket = conn_hashcode((unsigned char *)&hash_info, sizeof (hash_info)) % ctx->n_conn_buckets;

  for (conn = ctx->conn_buckets[bucket];conn;conn = conn->prev) {
    if (!memcmp(&conn->hash_info, &hash_info, sizeof (hash_info)))
      return conn;
  }

  return NULL;
}

static int
is_usable(dpl_conn_t *conn)
{
  struct pollfd pfd;
  int retval;

  memset(&pfd, 0, sizeof(struct pollfd));

  pfd.fd = conn->fd;
#ifdef POLLRDHUP
  pfd.events = POLLIN|POLLPRI|POLLRDHUP;
#else
  pfd.events = POLLIN|POLLPRI;
#endif

  retval = poll(&pfd, 1, 0);

  switch (retval)
    {
    case 1:
      {
        char  buf[1];
        int   size;

        if (conn->ctx->use_https)
          size = SSL_read(conn->ssl, buf, sizeof(buf));
        else
          size = recv(conn->fd, buf, sizeof(buf), MSG_DONTWAIT|MSG_PEEK);

        if (size == 0) {
          DPRINTF("is_usable: rv %d returning False\n", retval);
          return 0;
        }
      }
      /* fall down */
    case 0:
      DPRINTF("is_usable: rv %d returning True\n", retval);
      return 1;
    default:
      DPRINTF("is_usable: rv %d returning False\n", retval);
      return 0;
    }

  return 0; /* not reached */
}

static void
dpl_conn_add_nolock(dpl_conn_t *conn)
{
  u_int bucket;

  bucket = conn_hashcode((unsigned char *) &conn->hash_info, sizeof (conn->hash_info)) % conn->ctx->n_conn_buckets;

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

  bucket = conn_hashcode((unsigned char *) &conn->hash_info, sizeof (conn->hash_info)) % ctx->n_conn_buckets;

  if (conn->prev)
    conn->prev->next = conn->next;

  if (conn->next)
    conn->next->prev = conn->prev;

  if (ctx->conn_buckets[bucket] == conn)
    ctx->conn_buckets[bucket] = conn->prev;

}

static void
safe_close(dpl_ctx_t *ctx, int fd)
{
  int   ret;

  DPRINTF("closing fd=%d\n", fd);

  do {
    ret = close(fd);
  } while (ret == -1 && errno == EINTR);

  if (ret == -1)
    DPL_TRACE(ctx, DPL_TRACE_WARN, "close failed: %s", strerror(errno));
}

static void
dpl_conn_free(dpl_conn_t *conn)
{
  if (NULL != conn->ssl) {
    int ssl_ret;
    char buf[256];
    unsigned long ssl_err;
    SSL_set_shutdown(conn->ssl, SSL_SENT_SHUTDOWN|SSL_RECEIVED_SHUTDOWN);
    ssl_ret = SSL_shutdown(conn->ssl);
    if (1 == ssl_ret) {
      DPL_TRACE(conn->ctx, DPL_TRACE_WARN, "SSL shutdown was successfully completed");
    } else if (0 == ssl_ret) {
      ssl_err = SSL_get_error(conn->ssl, ssl_ret);
      ERR_error_string_n(ssl_err, buf, sizeof buf);
      DPL_TRACE(conn->ctx, DPL_TRACE_WARN, "SSL shutdown is not yet finished, calling for a second time: %s", buf);
      ssl_ret = SSL_shutdown(conn->ssl);
      if (1 == ssl_ret) {
        DPL_TRACE(conn->ctx, DPL_TRACE_WARN, "SSL shutdown was successfully completed");
      } else {
        DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "SSL shutdown was not successfully completed");
      }
    } else if (0 > ssl_ret) {
      ssl_err = SSL_get_error(conn->ssl, ssl_ret);
      ERR_error_string_n(ssl_err, buf, sizeof buf);
      DPL_TRACE(conn->ctx, DPL_TRACE_WARN, "SSL shutdown was not successful because a fatal error occurred: %s", buf);
    }
    SSL_free(conn->ssl);
  }

  if (-1 != conn->fd)
    safe_close(conn->ctx, conn->fd);

  if (NULL != conn->read_buf)
    free(conn->read_buf);

  if (NULL != conn->host)
    free(conn->host);

  if (NULL != conn->port)
    free(conn->port);

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
           struct hostent *host, u_short port)
{
  int                   fd = -1, ret, on, error;
  struct pollfd         fds;
  socklen_t             errorlen;
  char                  ident[DPL_ADDR_IDENT_STRLEN];

  fd = socket(host->h_addrtype, SOCK_STREAM, 0);
  if (fd == -1) {
    DPL_TRACE(ctx, DPL_TRACE_ERR, "socket failed");
    goto end;
  }

  on = 1;
  ret = ioctl(fd, FIONBIO, &on);
  if (-1 == ret)
    {
      DPL_LOG(ctx, DPL_ERROR, "ioctl(FIONBIO) failed: %s", strerror(errno));
      safe_close(ctx, fd);
      fd = -1;
      goto end;
    }

  dpl_addr_get_ident(host, port, ident, sizeof(ident));
  DPL_TRACE(ctx, DPL_TRACE_CONN, "connect %s", ident);

  if (host->h_addrtype == AF_INET) {
    struct sockaddr_in    sin;

    sin.sin_family = host->h_addrtype;
    sin.sin_port   = htons(port);
    memcpy(&sin.sin_addr, host->h_addr, host->h_length);

    ret = connect(fd, (struct sockaddr *) &sin, sizeof(sin));
  } else {
    struct sockaddr_in6   sin;

    sin.sin6_family = host->h_addrtype;
    sin.sin6_port   = htons(port);
    memcpy(&sin.sin6_addr, host->h_addr, host->h_length);

    ret = connect(fd, (struct sockaddr *) &sin, sizeof(sin));
  }

  if (-1 == ret)
    {
      if (EINPROGRESS != errno)
	{
	  DPL_LOG(ctx, DPL_ERROR, "Connect to server %s failed: %s", ident, strerror(errno));
	  safe_close(ctx, fd);
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
      DPL_LOG(ctx, DPL_ERROR, "poll failed: %s", strerror(errno));
      safe_close(ctx, fd);
      fd = -1;
      goto end;
    }

  if (0 == ret)
    {
      DPL_LOG(ctx, DPL_ERROR, "Timed out connecting to server %s after %d seconds",
	      ident, ctx->conn_timeout);
      safe_close(ctx, fd);
      fd = -1;
      goto end;
    }
  else if (!(fds.revents & POLLOUT))
    {
      DPL_LOG(ctx, DPL_ERROR, "poll returned strange results");
      safe_close(ctx, fd);
      fd = -1;
      goto end;
    }

  on = 0;
  ret = ioctl(fd, FIONBIO, &on);
  if (-1 == ret)
    {
      DPL_LOG(ctx, DPL_ERROR, "ioctl(FIONBIO) failed: %s", strerror(errno));
      safe_close(ctx, fd);
      fd = -1;
      goto end;
    }

  /* errors from the async connect() are reported through the SO_ERROR sockopt */

  errorlen = sizeof(error);
  error = 0;
  ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &errorlen);
  if (-1 == ret)
    {
      DPL_LOG(ctx, DPL_ERROR, "getsockopt(SO_ERROR) failed: %s", strerror(errno));
      safe_close(ctx, fd);
      fd = -1;
      goto end;
    }

  if (error != 0)
    {
      DPL_LOG(ctx, DPL_ERROR, "Connect to server %s failed: %s", ident, strerror(error));
      safe_close(ctx, fd);
      fd = -1;
      goto end;
    }

 end:

  DPL_TRACE(ctx, DPL_TRACE_CONN, "connect fd=%d", fd);

  return fd;
}

static int
init_ssl_conn(dpl_ctx_t *ctx, dpl_conn_t *conn)
{
  int ret;

  conn->ssl = SSL_new(ctx->ssl_ctx);
  if (conn->ssl == NULL)
    return 0;

  conn->bio = BIO_new_socket(conn->fd, BIO_NOCLOSE);
  if (conn->bio == NULL)
    return 0;

  SSL_set_bio(conn->ssl, conn->bio, conn->bio);

  ret = SSL_connect(conn->ssl);
  if (ret <= 0) {
    int ret_ssl = 0;

    SSL_get_error(conn->ssl, ret);

    DPL_SSL_PERROR(ctx, "SSL_connect");
    DPL_LOG(ctx, DPL_ERROR, "SSL connect error: %d: %d", ret, ret_ssl);

    ret_ssl = SSL_get_verify_result(conn->ssl);
    DPL_LOG(ctx, DPL_ERROR, "SSL certificate verification status: %ld: %s", ret_ssl, X509_verify_cert_error_string(ret_ssl));
    return 0;
  }
  if (0 == ctx->cert_verif) {
    long ret_ssl = 0;
    ret_ssl = SSL_get_verify_result(conn->ssl);
    DPL_TRACE(ctx, DPL_TRACE_SSL, "SSL certificate verification status: %ld: %s", ret_ssl, X509_verify_cert_error_string(ret_ssl));
  }
  DPL_TRACE(ctx, DPL_TRACE_SSL, "SSL cipher used: %s", SSL_get_cipher(conn->ssl));

  return 1;
}

/*
 * check an existing connection bound on (addr,port). if none is found
 * a new connection is created.
 */

static dpl_conn_t *
conn_open(dpl_ctx_t *ctx,
          struct hostent *host,
          u_short port)
{
  dpl_conn_t    *conn = NULL;
  time_t        now = time(0);
  char          ident[DPL_ADDR_IDENT_STRLEN];

  dpl_ctx_lock(ctx);

  dpl_addr_get_ident(host, port, ident, sizeof(ident));
  DPL_TRACE(ctx, DPL_TRACE_CONN, "conn_open %s", ident);

 again:

  conn = dpl_conn_get_nolock(ctx, host, port);

  if (NULL != conn)
    {
      if (0 == is_usable(conn))
        {
          dpl_conn_remove_nolock(ctx, conn);
          dpl_conn_terminate_nolock(conn);
          goto again;
        }

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
      DPL_TRACE(ctx, DPL_TRACE_ERR, "reaching limit %d", ctx->n_conn_fds);
      conn = NULL;
      goto end;
    }

  conn = malloc(sizeof (*conn));
  if (NULL == conn)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "malloc failed");
      conn = NULL;
      goto end;
    }

  DPL_TRACE(ctx, DPL_TRACE_CONN, "new_conn %s %p", ident, conn);

  memset(conn, 0, sizeof (*conn));

  conn->type = DPL_CONN_TYPE_HTTP;
  conn->ctx = ctx;
  conn->read_buf_size = ctx->read_buf_size;
  conn->fd = -1;

  if ((conn->read_buf = malloc(conn->read_buf_size)) == NULL)
    {
      dpl_conn_free(conn);
      conn = NULL;
      goto end;
    }

  memcpy(&conn->hash_info.addr, host->h_addr_list[0], host->h_length);
  conn->hash_info.port = port;

  conn->fd = do_connect(ctx, host, port);
  if (-1 == conn->fd)
    {
      dpl_conn_free(conn);
      conn = NULL;
      goto end;
    }

  conn->start_time = now;
  conn->n_hits = 0;

  if (ctx->use_https) {
    if (!init_ssl_conn(ctx, conn)) {
      dpl_conn_free(conn);
      conn = NULL;
      goto end;
    }
  }

  ctx->n_conn_fds++;

 end:

  dpl_ctx_unlock(ctx);

  DPL_TRACE(ctx, DPL_TRACE_CONN, "conn_open conn=%p", conn);

  return conn;
}

dpl_conn_t *
dpl_conn_open_host(dpl_ctx_t *ctx, int af,
                   const char *host,
                   const char *portstr)
{
  int                   ret2;
  struct hostent        hret, *hresult;
  char                  hbuf[1024];
  int                   herr = 0;
  u_short               port;
  dpl_conn_t            *conn = NULL;
  char                  *nstr;

  ret2 = dpl_gethostbyname2_r(host, af, &hret, hbuf, sizeof (hbuf), &hresult, &herr);
  if (0 != ret2 || hresult == NULL) {
    DPL_LOG(ctx, DPL_ERROR, "Failed to lookup hostname \"%s\": %s",
            host, hstrerror(herr));
    goto bad;
  }

  port = atoi(portstr);
  conn = conn_open(ctx, hresult, port);
  if (NULL == conn) {
    DPL_TRACE(ctx, DPL_TRACE_ERR, "connect failed");
    goto bad;
  }

  nstr = strdup(host);
  if (NULL == nstr)
    goto bad;

  if (NULL != conn->host)
    free(conn->host);

  conn->host = nstr;

  nstr = strdup(portstr);
  if (NULL == nstr)
    goto bad;

  if (NULL != conn->port)
    free(conn->port);

  conn->port = nstr;

  return conn;

 bad:

  if (NULL != conn)
    dpl_conn_release(conn);

  return NULL;
}

void
dpl_blacklist_host(dpl_ctx_t *ctx,
                   const char *host,
                   const char *portstr)
{
  DPL_TRACE(ctx, DPL_TRACE_CONN, "blacklisting %s:%s", host, portstr);

  (void) dpl_addrlist_blacklist(ctx->addrlist, host, portstr, ctx->blacklist_expiretime);
}

/**
 * Get a connection from the context.
 *
 * Creates or re-uses a connection suitable for use with the request
 * `req`.  The calling thread is guaranteed exclusive use of the
 * connection.  If a recently released connection is suitable, it will be
 * returned.
 *
 * If multiple hosts are specified in the `host` variable in
 * the profile, connections will be distributed between those hosts in
 * a round-robin manner.  Any failure while connecting will cause the
 * failing host to be blacklisted and the connection retried with
 * another host; if no hosts remain, `DPL_FAILURE` is returned.
 *
 * On success, a pointer to a connection is returned in `*connp`.  You
 * should release the connection by calling either `dpl_conn_release()` or
 * `dpl_conn_terminate()`.  On error the value in `*connp` is unchanged.
 *
 * @param ctx the context from which to create a connection
 * @param req the request for which this connection will be used
 * @param[out] connp used to return the new connection
 * @retval DPL_SUCCESS on success, or
 * @return a Droplet error code on failure
 */
dpl_status_t
dpl_try_connect(dpl_ctx_t *ctx,
                dpl_req_t *req,
                dpl_conn_t **connp)
{
  int           cur_host;
  dpl_addr_t    *addr;
  dpl_conn_t    *conn = NULL;
  dpl_status_t  ret, ret2;
  char          virtual_host[1024], *hostp = NULL;

 retry:
  pthread_mutex_lock(&ctx->lock);

  cur_host = ctx->cur_host;
  ++ctx->cur_host;

  pthread_mutex_unlock(&ctx->lock);

  ret2 = dpl_addrlist_get_nth(ctx->addrlist, cur_host, &addr);
  if (DPL_SUCCESS != ret2) {
    DPL_TRACE(ctx, DPL_TRACE_CONN, "no more host to contact, giving up");
    ret = DPL_FAILURE;
    goto end;
  }

  if (req->behavior_flags & DPL_BEHAVIOR_VIRTUAL_HOSTING) {
    snprintf(virtual_host, sizeof (virtual_host), "%s.%s", req->bucket, addr->host);
    hostp = virtual_host;
  } else
    hostp = addr->host;

  conn = dpl_conn_open_host(ctx, addr->h->h_addrtype, hostp, addr->portstr);
  if (NULL == conn) {
    if (req->behavior_flags & DPL_BEHAVIOR_VIRTUAL_HOSTING) {
      ret = DPL_FAILURE;
      goto end;
    } else {
      dpl_blacklist_host(ctx, addr->host, addr->portstr);
      goto retry;
    }
  }

  ret2 = dpl_req_set_host(req, hostp);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_req_set_port(req, addr->portstr);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret = DPL_SUCCESS;

  if (NULL != connp) {
    *connp = conn;
    conn = NULL; // consumed
  }

 end:

  if (NULL != conn)
    dpl_conn_terminate(conn);

  return ret;
}

/**
 * Release the connection after use.
 *
 * Releases a connection when you have finished using it.  The
 * connection cannot be used after calling `dpl_conn_release()`.
 * Note that `dpl_conn_release()` may choose to keep the connection
 * in an idle state for later re-use, i.e. the same connection
 * may be returned from a future call to `dpl_try_connect()`.
 * This means you should not call `dpl_conn_release()` if there has been
 * an error condition on the connection; instead you should call
 * `dpl_conn_terminate()`.
 *
 * @param conn connection to release
 */
void
dpl_conn_release(dpl_conn_t *conn)
{
  dpl_ctx_lock(conn->ctx);

  if (conn->type == DPL_CONN_TYPE_FILE)
    {
      if (-1 != conn->fd)
        close(conn->fd);
      dpl_ctx_unlock(conn->ctx);
      free(conn);
      return ;
    }

  DPL_TRACE(conn->ctx, DPL_TRACE_CONN, "conn_release conn=%p", conn);

  conn->close_time = time(0);
  dpl_conn_add_nolock(conn);

  dpl_ctx_unlock(conn->ctx);
}

/**
 * Release and immediately terminate a connection.
 *
 * Releases a connection when you have finished using it, with immediate
 * destruction of the underlying network socket.  Call this instead of
 * `dpl_conn_release()` if you have encountered any error conditions on
 * the connection.
 *
 * @param conn connection to release
 */
void
dpl_conn_terminate(dpl_conn_t *conn)
{
  dpl_ctx_t *ctx;

  DPRINTF("explicit termination n_hits=%d\n", conn->n_hits);

  ctx = conn->ctx;

  dpl_ctx_lock(ctx);

  assert(conn->type == DPL_CONN_TYPE_HTTP);
  dpl_conn_terminate_nolock(conn);

  dpl_ctx_unlock(ctx);
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

/*
 * Write an IO vector to a connection with retry and timeout
 *
 * @note: modifies the iov in place
 *
 * @param timeout in secs or -1
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

/*
 * Write an IO vector to a connection via the SSL library with retry
 *
 * Note the default semantics of SSL_write() are to handle partial
 * writes internally, so we don't need to do it ourselves.
 *
 * Timeout is ignored, the SSL library doesn't implement a per-write
 * timeout.  It has a session timeout, but that's a different beast
 * and not helpful to us.
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
  if (total_size == 0)
    return DPL_FAILURE;
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
      DPL_SSL_PERROR(conn->ctx, "SSL_write");
      free(ptr);
      return DPL_FAILURE;
    }

  free(ptr);
  return DPL_SUCCESS;
}

/**
 * Write an IO vector to the connection.
 *
 * Writes an IO vector to the connection.  If the `use_https` variable
 * in the profile is `true`, the data will be written via SSL.  All the
 * data is written, without partial writes.  The `iov` structure may be
 * modified.  Note that `timeout` is not implemented for SSL, due to a
 * limitation of  the SSL library.
 *
 * @param conn the connection to write to
 * @param iov IO vector which points to data to write
 * @param length of the IO vector
 * @param timeout per-write timeout in seconds or -1 for no timeout
 * @retval DPL_SUCCESS on success, or
 * @return a Droplet error code on failure
 */
dpl_status_t
dpl_conn_writev_all(dpl_conn_t *conn,
                    struct iovec *iov,
                    int n_iov,
                    int timeout)
{
  dpl_status_t ret;

  DPL_TRACE(conn->ctx, DPL_TRACE_IO, "writev conn=%p https=%d size=%ld", conn, conn->ctx->use_https, dpl_iov_size(iov, n_iov));

  if (conn->ctx->trace_buffers)
    dpl_iov_dump(iov, n_iov, dpl_iov_size(iov, n_iov), conn->ctx->trace_binary);

  if (0 == conn->ctx->use_https)
    ret = writev_all_plaintext(conn, iov, n_iov, timeout);
  else
    ret = writev_all_ssl(conn, iov, n_iov, timeout);

  if (DPL_SUCCESS != ret)
    {
      //blacklist host
      if (DPL_CONN_TYPE_HTTP == conn->type)
        dpl_blacklist_host(conn->ctx, conn->host, conn->port);
    }

  return ret;
}

/**
 * Create a connection to a local file
 *
 * This function is used by the `posix` backend to create a connection
 * which reads from or writes to an open local file.  Call `dpl_conn_release()`
 * to release the connection when you have finished using it.
 *
 * @param ctx the context from which to create a connection
 * @param fd an open file descriptor for a local file
 * @return a new context, or NULL on failure
 */
dpl_conn_t *
dpl_conn_open_file(dpl_ctx_t *ctx,
                   int fd)
{
  dpl_conn_t *conn = NULL;
  time_t now = time(0);

  DPL_TRACE(ctx, DPL_TRACE_CONN, "conn_open_file fd=%d", fd);

  DPRINTF("allocate new conn\n");

  conn = malloc(sizeof (*conn));
  if (NULL == conn)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "malloc failed");
      conn = NULL;
      goto end;
    }

  memset(conn, 0, sizeof (*conn));

  conn->type = DPL_CONN_TYPE_FILE;
  conn->ctx = ctx;
  conn->read_buf_size = ctx->read_buf_size;
  conn->fd = fd;

  if ((conn->read_buf = malloc(conn->read_buf_size)) == NULL)
    {
      dpl_conn_free(conn);
      conn = NULL;
      goto end;
    }

  conn->start_time = now;
  conn->n_hits = 0;

 end:

  DPL_TRACE(ctx, DPL_TRACE_CONN, "conn_open conn=%p", conn);

  return conn;
}

/* @} */
