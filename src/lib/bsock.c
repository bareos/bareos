/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.

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
 */

#include "bacula.h"
#include "jcr.h"
#include <netdb.h>
#include <netinet/tcp.h>

#ifndef ENODATA                    /* not defined on BSD systems */
#define ENODATA EPIPE
#endif

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#ifdef HAVE_WIN32
#define socketRead(fd, buf, len)  ::recv(fd, buf, len, 0)
#define socketWrite(fd, buf, len) ::send(fd, buf, len, 0)
#define socketClose(fd)           ::closesocket(fd)
#else
#define socketRead(fd, buf, len)  ::read(fd, buf, len)
#define socketWrite(fd, buf, len) ::write(fd, buf, len)
#define socketClose(fd)           ::close(fd)
#endif

/*
 * This is a non-class BSOCK "constructor"  because we want to 
 *   call the Bacula smartalloc routines instead of new.
 */
BSOCK *new_bsock()
{
   BSOCK *bsock = (BSOCK *)malloc(sizeof(BSOCK));
   bsock->init();
   return bsock;
}

void BSOCK::init()
{
   memset(this, 0, sizeof(BSOCK));
   m_blocking = 1;
   msg = get_pool_memory(PM_BSOCK);
   errmsg = get_pool_memory(PM_MESSAGE);
   /*
    * ****FIXME**** reduce this to a few hours once
    *   heartbeats are implemented
    */
   timeout = 60 * 60 * 6 * 24;   /* 6 days timeout */
}

/*
 * This is our "class destructor" that ensures that we use
 *   smartalloc rather than the system free().
 */
void BSOCK::free_bsock()
{
   destroy();
}

void BSOCK::free_tls()
{
   free_tls_connection(this->tls);
   this->tls = NULL;
}   

/*
 * Try to connect to host for max_retry_time at retry_time intervals.
 *   Note, you must have called the constructor prior to calling
 *   this routine.
 */
bool BSOCK::connect(JCR * jcr, int retry_interval, utime_t max_retry_time,
                    utime_t heart_beat,
                    const char *name, char *host, char *service, int port,
                    int verbose)
{
   bool ok = false;
   int i;
   int fatal = 0;
   time_t begin_time = time(NULL);
   time_t now;
   btimer_t *tid = NULL;

   /* Try to trap out of OS call when time expires */
   if (max_retry_time) {
      tid = start_thread_timer(jcr, pthread_self(), (uint32_t)max_retry_time);
   }
   
   for (i = 0; !open(jcr, name, host, service, port, heart_beat, &fatal);
        i -= retry_interval) {
      berrno be;
      if (fatal || (jcr && job_canceled(jcr))) {
         goto bail_out;
      }
      Dmsg4(100, "Unable to connect to %s on %s:%d. ERR=%s\n",
            name, host, port, be.bstrerror());
      if (i < 0) {
         i = 60 * 5;               /* complain again in 5 minutes */
         if (verbose)
            Qmsg4(jcr, M_WARNING, 0, _(
               "Could not connect to %s on %s:%d. ERR=%s\n"
               "Retrying ...\n"), name, host, port, be.bstrerror());
      }
      bmicrosleep(retry_interval, 0);
      now = time(NULL);
      if (begin_time + max_retry_time <= now) {
         Qmsg4(jcr, M_FATAL, 0, _("Unable to connect to %s on %s:%d. ERR=%s\n"),
               name, host, port, be.bstrerror());
         goto bail_out;
      }
   }
   ok = true;

bail_out:
   if (tid) {
      stop_thread_timer(tid);
   }
   return ok;
}

/*       
 * Finish initialization of the pocket structure.
 */
void BSOCK::fin_init(JCR * jcr, int sockfd, const char *who, const char *host, int port,
                     struct sockaddr *lclient_addr)
{
   Dmsg3(100, "who=%s host=%s port=%d\n", who, host, port);
   m_fd = sockfd;
   set_who(bstrdup(who));
   set_host(bstrdup(host));
   set_port(port);
   memcpy(&client_addr, lclient_addr, sizeof(client_addr));
   set_jcr(jcr);
}

