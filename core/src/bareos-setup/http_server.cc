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
#include "http/http_server.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <iostream>
#include <cerrno>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "embedded_assets.h"
#include "http_server.h"
#include "http_protocol.h"
#include "setup_steps.h"

// ---- helpers ---------------------------------------------------------------

namespace {

std::atomic_bool g_shutdown_requested{false};
std::atomic_int g_server_fd{-1};
constexpr int kMaxConcurrentWorkers = 64;
constexpr auto kHttpTimeout = std::chrono::seconds(10);

bool IsLoopbackPeer(const sockaddr_storage& peer)
{
  if (peer.ss_family == AF_INET) {
    const auto* addr = reinterpret_cast<const sockaddr_in*>(&peer);
    return (ntohl(addr->sin_addr.s_addr) & 0xff000000U) == 0x7f000000U;
  }
  if (peer.ss_family == AF_INET6) {
    const auto* addr = reinterpret_cast<const sockaddr_in6*>(&peer);
    if (IN6_IS_ADDR_LOOPBACK(&addr->sin6_addr)) return true;
    if (IN6_IS_ADDR_V4MAPPED(&addr->sin6_addr)) {
      uint32_t ipv4;
      std::memcpy(&ipv4, &addr->sin6_addr.s6_addr[12], sizeof(ipv4));
      return (ntohl(ipv4) & 0xff000000U) == 0x7f000000U;
    }
  }
  return false;
}

}  // namespace

/** Decode percent-encoded octets (and '+' as space) in a URL query
 * component, as produced by JavaScript's encodeURIComponent(). */
static std::string UrlDecode(const std::string& value)
{
  std::string result;
  result.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()
        && std::isxdigit(static_cast<unsigned char>(value[i + 1]))
        && std::isxdigit(static_cast<unsigned char>(value[i + 2]))) {
      std::string hex = value.substr(i + 1, 2);
      result += static_cast<char>(std::stoi(hex, nullptr, 16));
      i += 2;
    } else if (value[i] == '+') {
      result += ' ';
    } else {
      result += value[i];
    }
  }
  return result;
}

// ---- Static file serving --------------------------------------------------

/** Find an embedded asset by URL path. Returns nullptr if not found. */
static const EmbeddedFile* FindAsset(const std::string& path)
{
  for (size_t i = 0; i < kEmbeddedFilesCount; ++i) {
    if (path == kEmbeddedFiles[i].path) return &kEmbeddedFiles[i];
  }
  return nullptr;
}

/** Find /index.html (SPA fallback). */
static const EmbeddedFile* FindIndexHtml() { return FindAsset("/index.html"); }

static void ServeStaticFile(int fd, const std::string& path)
{
  const EmbeddedFile* ef = FindAsset(path);
  if (!ef) ef = FindIndexHtml();  // SPA fallback
  if (!ef) {
    SendHttpResponse(fd, kHttpTimeout, "HTTP/1.1 404 Not Found", "Not Found",
                     {{"Content-Type", "text/plain"}});
    return;
  }

  const std::string_view body(reinterpret_cast<const char*>(ef->data),
                              ef->size);
  if (ef->gzipped) {
    SendHttpResponse(fd, kHttpTimeout, "HTTP/1.1 200 OK", body,
                     {{"Content-Type", ef->mime},
                      {"Content-Encoding", "gzip"},
                      {"Cache-Control", "no-cache"}});
  } else {
    SendHttpResponse(
        fd, kHttpTimeout, "HTTP/1.1 200 OK", body,
        {{"Content-Type", ef->mime}, {"Cache-Control", "no-cache"}});
  }
}

// ---- Main server loop -----------------------------------------------------

