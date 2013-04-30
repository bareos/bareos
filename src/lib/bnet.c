/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Network Utility Routines
 *
 *  by Kern Sibbald
 *
 * Adapted and enhanced for Bacula, originally written
 * for inclusion in the Apcupsd package
 *
 */

#include "bacula.h"
#include "jcr.h"
#include <netdb.h>

#ifndef   INADDR_NONE
#define   INADDR_NONE    -1
#endif

#ifdef HAVE_WIN32
#define socketRead(fd, buf, len)  recv(fd, buf, len, 0)
#define socketWrite(fd, buf, len) send(fd, buf, len, 0)
#define socketClose(fd)           closesocket(fd)
#else
#define socketRead(fd, buf, len)  read(fd, buf, len)
#define socketWrite(fd, buf, len) write(fd, buf, len)
#define socketClose(fd)           close(fd)
#endif

#ifndef HAVE_GETADDRINFO
static pthread_mutex_t ip_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * Read a nbytes from the network.
 * It is possible that the total bytes require in several
 * read requests
 */
int32_t read_nbytes(BSOCK * bsock, char *ptr, int32_t nbytes)
{
   int32_t nleft, nread;

#ifdef HAVE_TLS
   if (bsock->tls) {
      /* TLS enabled */
      return (tls_bsock_readn(bsock, ptr, nbytes));
   }
#endif /* HAVE_TLS */

   nleft = nbytes;
   while (nleft > 0) {
      errno = 0;
      nread = socketRead(bsock->m_fd, ptr, nleft);
      if (bsock->is_timed_out() || bsock->is_terminated()) {
         return -1;
      }

#ifdef HAVE_WIN32
      /*
       * For Windows, we must simulate Unix errno on a socket
       *  error in order to handle errors correctly.
       */
      if (nread == SOCKET_ERROR) {
        DWORD err = WSAGetLastError();
        nread = -1;
        if (err == WSAEINTR) {
           errno = EINTR;
        } else if (err == WSAEWOULDBLOCK) {
           errno = EAGAIN;
        } else {
           errno = EIO;            /* some other error */
        }
     }
#endif

      if (nread == -1) {
         if (errno == EINTR) {
            continue;
         }
         if (errno == EAGAIN) {
            bmicrosleep(0, 20000);  /* try again in 20ms */
            continue;
         }
      }
      if (nread <= 0) {
         return -1;                /* error, or EOF */
      }
      nleft -= nread;
      ptr += nread;
   }
   return nbytes - nleft;          /* return >= 0 */
}

/*
 * Write nbytes to the network.
 * It may require several writes.
 */

int32_t write_nbytes(BSOCK * bsock, char *ptr, int32_t nbytes)
{
   int32_t nleft, nwritten;

   if (bsock->is_spooling()) {
      nwritten = fwrite(ptr, 1, nbytes, bsock->m_spool_fd);
      if (nwritten != nbytes) {
         berrno be;
         bsock->b_errno = errno;
         Qmsg1(bsock->jcr(), M_FATAL, 0, _("Attr spool write error. ERR=%s\n"),
               be.bstrerror());
         Dmsg2(400, "nwritten=%d nbytes=%d.\n", nwritten, nbytes);
         errno = bsock->b_errno;
         return -1;
      }
      return nbytes;
   }

#ifdef HAVE_TLS
   if (bsock->tls) {
      /* TLS enabled */
      return (tls_bsock_writen(bsock, ptr, nbytes));
   }
#endif /* HAVE_TLS */

   nleft = nbytes;
   while (nleft > 0) {
      do {
         errno = 0;
         nwritten = socketWrite(bsock->m_fd, ptr, nleft);
         if (bsock->is_timed_out() || bsock->is_terminated()) {
            return -1;
         }

#ifdef HAVE_WIN32
         /*
          * For Windows, we must simulate Unix errno on a socket
          *  error in order to handle errors correctly.
          */
         if (nwritten == SOCKET_ERROR) {
            DWORD err = WSAGetLastError();
            nwritten = -1;
            if (err == WSAEINTR) {
               errno = EINTR;
            } else if (err == WSAEWOULDBLOCK) {
               errno = EAGAIN;
            } else {
               errno = EIO;        /* some other error */
            }
         }
#endif

      } while (nwritten == -1 && errno == EINTR);
      /*
       * If connection is non-blocking, we will get EAGAIN, so
       * use select() to keep from consuming all the CPU
       * and try again.
       */
      if (nwritten == -1 && errno == EAGAIN) {
         fd_set fdset;
         struct timeval tv;

         FD_ZERO(&fdset);
         FD_SET((unsigned)bsock->m_fd, &fdset);
         tv.tv_sec = 1;
         tv.tv_usec = 0;
         select(bsock->m_fd + 1, NULL, &fdset, NULL, &tv);
         continue;
      }
      if (nwritten <= 0) {
         return -1;                /* error */
      }
      nleft -= nwritten;
      ptr += nwritten;
   }
   return nbytes - nleft;
}

