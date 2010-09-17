/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
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

#include "dropletsp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

//#define DEBUG

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