/*
 * Copy the address from the configuration dlist that gets passed in
 */
void BSOCK::set_source_address(dlist *src_addr_list)
{
   IPADDR *addr = NULL;

   // delete the object we already have, if it's allocated
   if (src_addr) {
     free( src_addr);
     src_addr = NULL;
   }

   if (src_addr_list) {
     addr = (IPADDR*) src_addr_list->first();
     src_addr = New( IPADDR(*addr));
   }
}

/*
 * Open a TCP connection to the server
 * Returns NULL
 * Returns BSOCK * pointer on success
 */
bool BSOCK::open(JCR *jcr, const char *name, char *host, char *service,
                 int port, utime_t heart_beat, int *fatal)
{
   int sockfd = -1;
   dlist *addr_list;
   IPADDR *ipaddr, *next;
   bool connected = false;
   int turnon = 1;
   const char *errstr;
   int save_errno = 0;

   /*
    * Fill in the structure serv_addr with the address of
    * the server that we want to connect with.
    */
   if ((addr_list = bnet_host2ipaddrs(host, 0, &errstr)) == NULL) {
      /* Note errstr is not malloc'ed */
      Qmsg2(jcr, M_ERROR, 0, _("bnet_host2ipaddrs() for host \"%s\" failed: ERR=%s\n"),
            host, errstr);
      Dmsg2(100, "bnet_host2ipaddrs() for host %s failed: ERR=%s\n",
            host, errstr);
      *fatal = 1;
      return false;
   }

   /*
    * Remove any duplicate addresses.
    */
   for (ipaddr = (IPADDR *)addr_list->first();
        ipaddr; ipaddr = (IPADDR *)addr_list->next(ipaddr)) {
      for (next = (IPADDR *)addr_list->next(ipaddr); next;
           next = (IPADDR *)addr_list->next(next)) {
         if (ipaddr->get_sockaddr_len() == next->get_sockaddr_len() &&
             memcmp(ipaddr->get_sockaddr(), next->get_sockaddr(),
                    ipaddr->get_sockaddr_len()) == 0) {
            addr_list->remove(next);
         }
      }
   }

   foreach_dlist(ipaddr, addr_list) {
      ipaddr->set_port_net(htons(port));
      char allbuf[256 * 10];
      char curbuf[256];
      Dmsg2(100, "Current %sAll %s\n",
                   ipaddr->build_address_str(curbuf, sizeof(curbuf)),
                   build_addresses_str(addr_list, allbuf, sizeof(allbuf)));
      /* Open a TCP socket */
      if ((sockfd = socket(ipaddr->get_family(), SOCK_STREAM, 0)) < 0) {
         berrno be;
         save_errno = errno;
         switch (errno) {
#ifdef EPFNOSUPPORT
         case EPFNOSUPPORT:
            /*
             * The name lookup of the host returned an address in a protocol family
             * we don't support. Suppress the error and try the next address.
             */
            break;
#endif
#ifdef EAFNOSUPPORT
         case EAFNOSUPPORT:
            /*
             * The name lookup of the host returned an address in a address family
             * we don't support. Suppress the error and try the next address.
             */
            break;
#endif
         default:
            *fatal = 1;
            Pmsg3(000, _("Socket open error. proto=%d port=%d. ERR=%s\n"),
               ipaddr->get_family(), ipaddr->get_port_host_order(), be.bstrerror());
            break;
         }
         continue;
      }

      /* Bind to the source address if it is set */
      if (src_addr) {
         if (bind(sockfd, src_addr->get_sockaddr(), src_addr->get_sockaddr_len()) < 0) {
            berrno be;
            save_errno = errno;
            *fatal = 1;
            Pmsg2(000, _("Source address bind error. proto=%d. ERR=%s\n"),
                  src_addr->get_family(), be.bstrerror() );
            continue;
         }
      }

      /*
       * Keep socket from timing out from inactivity
       */
      if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_val_t)&turnon, sizeof(turnon)) < 0) {
         berrno be;
         Qmsg1(jcr, M_WARNING, 0, _("Cannot set SO_KEEPALIVE on socket: %s\n"),
               be.bstrerror());
      }