/*
 * Receive a message from the other end. Each message consists of
 * two packets. The first is a header that contains the size
 * of the data that follows in the second packet.
 * Returns number of bytes read (may return zero)
 * Returns -1 on signal (BNET_SIGNAL)
 * Returns -2 on hard end of file (BNET_HARDEOF)
 * Returns -3 on error  (BNET_ERROR)
 *
 *  Unfortunately, it is a bit complicated because we have these
 *    four return types:
 *    1. Normal data
 *    2. Signal including end of data stream
 *    3. Hard end of file
 *    4. Error
 *  Using is_bnet_stop() and is_bnet_error() you can figure this all out.
 */
int32_t bnet_recv(BSOCK * bsock)
{
   return bsock->recv();
}


/*
 * Return 1 if there are errors on this bsock or it is closed,
 *   i.e. stop communicating on this line.
 */
bool is_bnet_stop(BSOCK * bsock)
{
   return bsock->is_stop();
}

/*
 * Return number of errors on socket
 */
int is_bnet_error(BSOCK * bsock)
{
   return bsock->is_error();
}

/*
 * Call here after error during closing to suppress error
 *  messages which are due to the other end shutting down too.
 */
void bnet_suppress_error_messages(BSOCK * bsock, bool flag)
{
   bsock->m_suppress_error_msgs = flag;
}

/*
 * Send a message over the network. The send consists of
 * two network packets. The first is sends a 32 bit integer containing
 * the length of the data packet which follows.
 *
 * Returns: false on failure
 *          true  on success
 */
bool bnet_send(BSOCK *bsock)
{
   return bsock->send();
}


/*
 * Establish a TLS connection -- server side
 *  Returns: true  on success
 *           false on failure
 */
#ifdef HAVE_TLS
bool bnet_tls_server(TLS_CONTEXT *ctx, BSOCK * bsock, alist *verify_list)
{
   TLS_CONNECTION *tls;
   JCR *jcr = bsock->jcr();
   
   tls = new_tls_connection(ctx, bsock->m_fd);
   if (!tls) {
      Qmsg0(bsock->jcr(), M_FATAL, 0, _("TLS connection initialization failed.\n"));
      return false;
   }

   bsock->tls = tls;

   /* Initiate TLS Negotiation */
   if (!tls_bsock_accept(bsock)) {
      Qmsg0(bsock->jcr(), M_FATAL, 0, _("TLS Negotiation failed.\n"));
      goto err;
   }

   if (verify_list) {
      if (!tls_postconnect_verify_cn(jcr, tls, verify_list)) {
         Qmsg1(bsock->jcr(), M_FATAL, 0, _("TLS certificate verification failed."
                                         " Peer certificate did not match a required commonName\n"),
                                         bsock->host());
         goto err;
      }
   }
   Dmsg0(50, "TLS server negotiation established.\n");
   return true;

err:
   free_tls_connection(tls);
   bsock->tls = NULL;
   return false;
}

