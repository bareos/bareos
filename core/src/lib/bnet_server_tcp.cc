/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
#include "lib/alist.h"
#include "lib/berrno.h"
#include "lib/bnet_server_tcp.h"
#include "lib/bsock_tcp.h"
#include "lib/bsys.h"
#include "lib/thread_list.h"
#include "lib/watchdog.h"
#include "lib/address_conf.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#if __has_include(<arpa/nameser.h>)
#  include <arpa/nameser.h>
#endif

#ifdef HAVE_POLL
#  include <poll.h>
#endif

#include <algorithm>
#include <atomic>
#include <array>
#include <vector>

#ifdef HAVE_WIN32
#  define socketClose(fd) ::closesocket(fd)
#else
#  define socketClose(fd) ::close(fd)
#endif

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static std::atomic<bool> quit{false};

/**
 * Stop the Threaded Network Server if its really running in a separate thread.
 * e.g. set the quit flag and wait for the other thread to exit cleanly.
 */
void BnetStopAndWaitForThreadServerTcp(pthread_t tid)
{
  Dmsg0(100, "BnetThreadServer: Request Stop\n");
  quit = true;
  if (!pthread_equal(tid, pthread_self())) {
    Dmsg0(100, "BnetThreadServer: Wait until finished\n");
    pthread_join(tid, nullptr);
    Dmsg0(100, "BnetThreadServer: finished\n");
  }
}

/**
 * Perform a cleanup for the Threaded Network Server check if there is still
 * something to do or that the cleanup already took place.
 */
static void CleanupBnetThreadServerTcp(ThreadList& thread_list)
{
  Dmsg0(100, "CleanupBnetThreadServerTcp: start\n");

  if (!thread_list.ShutdownAndWaitForThreadsToFinish()) {
    Emsg1(M_ERROR, 0, T_("Could not destroy thread list.\n"));
  }
  Dmsg0(100, "CleanupBnetThreadServerTcp: finish\n");
}

class BNetThreadServerCleanupObject {
 public:
  BNetThreadServerCleanupObject(ThreadList& thread_list)
      : thread_list_(thread_list)
  {
  }

  ~BNetThreadServerCleanupObject() { CleanupBnetThreadServerTcp(thread_list_); }

 private:
  ThreadList& thread_list_;
};

void RemoveDuplicateAddresses(dlist<IPADDR>* addr_list)
{
  IPADDR* ipaddr = nullptr;
  IPADDR* next = nullptr;
  IPADDR* to_free = nullptr;

  for (ipaddr = (IPADDR*)addr_list->first(); ipaddr;
       ipaddr = (IPADDR*)addr_list->next(ipaddr)) {
    next = (IPADDR*)addr_list->next(ipaddr);
    while (next) {
      // See if the addresses match.
      if (IsSameIpAddress(ipaddr, next)) {
        to_free = next;
        next = (IPADDR*)addr_list->next(next);
        addr_list->remove(to_free);
        delete to_free;
      } else {
        next = (IPADDR*)addr_list->next(next);
      }
    }
  }
}

static void LogAllAddresses(dlist<IPADDR>* addr_list)
{
  std::vector<char> buf(256 * addr_list->size());

  Dmsg1(100, "Addresses %s\n",
        BuildAddressesString(addr_list, buf.data(), buf.size()));
}

