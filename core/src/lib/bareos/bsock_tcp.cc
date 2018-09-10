/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * TCP Sockets abstraction.
 *
 * Kern Sibbald
 *
 * Extracted from other source files by Marco van Wieringen, September 2013.
 */

#include "include/bareos.h"
#include "include/jcr.h"
#include <netdb.h>
#include <netinet/tcp.h>
#include "lib/bnet.h"
#include "lib/bpoll.h"
#include "lib/btimers.h"
#include "lib/tls_openssl.h"

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

BareosSocketTCP::BareosSocketTCP() : BareosSocket()
{
}

BareosSocketTCP::~BareosSocketTCP()
{
   destroy();
}

BareosSocket *BareosSocketTCP::clone()
{
   BareosSocketTCP *clone = New(BareosSocketTCP(*this));

   /* do not use memory buffer from copied socket */
   clone->msg = GetPoolMemory(PM_BSOCK);
   clone->errmsg = GetPoolMemory(PM_MESSAGE);

   if (src_addr) {
      src_addr = New(IPADDR(*(src_addr)));
   }
   if (who_) {
      who_ = bstrdup(who_);
   }
   if (host_) {
      host_ = bstrdup(host_);
   }

   /* duplicate file descriptors */
   if (fd_ >= 0) {
      clone->fd_ = dup(fd_);
   }
   if (spool_fd_ >= 0) {
      clone->spool_fd_ = dup(spool_fd_);
   }

   clone->cloned_ = true;

   return clone;
}

/*
 * Try to connect to host for max_retry_time at retry_time intervals.
 * Note, you must have called the constructor prior to calling
 * this routine.
 */
