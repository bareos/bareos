/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
#include "proxy_server.h"
#include "proxy_session.h"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

bool IsTransientAcceptError(int err)
{
  switch (err) {
    case EINTR:
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
    case ECONNABORTED:
    case ENETDOWN:
    case ENOPROTOOPT:
    case EHOSTDOWN:
#ifdef ENONET
    case ENONET:
#endif
    case EHOSTUNREACH:
    case EOPNOTSUPP:
    case ENETUNREACH:
    case ENOBUFS:
    case ENOMEM:
    case EMFILE:
    case ENFILE:
    case EPROTO:
      return true;
    default:
      return false;
  }
}

}  // namespace

void ProxyServer::Stop()
{
  stop_requested_ = 1;
}

void ProxyServer::CleanupSockets()
{
  for (int fd : listen_fds_) {
    if (fd >= 0) { ::close(fd); }
  }
  listen_fds_.clear();
}

void ProxyServer::Run()
{
  struct addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo* res = nullptr;
  const std::string port_str = std::to_string(cfg_.port);
  int rc
      = getaddrinfo(cfg_.bind_host.empty() ? nullptr : cfg_.bind_host.c_str(),
                    port_str.c_str(), &hints, &res);
  if (rc != 0) {
    throw std::runtime_error(std::string("ProxyServer: getaddrinfo: ")
                             + gai_strerror(rc));
  }

  // Bind a listen socket for every address returned (both IPv4 and IPv6 when
  // the host resolves to multiple addresses, e.g. "localhost" → 127.0.0.1 +
  // ::1).  This matches the Python websockets library behaviour and ensures
  // clients can always connect regardless of which protocol their browser uses.
  for (const struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
    int fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) { continue; }

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (::bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 && ::listen(fd, 16) == 0) {
      listen_fds_.push_back(fd);
    } else {
      ::close(fd);
    }
  }
  freeaddrinfo(res);

  if (listen_fds_.empty()) {
    throw std::runtime_error("ProxyServer: could not bind to any address for "
                             + cfg_.bind_host + ":" + port_str);
  }

  fprintf(stderr, "[proxy] listening on ws://%s:%d (%zu socket(s))\n",
          cfg_.bind_host.c_str(), cfg_.port, listen_fds_.size());
  fprintf(stderr, "[proxy] default director: %s @ %s:%d\n",
          cfg_.director.name.c_str(), cfg_.director.host.c_str(),
          cfg_.director.port);

  // Accept loop: poll all listen sockets, accept on whichever is ready.
  while (true) {
    if (stop_requested_) { break; }

    std::vector<struct pollfd> pfds;
    pfds.reserve(listen_fds_.size());
    for (int fd : listen_fds_) { pfds.push_back({fd, POLLIN, 0}); }

    int nready = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 1000);
    if (nready < 0) {
      if (errno == EINTR && !stop_requested_) { continue; }
      break;  // Unexpected poll error.
    }

    if (stop_requested_) { break; }

    bool any_closed = false;
    for (const auto& pfd : pfds) {
      if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        any_closed = true;
        continue;
      }
      if (!(pfd.revents & POLLIN)) { continue; }

      struct sockaddr_storage client_addr{};
      socklen_t addr_len = sizeof(client_addr);
      int client_fd = ::accept(
          pfd.fd, reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
      if (client_fd < 0) {
        if (stop_requested_) {
          any_closed = true;
          continue;
        }
        if (IsTransientAcceptError(errno)) { continue; }
        fprintf(stderr, "[proxy] accept failed: %s\n", std::strerror(errno));
        any_closed = true;
        continue;
      }

      // Build a human-readable peer string
      char host_buf[NI_MAXHOST] = {};
      char port_buf[NI_MAXSERV] = {};
      getnameinfo(reinterpret_cast<struct sockaddr*>(&client_addr), addr_len,
                  host_buf, sizeof(host_buf), port_buf, sizeof(port_buf),
                  NI_NUMERICHOST | NI_NUMERICSERV);
      std::string peer = std::string(host_buf) + ":" + port_buf;

      // Move fd and defaults into a detached thread — thread owns the socket.
      int cfd = client_fd;
      DefaultDirectorConfig dir = cfg_.director;
      std::thread([cfd, peer, dir]() {
        try {
          RunProxySession(cfd, peer, dir);
        } catch (const std::exception& ex) {
          fprintf(stderr, "[proxy] %s session aborted: %s\n", peer.c_str(),
                  ex.what());
        } catch (...) {
          fprintf(stderr, "[proxy] %s session aborted (unknown exception)\n",
                  peer.c_str());
        }
      }).detach();
    }

    if (any_closed) { break; }  // Stop() was called.
  }

  CleanupSockets();
  fprintf(stderr, "[proxy] accept loop exited\n");
}
