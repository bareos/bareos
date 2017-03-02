/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
  * Originally written by Kern Sibbald for inclusion in apcupsd,
  * but heavily modified for BAREOS
  */

#include "bareos.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
//#include <resolv.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#elif HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef HAVE_LIBWRAP
#include "tcpd.h"
int allow_severity = LOG_NOTICE;
int deny_severity = LOG_WARNING;
#endif

static bool quit = false;

struct s_sockfd {
   int fd;
   int port;
};

/*
 * Stop the Threaded Network Server if its realy running in a separate thread.
 * e.g. set the quit flag and wait for the other thread to exit cleanly.
 */
void bnet_stop_thread_server_tcp(pthread_t tid)
{
   quit = true;
   if (!pthread_equal(tid, pthread_self())) {
      pthread_kill(tid, TIMEOUT_SIGNAL);
   }
}

/*
 * Perform a cleanup for the Threaded Network Server check if there is still
 * something to do or that the cleanup already took place.
 */
void cleanup_bnet_thread_server_tcp(alist *sockfds, workq_t *client_wq)
{
   int status;
   s_sockfd *fd_ptr = NULL;

   Dmsg0(100, "cleanup_bnet_thread_server_tcp: start\n");

   if (!sockfds->empty()) {
      /*
       * Cleanup open files and pointers to them
       */
      fd_ptr = (s_sockfd *)sockfds->first();
      while (fd_ptr) {
         close(fd_ptr->fd);
         fd_ptr = (s_sockfd *)sockfds->next();
      }

      sockfds->destroy();

      /*
       * Stop work queue thread
       */
      if ((status = workq_destroy(client_wq)) != 0) {
         berrno be;
         be.set_errno(status);
         Emsg1(M_FATAL, 0, _("Could not destroy client queue: ERR=%s\n"),
               be.bstrerror());
      }
   }

   Dmsg0(100, "cleanup_bnet_thread_server_tcp: finish\n");
}

/*
 * Become Threaded Network Server
 *
 * This function is able to handle multiple server ips in
 * ipv4 and ipv6 style. The Addresse are give in a comma
 * separated string in bind_addr
 *
 * At the moment it is impossible to bind to different ports.
 */
void bnet_thread_server_tcp(dlist *addr_list,
                            int max_clients,
                            alist *sockfds,
                            workq_t *client_wq,
                            bool nokeepalive,
                            void *handle_client_request(void *bsock))
{
   int newsockfd, status;
   socklen_t clilen;
   struct sockaddr cli_addr;       /* client's address */
   int tlog, tmax;
   int value;
#ifdef HAVE_LIBWRAP
   struct request_info request;
#endif
   IPADDR *ipaddr, *next, *to_free;
   s_sockfd *fd_ptr = NULL;
   char buf[128];
#ifdef HAVE_POLL
   nfds_t nfds;
   int events;
   struct pollfd *pfds;
#endif

   char allbuf[256 * 10];

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
         if (ipaddr->get_sockaddr_len() == next->get_sockaddr_len() &&
             memcmp(ipaddr->get_sockaddr(), next->get_sockaddr(),
                    ipaddr->get_sockaddr_len()) == 0) {
            to_free = next;
            next = (IPADDR *)addr_list->next(next);
            addr_list->remove(to_free);
            delete to_free;
         } else {
            next = (IPADDR *)addr_list->next(next);
         }
      }
   }

   Dmsg1(100, "Addresses %s\n", build_addresses_str(addr_list, allbuf, sizeof(allbuf)));

   if (nokeepalive) {
      value = 0;
   } else {
      value = 1;
   }

#ifdef HAVE_POLL
   nfds = 0;
#endif
   foreach_dlist(ipaddr, addr_list) {
      /*
       * Allocate on stack from -- no need to free
       */
      fd_ptr = (s_sockfd *)alloca(sizeof(s_sockfd));
      fd_ptr->port = ipaddr->get_port_net_order();

      /*
       * Open a TCP socket
       */
      for (tlog= 60; (fd_ptr->fd=socket(ipaddr->get_family(), SOCK_STREAM, 0)) < 0; tlog -= 10) {
         if (tlog <= 0) {
            berrno be;
            char curbuf[256];
            Emsg3(M_ABORT, 0, _("Cannot open stream socket. ERR=%s. Current %s All %s\n"),
                       be.bstrerror(),
                       ipaddr->build_address_str(curbuf, sizeof(curbuf)),
                       build_addresses_str(addr_list, allbuf, sizeof(allbuf)));
         }
         bmicrosleep(10, 0);
      }

      /*
       * Reuse old sockets
       */
      if (setsockopt(fd_ptr->fd, SOL_SOCKET, SO_REUSEADDR, (sockopt_val_t)&value, sizeof(value)) < 0) {
         berrno be;
         Emsg1(M_WARNING, 0, _("Cannot set SO_REUSEADDR on socket: %s\n"),
               be.bstrerror());
      }

      tmax = 30 * (60 / 5);        /* wait 30 minutes max */
      for (tlog = 0; bind(fd_ptr->fd, ipaddr->get_sockaddr(), ipaddr->get_sockaddr_len()) < 0; tlog -= 5) {
         berrno be;
         if (tlog <= 0) {
            tlog = 2 * 60;         /* Complain every 2 minutes */
            Emsg2(M_WARNING, 0, _("Cannot bind port %d: ERR=%s: Retrying ...\n"),
                  ntohs(fd_ptr->port), be.bstrerror());
         }
         bmicrosleep(5, 0);
         if (--tmax <= 0) {
            Emsg2(M_ABORT, 0, _("Cannot bind port %d: ERR=%s.\n"), ntohs(fd_ptr->port),
                  be.bstrerror());
         }
      }

      listen(fd_ptr->fd, 50);      /* tell system we are ready */
      sockfds->append(fd_ptr);

#ifdef HAVE_POLL
      nfds++;
#endif
   }

   /*
    * Start work queue thread
    */
   if ((status = workq_init(client_wq, max_clients, handle_client_request)) != 0) {
      berrno be;
      be.set_errno(status);
      Emsg1(M_ABORT, 0, _("Could not init client queue: ERR=%s\n"), be.bstrerror());
   }