bool BareosSocketTCP::connect(JobControlRecord * jcr, int retry_interval, utime_t max_retry_time,
                        utime_t heart_beat, const char *name, char *host,
                        char *service, int port, bool verbose)
{
   bool ok = false;
   int i;
   int fatal = 0;
   time_t begin_time = time(NULL);
   time_t now;
   btimer_t *tid = NULL;

   /*
    * Try to trap out of OS call when time expires
    */
   if (max_retry_time) {
      tid = start_thread_timer(jcr, pthread_self(), (uint32_t)max_retry_time);
   }

   for (i = 0; !open(jcr, name, host, service, port, heart_beat, &fatal);
        i -= retry_interval) {
      BErrNo be;
      if (fatal || (jcr && JobCanceled(jcr))) {
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
      Bmicrosleep(retry_interval, 0);
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
      StopThreadTimer(tid);
   }
   return ok;
}

/*
 * Finish initialization of the pocket structure.
 */
void BareosSocketTCP::FinInit(JobControlRecord * jcr, int sockfd, const char *who, const char *host,
                         int port, struct sockaddr *lclient_addr)
{
   Dmsg3(100, "who=%s host=%s port=%d\n", who, host, port);
   SetWho(bstrdup(who));
   SetHost(bstrdup(host));
   SetPort(port);
   memcpy(&client_addr, lclient_addr, sizeof(client_addr));
   SetJcr(jcr);
}

/*
 * Open a TCP connection to the server
 *
 * Returns NULL
 * Returns BareosSocket * pointer on success
 */
bool BareosSocketTCP::open(JobControlRecord *jcr, const char *name, char *host, char *service,
                     int port, utime_t heart_beat, int *fatal)
{
   int sockfd = -1;
   dlist *addr_list;
   IPADDR *ipaddr, *next, *to_free;
   bool connected = false;
   int value;
   const char *errstr;
   int save_errno = 0;

   /*
    * Fill in the structure serv_addr with the address of
    * the server that we want to connect with.
    */
   if ((addr_list = BnetHost2IpAddrs(host, 0, &errstr)) == NULL) {
      /*
       * Note errstr is not malloc'ed
       */
      Qmsg2(jcr, M_ERROR, 0, _("BnetHost2IpAddrs() for host \"%s\" failed: ERR=%s\n"),
            host, errstr);
      Dmsg2(100, "BnetHost2IpAddrs() for host %s failed: ERR=%s\n",
            host, errstr);
      *fatal = 1;
      return false;
   }

   /*
    * Remove any duplicate addresses.
    */
   for (ipaddr = (IPADDR *)addr_list->first();
        ipaddr;
        ipaddr = (IPADDR *)addr_list->next(ipaddr)) {
      next = (IPADDR *)addr_list->next(ipaddr);
      while (next) {
         /*
          * See if the addresses match.
          */
         if (ipaddr->GetSockaddrLen() == next->GetSockaddrLen() &&
             memcmp(ipaddr->get_sockaddr(), next->get_sockaddr(),
                    ipaddr->GetSockaddrLen()) == 0) {
            to_free = next;
            next = (IPADDR *)addr_list->next(next);
            addr_list->remove(to_free);
            delete to_free;
         } else {
            next = (IPADDR *)addr_list->next(next);
         }
      }
   }

   if (use_keepalive_) {
      value = 1;
   } else {
      value = 0;
   }

   foreach_dlist(ipaddr, addr_list) {
      ipaddr->SetPortNet(htons(port));
      char allbuf[256 * 10];
      char curbuf[256];
      Dmsg2(100, "Current %s All %s\n",
                   ipaddr->build_address_str(curbuf, sizeof(curbuf)),
                   BuildAddressesString(addr_list, allbuf, sizeof(allbuf)));
      /* Open a TCP socket */
      if ((sockfd = socket(ipaddr->GetFamily(), SOCK_STREAM, 0)) < 0) {
         BErrNo be;
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
               ipaddr->GetFamily(), ipaddr->GetPortHostOrder(), be.bstrerror());
            break;
         }
         continue;
      }

      /*
       * Bind to the source address if it is set
       */
      if (src_addr) {
         if (bind(sockfd, src_addr->get_sockaddr(), src_addr->GetSockaddrLen()) < 0) {
            BErrNo be;
            save_errno = errno;
            *fatal = 1;
            Pmsg2(000, _("Source address bind error. proto=%d. ERR=%s\n"),
                  src_addr->GetFamily(), be.bstrerror() );
            if (sockfd >= 0) {
               socketClose(sockfd);
               sockfd = -1;
            }
            continue;
         }
      }

      /*
       * Keep socket from timing out from inactivity
       */
      SetKeepalive(jcr, sockfd, use_keepalive_, heart_beat, heart_beat);

      /*
       * Connect to server
       */
      if (::connect(sockfd, ipaddr->get_sockaddr(), ipaddr->GetSockaddrLen()) < 0) {
         save_errno = errno;
         if (sockfd >= 0) {
            socketClose(sockfd);
            sockfd = -1;
         }
         continue;
      }
      *fatal = 0;
      connected = true;
      break;
   }

   if (!connected) {
      FreeAddresses(addr_list);
      errno = save_errno | b_errno_win32;
      if (sockfd >= 0) {
         socketClose(sockfd);
         sockfd = -1;
      }
      return false;
   }

   /*
    * Keep socket from timing out from inactivity
    * Do this a second time out of paranoia
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_val_t)&value, sizeof(value)) < 0) {
      BErrNo be;
      Qmsg1(jcr, M_WARNING, 0, _("Cannot set SO_KEEPALIVE on socket: %s\n"), be.bstrerror());
   }

   FinInit(jcr, sockfd, name, host, port, ipaddr->get_sockaddr());
   FreeAddresses(addr_list);
   fd_ = sockfd;
   return true;
}


bool BareosSocketTCP::SetKeepalive(JobControlRecord *jcr, int sockfd, bool enable, int keepalive_start, int keepalive_interval)
{
   int value = int(enable);

   /*
    * Keep socket from timing out from inactivity
    */
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_val_t)&value, sizeof(value)) < 0) {
      BErrNo be;
      Qmsg1(jcr, M_WARNING, 0, _("Failed to set SO_KEEPALIVE on socket: %s\n"), be.bstrerror());
      return false;
   }

   if (enable && keepalive_interval) {
#ifdef HAVE_WIN32
      struct s_tcp_keepalive {
         u_long  onoff;
         u_long  keepalivetime;
         u_long  keepaliveinterval;
      } val;
      val.onoff = enable;
      val.keepalivetime = keepalive_start*1000L;
      val.keepaliveinterval = keepalive_interval*1000L;
      DWORD got = 0;
      if (WSAIoctl(sockfd, SIO_KEEPALIVE_VALS, &val, sizeof(val), NULL, 0, &got, NULL, NULL) != 0) {
         Qmsg1(jcr, M_WARNING, 0, "Failed to set SIO_KEEPALIVE_VALS on socket: %d\n", WSAGetLastError());
         return false;
      }
#else
#if defined(TCP_KEEPIDLE)
      if (setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, (sockopt_val_t)&keepalive_start, sizeof(keepalive_start)) < 0) {
         BErrNo be;
         Qmsg2(jcr, M_WARNING, 0, _("Failed to set TCP_KEEPIDLE = %d on socket: %s\n"),
               keepalive_start, be.bstrerror());
         return false;
      }
      if (setsockopt(sockfd, SOL_TCP, TCP_KEEPINTVL, (sockopt_val_t)&keepalive_interval, sizeof(keepalive_interval)) < 0) {
         BErrNo be;
         Qmsg2(jcr, M_WARNING, 0, _("Failed to set TCP_KEEPINTVL = %d on socket: %s\n"),
               keepalive_interval, be.bstrerror());
         return false;
      }
