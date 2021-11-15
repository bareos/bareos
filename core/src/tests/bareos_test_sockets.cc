/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2020 Bareos GmbH & Co. KG

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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "bareos_test_sockets.h"
#include "tests/bsock_test.h"

#include "lib/bsock_tcp.h"

#if HAVE_WIN32
#  include <cstdlib>
#  include <mutex>
static void exithandler() { WSACleanup(); }
static void init_once()
{
  if (WSA_Init() == 0) { std::atexit(exithandler); }
}
#endif

static int create_listening_server_socket(int port)
{
#if HAVE_WIN32
  try {
    static std::once_flag f;
    std::call_once(f, init_once);
  } catch (...) {
    return -1;
  }
#endif

  int listen_file_descriptor;

  if ((listen_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    return -2;
  }

#if defined(HAVE_WIN32)
  BOOL option = TRUE;
  auto r1 = setsockopt(listen_file_descriptor, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char*>(&option), sizeof(option));
#else
  int option = 1;
  auto r1 = setsockopt(listen_file_descriptor, SOL_SOCKET, SO_REUSEADDR,
                       static_cast<const void*>(&option), sizeof(option));
#endif

  if (r1 < 0) {
    perror("setsockopt");
    return -3;
  }

  struct sockaddr_in address;
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);
  if (bind(listen_file_descriptor, (struct sockaddr*)&address, sizeof(address))
      < 0) {
    perror("bind failed");
    return -4;
  }

#if defined(HAVE_WIN32)
  DWORD timeout = 10000;  // after 10 seconds connect() will timeout
  auto r2
      = (setsockopt(listen_file_descriptor, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout), sizeof(timeout))
         == SOCKET_ERROR)
            ? -1
            : 0;
#else
  struct timeval timeout;
  timeout.tv_sec = 10;  // after 10 seconds connect() will timeout
  timeout.tv_usec = 0;

  auto r2 = setsockopt(listen_file_descriptor, SOL_SOCKET, SO_RCVTIMEO,
                       static_cast<const void*>(&timeout), sizeof(timeout));
#endif

  if (r2 < 0) {
    perror("setsockopt");
    return -5;
  }

  if (listen(listen_file_descriptor, 3) < 0) {
    perror("listen");
    return -6;
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
  if (sock_fd > 0) { sock_fd = accept_server_socket(sock_fd); }
  return sock_fd;
}

std::unique_ptr<TestSockets> create_connected_server_and_client_bareos_socket()
{
  std::unique_ptr<TestSockets> test_sockets(new TestSockets);

  uint16_t portnumber = create_unique_socket_number();

  int server_file_descriptor = create_listening_server_socket(portnumber);

  EXPECT_GE(server_file_descriptor, 0) << "Could not create listening socket";
  if (server_file_descriptor < 0) { return nullptr; }

  test_sockets->client.reset(new BareosSocketTCP);
  test_sockets->client->sleep_time_after_authentication_error = 0;

  bool ok = test_sockets->client->connect(NULL, 1, 1, 0, "Director daemon",
                                          HOST, NULL, portnumber, false);
  EXPECT_EQ(ok, true) << "Could not connect client socket with server socket.";
  if (!ok) { return nullptr; }

  server_file_descriptor = accept_server_socket(server_file_descriptor);
  EXPECT_GE(server_file_descriptor, 0) << "Could not accept server socket.";
  if (server_file_descriptor <= 0) { return nullptr; }

  test_sockets->server.reset(create_new_bareos_socket(server_file_descriptor));

  return test_sockets;
}

BareosSocket* create_new_bareos_socket(int fd)
{
  BareosSocket* bs;
  bs = new BareosSocketTCP;
  bs->sleep_time_after_authentication_error = 0;
  bs->fd_ = fd;
  bs->SetWho(strdup("client"));
  memset(&bs->peer_addr, 0, sizeof(bs->peer_addr));
  return bs;
}


#include <sys/types.h>
#include <unistd.h>

static uint16_t listening_server_port_number = 0;

uint16_t create_unique_socket_number()
{
  if (listening_server_port_number == 0) {
    pid_t pid = getpid();
    uint16_t port_number = 5 * (static_cast<uint32_t>(pid) % 10000) + 10000;
    listening_server_port_number = port_number;
  } else {
    ++listening_server_port_number;
  }

  return listening_server_port_number;
}
