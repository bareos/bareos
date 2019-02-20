/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
/*
 * @file
 * tcp server code
 */

#include "include/bareos.h"
#include "lib/bnet_server_tcp.h"
#include "lib/bsys.h"

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
#include <atomic>

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

/**
 * Stop the Threaded Network Server if its realy running in a separate thread.
 * e.g. set the quit flag and wait for the other thread to exit cleanly.
 */
void BnetStopAndWaitForThreadServerTcp(pthread_t tid)
{
  Dmsg0(100, "BnetThreadServer: Request Stop\n");
  quit = true;
  if (!pthread_equal(tid, pthread_self())) {
    pthread_kill(tid, TIMEOUT_SIGNAL);
    Dmsg0(100, "BnetThreadServer: Wait until finished\n");
    pthread_join(tid, nullptr);
    Dmsg0(100, "BnetThreadServer: finished\n");
  }
}

/**
 * Perform a cleanup for the Threaded Network Server check if there is still
 * something to do or that the cleanup already took place.
 */
static void CleanupBnetThreadServerTcp(alist* sockfds, workq_t* client_wq)
{
  Dmsg0(100, "CleanupBnetThreadServerTcp: start\n");

  if (sockfds && !sockfds->empty()) {
    s_sockfd* fd_ptr = (s_sockfd*)sockfds->first();
    while (fd_ptr) {
      close(fd_ptr->fd);
      fd_ptr = (s_sockfd*)sockfds->next();
    }
    sockfds->destroy();
  }

  if (client_wq) {
    int status = WorkqDestroy(client_wq);
    if (status != 0) {
      BErrNo be;
      be.SetErrno(status);
      Emsg1(M_ERROR, 0, _("Could not destroy client queue: ERR=%s\n"),
            be.bstrerror());
    }
  }
  Dmsg0(100, "CleanupBnetThreadServerTcp: finish\n");
}

class BNetThreadServerCleanupObject {
 public:
  BNetThreadServerCleanupObject(alist* sockfds, workq_t* client_wq)
      : sockfds_(sockfds), client_wq_(client_wq)
  {
  }

  ~BNetThreadServerCleanupObject()
  {
    CleanupBnetThreadServerTcp(sockfds_, client_wq_);
  }

 private:
  alist* sockfds_;
  workq_t* client_wq_;
};

/**
 * Become Threaded Network Server
 *
 * This function is able to handle multiple server ips in
 * ipv4 and ipv6 style. The Addresse are give in a comma
 * separated string in bind_addr
 *
 * At the moment it is impossible to bind to different ports.
 */