int OpenSocketAndBind(IPADDR* ipaddr,
                      dlist<IPADDR>* addr_list,
                      uint16_t port_number)
{
  int fd = -1;
  int tries = 0;

  do {
    ++tries;
    if ((fd = socket(ipaddr->GetFamily(), SOCK_STREAM, 0)) < 0) {
      Bmicrosleep(10, 0);
    }
  } while (fd < 0 && tries < 6);

  if (fd < 0) {
    BErrNo be;
    std::array<char, 256> buf1;
    std::vector<char> buf2(256 * addr_list->size());

    Emsg3(M_WARNING, 0,
          T_("Cannot open stream socket. ERR=%s. Current %s All %s\n"),
          be.bstrerror(), ipaddr->build_address_str(buf1.data(), buf1.size()),
          BuildAddressesString(addr_list, buf2.data(), buf2.size()));

    return -1;
  }

  int reuseaddress = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (sockopt_val_t)&reuseaddress,
                 sizeof(reuseaddress))
      < 0) {
    BErrNo be;
    Emsg1(M_WARNING, 0, T_("Cannot set SO_REUSEADDR on socket: %s\n"),
          be.bstrerror());
    return -2;
  }

  if (ipaddr->GetFamily() == AF_INET6) {
    int ipv6only_option_value = 1;
    socklen_t option_len = sizeof(int);

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY,
                   (sockopt_val_t)&ipv6only_option_value, option_len)
        < 0) {
      BErrNo be;
      Emsg1(M_WARNING, 0, T_("Cannot set IPV6_V6ONLY on socket: %s\n"),
            be.bstrerror());

      return -2;
    }
  }

  tries = 0;

  do {
    ++tries;
    if (bind(fd, ipaddr->get_sockaddr(), ipaddr->GetSockaddrLen()) != 0) {
      BErrNo be;
      char tmp[1024];
#ifdef HAVE_WIN32
      Emsg2(M_WARNING, 0,
            T_("Cannot bind address %s port %d ERR=%u. Retrying...\n"),
            ipaddr->GetAddress(tmp, sizeof(tmp) - 1), ntohs(port_number),
            WSAGetLastError());
#else
      Emsg2(M_WARNING, 0,
            T_("Cannot bind address %s port %d ERR=%s. Retrying...\n"),
            ipaddr->GetAddress(tmp, sizeof(tmp) - 1), ntohs(port_number),
            be.bstrerror());
#endif
      Bmicrosleep(5, 0);
    } else {
      // success
      return fd;
    }
  } while (tries < 3);

  return -3;
}

