/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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


#ifndef BAREOS_TESTS_BAREOS_TEST_SOCKETS_H_
#define BAREOS_TESTS_BAREOS_TEST_SOCKETS_H_

#include <memory>
#include <optional>

class BareosSocketTCP;
class BareosSocket;

class TestSockets {
 public:
  std::unique_ptr<BareosSocket> server;
  std::unique_ptr<BareosSocketTCP> client;

  TestSockets() = default;
  TestSockets(const TestSockets&) = delete;
};

struct listening_socket {
  uint16_t port{};
  int sockfd{};

  listening_socket() = default;
  listening_socket(uint16_t port_, int sockfd_) : port{port_}, sockfd{sockfd_}
  {
  }
  listening_socket(const listening_socket&) = delete;
  listening_socket& operator=(const listening_socket&) = delete;
  listening_socket(listening_socket&& other) { *this = std::move(other); }
  listening_socket& operator=(listening_socket&& other)
  {
    std::swap(port, other.port);
    std::swap(sockfd, other.sockfd);
    return *this;
  }
  ~listening_socket();
};

std::optional<listening_socket> create_listening_socket();
int accept_socket(const listening_socket& ls);

BareosSocket* create_new_bareos_socket(int fd);
std::unique_ptr<TestSockets> create_connected_server_and_client_bareos_socket();

#endif  // BAREOS_TESTS_BAREOS_TEST_SOCKETS_H_
