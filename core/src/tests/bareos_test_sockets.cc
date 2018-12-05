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

#include "bareos_test_sockets.h"
#include "tests/bsock_test.h"

#include "gtest/gtest.h"
#include "include/bareos.h"

int listening_server_port_number = BSOCK_TEST_PORT_NUMBER;

static int create_listening_server_socket(int port)
{
  int listen_file_descriptor;
  int opt = 1;

  if ((listen_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    return -1;
  }

  if (setsockopt(listen_file_descriptor, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    perror("setsockopt");
    return -1;
  }

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  if (bind(listen_file_descriptor, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    return -1;
  }

  struct timeval timeout;
  timeout.tv_sec  = 1;  // after 1 seconds connect() will timeout
  timeout.tv_usec = 0;

  if (setsockopt(listen_file_descriptor, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setsockopt");
    return -1;
  }

  if (listen(listen_file_descriptor, 3) < 0) {
    perror("listen");
    return -1;
  }
  return listen_file_descriptor;
}

static int accept_server_socket(int listen_file_descriptor)
{
  int new_socket = accept(listen_file_descriptor, nullptr, nullptr);

  if (new_socket < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      perror("Socket accept timeout");
    }
    return -1;
  }

  close(listen_file_descriptor);

  return new_socket;
}

int create_accepted_server_socket(int port)
{
  int sock_fd = create_listening_server_socket(port);
  if (sock_fd > 0) {
    sock_fd = accept_server_socket(sock_fd);
  }
  return sock_fd;
}

std::unique_ptr<TestSockets> create_connected_server_and_client_bareos_socket()
{
  std::unique_ptr<TestSockets> test_sockets(new TestSockets);

  listening_server_port_number++;
  int server_file_descriptor = create_listening_server_socket(listening_server_port_number);

  EXPECT_GE(server_file_descriptor, 0) << "Could not create listening socket";
  if (server_file_descriptor < 0) {
    return nullptr;
  }

  test_sockets->client.reset(New(BareosSocketTCP));
  test_sockets->client->sleep_time_after_authentication_error = 0;

  bool ok = test_sockets->client->connect(NULL, 1, 1, 0, "Director daemon", HOST,
                        NULL, listening_server_port_number, false);
  EXPECT_EQ(ok, true) << "Could not connect client socket with server socket.";
  if (!ok) { return nullptr; }

  server_file_descriptor = accept_server_socket(server_file_descriptor);
  EXPECT_GE(server_file_descriptor, 0) << "Could not accept server socket.";
  if (server_file_descriptor <= 0) { return nullptr; }

  test_sockets->server.reset(create_new_bareos_socket(server_file_descriptor));

  return test_sockets;
}

BareosSocket *create_new_bareos_socket(int fd)
{
  BareosSocket *bs;
  bs = New(BareosSocketTCP);
  bs->sleep_time_after_authentication_error = 0;
  bs->fd_ = fd;
  bs->SetWho(bstrdup("client"));
  memset(&bs->peer_addr, 0, sizeof(bs->peer_addr));
  return bs;
}
