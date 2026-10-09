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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#include "http_server.h"

#include <cstring>
#include <stdexcept>
#include <string>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

std::vector<int> CreateHttpListenSockets(std::string_view address,
                                         uint16_t port,
                                         int backlog,
                                         bool numeric_only)
{
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = address.empty() ? AI_PASSIVE : 0;
  if (numeric_only) { hints.ai_flags |= AI_NUMERICHOST; }

  addrinfo* results = nullptr;
  const std::string address_string(address);
  const std::string port_string = std::to_string(port);
  const int result = getaddrinfo(address.empty() ? nullptr : address_string.c_str(),
                                 port_string.c_str(), &hints, &results);
  if (result != 0) {
    throw std::runtime_error(std::string("getaddrinfo: ") + gai_strerror(result));
  }

  std::vector<int> sockets;
  for (const addrinfo* current = results; current; current = current->ai_next) {
    const int fd
        = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    if (fd < 0) { continue; }

    int reuse_address = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
               sizeof(reuse_address));
    if (current->ai_family == AF_INET6) {
      int ipv6_only = 1;
      if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &ipv6_only,
                     sizeof(ipv6_only))
          < 0) {
        close(fd);
        continue;
      }
    }

    if (bind(fd, current->ai_addr, current->ai_addrlen) == 0
        && listen(fd, backlog) == 0) {
      sockets.push_back(fd);
    } else {
      close(fd);
    }
  }
  freeaddrinfo(results);

  if (sockets.empty()) {
    throw std::runtime_error("could not bind to any address for "
                             + (address.empty() ? std::string("*")
                                                : address_string)
                             + ":" + port_string);
  }
  return sockets;
}
