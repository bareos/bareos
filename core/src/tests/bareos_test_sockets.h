/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

class BareosSocketTCP;
class BareosSocket;

class TestSockets {
public:
  std::unique_ptr<BareosSocket> server;
  std::unique_ptr<BareosSocketTCP> client;

  TestSockets() = default;
  TestSockets(const TestSockets &) = delete;
};

int create_accepted_server_socket(int port);
BareosSocket *create_new_bareos_socket(int fd);
std::unique_ptr<TestSockets> create_connected_server_and_client_bareos_socket();

#endif /* BAREOS_TESTS_BAREOS_TEST_SOCKETS_H_ */