std::vector<s_sockfd> OpenAndBindSockets(dlist<IPADDR>* addr_list)
{
  RemoveDuplicateAddresses(addr_list);
  LogAllAddresses(addr_list);

  std::vector<s_sockfd> bound_sockets;
  IPADDR* ipaddr = nullptr;

  foreach_dlist (ipaddr, addr_list) {
    s_sockfd sock;
    sock.port = ipaddr->GetPortNetOrder();
    sock.fd = OpenSocketAndBind(ipaddr, addr_list, sock.port);

    if (sock.fd < 0) {
      BErrNo be;
      char tmp[1024];
#ifdef HAVE_WIN32
      Emsg2(M_ERROR, 0, T_("Cannot bind address %s port %d: ERR=%u.\n"),
            ipaddr->GetAddress(tmp, sizeof(tmp) - 1), ntohs(sock.port),
            WSAGetLastError());
#else
      Emsg2(M_ERROR, 0, T_("Cannot bind address %s port %d: ERR=%s.\n"),
            ipaddr->GetAddress(tmp, sizeof(tmp) - 1), ntohs(sock.port),
            be.bstrerror());
#endif
      return {};
    } else {
      bound_sockets.emplace_back(std::move(sock));
    }
  }

  return bound_sockets;
}

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
    std::vector<s_sockfd> bound_sockets,
    ThreadList& thread_list,
    std::function<void*(ConfigurationParser* config, void* bsock)>
        HandleConnectionRequest,
    ConfigurationParser* config,
    std::atomic<BnetServerState>* const server_state,
    std::function<void*(void* bsock)> UserAgentShutdownCallback,
    std::function<void()> CustomCallback)
{
  BNetThreadServerCleanupObject cleanup_object(thread_list);

  quit = false;  // allow other threads to set this true during initialization
  if (server_state) { server_state->store(BnetServerState::kStarting); }

#ifdef HAVE_POLL
  nfds_t number_of_filedescriptors = bound_sockets.size();
#endif

  for (auto& sock : bound_sockets) { listen(sock.fd, kListenBacklog); }

  thread_list.Init(HandleConnectionRequest, UserAgentShutdownCallback);

#ifdef HAVE_POLL
  struct pollfd* pfds = (struct pollfd*)alloca(sizeof(struct pollfd)
                                               * number_of_filedescriptors);
  memset(pfds, 0, sizeof(struct pollfd) * number_of_filedescriptors);

  int events = POLLIN;
#  if defined(POLLRDNORM)
  events |= POLLRDNORM;
#  endif
#  if defined(POLLRDBAND)
  events |= POLLRDBAND;
#  endif
#  if defined(POLLPRI)
  events |= POLLPRI;
#  endif

  int i = 0;

  for (auto& sock : bound_sockets) {
    pfds[i].fd = sock.fd;
    pfds[i].events = events;
    i++;
  }
#endif

  if (server_state) { server_state->store(BnetServerState::kStarted); }

  while (!quit) {
    if (CustomCallback) { CustomCallback(); }
#ifndef HAVE_POLL
    int maxfd = 0;
    fd_set sockset;
    FD_ZERO(&sockset);

    for (auto& sock : bound_sockets) {
      FD_SET(sock.fd, &sockset);
      maxfd = std::max(sock.fd, maxfd);
    }

    struct timeval timeout{.tv_sec = 1, .tv_usec = 0};

    errno = 0;
    int status = select(maxfd + 1, &sockset, NULL, NULL, &timeout);

    if (status == 0) {
      continue;  // timeout: check if thread should quit
    } else if (status < 0) {
      BErrNo be; /* capture errno */
      if (errno == EINTR) { continue; }
      if (server_state) { server_state->store(BnetServerState::kError); }
      Emsg1(M_FATAL, 0, T_("Error in select: %s\n"), be.bstrerror());
      break;
    }

    for (auto& sock : bound_sockets) {
      if (FD_ISSET(sock.fd, &sockset)) {
#else
    static constexpr int timeout_ms{1000};

    errno = 0;
    int status = poll(pfds, number_of_filedescriptors, timeout_ms);

    if (status == 0) {
      continue;  // timeout: check if thread should quit
    } else if (status < 0) {
      BErrNo be; /* capture errno */
      if (errno == EINTR) { continue; }
      Emsg1(M_FATAL, 0, T_("Error in poll: %s\n"), be.bstrerror());

      break;
    }

    int cnt = 0;
    for (auto& sock : bound_sockets) {
      if (pfds[cnt++].revents & events) {
#endif
        int newsockfd = -1;
        socklen_t clilen;
        struct sockaddr_storage cli_addr; /* client's address */

        do {
          clilen = sizeof(cli_addr);
          newsockfd = accept(sock.fd, reinterpret_cast<sockaddr*>(&cli_addr),
                             &clilen);
        } while (newsockfd < 0 && errno == EINTR);
        if (newsockfd < 0) { continue; }

#ifdef HAVE_LINUX_OS
#  ifdef TCP_ULP
        // without this you cannot enable ktls on linux
        if (setsockopt(newsockfd, SOL_TCP, TCP_ULP, "tls", sizeof("tls")) < 0) {
          BErrNo be;
          Dmsg1(250,
                "Cannot set TCP_ULP on socket: ERR=%s.\n"
                "Is the tls module not loaded?  kTLS will not work without it.",
                be.bstrerror());
        }
#  endif
#endif

        int keepalive = 1;
        if (setsockopt(newsockfd, SOL_SOCKET, SO_KEEPALIVE,
                       (sockopt_val_t)&keepalive, sizeof(keepalive))
            < 0) {
          BErrNo be;
          Emsg1(M_WARNING, 0, T_("Cannot set SO_KEEPALIVE on socket: %s\n"),
                be.bstrerror());
        }

        // See who client is. i.e. who connected to us.
        char buf[128];

        lock_mutex(mutex);
        SockaddrToAscii(&cli_addr, buf, sizeof(buf));
        unlock_mutex(mutex);

        BareosSocket* bs;
        bs = new BareosSocketTCP;

        bs->fd_ = newsockfd;
        bs->SetWho(strdup("client"));
        bs->SetHost(strdup(buf));
        bs->SetPort(ntohs(sock.port));
        memset(&bs->peer_addr, 0, sizeof(bs->peer_addr));
        memcpy(&bs->client_addr, &cli_addr, sizeof(bs->client_addr));

        if (!thread_list.CreateAndAddNewThread(config, bs)) {
          Jmsg1(NULL, M_ABORT, 0, T_("Could not add thread to list.\n"));
        }
      }
    }
  }
  if (server_state) { server_state->store(BnetServerState::kEnded); }
}

void close_socket(int fd)
{
#ifdef HAVE_WIN32
  closesocket(fd);
#else
  close(fd);
#endif
}