#endif
#endif
   }
   return true;
}


bool BareosSocketTCP::SendPacket(int32_t *hdr, int32_t pktsiz)
{
   Enter(400);

   int32_t rc;
   bool ok = true;

   out_msg_no++;            /* increment message number */

   /*
    * Send data packet
    */
   timer_start = watchdog_time;  /* start timer */
   ClearTimedOut();

   /*
    * Full I/O done in one write
    */
   rc = write_nbytes((char *)hdr, pktsiz);
   timer_start = 0;         /* clear timer */
   if (rc != pktsiz) {
      errors++;
      if (errno == 0) {
         b_errno = EIO;
      } else {
         b_errno = errno;
      }
      if (rc < 0) {
         if (!suppress_error_msgs_) {
            Qmsg5(jcr_, M_ERROR, 0,
                  _("Write error sending %d bytes to %s:%s:%d: ERR=%s\n"),
                  message_length, who_,
                  host_, port_, this->bstrerror());
         }
      } else {
         Qmsg5(jcr_, M_ERROR, 0,
               _("Wrote %d bytes to %s:%s:%d, but only %d accepted.\n"),
               message_length, who_, host_, port_, rc);
      }
      ok = false;
   }

   Leave(400);

   return ok;
}

/*
 * Send a message over the network. The send consists of
 * two network packets. The first is sends a 32 bit integer containing
 * the length of the data packet which follows.
 *
 * Returns: false on failure
 *          true  on success
 */
