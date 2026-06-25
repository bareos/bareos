/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#include "proxy_log.h"
#include "proxy_session.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr int kListenBacklog = SOMAXCONN;

struct FdGuard {
  FdGuard() = default;
  explicit FdGuard(int fd_value) : fd(fd_value) {}

  ~FdGuard()
  {
    if (fd >= 0) { ::close(fd); }
  }

  FdGuard(const FdGuard&) = delete;
  FdGuard& operator=(const FdGuard&) = delete;

  FdGuard(FdGuard&& other) noexcept : fd(other.fd) { other.fd = -1; }

  FdGuard& operator=(FdGuard&& other) noexcept
  {
    if (this != &other) {
      if (fd >= 0) { ::close(fd); }
      fd = other.fd;
      other.fd = -1;
    }
    return *this;
  }

  int get() const { return fd; }

 private:
  int fd{-1};
};

// accept(2) can fail transiently when interrupted, when the listening socket is
// momentarily not ready anymore, or when the kernel reports a temporary
// resource/network condition for one pending connection. In the accept loop we
// just ignore these and wait for the next readiness notification.
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

bool SendHttpServiceUnavailable(int fd)
{
  const char response[] =
      "HTTP/1.1 503 Service Unavailable\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 27\r\n"
      "Connection: close\r\n"
      "\r\n"
      "Connection limit exceeded.\r\n";

  return ::send(fd, response, sizeof(response) - 1, MSG_NOSIGNAL) > 0;
}

}  // namespace

volatile std::sig_atomic_t g_proxy_shutdown_requested = 0;

void RunProxyServer(const ProxyConfig& cfg)
{
  struct addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo* res = nullptr;
  const std::string port_str = std::to_string(cfg.port);
  int rc = getaddrinfo(
      cfg.bind_address.empty() ? nullptr : cfg.bind_address.c_str(),
      port_str.c_str(), &hints, &res);
  if (rc != 0) {
    throw std::runtime_error(std::string("ProxyServer: getaddrinfo: ")
                             + gai_strerror(rc));
  }

  std::vector<FdGuard> listen_fds;

  // Bind a listen socket for every address returned (both IPv4 and IPv6 when
  // the host resolves to multiple addresses, e.g. "localhost" → 127.0.0.1 +
  // ::1).  This matches the Python websockets library behaviour and ensures
  // clients can always connect regardless of which protocol their browser uses.
  for (const struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
    int fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) { continue; }
    FdGuard fd_guard(fd);

    int opt = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (::bind(fd, ai->ai_addr, ai->ai_addrlen) == 0
        && ::listen(fd, kListenBacklog) == 0) {
      listen_fds.push_back(std::move(fd_guard));
    }
  }
  freeaddrinfo(res);

  if (listen_fds.empty()) {
    throw std::runtime_error("ProxyServer: could not bind to any address for "
                             + cfg.bind_address + ":" + port_str);
  }

  PROXY_LOG_INFO("", "listening on ws://%s:%d (%zu socket(s))",
                 cfg.bind_address.c_str(), cfg.port, listen_fds.size());
  PROXY_LOG_INFO("", "configured directors: %zu",
                 cfg.configured_directors.size());

  // Build the pollfd array once; it doesn't change.
  std::vector<struct pollfd> pfds;
  pfds.reserve(listen_fds.size());
  // poll() overwrites revents on each call, so there is no need to clear it.
  for (const auto& fd : listen_fds) { pfds.push_back({fd.get(), POLLIN, 0}); }

  // Track unauthenticated connections to enforce DoS limit
  std::atomic<int> unauthenticated_connection_count(0);

  // Accept loop: poll all listen sockets, accept on whichever is ready.
  while (true) {
    if (g_proxy_shutdown_requested) { break; }

    int nready = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), 1000);
    if (nready < 0) {
      if (errno == EINTR && !g_proxy_shutdown_requested) {
        PROXY_LOG_DEBUG("", "poll interrupted by signal");
        continue;
      }
      break;  // Unexpected poll error.
    }

    if (g_proxy_shutdown_requested) { break; }

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
        if (g_proxy_shutdown_requested) {
          any_closed = true;
          continue;
        }
        if (IsTransientAcceptError(errno)) {
          PROXY_LOG_DEBUG("", "accept transient failure: %s",
                          std::strerror(errno));
          continue;
        }
        PROXY_LOG_ERROR("", "accept failed: %s", std::strerror(errno));
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

      // Check unauthenticated connection limit for DoS protection
      if (unauthenticated_connection_count.load() >=
          cfg.max_unauthenticated_connections) {
        PROXY_LOG_DEBUG(peer, "rejecting connection (unauthenticated limit %d)",
                        cfg.max_unauthenticated_connections);
        SendHttpServiceUnavailable(client_fd);
        ::close(client_fd);
        continue;
      }

      // Prepare socket descriptor and config for the detached thread.
      // Each thread needs its own copy of the config so it can be captured
      // and owned independently for the lifetime of the detached thread.
      int cfd = client_fd;
      ProxyConfig session_cfg = cfg;
      try {
        // Increment counter; it will be decremented when thread exits
        unauthenticated_connection_count.fetch_add(1);
        std::thread([cfd, peer, session_cfg,
                     &unauthenticated_connection_count]() {
          try {
            RunProxySession(cfd, peer, session_cfg);
          } catch (const std::exception& ex) {
            PROXY_LOG_ERROR(peer, "session aborted: %s", ex.what());
          } catch (...) {
            PROXY_LOG_ERROR(peer, "session aborted (unknown exception)");
          }
          // Decrement when session ends (whether auth succeeded or not)
          unauthenticated_connection_count.fetch_sub(1);
        }).detach();
      } catch (const std::system_error& ex) {
        PROXY_LOG_ERROR(peer, "failed to start session thread: %s", ex.what());
        unauthenticated_connection_count.fetch_sub(1);
        ::close(cfd);
        continue;
      }
    }

    if (any_closed) { break; }  // A listen socket became unusable.
  }

  PROXY_LOG_INFO("", "accept loop exited");
}
