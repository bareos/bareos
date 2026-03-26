/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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
#include <stdexcept>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

void ProxyServer::Stop()
{
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
}

void ProxyServer::Run()
{
  struct addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo* res = nullptr;
  const std::string port_str = std::to_string(cfg_.port);
  int rc = getaddrinfo(cfg_.bind_host.empty() ? nullptr
                                              : cfg_.bind_host.c_str(),
                       port_str.c_str(), &hints, &res);
  if (rc != 0) {
    throw std::runtime_error(std::string("ProxyServer: getaddrinfo: ")
                             + gai_strerror(rc));
  }

  for (auto* ai = res; ai != nullptr; ai = ai->ai_next) {
    listen_fd_ = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (listen_fd_ < 0) { continue; }

    int opt = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (::bind(listen_fd_, ai->ai_addr, ai->ai_addrlen) == 0) { break; }
    ::close(listen_fd_);
    listen_fd_ = -1;
  }
  freeaddrinfo(res);

  if (listen_fd_ < 0) {
    throw std::runtime_error("ProxyServer: could not bind to "
                             + cfg_.bind_host + ":" + port_str);
  }

  if (::listen(listen_fd_, 16) < 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    throw std::runtime_error("ProxyServer: listen() failed");
  }

  fprintf(stderr, "[proxy] listening on ws://%s:%d\n",
          cfg_.bind_host.c_str(), cfg_.port);
  fprintf(stderr, "[proxy] default director: %s @ %s:%d\n",
          cfg_.director.name.c_str(), cfg_.director.host.c_str(),
          cfg_.director.port);

  while (true) {
    struct sockaddr_storage client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    int client_fd
        = ::accept(listen_fd_,
                   reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0) {
      // listen_fd_ was closed by Stop()
      break;
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
      RunProxySession(cfd, peer, dir);
    }).detach();
  }

  fprintf(stderr, "[proxy] accept loop exited\n");
}