void RunHttpServer(const std::string& bind_address,
                   int port,
                   const std::string& setup_token,
                   WsHandler ws_handler)
{
  g_shutdown_requested = false;
  auto listen_sockets = CreateHttpListenSockets(
      bind_address, static_cast<uint16_t>(port), 16, true);
  const int srv = listen_sockets.front();
  for (size_t i = 1; i < listen_sockets.size(); ++i) {
    close(listen_sockets[i]);
  }
  g_server_fd = srv;

  const std::string url_host = bind_address.find(':') == std::string::npos
                                   ? bind_address
                                   : "[" + bind_address + "]";
  std::cout << "bareos-setup listening on http://" << url_host << ":" << port
            << "/\n"
            << std::flush;

  auto active_workers = std::make_shared<std::atomic_int>(0);
  while (!g_shutdown_requested) {
    sockaddr_storage peer{};
    socklen_t peer_len = sizeof(peer);
    int fd = accept(srv, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (fd < 0) {
      if (g_shutdown_requested) break;
      continue;
    }
    if (active_workers->fetch_add(1, std::memory_order_relaxed)
        >= kMaxConcurrentWorkers) {
      active_workers->fetch_sub(1, std::memory_order_relaxed);
      try {
        SendHttpResponse(fd, kHttpTimeout, "HTTP/1.1 503 Service Unavailable",
                         "Service Unavailable");
      } catch (const std::exception& error) {
        std::cerr << "Failed to send HTTP 503 response: " << error.what()
                  << "\n";
      }
      close(fd);
      continue;
    }

    // Handle each connection in its own thread
    const bool peer_is_loopback = IsLoopbackPeer(peer);
    std::thread([fd, setup_token, ws_handler, peer_is_loopback,
                 active_workers]() {
      struct WorkerGuard {
        int fd;
        std::shared_ptr<std::atomic_int> active_workers;
        ~WorkerGuard()
        {
          close(fd);
          active_workers->fetch_sub(1, std::memory_order_relaxed);
        }
      } guard{fd, active_workers};
      try {
        auto request = ReadHttpRequest(fd, kHttpTimeout);
        if (IsWebSocketUpgradeRequest(request)) {
          const auto origin_header = request.HeaderValue("Origin");
          const auto host_header = request.HeaderValue("Host");
          const std::string origin(origin_header.value_or(""));
          const std::string host(host_header.value_or(""));
          const std::string& path = request.target;
          const auto path_end = path.find('?');
          const std::string request_path
              = path_end == std::string::npos ? path : path.substr(0, path_end);
          const auto query = path.find('?');
          const std::string query_string
              = query == std::string::npos ? "" : path.substr(query + 1);
          bool token_valid = false;
          std::istringstream query_stream(query_string);
          std::string parameter;
          while (std::getline(query_stream, parameter, '&')) {
            constexpr const char kTokenPrefix[] = "token=";
            constexpr size_t kTokenPrefixLen = sizeof(kTokenPrefix) - 1;
            if (parameter.compare(0, kTokenPrefixLen, kTokenPrefix) == 0
                && UrlDecode(parameter.substr(kTokenPrefixLen))
                       == setup_token) {
              token_valid = true;
              break;
            }
          }
          if (request_path != "/ws" || !IsValidSetupOrigin(origin, host)
              || !token_valid) {
            SendHttpResponse(fd, kHttpTimeout, "HTTP/1.1 403 Forbidden",
                             "Forbidden\n");
            return;
          }
          ws_handler(fd, peer_is_loopback, std::move(request.raw_headers),
                     std::move(request.pending_input));
        } else {
          std::string path = std::move(request.target);
          const auto query = path.find('?');
          if (query != std::string::npos) { path.resize(query); }
          ServeStaticFile(fd, path);
        }
      } catch (const std::exception& error) {
        std::cerr << "HTTP request failed: " << error.what() << "\n";
      }
    }).detach();
  }

  const int fd = g_server_fd.exchange(-1);
  if (fd >= 0) close(fd);
}

void RequestHttpServerShutdown()
{
  g_shutdown_requested = true;
  const int fd = g_server_fd.exchange(-1);
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }
}