#if defined(TCP_KEEPIDLE)
      if (heart_beat) {
         int opt = heart_beat;
         if (setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (sockopt_val_t)&opt, sizeof(opt)) < 0) {
            berrno be;
            Qmsg1(jcr, M_WARNING, 0, _("Cannot set TCP_KEEPIDLE on socket: %s\n"),
                  be.bstrerror());
         }
      }
#endif

      /* connect to server */
      if (::connect(sockfd, ipaddr->get_sockaddr(), ipaddr->get_sockaddr_len()) < 0) {
         save_errno = errno;
         socketClose(sockfd);
         continue;
      }
      *fatal = 0;
      connected = true;
      break;
   }

   if (!connected) {
      free_addresses(addr_list);
      errno = save_errno | b_errno_win32;
      return false;
   }
   /*
    * Keep socket from timing out from inactivity
    *   Do this a second time out of paranoia
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_val_t)&turnon, sizeof(turnon)) < 0) {
      berrno be;
      Qmsg1(jcr, M_WARNING, 0, _("Cannot set SO_KEEPALIVE on socket: %s\n"),
            be.bstrerror());
   }
   fin_init(jcr, sockfd, name, host, port, ipaddr->get_sockaddr());
   free_addresses(addr_list);
   return true;
}

/*
 * Force read/write to use locking
 */
bool BSOCK::set_locking()
{
   int stat;
   if (m_use_locking) {
      return true;                      /* already set */
   }
   if ((stat = pthread_mutex_init(&m_mutex, NULL)) != 0) {
      berrno be;
      Qmsg(m_jcr, M_FATAL, 0, _("Could not init bsock mutex. ERR=%s\n"),
         be.bstrerror(stat));
      return false;
   }
   m_use_locking = true;
   return true;
}

void BSOCK::clear_locking()
{
   if (!m_use_locking) {
      return;
   }
   m_use_locking = false;
   pthread_mutex_destroy(&m_mutex);
   return;
}

/*
 * Send a message over the network. The send consists of
 * two network packets. The first is sends a 32 bit integer containing
 * the length of the data packet which follows.
 *
 * Returns: false on failure
 *          true  on success
 */
bool BSOCK::send()
{
   int32_t rc;
   int32_t pktsiz;
   int32_t *hdr;
   bool ok = true;

   if (errors) {
      if (!m_suppress_error_msgs) {
         Qmsg4(m_jcr, M_ERROR, 0,  _("Socket has errors=%d on call to %s:%s:%d\n"),
             errors, m_who, m_host, m_port);
      }
      return false;
   }
   if (is_terminated()) {
      if (!m_suppress_error_msgs) {
         Qmsg4(m_jcr, M_ERROR, 0,  _("Socket is terminated=%d on call to %s:%s:%d\n"),
             is_terminated(), m_who, m_host, m_port);
      }
      return false;
   }
   if (msglen > 4000000) {
      if (!m_suppress_error_msgs) {
         Qmsg4(m_jcr, M_ERROR, 0,
            _("Socket has insane msglen=%d on call to %s:%s:%d\n"),
             msglen, m_who, m_host, m_port);
      }
      return false;
   }

   if (m_use_locking) P(m_mutex);
   /* Compute total packet length */
   if (msglen <= 0) {
      pktsiz = sizeof(pktsiz);               /* signal, no data */
   } else {
      pktsiz = msglen + sizeof(pktsiz);      /* data */
   }
   /* Store packet length at head of message -- note, we
    *  have reserved an int32_t just before msg, so we can
    *  store there 
    */
   hdr = (int32_t *)(msg - (int)sizeof(pktsiz));
   *hdr = htonl(msglen);                     /* store signal/length */

   out_msg_no++;            /* increment message number */

   /* send data packet */
   timer_start = watchdog_time;  /* start timer */
   clear_timed_out();
   /* Full I/O done in one write */
   rc = write_nbytes(this, (char *)hdr, pktsiz);
   timer_start = 0;         /* clear timer */
   if (rc != pktsiz) {
      errors++;
      if (errno == 0) {
         b_errno = EIO;
      } else {
         b_errno = errno;
      }
      if (rc < 0) {
         if (!m_suppress_error_msgs) {
            Qmsg5(m_jcr, M_ERROR, 0,
                  _("Write error sending %d bytes to %s:%s:%d: ERR=%s\n"), 
                  msglen, m_who,
                  m_host, m_port, this->bstrerror());
         }
      } else {
         Qmsg5(m_jcr, M_ERROR, 0,
               _("Wrote %d bytes to %s:%s:%d, but only %d accepted.\n"),
               msglen, m_who, m_host, m_port, rc);
      }
      ok = false;
   }
   if (m_use_locking) V(m_mutex);
   return ok;
}