/*
 * Establish a TLS connection -- client side
 * Returns: true  on success
 *          false on failure
 */
bool bnet_tls_client(TLS_CONTEXT *ctx, BSOCK * bsock, alist *verify_list)
{
   TLS_CONNECTION *tls;
   JCR *jcr = bsock->jcr();

   tls  = new_tls_connection(ctx, bsock->m_fd);
   if (!tls) {
      Qmsg0(bsock->jcr(), M_FATAL, 0, _("TLS connection initialization failed.\n"));
      return false;
   }

   bsock->tls = tls;

   /* Initiate TLS Negotiation */
   if (!tls_bsock_connect(bsock)) {
      goto err;
   }

   /* If there's an Allowed CN verify list, use that to validate the remote
    * certificate's CN. Otherwise, we use standard host/CN matching. */
   if (verify_list) {
      if (!tls_postconnect_verify_cn(jcr, tls, verify_list)) {
         Qmsg1(bsock->jcr(), M_FATAL, 0, _("TLS certificate verification failed."
                                         " Peer certificate did not match a required commonName\n"),
                                         bsock->host());
         goto err;
      }
   } else {
      if (!tls_postconnect_verify_host(jcr, tls, bsock->host())) {
         Qmsg1(bsock->jcr(), M_FATAL, 0, _("TLS host certificate verification failed. Host name \"%s\" did not match presented certificate\n"), 
               bsock->host());
         goto err;
      }
   }
   Dmsg0(50, "TLS client negotiation established.\n");
   return true;

err:
   free_tls_connection(tls);
   bsock->tls = NULL;
   return false;
}
#else

bool bnet_tls_server(TLS_CONTEXT *ctx, BSOCK * bsock, alist *verify_list)
{
   Jmsg(bsock->jcr(), M_ABORT, 0, _("TLS enabled but not configured.\n"));
   return false;
}

bool bnet_tls_client(TLS_CONTEXT *ctx, BSOCK * bsock, alist *verify_list)
{
   Jmsg(bsock->jcr(), M_ABORT, 0, _("TLS enable but not configured.\n"));
   return false;
}

#endif /* HAVE_TLS */

/*
 * Wait for a specified time for data to appear on
 * the BSOCK connection.
 *
 *   Returns: 1 if data available
 *            0 if timeout
 *           -1 if error
 */
int bnet_wait_data(BSOCK * bsock, int sec)
{
   return bsock->wait_data(sec);
}

/*
 * As above, but returns on interrupt
 */
int bnet_wait_data_intr(BSOCK * bsock, int sec)
{
   return bsock->wait_data_intr(sec);
}

#ifndef NETDB_INTERNAL
#define NETDB_INTERNAL  -1         /* See errno. */
#endif
#ifndef NETDB_SUCCESS
#define NETDB_SUCCESS   0          /* No problem. */
#endif
#ifndef HOST_NOT_FOUND
#define HOST_NOT_FOUND  1          /* Authoritative Answer Host not found. */
#endif
#ifndef TRY_AGAIN
#define TRY_AGAIN       2          /* Non-Authoritative Host not found, or SERVERFAIL. */
#endif
#ifndef NO_RECOVERY
#define NO_RECOVERY     3          /* Non recoverable errors, FORMERR, REFUSED, NOTIMP. */
#endif
#ifndef NO_DATA
#define NO_DATA         4          /* Valid name, no data record of requested type. */
#endif

