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

#include "../../lib/ws_codec.h"

#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <chrono>
#include <iterator>
#include <stdexcept>

namespace {

class SocketPair {
 public:
  SocketPair()
  {
    EXPECT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds_), 0);
  }

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

}  // namespace

TEST(WsCodec, HandshakeTimesOutWhenPeerStalls)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50));

  try {
    codec.Handshake();
    FAIL() << "expected timeout";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("timeout"), std::string::npos);
  }
}

TEST(WsCodec, RecvMessageTimesOutWhenFramePayloadStalls)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50));

  const unsigned char partial_frame[] = {
      0x81u, 0x83u, 0x01u, 0x02u, 0x03u, 0x04u, 0x69u,
  };
  WriteAll(sockets.peer(), partial_frame, std::size(partial_frame));

  try {
    codec.RecvMessage();
    FAIL() << "expected timeout";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("timeout"), std::string::npos);
  }
}
