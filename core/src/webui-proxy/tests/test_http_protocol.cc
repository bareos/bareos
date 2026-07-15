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

#include "../http_protocol.h"

#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <array>
#include <string>

namespace {

class SocketPair {
 public:
  SocketPair() { EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds_), 0); }

  ~SocketPair()
  {
    if (fds_[0] >= 0) { close(fds_[0]); }
    if (fds_[1] >= 0) { close(fds_[1]); }
  }

  int local() const { return fds_[0]; }
  int peer() const { return fds_[1]; }

 private:
  int fds_[2] = {-1, -1};
};

void WriteAll(int fd, const void* data, size_t size)
{
  const auto* ptr = static_cast<const char*>(data);
  size_t remaining = size;
  while (remaining > 0) {
    const auto written = ::write(fd, ptr, remaining);
    ASSERT_GT(written, 0);
    ptr += written;
    remaining -= static_cast<size_t>(written);
  }
}

std::string ReadSome(int fd)
{
  std::array<char, 512> buffer{};
  const auto received = ::recv(fd, buffer.data(), buffer.size(), 0);
  EXPECT_GT(received, 0);
  return std::string(buffer.data(), static_cast<size_t>(received));
}

}  // namespace

TEST(HttpProtocol, ReadsJsonRequestBodyAndPendingInput)
{
  SocketPair sockets;
  const std::string request
      = "POST /api/session/login HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 13\r\n\r\n"
        "{\"hello\":\"x\"}EXTRA";
  WriteAll(sockets.peer(), request.data(), request.size());

  const auto parsed
      = ReadHttpRequest(sockets.local(), std::chrono::milliseconds(50));

  EXPECT_EQ(parsed.method, "POST");
  EXPECT_EQ(parsed.target, "/api/session/login");
  EXPECT_EQ(parsed.body, "{\"hello\":\"x\"}");
  EXPECT_EQ(parsed.pending_input, "EXTRA");
  ASSERT_TRUE(parsed.HeaderValue("Host"));
  EXPECT_EQ(*parsed.HeaderValue("Host"), "localhost");
}

TEST(HttpProtocol, DetectsWebSocketUpgradeRequests)
{
  SocketPair sockets;
  const std::string request
      = "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: keep-alive, Upgrade\r\n\r\n";
  WriteAll(sockets.peer(), request.data(), request.size());

  const auto parsed
      = ReadHttpRequest(sockets.local(), std::chrono::milliseconds(50));

  EXPECT_TRUE(IsWebSocketUpgradeRequest(parsed));
}

TEST(HttpProtocol, FindsNamedCookieValues)
{
  const auto value
      = FindCookieValue("foo=bar; bareos_proxy_session=session123; baz=qux",
                        "bareos_proxy_session");

  ASSERT_TRUE(value);
  EXPECT_EQ(*value, "session123");
}

TEST(HttpProtocol, SendsHttpResponsesWithContentLength)
{
  SocketPair sockets;
  SendHttpResponse(sockets.local(), std::chrono::milliseconds(50),
                   "HTTP/1.1 200 OK", "{\"ok\":true}",
                   {{"Content-Type", "application/json"}});

  const auto response = ReadSome(sockets.peer());
  EXPECT_NE(response.find("HTTP/1.1 200 OK"), std::string::npos);
  EXPECT_NE(response.find("Content-Length: 11"), std::string::npos);
  EXPECT_NE(response.find("{\"ok\":true}"), std::string::npos);
}