#if HAVE_GETADDRINFO
const char *resolv_host(int family, const char *host, dlist *addr_list)
{
   int res;
   struct addrinfo hints;
   struct addrinfo *ai, *rp;
   IPADDR *addr;

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = family;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_flags = 0;

   res = getaddrinfo(host, NULL, &hints, &ai);
   if (res != 0) {
      return gai_strerror(res);
   }

   for (rp = ai; rp != NULL; rp = rp->ai_next) {
      switch (rp->ai_addr->sa_family) {
      case AF_INET:
         addr = New(IPADDR(rp->ai_addr->sa_family));
         addr->set_type(IPADDR::R_MULTIPLE);
         /*
          * Some serious casting to get the struct in_addr *
          * rp->ai_addr == struct sockaddr
          * as this is AF_INET family we can cast that
          * to struct_sockaddr_in. Of that we need the
          * address of the sin_addr member which contains a
          * struct in_addr
          */
         addr->set_addr4(&(((struct sockaddr_in *)rp->ai_addr)->sin_addr));
         break;
#ifdef HAVE_IPV6
      case AF_INET6:
         addr = New(IPADDR(rp->ai_addr->sa_family));
         addr->set_type(IPADDR::R_MULTIPLE);
         /*
          * Some serious casting to get the struct in6_addr *
          * rp->ai_addr == struct sockaddr
          * as this is AF_INET6 family we can cast that
          * to struct_sockaddr_in6. Of that we need the
          * address of the sin6_addr member which contains a
          * struct in6_addr
          */
         addr->set_addr6(&(((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr));
         break;
#endif
      default:
         continue;
      }
      addr_list->append(addr);
   }
   freeaddrinfo(ai);
   return NULL;
}
#else
/*
 * Get human readable error for gethostbyname()
 */
static const char *gethost_strerror()
{
   const char *msg;
   berrno be;
   switch (h_errno) {
   case NETDB_INTERNAL:
      msg = be.bstrerror();
      break;
   case NETDB_SUCCESS:
      msg = _("No problem.");
      break;
   case HOST_NOT_FOUND:
      msg = _("Authoritative answer for host not found.");
      break;
   case TRY_AGAIN:
      msg = _("Non-authoritative for host not found, or ServerFail.");
      break;
   case NO_RECOVERY:
      msg = _("Non-recoverable errors, FORMERR, REFUSED, or NOTIMP.");
      break;
   case NO_DATA:
      msg = _("Valid name, no data record of resquested type.");
      break;
   default:
      msg = _("Unknown error.");
   }
   return msg;
}

static const char *resolv_host(int family, const char *host, dlist *addr_list)
{
   struct hostent *hp;
   const char *errmsg;
   char **p;
   IPADDR *addr;

   P(ip_mutex);                       /* gethostbyname() is not thread safe */
#ifdef HAVE_GETHOSTBYNAME2
   if ((hp = gethostbyname2(host, family)) == NULL) {
#else
   if ((hp = gethostbyname(host)) == NULL) {
#endif
      /* may be the strerror give not the right result -:( */
      errmsg = gethost_strerror();
      V(ip_mutex);
      return errmsg;
   } else {
      for (p = hp->h_addr_list; *p != 0; p++) {
         switch (hp->h_addrtype) {
         case AF_INET:
            addr = New(IPADDR(hp->h_addrtype));
            addr->set_type(IPADDR::R_MULTIPLE);
            addr->set_addr4((struct in_addr *)*p);
            break;
#ifdef HAVE_IPV6
          case AF_INET6:
            addr = New(IPADDR(hp->h_addrtype));
            addr->set_type(IPADDR::R_MULTIPLE);
            addr->set_addr6((struct in6_addr *)*p);
            break;
#endif
         default:
            continue;
         }
         addr_list->append(addr);
      }
      V(ip_mutex);
   }
   return NULL;
}
#endif

static IPADDR *add_any(int family)
{
   IPADDR *addr = New(IPADDR(family));
   addr->set_type(IPADDR::R_MULTIPLE);
   addr->set_addr_any();
   return addr;
}

/*
 * i host = 0 mean INADDR_ANY only ipv4
 */
dlist *bnet_host2ipaddrs(const char *host, int family, const char **errstr)
{
   struct in_addr inaddr;
   IPADDR *addr = 0;
   const char *errmsg;
#ifdef HAVE_IPV6
   struct in6_addr inaddr6;
#endif

   dlist *addr_list = New(dlist(addr, &addr->link));
   if (!host || host[0] == '\0') {
      if (family != 0) {
         addr_list->append(add_any(family));
      } else {
         addr_list->append(add_any(AF_INET));
#ifdef HAVE_IPV6
         addr_list->append(add_any(AF_INET6));
#endif
      }
   } else if (inet_aton(host, &inaddr)) { /* MA Bug 4 */
      addr = New(IPADDR(AF_INET));
      addr->set_type(IPADDR::R_MULTIPLE);
      addr->set_addr4(&inaddr);
      addr_list->append(addr);
#ifdef HAVE_IPV6
   } else if (inet_pton(AF_INET6, host, &inaddr6) == 1) {
      addr = New(IPADDR(AF_INET6));
      addr->set_type(IPADDR::R_MULTIPLE);
      addr->set_addr6(&inaddr6);
      addr_list->append(addr);
#endif
   } else {
      if (family != 0) {
         errmsg = resolv_host(family, host, addr_list);
         if (errmsg) {
            *errstr = errmsg;
            free_addresses(addr_list);
            return 0;
         }
      } else {
#ifdef HAVE_IPV6
         /* We try to resolv host for ipv6 and ipv4, the connection procedure
          * will try to reach the host for each protocols. We report only "Host
          * not found" ipv4 message (no need to have ipv6 and ipv4 messages).
          */
         resolv_host(AF_INET6, host, addr_list);
#endif
         errmsg = resolv_host(AF_INET, host, addr_list);

         if (addr_list->size() == 0) {
            *errstr = errmsg;
            free_addresses(addr_list);
            return 0;
         }
      }
   }
   return addr_list;
}

/*
 * This is the "old" way of opening a connection.  The preferred way is
 *   now to do what this subroutine does, but inline. That allows the 
 *   connect() call to return error status, ...
 */      
BSOCK *bnet_connect(JCR * jcr, int retry_interval, utime_t max_retry_time,
                    utime_t heart_beat,
                    const char *name, char *host, char *service, int port,
                    int verbose)
{
   BSOCK *bsock = new_bsock();
   if (!bsock->connect(jcr, retry_interval, max_retry_time, heart_beat,
                       name, host, service, port, verbose)) {
       bsock->destroy();
       bsock = NULL;
   }
   return bsock;
}

/*
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
const char *bnet_strerror(BSOCK * bsock)
{
   return bsock->bstrerror();
}

/*
 * Format and send a message
 *  Returns: false on error
 *           true  on success
 */
bool bnet_fsend(BSOCK * bs, const char *fmt, ...)
{
   va_list arg_ptr;
   int maxlen;

   if (bs->errors || bs->is_terminated()) {
      return false;
   }
   /* This probably won't work, but we vsnprintf, then if we
    * get a negative length or a length greater than our buffer
    * (depending on which library is used), the printf was truncated, so
    * get a bigger buffer and try again.
    */
   for (;;) {
      maxlen = sizeof_pool_memory(bs->msg) - 1;
      va_start(arg_ptr, fmt);
      bs->msglen = bvsnprintf(bs->msg, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (bs->msglen > 0 && bs->msglen < (maxlen - 5)) {
         break;
      }
      bs->msg = realloc_pool_memory(bs->msg, maxlen + maxlen / 2);
   }
   return bs->send();
}

int bnet_get_peer(BSOCK *bs, char *buf, socklen_t buflen) 
{
   return bs->get_peer(buf, buflen);  
}

/*
 * Set the network buffer size, suggested size is in size.
 *  Actual size obtained is returned in bs->msglen
 *
 *  Returns: 0 on failure
 *           1 on success
 */
bool bnet_set_buffer_size(BSOCK * bs, uint32_t size, int rw)
{
   return bs->set_buffer_size(size, rw);
}

/*
 * Set socket non-blocking
 * Returns previous socket flag
 */
int bnet_set_nonblocking(BSOCK *bsock) 
{
   return bsock->set_nonblocking();
}

/*
 * Set socket blocking
 * Returns previous socket flags
 */
int bnet_set_blocking(BSOCK *bsock) 
{
   return bsock->set_blocking();
}

/*
 * Restores socket flags
 */
void bnet_restore_blocking (BSOCK *bsock, int flags) 
{
   bsock->restore_blocking(flags);
}

/*
 * Send a network "signal" to the other end
 *  This consists of sending a negative packet length
 *
 *  Returns: false on failure
 *           true  on success
 */
bool bnet_sig(BSOCK * bs, int signal)
{
   return bs->signal(signal);
}

/*
 * Convert a network "signal" code into
 * human readable ASCII.
 */
const char *bnet_sig_to_ascii(BSOCK * bs)
{
   static char buf[30];
   switch (bs->msglen) {
   case BNET_EOD:
      return "BNET_EOD";           /* end of data stream */
   case BNET_EOD_POLL:
      return "BNET_EOD_POLL";
   case BNET_STATUS:
      return "BNET_STATUS";
   case BNET_TERMINATE:
      return "BNET_TERMINATE";     /* terminate connection */
   case BNET_POLL:
      return "BNET_POLL";
   case BNET_HEARTBEAT:
      return "BNET_HEARTBEAT";
   case BNET_HB_RESPONSE:
      return "BNET_HB_RESPONSE";
   case BNET_SUB_PROMPT:
      return "BNET_SUB_PROMPT";
   case BNET_TEXT_INPUT:
      return "BNET_TEXT_INPUT";
   default:
      sprintf(buf, _("Unknown sig %d"), (int)bs->msglen);
      return buf;
   }
}

/* Initialize internal socket structure.
 *  This probably should be done in net_open
 */
BSOCK *init_bsock(JCR * jcr, int sockfd, const char *who, const char *host, int port,
                  struct sockaddr *client_addr)
{
   Dmsg3(100, "who=%s host=%s port=%d\n", who, host, port);
   BSOCK *bsock = (BSOCK *)malloc(sizeof(BSOCK));
   memset(bsock, 0, sizeof(BSOCK));
   bsock->m_fd = sockfd;
   bsock->tls = NULL;
   bsock->errors = 0;
   bsock->m_blocking = 1;
   bsock->msg = get_pool_memory(PM_BSOCK);
   bsock->errmsg = get_pool_memory(PM_MESSAGE);
   bsock->set_who(bstrdup(who));
   bsock->set_host(bstrdup(host));
   bsock->set_port(port);
   memset(&bsock->peer_addr, 0, sizeof(bsock->peer_addr));
   memcpy(&bsock->client_addr, client_addr, sizeof(bsock->client_addr));
   /*
    * ****FIXME**** reduce this to a few hours once
    *   heartbeats are implemented
    */
   bsock->timeout = 60 * 60 * 6 * 24;   /* 6 days timeout */
   bsock->set_jcr(jcr);
   return bsock;
}

BSOCK *dup_bsock(BSOCK *osock)
{
   BSOCK *bsock = (BSOCK *)malloc(sizeof(BSOCK));
   memcpy(bsock, osock, sizeof(BSOCK));
   bsock->msg = get_pool_memory(PM_BSOCK);
   bsock->errmsg = get_pool_memory(PM_MESSAGE);
   if (osock->who()) {
      bsock->set_who(bstrdup(osock->who()));
   }
   if (osock->host()) {
      bsock->set_host(bstrdup(osock->host()));
   }
   if (osock->src_addr) {
      bsock->src_addr = New( IPADDR( *(osock->src_addr)) );
   }
   bsock->set_duped();
   return bsock;
}

/* Close the network connection */
void bnet_close(BSOCK * bsock)
{
   bsock->close();
}

void term_bsock(BSOCK * bsock)
{
   bsock->destroy();
}
