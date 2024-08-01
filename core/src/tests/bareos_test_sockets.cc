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

#include <thread>

#ifdef HAVE_WIN32
#  define socketClose(fd) ::closesocket(fd)
#else
#  define socketClose(fd) ::close(fd)
#endif

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

  int bindresult = -1;
  for (int i = 0; i < 6; i++) {
    bindresult = bind(listen_file_descriptor, (struct sockaddr*)&address,
                      sizeof(address));
    if (bindresult == 0) {
      std::cout << "bind successful after " << i + 1 << " tries" << std::endl;
      break;
    } else {
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  }
  std::string errormessage{"bind failed for port "};
  errormessage.append(std::to_string(port));
  if (bindresult < 0) {
    perror(errormessage.c_str());
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
  int listenresult = -1;
  for (int i = 0; i < 6; i++) {
    listenresult = listen(listen_file_descriptor, 3);
    if (listenresult == 0) {
      std::cout << "listen successful after " << i + 1 << " tries" << std::endl;
      break;
    } else {
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  }
  if (listenresult < 0) {
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

  socketClose(listen_file_descriptor);

  return new_socket;
}

static std::optional<uint16_t> port_number_of(int sockfd)
{
  union {
    struct sockaddr addr;
    struct sockaddr_in addr4;
    struct sockaddr_in6 addr6;
  } buf = {};

  // the port gets chosen during StartSocketServer, so we need to query
  // the port number afterwards.
  socklen_t len = sizeof(buf);
  auto error = getsockname(sockfd, &buf.addr, &len);
  EXPECT_EQ(error, 0);
  if (error != 0) {
    perror("sock name error");
    return false;
  }

  if (buf.addr.sa_family != AF_INET && buf.addr.sa_family != AF_INET6) {
    return std::nullopt;
  }

  auto nport = (buf.addr.sa_family == AF_INET) ? buf.addr4.sin_port
                                               : buf.addr6.sin6_port;

  auto port = ntohs(nport);

  return port;
}

std::unique_ptr<TestSockets> create_connected_server_and_client_bareos_socket()
{
  std::unique_ptr<TestSockets> test_sockets(new TestSockets);

  int listen_fd = create_listening_server_socket(0);

  EXPECT_GE(listen_fd, 0) << "Could not create listening socket";
  if (listen_fd < 0) { return nullptr; }

  auto portnumber_opt = port_number_of(listen_fd);

  EXPECT_NE(portnumber_opt, std::nullopt) << "Could not find used port number";
  if (!portnumber_opt) {
    socketClose(listen_fd);
    return nullptr;
  }

  auto portnumber = *portnumber_opt;

  test_sockets->client.reset(new BareosSocketTCP);
  test_sockets->client->sleep_time_after_authentication_error = 0;

  bool ok = test_sockets->client->connect(NULL, 1, 1, 0, "Director daemon",
                                          HOST, NULL, portnumber, false);
  EXPECT_EQ(ok, true) << "Could not connect client socket with server socket.";
  if (!ok) {
    socketClose(listen_fd);
    return nullptr;
  }

  auto server_fd = accept_server_socket(listen_fd);
  EXPECT_GE(server_fd, 0) << "Could not accept server socket.";
  if (server_fd <= 0) {
    socketClose(listen_fd);
    return nullptr;
  }

  socketClose(listen_fd);

  test_sockets->server.reset(create_new_bareos_socket(server_fd));

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

std::optional<listening_socket> create_listening_socket()
{
  int sock_fd = create_listening_server_socket(0);

  if (sock_fd < 0) { return std::nullopt; }

  auto port = port_number_of(sock_fd);

  if (!port) {
    socketClose(sock_fd);
    return std::nullopt;
  }

  return listening_socket{*port, sock_fd};
}

int accept_socket(const listening_socket& ls)
{
  auto fd = accept_server_socket(ls.sockfd);
  return fd;
}

listening_socket::~listening_socket()
{
  if (sockfd >= 0) { socketClose(sockfd); }
}