/*
 * Format and send a message
 *  Returns: false on error
 *           true  on success
 */
bool BSOCK::fsend(const char *fmt, ...)
{
   va_list arg_ptr;
   int maxlen;

   if (errors || is_terminated()) {
      return false;
   }
   /* This probably won't work, but we vsnprintf, then if we
    * get a negative length or a length greater than our buffer
    * (depending on which library is used), the printf was truncated, so
    * get a bigger buffer and try again.
    */
   for (;;) {
      maxlen = sizeof_pool_memory(msg) - 1;
      va_start(arg_ptr, fmt);
      msglen = bvsnprintf(msg, maxlen, fmt, arg_ptr);
      va_end(arg_ptr);
      if (msglen > 0 && msglen < (maxlen - 5)) {
         break;
      }
      msg = realloc_pool_memory(msg, maxlen + maxlen / 2);
   }
   return send();
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
int32_t BSOCK::recv()
{
   int32_t nbytes;
   int32_t pktsiz;

   msg[0] = 0;
   msglen = 0;
   if (errors || is_terminated()) {
      return BNET_HARDEOF;
   }

   if (m_use_locking) P(m_mutex);
   read_seqno++;            /* bump sequence number */
   timer_start = watchdog_time;  /* set start wait time */
   clear_timed_out();
   /* get data size -- in int32_t */
   if ((nbytes = read_nbytes(this, (char *)&pktsiz, sizeof(int32_t))) <= 0) {
      timer_start = 0;      /* clear timer */
      /* probably pipe broken because client died */
      if (errno == 0) {
         b_errno = ENODATA;
      } else {
         b_errno = errno;
      }
      errors++;
      nbytes = BNET_HARDEOF;        /* assume hard EOF received */
      goto get_out;
   }
   timer_start = 0;         /* clear timer */
   if (nbytes != sizeof(int32_t)) {
      errors++;
      b_errno = EIO;
      Qmsg5(m_jcr, M_ERROR, 0, _("Read expected %d got %d from %s:%s:%d\n"),
            sizeof(int32_t), nbytes, m_who, m_host, m_port);
      nbytes = BNET_ERROR;
      goto get_out;
   }

   pktsiz = ntohl(pktsiz);         /* decode no. of bytes that follow */

   if (pktsiz == 0) {              /* No data transferred */
      timer_start = 0;             /* clear timer */
      in_msg_no++;
      msglen = 0;
      nbytes = 0;                  /* zero bytes read */
      goto get_out;
   }

   /* If signal or packet size too big */
   if (pktsiz < 0 || pktsiz > 1000000) {
      if (pktsiz > 0) {            /* if packet too big */
         Qmsg3(m_jcr, M_FATAL, 0,
               _("Packet size too big from \"%s:%s:%d. Terminating connection.\n"),
               m_who, m_host, m_port);
         pktsiz = BNET_TERMINATE;  /* hang up */
      }
      if (pktsiz == BNET_TERMINATE) {
         set_terminated();
      }
      timer_start = 0;                /* clear timer */
      b_errno = ENODATA;
      msglen = pktsiz;                /* signal code */
      nbytes =  BNET_SIGNAL;          /* signal */
      goto get_out;
   }

   /* Make sure the buffer is big enough + one byte for EOS */
   if (pktsiz >= (int32_t) sizeof_pool_memory(msg)) {
      msg = realloc_pool_memory(msg, pktsiz + 100);
   }

   timer_start = watchdog_time;  /* set start wait time */
   clear_timed_out();
   /* now read the actual data */
   if ((nbytes = read_nbytes(this, msg, pktsiz)) <= 0) {
      timer_start = 0;      /* clear timer */
      if (errno == 0) {
         b_errno = ENODATA;
      } else {
         b_errno = errno;
      }
      errors++;
      Qmsg4(m_jcr, M_ERROR, 0, _("Read error from %s:%s:%d: ERR=%s\n"),
            m_who, m_host, m_port, this->bstrerror());
      nbytes = BNET_ERROR;
      goto get_out;
   }
   timer_start = 0;         /* clear timer */
   in_msg_no++;
   msglen = nbytes;
   if (nbytes != pktsiz) {
      b_errno = EIO;
      errors++;
      Qmsg5(m_jcr, M_ERROR, 0, _("Read expected %d got %d from %s:%s:%d\n"),
            pktsiz, nbytes, m_who, m_host, m_port);
      nbytes = BNET_ERROR;
      goto get_out;
   }
   /* always add a zero by to properly terminate any
    * string that was send to us. Note, we ensured above that the
    * buffer is at least one byte longer than the message length.
    */
   msg[nbytes] = 0; /* terminate in case it is a string */
   /*
    * The following uses *lots* of resources so turn it on only for 
    * serious debugging.
    */
   Dsm_check(300);

get_out:
   if (m_use_locking) V(m_mutex);
   return nbytes;                  /* return actual length of message */
}

/*
 * Send a signal
 */
bool BSOCK::signal(int signal)
{
   msglen = signal;
   if (signal == BNET_TERMINATE) {
      m_suppress_error_msgs = true;
   }
   return send();
}

/* 
 * Despool spooled attributes
 */
bool BSOCK::despool(void update_attr_spool_size(ssize_t size), ssize_t tsize)
{
   int32_t pktsiz;
   size_t nbytes;
   ssize_t last = 0, size = 0;
   int count = 0;
   JCR *jcr = get_jcr();

   rewind(m_spool_fd);

#if defined(HAVE_POSIX_FADVISE) && defined(POSIX_FADV_WILLNEED)
   posix_fadvise(fileno(m_spool_fd), 0, 0, POSIX_FADV_WILLNEED);
#endif

   while (fread((char *)&pktsiz, 1, sizeof(int32_t), m_spool_fd) ==
          sizeof(int32_t)) {
      size += sizeof(int32_t);
      msglen = ntohl(pktsiz);
      if (msglen > 0) {
         if (msglen > (int32_t)sizeof_pool_memory(msg)) {
            msg = realloc_pool_memory(msg, msglen + 1);
         }
         nbytes = fread(msg, 1, msglen, m_spool_fd);
         if (nbytes != (size_t)msglen) {
            berrno be;
            Dmsg2(400, "nbytes=%d msglen=%d\n", nbytes, msglen);
            Qmsg1(get_jcr(), M_FATAL, 0, _("fread attr spool error. ERR=%s\n"),
                  be.bstrerror());
            update_attr_spool_size(tsize - last);
            return false;
         }
         size += nbytes;
         if ((++count & 0x3F) == 0) {
            update_attr_spool_size(size - last);
            last = size;
         }
      }
      send();
      if (jcr && job_canceled(jcr)) {
         return false;
      }
   }
   update_attr_spool_size(tsize - last);
   if (ferror(m_spool_fd)) {
      Qmsg(jcr, M_FATAL, 0, _("fread attr spool I/O error.\n"));
      return false;
   }
   return true;
}

/*
 * Return the string for the error that occurred
 * on the socket. Only the first error is retained.
 */
const char *BSOCK::bstrerror()
{
   berrno be;
   if (errmsg == NULL) {
      errmsg = get_pool_memory(PM_MESSAGE);
   }
   pm_strcpy(errmsg, be.bstrerror(b_errno));
   return errmsg;
}

int BSOCK::get_peer(char *buf, socklen_t buflen) 
{
#if !defined(HAVE_WIN32)
    if (peer_addr.sin_family == 0) {
        socklen_t salen = sizeof(peer_addr);
        int rval = (getpeername)(m_fd, (struct sockaddr *)&peer_addr, &salen);
        if (rval < 0) return rval;
    }
    if (!inet_ntop(peer_addr.sin_family, &peer_addr.sin_addr, buf, buflen))
        return -1;

    return 0;
#else
    return -1;
#endif
}

/*
 * Set the network buffer size, suggested size is in size.
 *  Actual size obtained is returned in bs->msglen
 *
 *  Returns: false on failure
 *           true  on success
 */
bool BSOCK::set_buffer_size(uint32_t size, int rw)
{
   uint32_t dbuf_size, start_size;

#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
   int opt;
   opt = IPTOS_THROUGHPUT;
   setsockopt(fd, IPPROTO_IP, IP_TOS, (sockopt_val_t)&opt, sizeof(opt));
#endif

   if (size != 0) {
      dbuf_size = size;
   } else {
      dbuf_size = DEFAULT_NETWORK_BUFFER_SIZE;
   }
   start_size = dbuf_size;
   if ((msg = realloc_pool_memory(msg, dbuf_size + 100)) == NULL) {
      Qmsg0(get_jcr(), M_FATAL, 0, _("Could not malloc BSOCK data buffer\n"));
      return false;
   }

   /*
    * If user has not set the size, use the OS default -- i.e. do not
    *   try to set it.  This allows sys admins to set the size they
    *   want in the OS, and Bacula will comply. See bug #1493
    */
   if (size == 0) {
      msglen = dbuf_size;
      return true;
   }

   if (rw & BNET_SETBUF_READ) {
      while ((dbuf_size > TAPE_BSIZE) && (setsockopt(m_fd, SOL_SOCKET,
              SO_RCVBUF, (sockopt_val_t) & dbuf_size, sizeof(dbuf_size)) < 0)) {
         berrno be;
         Qmsg1(get_jcr(), M_ERROR, 0, _("sockopt error: %s\n"), be.bstrerror());
         dbuf_size -= TAPE_BSIZE;
      }
      Dmsg1(200, "set network buffer size=%d\n", dbuf_size);
      if (dbuf_size != start_size) {
         Qmsg1(get_jcr(), M_WARNING, 0,
               _("Warning network buffer = %d bytes not max size.\n"), dbuf_size);
      }
   }
   if (size != 0) {
      dbuf_size = size;
   } else {
      dbuf_size = DEFAULT_NETWORK_BUFFER_SIZE;
   }
   start_size = dbuf_size;
   if (rw & BNET_SETBUF_WRITE) {
      while ((dbuf_size > TAPE_BSIZE) && (setsockopt(m_fd, SOL_SOCKET,
              SO_SNDBUF, (sockopt_val_t) & dbuf_size, sizeof(dbuf_size)) < 0)) {
         berrno be;
         Qmsg1(get_jcr(), M_ERROR, 0, _("sockopt error: %s\n"), be.bstrerror());
         dbuf_size -= TAPE_BSIZE;
      }
      Dmsg1(900, "set network buffer size=%d\n", dbuf_size);
      if (dbuf_size != start_size) {
         Qmsg1(get_jcr(), M_WARNING, 0,
               _("Warning network buffer = %d bytes not max size.\n"), dbuf_size);
      }
   }

   msglen = dbuf_size;
   return true;
}

/*
 * Set socket non-blocking
 * Returns previous socket flag
 */
int BSOCK::set_nonblocking()
{
#ifndef HAVE_WIN32
   int oflags;

   /* Get current flags */
   if ((oflags = fcntl(m_fd, F_GETFL, 0)) < 0) {
      berrno be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_GETFL error. ERR=%s\n"), be.bstrerror());
   }

   /* Set O_NONBLOCK flag */
   if ((fcntl(m_fd, F_SETFL, oflags|O_NONBLOCK)) < 0) {
      berrno be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_SETFL error. ERR=%s\n"), be.bstrerror());
   }

   m_blocking = 0;
   return oflags;
#else
   int flags;
   u_long ioctlArg = 1;

   flags = m_blocking;
   ioctlsocket(m_fd, FIONBIO, &ioctlArg);
   m_blocking = 0;

   return flags;
#endif
}

/*
 * Set socket blocking
 * Returns previous socket flags
 */
int BSOCK::set_blocking()
{
#ifndef HAVE_WIN32
   int oflags;
   /* Get current flags */
   if ((oflags = fcntl(m_fd, F_GETFL, 0)) < 0) {
      berrno be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_GETFL error. ERR=%s\n"), be.bstrerror());
   }

   /* Set O_NONBLOCK flag */
   if ((fcntl(m_fd, F_SETFL, oflags & ~O_NONBLOCK)) < 0) {
      berrno be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_SETFL error. ERR=%s\n"), be.bstrerror());
   }

   m_blocking = 1;
   return oflags;
#else
   int flags;
   u_long ioctlArg = 0;

   flags = m_blocking;
   ioctlsocket(m_fd, FIONBIO, &ioctlArg);
   m_blocking = 1;

   return flags;
#endif
}

void BSOCK::set_killable(bool killable)
{
   if (m_jcr) {
      m_jcr->set_killable(killable);
   }
}

/*
 * Restores socket flags
 */
void BSOCK::restore_blocking (int flags) 
{
#ifndef HAVE_WIN32
   if ((fcntl(m_fd, F_SETFL, flags)) < 0) {
      berrno be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_SETFL error. ERR=%s\n"), be.bstrerror());
   }

   m_blocking = (flags & O_NONBLOCK) ? true : false;
#else
   u_long ioctlArg = flags;

   ioctlsocket(m_fd, FIONBIO, &ioctlArg);
   m_blocking = 1;
#endif
}

/*
 * Wait for a specified time for data to appear on
 * the BSOCK connection.
 *
 *   Returns: 1 if data available
 *            0 if timeout
 *           -1 if error
 */
int BSOCK::wait_data(int sec, int usec)
{
   fd_set fdset;
   struct timeval tv;

   FD_ZERO(&fdset);
   FD_SET((unsigned)m_fd, &fdset);
   for (;;) {
      tv.tv_sec = sec;
      tv.tv_usec = usec;
      switch (select(m_fd + 1, &fdset, NULL, NULL, &tv)) {
      case 0:                      /* timeout */
         b_errno = 0;
         return 0;
      case -1:
         b_errno = errno;
         if (errno == EINTR) {
            continue;
         }
         return -1;                /* error return */
      default:
         b_errno = 0;
         return 1;
      }
   }
}

/*
 * As above, but returns on interrupt
 */
int BSOCK::wait_data_intr(int sec, int usec)
{
   fd_set fdset;
   struct timeval tv;

   if (this == NULL) {
      return -1;
   }
   FD_ZERO(&fdset);
   FD_SET((unsigned)m_fd, &fdset);
   tv.tv_sec = sec;
   tv.tv_usec = usec;
   switch (select(m_fd + 1, &fdset, NULL, NULL, &tv)) {
   case 0:                      /* timeout */
      b_errno = 0;
      return 0;
   case -1:
      b_errno = errno;
      return -1;                /* error return */
   default:
      b_errno = 0;
      break;
   }
   return 1;
}

/*
 * Note, this routine closes and destroys all the sockets
 *  that are open including the duped ones.
 */
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

void BSOCK::close()
{
   BSOCK *bsock = this;
   BSOCK *next;

   if (!m_duped) {
      clear_locking();
   }
   for (; bsock; bsock = next) {
      next = bsock->m_next;           /* get possible pointer to next before destoryed */
      if (!bsock->m_duped) {
         /* Shutdown tls cleanly. */
         if (bsock->tls) {
            tls_bsock_shutdown(bsock);
            free_tls_connection(bsock->tls);
            bsock->tls = NULL;
         }
         if (bsock->is_timed_out()) {
            shutdown(bsock->m_fd, SHUT_RDWR);   /* discard any pending I/O */
         }
         socketClose(bsock->m_fd);      /* normal close */
      }
      bsock->destroy();
   }
   return;
}

void BSOCK::destroy()
{
   if (msg) {
      free_pool_memory(msg);
      msg = NULL;
   } else {
      ASSERT(1 == 0);              /* double close */
   }
   if (errmsg) {
      free_pool_memory(errmsg);
      errmsg = NULL;
   }
   if (m_who) {
      free(m_who);
      m_who = NULL;
   }
   if (m_host) {
      free(m_host);
      m_host = NULL;
   }
   if (src_addr) {
      free(src_addr);
      src_addr = NULL;
   } 
   free(this);
}

/* Commands sent to Director */
static char hello[]    = "Hello %s calling\n";

/* Response from Director */
static char OKhello[]   = "1000 OK:";

/*
 * Authenticate Director
 */
bool BSOCK::authenticate_director(const char *name, const char *password,
                                  TLS_CONTEXT *tls_ctx, char *response, int response_len)
{
   int tls_local_need = BNET_TLS_NONE;
   int tls_remote_need = BNET_TLS_NONE;
   int compatible = true;
   char bashed_name[MAX_NAME_LENGTH];
   BSOCK *dir = this;        /* for readability */

   response[0] = 0;
   /*
    * Send my name to the Director then do authentication
    */

   /* Timeout Hello after 15 secs */
   dir->start_timer(15);
   dir->fsend(hello, bashed_name);

   if (get_tls_enable(tls_ctx)) {
      tls_local_need = get_tls_enable(tls_ctx) ? BNET_TLS_REQUIRED : BNET_TLS_OK;
   }

   /* respond to Dir challenge */
   if (!cram_md5_respond(dir, password, &tls_remote_need, &compatible) ||
       /* Now challenge dir */
       !cram_md5_challenge(dir, password, tls_local_need, compatible)) {
      bsnprintf(response, response_len, _("Director authorization problem at \"%s:%d\"\n"),
         dir->host(), dir->port());
      goto bail_out;
   }

   /* Verify that the remote host is willing to meet our TLS requirements */
   if (tls_remote_need < tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      bsnprintf(response, response_len, _("Authorization problem:"
             " Remote server at \"%s:%d\" did not advertise required TLS support.\n"),
             dir->host(), dir->port());
      goto bail_out;
   }

   /* Verify that we are willing to meet the remote host's requirements */
   if (tls_remote_need > tls_local_need && tls_local_need != BNET_TLS_OK && tls_remote_need != BNET_TLS_OK) {
      bsnprintf(response, response_len, _("Authorization problem with Director at \"%s:%d\":"
                     " Remote server requires TLS.\n"),
                     dir->host(), dir->port());

      goto bail_out;
   }

   /* Is TLS Enabled? */
   if (have_tls) {
      if (tls_local_need >= BNET_TLS_OK && tls_remote_need >= BNET_TLS_OK) {
         /* Engage TLS! Full Speed Ahead! */
         if (!bnet_tls_client(tls_ctx, dir, NULL)) {
            bsnprintf(response, response_len, _("TLS negotiation failed with Director at \"%s:%d\"\n"),
               dir->host(), dir->port());
            goto bail_out;
         }
      }
   }

   Dmsg1(6, ">dird: %s", dir->msg);
   if (dir->recv() <= 0) {
      dir->stop_timer();
      bsnprintf(response, response_len, _("Bad response to Hello command: ERR=%s\n"
                      "The Director at \"%s:%d\" is probably not running.\n"),
                    dir->bstrerror(), dir->host(), dir->port());
      return false;
   }

   dir->stop_timer();
   Dmsg1(10, "<dird: %s", dir->msg);
   if (strncmp(dir->msg, OKhello, sizeof(OKhello)-1) != 0) {
      bsnprintf(response, response_len, _("Director at \"%s:%d\" rejected Hello command\n"),
         dir->host(), dir->port());
      return false;
   } else {
      bsnprintf(response, response_len, "%s", dir->msg);
   }
   return true;

bail_out:
   dir->stop_timer();
   bsnprintf(response, response_len, _("Authorization problem with Director at \"%s:%d\"\n"
             "Most likely the passwords do not agree.\n"
             "If you are using TLS, there may have been a certificate validation error during the TLS handshake.\n"
             "Please see " MANUAL_AUTH_URL " for help.\n"),
             dir->host(), dir->port());
   return false;
}