void BnetThreadServerTcp(
    dlist* addr_list,
    int max_clients,
    alist* sockfds,
    workq_t* client_wq,
    bool nokeepalive,
    void* HandleConnectionRequest(ConfigurationParser* config, void* bsock),
    ConfigurationParser* config,
    std::atomic<BnetServerState>* const server_state)
{
  int newsockfd, status;
  socklen_t clilen;
  struct sockaddr cli_addr; /* client's address */
  int tlog;
  int value;
#ifdef HAVE_LIBWRAP
  struct request_info request;
#endif
  IPADDR *ipaddr, *next, *to_free;
  s_sockfd* fd_ptr = NULL;
  char buf[128];
#ifdef HAVE_POLL
  nfds_t nfds;
  int events;
  struct pollfd* pfds;
#endif

  char allbuf[256 * 10];

  BNetThreadServerCleanupObject cleanup_object(sockfds, client_wq);

  quit = false;
  if (server_state) { server_state->store(BnetServerState::kStarting); }

  /*
   * Remove any duplicate addresses.
   */
  for (ipaddr = (IPADDR*)addr_list->first(); ipaddr;
       ipaddr = (IPADDR*)addr_list->next(ipaddr)) {
    next = (IPADDR*)addr_list->next(ipaddr);
    while (next) {
      /*
       * See if the addresses match.
       */
      if (ipaddr->GetSockaddrLen() == next->GetSockaddrLen() &&
          memcmp(ipaddr->get_sockaddr(), next->get_sockaddr(),
                 ipaddr->GetSockaddrLen()) == 0) {
        to_free = next;
        next = (IPADDR*)addr_list->next(next);
        addr_list->remove(to_free);
        delete to_free;
      } else {
        next = (IPADDR*)addr_list->next(next);
      }
    }
  }

  Dmsg1(100, "Addresses %s\n",
        BuildAddressesString(addr_list, allbuf, sizeof(allbuf)));

  if (nokeepalive) {
    value = 0;
  } else {
    value = 1;
  }

#ifdef HAVE_POLL
  nfds = 0;
#endif
  foreach_dlist (ipaddr, addr_list) {
    /*
     * Allocate on stack from -- no need to free
     */
    fd_ptr = (s_sockfd*)alloca(sizeof(s_sockfd));
    fd_ptr->port = ipaddr->GetPortNetOrder();

    /*
     * Open a TCP socket
     */
    for (tlog = 60;
         (fd_ptr->fd = socket(ipaddr->GetFamily(), SOCK_STREAM, 0)) < 0;
         tlog -= 10) {
      if (tlog <= 0) {
        BErrNo be;
        char curbuf[256];
        Emsg3(M_ABORT, 0,
              _("Cannot open stream socket. ERR=%s. Current %s All %s\n"),
              be.bstrerror(), ipaddr->build_address_str(curbuf, sizeof(curbuf)),
              BuildAddressesString(addr_list, allbuf, sizeof(allbuf)));
      }
      Bmicrosleep(10, 0);
    }

    if (setsockopt(fd_ptr->fd, SOL_SOCKET, SO_REUSEADDR, (sockopt_val_t)&value,
                   sizeof(value)) < 0) {
      BErrNo be;
      Emsg1(M_WARNING, 0, _("Cannot set SO_REUSEADDR on socket: %s\n"),
            be.bstrerror());
    }

    int tries = 3;
    int wait_seconds = 5;
    bool ok = false;

    do {
      int ret =
          bind(fd_ptr->fd, ipaddr->get_sockaddr(), ipaddr->GetSockaddrLen());
      if (ret < 0) {
        BErrNo be;
        Emsg2(M_WARNING, 0, _("Cannot bind port %d: ERR=%s: Retrying ...\n"),
              ntohs(fd_ptr->port), be.bstrerror());
        Bmicrosleep(wait_seconds, 0);
      } else {
        ok = true;
      }
    } while (!ok && --tries);

    if (!ok) {
      BErrNo be;
      Emsg2(M_ERROR, 0, _("Cannot bind port %d: ERR=%s.\n"),
            ntohs(fd_ptr->port), be.bstrerror());
      if (server_state) { server_state->store(BnetServerState::kError); }
      return;
    }

    listen(fd_ptr->fd, 50); /* tell system we are ready */
    sockfds->append(fd_ptr);

#ifdef HAVE_POLL
    nfds++;
#endif
  }

  /*
   * Start work queue thread
   */
  if ((status = WorkqInit(client_wq, max_clients, HandleConnectionRequest)) !=
      0) {
    BErrNo be;
    be.SetErrno(status);
    Emsg1(M_ABORT, 0, _("Could not init client queue: ERR=%s\n"),
          be.bstrerror());
  }

#ifdef HAVE_POLL
  /*
   * Allocate on stack from -- no need to free
   */
  pfds = (struct pollfd*)alloca(sizeof(struct pollfd) * nfds);
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

  foreach_alist (fd_ptr, sockfds) {
    pfds[nfds].fd = fd_ptr->fd;
    pfds[nfds].events = events;
    nfds++;
  }
#endif

  if (server_state) { server_state->store(BnetServerState::kStarted); }

  while (!quit) {
#ifndef HAVE_POLL
    unsigned int maxfd = 0;
    fd_set sockset;
    FD_ZERO(&sockset);

    foreach_alist (fd_ptr, sockfds) {
      FD_SET((unsigned)fd_ptr->fd, &sockset);
      if ((unsigned)fd_ptr->fd > maxfd) { maxfd = fd_ptr->fd; }
    }

    errno = 0;
    if ((status = select(maxfd + 1, &sockset, NULL, NULL, NULL)) < 0) {
      BErrNo be; /* capture errno */
      if (errno == EINTR) { continue; }
      if (server_state) { server_state->store(BnetServerState::kError); }
      Emsg1(M_FATAL, 0, _("Error in select: %s\n"), be.bstrerror());
      break;
    }

    foreach_alist (fd_ptr, sockfds) {
      if (FD_ISSET(fd_ptr->fd, &sockset)) {
#else
    int cnt;

    errno = 0;
    if ((status = poll(pfds, nfds, -1)) < 0) {
      BErrNo be; /* capture errno */
      if (errno == EINTR) { continue; }
      Emsg1(M_FATAL, 0, _("Error in poll: %s\n"), be.bstrerror());

      break;
    }

    cnt = 0;
    foreach_alist (fd_ptr, sockfds) {
      if (pfds[cnt++].revents & events) {
#endif
        /*
         * Got a connection, now accept it.
         */
        do {
          clilen = sizeof(cli_addr);
          newsockfd = accept(fd_ptr->fd, &cli_addr, &clilen);
        } while (newsockfd < 0 && errno == EINTR);
        if (newsockfd < 0) { continue; }
#ifdef HAVE_LIBWRAP
        P(mutex); /* HostsAccess is not thread safe */
        request_init(&request, RQ_DAEMON, my_name, RQ_FILE, newsockfd, 0);
        fromhost(&request);
        if (!HostsAccess(&request)) {
          V(mutex);
          Jmsg2(NULL, M_SECURITY, 0,
                _("Connection from %s:%d refused by hosts.access\n"),
                SockaddrToAscii(&cli_addr, buf, sizeof(buf)),
                SockaddrGetPort(&cli_addr));
          close(newsockfd);
          continue;
        }
        V(mutex);
#endif

        /*
         * Receive notification when connection dies.
         */
        if (setsockopt(newsockfd, SOL_SOCKET, SO_KEEPALIVE,
                       (sockopt_val_t)&value, sizeof(value)) < 0) {
          BErrNo be;
          Emsg1(M_WARNING, 0, _("Cannot set SO_KEEPALIVE on socket: %s\n"),
                be.bstrerror());
        }

        /*
         * See who client is. i.e. who connected to us.
         */
        P(mutex);
        SockaddrToAscii(&cli_addr, buf, sizeof(buf));
        V(mutex);

        BareosSocket* bs;
        bs = New(BareosSocketTCP);
        if (nokeepalive) { bs->ClearKeepalive(); }

        bs->fd_ = newsockfd;
        bs->SetWho(bstrdup("client"));
        bs->SetHost(bstrdup(buf));
        bs->SetPort(ntohs(fd_ptr->port));
        memset(&bs->peer_addr, 0, sizeof(bs->peer_addr));
        memcpy(&bs->client_addr, &cli_addr, sizeof(bs->client_addr));

        /*
         * Queue client to be served
         */
        if ((status = WorkqAdd(client_wq, config, (void*)bs, NULL)) != 0) {
          BErrNo be;
          be.SetErrno(status);
          Jmsg1(NULL, M_ABORT, 0,
                _("Could not add job to client queue: ERR=%s\n"),
                be.bstrerror());
        }
      }
    }
  }
  if (server_state) { server_state->store(BnetServerState::kEnded); }
}