#ifdef HAVE_POLL
   /*
    * Allocate on stack from -- no need to free
    */
   pfds = (struct pollfd *)alloca(sizeof(struct pollfd) * nfds);
   memset(pfds, 0, sizeof(struct pollfd) * nfds);

   nfds = 0;
   events = POLLIN;
#if defined(POLLRDNORM)
   events |= POLLRDNORM;
#endif
#if defined(POLLRDBAND)
   events |= POLLRDBAND;
#endif
#if defined(POLLPRI)
   events |= POLLPRI;
#endif

   foreach_alist(fd_ptr, sockfds) {
      pfds[nfds].fd = fd_ptr->fd;
      pfds[nfds].events = events;
      nfds++;
   }
#endif

   /*
    * Wait for a connection from the client process.
    */
   while (!quit) {
#ifndef HAVE_POLL
      unsigned int maxfd = 0;
      fd_set sockset;
      FD_ZERO(&sockset);

      foreach_alist(fd_ptr, sockfds) {
         FD_SET((unsigned)fd_ptr->fd, &sockset);
         if ((unsigned)fd_ptr->fd > maxfd) {
            maxfd = fd_ptr->fd;
         }
      }

      errno = 0;
      if ((status = select(maxfd + 1, &sockset, NULL, NULL, NULL)) < 0) {
         berrno be;                   /* capture errno */
         if (errno == EINTR) {
            continue;
         }
         Emsg1(M_FATAL, 0, _("Error in select: %s\n"), be.bstrerror());
         break;
      }

      foreach_alist(fd_ptr, sockfds) {
         if (FD_ISSET(fd_ptr->fd, &sockset)) {
#else
      int cnt;

      errno = 0;
      if ((status = poll(pfds, nfds, -1)) < 0) {
         berrno be;                   /* capture errno */
         if (errno == EINTR) {
            continue;
         }
         Emsg1(M_FATAL, 0, _("Error in poll: %s\n"), be.bstrerror());
         break;
      }

      cnt = 0;
      foreach_alist(fd_ptr, sockfds) {
         if (pfds[cnt++].revents & events) {
#endif
            /*
             * Got a connection, now accept it.
             */
            do {
               clilen = sizeof(cli_addr);
               newsockfd = accept(fd_ptr->fd, &cli_addr, &clilen);
            } while (newsockfd < 0 && errno == EINTR);
            if (newsockfd < 0) {
               continue;
            }
#ifdef HAVE_LIBWRAP
            P(mutex);              /* hosts_access is not thread safe */
            request_init(&request, RQ_DAEMON, my_name, RQ_FILE, newsockfd, 0);
            fromhost(&request);
            if (!hosts_access(&request)) {
               V(mutex);
               Jmsg2(NULL, M_SECURITY, 0,
                     _("Connection from %s:%d refused by hosts.access\n"),
                     sockaddr_to_ascii(&cli_addr, buf, sizeof(buf)),
                     sockaddr_get_port(&cli_addr));
               close(newsockfd);
               continue;
            }
            V(mutex);
#endif

            /*
             * Receive notification when connection dies.
             */
            if (setsockopt(newsockfd, SOL_SOCKET, SO_KEEPALIVE, (sockopt_val_t)&value, sizeof(value)) < 0) {
               berrno be;
               Emsg1(M_WARNING, 0, _("Cannot set SO_KEEPALIVE on socket: %s\n"), be.bstrerror());
            }

            /*
             * See who client is. i.e. who connected to us.
             */
            P(mutex);
            sockaddr_to_ascii(&cli_addr, buf, sizeof(buf));
            V(mutex);

            BSOCK *bs;
            bs = New(BSOCK_TCP);
            if (nokeepalive) {
               bs->clear_keepalive();
            }

            bs->m_fd = newsockfd;
            bs->set_who(bstrdup("client"));
            bs->set_host(bstrdup(buf));
            bs->set_port(ntohs(fd_ptr->port));
            memset(&bs->peer_addr, 0, sizeof(bs->peer_addr));
            memcpy(&bs->client_addr, &cli_addr, sizeof(bs->client_addr));

            /*
             * Queue client to be served
             */
            if ((status = workq_add(client_wq, (void *)bs, NULL, 0)) != 0) {
               berrno be;
               be.set_errno(status);
               Jmsg1(NULL, M_ABORT, 0, _("Could not add job to client queue: ERR=%s\n"),
                     be.bstrerror());
            }
         }
      }
   }

   cleanup_bnet_thread_server_tcp(sockfds, client_wq);
}