bool BareosSocketTCP::send()
{
   /*
    * Send msg (length: message_length).
    * As send() and recv() uses the same buffer (msg and message_length)
    * store the original message_length in an own variable,
    * that will not be modifed by recv().
    */
   const int32_t o_msglen = message_length;
   int32_t pktsiz;
   int32_t written = 0;
   int32_t packet_msglen = 0;
   bool ok = true;
   /*
    * Store packet length at head of message -- note, we have reserved an int32_t just before msg,
    * So we can store there
    */
   int32_t *hdr = (int32_t *)(msg - (int)header_length);

   if (errors) {
      if (!suppress_error_msgs_) {
         Qmsg4(jcr_, M_ERROR, 0,  _("Socket has errors=%d on call to %s:%s:%d\n"),
             errors, who_, host_, port_);
      }
      return false;
   }

   if (IsTerminated()) {
      if (!suppress_error_msgs_) {
         Qmsg4(jcr_, M_ERROR, 0,  _("Socket is terminated=%d on call to %s:%s:%d\n"),
             IsTerminated(), who_, host_, port_);
      }
      return false;
   }

   if (mutex_) {
      mutex_->lock();
   }

   /*
    * Compute total packet length
    */
   if (o_msglen <= 0) {
      pktsiz = header_length;                /* signal, no data */
      *hdr = htonl(o_msglen);                /* store signal */
      ok = SendPacket(hdr, pktsiz);
   } else {
      /*
       * msg might be to long for a single Bareos packet.
       * If so, send msg as multiple packages.
       */
      while (ok && (written < o_msglen)) {
         if ((o_msglen - written) > max_message_len) {
            /*
             * Message is to large for a single Bareos packet.
             * Send it via multiple packets.
             */
            pktsiz = max_packet_size;                  /* header + data */
            packet_msglen = max_message_len;
         } else {
            /*
             * Remaining message fits into one Bareos packet
             */
            pktsiz = header_length + (o_msglen-written); /* header + data */
            packet_msglen = (o_msglen-written);
         }

         *hdr = htonl(packet_msglen);        /* store length */
         ok = SendPacket(hdr, pktsiz);
         written += packet_msglen;
         hdr = (int32_t *)(msg + written - (int)header_length);
      }
   }

   if (mutex_) {
      mutex_->unlock();
   }

   return ok;
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
 *  Using IsBnetStop() and IsBnetError() you can figure this all out.
 */
int32_t BareosSocketTCP::recv()
{
   int32_t nbytes;
   int32_t pktsiz;

   msg[0] = 0;
   message_length = 0;
   if (errors || IsTerminated()) {
      return BNET_HARDEOF;
   }

   if (mutex_) {
      mutex_->lock();
   }

   read_seqno++;            /* bump sequence number */
   timer_start = watchdog_time;  /* set start wait time */
   ClearTimedOut();

   /*
    * Get data size -- in int32_t
    */
   if ((nbytes = read_nbytes((char *)&pktsiz, header_length)) <= 0) {
      timer_start = 0;      /* clear timer */
      /*
       * Probably pipe broken because client died
       */
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
   if (nbytes != header_length) {
      errors++;
      b_errno = EIO;
      Qmsg5(jcr_, M_ERROR, 0, _("Read expected %d got %d from %s:%s:%d\n"),
            header_length, nbytes, who_, host_, port_);
      nbytes = BNET_ERROR;
      goto get_out;
   }

   pktsiz = ntohl(pktsiz);         /* decode no. of bytes that follow */

   if (pktsiz == 0) {              /* No data transferred */
      timer_start = 0;             /* clear timer */
      in_msg_no++;
      message_length = 0;
      nbytes = 0;                  /* zero bytes read */
      goto get_out;
   }

   /*
    * If signal or packet size too big
    */
   if (pktsiz < 0 || pktsiz > max_packet_size) {
      if (pktsiz > 0) {            /* if packet too big */
         Qmsg3(jcr_, M_FATAL, 0,
               _("Packet size too big from \"%s:%s:%d. Terminating connection.\n"),
               who_, host_, port_);
         pktsiz = BNET_TERMINATE;  /* hang up */
      }
      if (pktsiz == BNET_TERMINATE) {
         SetTerminated();
      }
      timer_start = 0;                /* clear timer */
      b_errno = ENODATA;
      message_length = pktsiz;                /* signal code */
      nbytes =  BNET_SIGNAL;          /* signal */
      goto get_out;
   }

   /*
    * Make sure the buffer is big enough + one byte for EOS
    */
   if (pktsiz >= (int32_t) SizeofPoolMemory(msg)) {
      msg = ReallocPoolMemory(msg, pktsiz + 100);
   }

   timer_start = watchdog_time;  /* set start wait time */
   ClearTimedOut();

   /*
    * Now read the actual data
    */
   if ((nbytes = read_nbytes(msg, pktsiz)) <= 0) {
      timer_start = 0;      /* clear timer */
      if (errno == 0) {
         b_errno = ENODATA;
      } else {
         b_errno = errno;
      }
      errors++;
      Qmsg4(jcr_, M_ERROR, 0, _("Read error from %s:%s:%d: ERR=%s\n"),
            who_, host_, port_, this->bstrerror());
      nbytes = BNET_ERROR;
      goto get_out;
   }
   timer_start = 0;         /* clear timer */
   in_msg_no++;
   message_length = nbytes;
   if (nbytes != pktsiz) {
      b_errno = EIO;
      errors++;
      Qmsg5(jcr_, M_ERROR, 0, _("Read expected %d got %d from %s:%s:%d\n"),
            pktsiz, nbytes, who_, host_, port_);
      nbytes = BNET_ERROR;
      goto get_out;
   }

   /*
    * Always add a zero by to properly Terminate any string that was send to us.
    * Note, we ensured above that the buffer is at least one byte longer than
    * the message length.
    */
   msg[nbytes] = 0; /* Terminate in case it is a string */

   /*
    * The following uses *lots* of resources so turn it on only for serious debugging.
    */
   Dsm_check(300);

get_out:
   if (mutex_) {
      mutex_->unlock();
   }

   return nbytes;                  /* return actual length of message */
}

int BareosSocketTCP::GetPeer(char *buf, socklen_t buflen)
{
#if !defined(HAVE_WIN32)
    if (peer_addr.sin_family == 0) {
        socklen_t salen = sizeof(peer_addr);
        int rval = (getpeername)(fd_, (struct sockaddr *)&peer_addr, &salen);
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
 * Actual size obtained is returned in bs->message_length
 *
 * Returns: false on failure
 *          true  on success
 */
bool BareosSocketTCP::SetBufferSize(uint32_t size, int rw)
{
   uint32_t dbuf_size, start_size;

#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
   int opt;

   opt = IPTOS_THROUGHPUT;
   setsockopt(fd_, IPPROTO_IP, IP_TOS, (sockopt_val_t)&opt, sizeof(opt));
#endif

   if (size != 0) {
      dbuf_size = size;
   } else {
      dbuf_size = DEFAULT_NETWORK_BUFFER_SIZE;
   }
   start_size = dbuf_size;
   if ((msg = ReallocPoolMemory(msg, dbuf_size + 100)) == NULL) {
      Qmsg0(get_jcr(), M_FATAL, 0, _("Could not malloc BareosSocket data buffer\n"));
      return false;
   }

   /*
    * If user has not set the size, use the OS default -- i.e. do not
    * try to set it.  This allows sys admins to set the size they
    * want in the OS, and BAREOS will comply. See bug #1493
    */
   if (size == 0) {
      message_length = dbuf_size;
      return true;
   }

   if (rw & BNET_SETBUF_READ) {
      while ((dbuf_size > TAPE_BSIZE) && (setsockopt(fd_, SOL_SOCKET,
              SO_RCVBUF, (sockopt_val_t) & dbuf_size, sizeof(dbuf_size)) < 0)) {
         BErrNo be;
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
      while ((dbuf_size > TAPE_BSIZE) && (setsockopt(fd_, SOL_SOCKET,
              SO_SNDBUF, (sockopt_val_t) & dbuf_size, sizeof(dbuf_size)) < 0)) {
         BErrNo be;
         Qmsg1(get_jcr(), M_ERROR, 0, _("sockopt error: %s\n"), be.bstrerror());
         dbuf_size -= TAPE_BSIZE;
      }
      Dmsg1(900, "set network buffer size=%d\n", dbuf_size);
      if (dbuf_size != start_size) {
         Qmsg1(get_jcr(), M_WARNING, 0,
               _("Warning network buffer = %d bytes not max size.\n"), dbuf_size);
      }
   }

   message_length = dbuf_size;
   return true;
}

/*
 * Set socket non-blocking
 *
 * Returns previous socket flag
 */
int BareosSocketTCP::SetNonblocking()
{
#ifndef HAVE_WIN32
   int oflags;

   /*
    * Get current flags
    */
   if ((oflags = fcntl(fd_, F_GETFL, 0)) < 0) {
      BErrNo be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_GETFL error. ERR=%s\n"), be.bstrerror());
   }

   /*
    * Set O_NONBLOCK flag
    */
   if ((fcntl(fd_, F_SETFL, oflags|O_NONBLOCK)) < 0) {
      BErrNo be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_SETFL error. ERR=%s\n"), be.bstrerror());
   }

   blocking_ = 0;
   return oflags;
#else
   int flags;
   u_long ioctlArg = 1;

   flags = blocking_;
   ioctlsocket(fd_, FIONBIO, &ioctlArg);
   blocking_ = 0;

   return flags;
#endif
}

/*
 * Set socket blocking
 *
 * Returns previous socket flags
 */
int BareosSocketTCP::SetBlocking()
{
#ifndef HAVE_WIN32
   int oflags;

   /*
    * Get current flags
    */
   if ((oflags = fcntl(fd_, F_GETFL, 0)) < 0) {
      BErrNo be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_GETFL error. ERR=%s\n"), be.bstrerror());
   }

   /*
    * Set O_NONBLOCK flag
    */
   if ((fcntl(fd_, F_SETFL, oflags & ~O_NONBLOCK)) < 0) {
      BErrNo be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_SETFL error. ERR=%s\n"), be.bstrerror());
   }

   blocking_ = 1;
   return oflags;
#else
   int flags;
   u_long ioctlArg = 0;

   flags = blocking_;
   ioctlsocket(fd_, FIONBIO, &ioctlArg);
   blocking_ = 1;

   return flags;
#endif
}

/*
 * Restores socket flags
 */
void BareosSocketTCP::RestoreBlocking(int flags)
{
#ifndef HAVE_WIN32
   if ((fcntl(fd_, F_SETFL, flags)) < 0) {
      BErrNo be;
      Qmsg1(get_jcr(), M_ABORT, 0, _("fcntl F_SETFL error. ERR=%s\n"), be.bstrerror());
   }

   blocking_ = (flags & O_NONBLOCK) ? true : false;
#else
   u_long ioctlArg = flags;

   ioctlsocket(fd_, FIONBIO, &ioctlArg);
   blocking_ = 1;
#endif
}

/*
 * Wait for a specified time for data to appear on
 * the BareosSocket connection.
 *
 * Returns: 1 if data available
 *          0 if timeout
 *         -1 if error
 */
int BareosSocketTCP::WaitData(int sec, int usec)
{
   int msec;

   msec = (sec * 1000) + (usec / 1000);
   switch (WaitForReadableFd(fd_, msec, true)) {
   case 0:
      b_errno = 0;
      return 0;
   case -1:
      b_errno = errno;
      return -1;                /* error return */
   default:
      b_errno = 0;
      return 1;
   }
}

/*
 * As above, but returns on interrupt
 */
int BareosSocketTCP::WaitDataIntr(int sec, int usec)
{
   int msec;

   msec = (sec * 1000) + (usec / 1000);
   switch (WaitForReadableFd(fd_, msec, false)) {
   case 0:
      b_errno = 0;
      return 0;
   case -1:
      b_errno = errno;
      return -1;                /* error return */
   default:
      b_errno = 0;
      return 1;
   }
}

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

void BareosSocketTCP::close()
{
   if (!cloned_) {
      ClearLocking();

      if (tls_conn) {
         CloseTlsConnectionAndFreeMemory();
      }
   }

   if (fd_ >= 0) {
      if (!cloned_) {
         if (IsTimedOut()) {
            shutdown(fd_, SHUT_RDWR);
         }
      }
      socketClose(fd_);
      fd_ = -1;
   }
}

void BareosSocketTCP::destroy()
{
   /* if this object is cloned
    * some memory or file descriptors
    * are duplicated not just copied */

   if (msg) { /* duplicated */
      FreePoolMemory(msg);
      msg = nullptr;
   }
   if (errmsg) { /* duplicated */
      FreePoolMemory(errmsg);
      errmsg = nullptr;
   }
   if (who_) { /* duplicated */
      free(who_);
      who_ = nullptr;
   }
   if (host_) { /* duplicated */
      free(host_);
      host_ = nullptr;
   }
   if (src_addr) { /* duplicated */
      free(src_addr);
      src_addr = nullptr;
   }
   if (fd_ >= 0) { /* duplicated */
      socketClose(fd_);
      fd_ = -1;
   }
   if(spool_fd_ >= 0) { /* duplicated */
      socketClose(spool_fd_);
      spool_fd_ = -1;
   }
}

/*
 * Read a nbytes from the network.
 * It is possible that the total bytes require in several
 * read requests
 */
int32_t BareosSocketTCP::read_nbytes(char *ptr, int32_t nbytes)
{
   int32_t nleft, nread;

#ifdef HAVE_TLS
   if (tls_conn) {
      return (tls_conn->TlsBsockReadn(this, ptr, nbytes));
   }
#endif /* HAVE_TLS */

   nleft = nbytes;
   while (nleft > 0) {
      errno = 0;
      nread = socketRead(fd_, ptr, nleft);
      if (IsTimedOut() || IsTerminated()) {
         return -1;
      }

#ifdef HAVE_WIN32
      /*
       * For Windows, we must simulate Unix errno on a socket error in order to handle errors correctly.
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
            Bmicrosleep(0, 20000); /* try again in 20ms */
            continue;
         }
      }

      if (nread <= 0) {
         return -1;                /* error, or EOF */
      }

      nleft -= nread;
      ptr += nread;
      if (UseBwlimit()) {
         ControlBwlimit(nread);
      }
   }

   return nbytes - nleft;          /* return >= 0 */
}

/*
 * Write nbytes to the network.
 * It may require several writes.
 */
int32_t BareosSocketTCP::write_nbytes(char *ptr, int32_t nbytes)
{
   int32_t nleft, nwritten;

   if (IsSpooling()) {
      nwritten = write(spool_fd_, ptr, nbytes);
      if (nwritten != nbytes) {
         BErrNo be;
         b_errno = errno;
         Qmsg1(jcr(), M_FATAL, 0, _("Attr spool write error. ERR=%s\n"), be.bstrerror());
         Dmsg2(400, "nwritten=%d nbytes=%d.\n", nwritten, nbytes);
         errno = b_errno;
         return -1;
      }
      return nbytes;
   }

#ifdef HAVE_TLS
   if (tls_conn) {
      return (tls_conn->TlsBsockWriten(this, ptr, nbytes));
   }
#endif /* HAVE_TLS */

   nleft = nbytes;
   while (nleft > 0) {
      do {
         errno = 0;
         nwritten = socketWrite(fd_, ptr, nleft);
         if (IsTimedOut() || IsTerminated()) {
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
       * use select()/poll() to keep from consuming all
       * the CPU and try again.
       */
      if (nwritten == -1 && errno == EAGAIN) {
         WaitForWritableFd(fd_, 1, false);
         continue;
      }

      if (nwritten <= 0) {
         return -1;                /* error */
      }

      nleft -= nwritten;
      ptr += nwritten;
      if (UseBwlimit()) {
         ControlBwlimit(nwritten);
      }
   }

   return nbytes - nleft;
}
