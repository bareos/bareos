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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <gtest/gtest.h>

namespace {

uint16_t ReserveLoopbackPort()
{
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) { throw std::runtime_error("socket() failed"); }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    close(fd);
    throw std::runtime_error("bind() failed");
  }

  socklen_t address_size = sizeof(address);
  if (getsockname(fd, reinterpret_cast<sockaddr*>(&address), &address_size)
      < 0) {
    close(fd);
    throw std::runtime_error("getsockname() failed");
  }
  close(fd);
  return ntohs(address.sin_port);
}

class RunningHttpServer {
 public:
  RunningHttpServer()
      : port_(ReserveLoopbackPort()),
        thread_([this] {
          try {
            RunHttpServer("127.0.0.1", port_, "expected-token",
                          [this](int, bool, std::string, std::string) {
                            handler_called_ = true;
                          });
          } catch (...) {
            error_ = std::current_exception();
          }
        })
  {
  }

  ~RunningHttpServer() { Shutdown(); }

  int Connect()
  {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port_);

    for (int attempt = 0; attempt < 100; ++attempt) {
      const int fd = socket(AF_INET, SOCK_STREAM, 0);
      if (fd < 0) { throw std::runtime_error("socket() failed"); }
      if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address))
          == 0) {
        timeval timeout{1, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        return fd;
      }
      close(fd);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("HTTP server did not start listening");
  }

  void Stop()
  {
    Shutdown();
    if (error_) { std::rethrow_exception(error_); }
  }

  bool handler_called() const { return handler_called_; }
  uint16_t port() const { return port_; }

 private:
  void Shutdown()
  {
    if (thread_.joinable()) {
      RequestHttpServerShutdown();
      thread_.join();
    }
  }

  uint16_t port_;
  std::atomic_bool handler_called_{false};
  std::exception_ptr error_;
  std::thread thread_;
};

void WriteAll(int fd, std::string_view data)
{
  size_t written = 0;
  while (written < data.size()) {
    const auto result = send(fd, data.data() + written, data.size() - written,
                             MSG_NOSIGNAL);
    if (result <= 0) { throw std::runtime_error("send() failed"); }
    written += static_cast<size_t>(result);
  }
}

}  // namespace

TEST(BareosSetupHttpServer, RejectsWebSocketUpgradeWithInvalidToken)
{
  RunningHttpServer server;
  const int client = server.Connect();
  ASSERT_GE(client, 0);

  const std::string origin = "http://127.0.0.1:" + std::to_string(server.port());
  const std::string request
      = "GET /ws?token=wrong HTTP/1.1\r\n"
        "Host: 127.0.0.1:"
        + std::to_string(server.port())
        + "\r\nOrigin: " + origin
        + "\r\nUpgrade: websocket\r\n"
          "Connection: Upgrade\r\n"
          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
          "Sec-WebSocket-Version: 13\r\n\r\n";
  WriteAll(client, request);

  char response[512]{};
  const auto received = recv(client, response, sizeof(response), 0);
  EXPECT_GT(received, 0);
  EXPECT_NE(std::string(response, static_cast<size_t>(received))
                .find("HTTP/1.1 403 Forbidden"),
            std::string::npos);
  close(client);

  server.Stop();
  EXPECT_FALSE(server.handler_called());
}
