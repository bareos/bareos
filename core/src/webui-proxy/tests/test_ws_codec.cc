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

#include <array>
#include <chrono>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

constexpr std::string_view kValidHandshakeRequest
    = "GET /ws HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n\r\n";

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

std::string BuildMaskedFrame(uint8_t opcode,
                             std::string_view payload,
                             bool fin = true)
{
  std::string frame;
  frame.push_back(static_cast<char>((fin ? 0x80u : 0u) | (opcode & 0x0Fu)));

  constexpr std::array<unsigned char, 4> kMask = {0x01u, 0x02u, 0x03u, 0x04u};
  const size_t len = payload.size();
  if (len <= 125) {
    frame.push_back(static_cast<char>(0x80u | len));
  } else if (len <= 65535) {
    frame.push_back(static_cast<char>(0x80u | 126u));
    frame.push_back(static_cast<char>((len >> 8) & 0xFFu));
    frame.push_back(static_cast<char>(len & 0xFFu));
  } else {
    frame.push_back(static_cast<char>(0x80u | 127u));
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFFu));
    }
  }

  frame.append(reinterpret_cast<const char*>(kMask.data()), kMask.size());
  for (size_t i = 0; i < payload.size(); ++i) {
    frame.push_back(static_cast<char>(payload[i] ^ kMask[i % kMask.size()]));
  }
  return frame;
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

TEST(WsCodec, HandshakeRejectsMissingVersionWithUpgradeRequired)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50));

  const std::string request
      = "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n";
  WriteAll(sockets.peer(), request.data(), request.size());

  EXPECT_THROW(codec.Handshake(), std::runtime_error);
  const auto response = ReadSome(sockets.peer());
  EXPECT_NE(response.find("HTTP/1.1 426 Upgrade Required"), std::string::npos);
  EXPECT_NE(response.find("Sec-WebSocket-Version: 13"), std::string::npos);
}

TEST(WsCodec, HandshakeRejectsUnsupportedVersionWithUpgradeRequired)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50));

  const std::string request
      = "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 12\r\n\r\n";
  WriteAll(sockets.peer(), request.data(), request.size());

  EXPECT_THROW(codec.Handshake(), std::runtime_error);
  const auto response = ReadSome(sockets.peer());
  EXPECT_NE(response.find("HTTP/1.1 426 Upgrade Required"), std::string::npos);
  EXPECT_NE(response.find("Sec-WebSocket-Version: 13"), std::string::npos);
}

TEST(WsCodec, HandshakeRejectsMissingUpgradeHeaderWithBadRequest)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50));

  const std::string request
      = "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
  WriteAll(sockets.peer(), request.data(), request.size());

  EXPECT_THROW(codec.Handshake(), std::runtime_error);
  const auto response = ReadSome(sockets.peer());
  EXPECT_NE(response.find("HTTP/1.1 400 Bad Request"), std::string::npos);
}

TEST(WsCodec, HandshakeAcceptsVersion13Request)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50));

  WriteAll(sockets.peer(), kValidHandshakeRequest.data(),
           kValidHandshakeRequest.size());

  EXPECT_NO_THROW(codec.Handshake());
  const auto response = ReadSome(sockets.peer());
  EXPECT_NE(response.find("HTTP/1.1 101 Switching Protocols"),
            std::string::npos);
  EXPECT_NE(response.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="),
            std::string::npos);
}

TEST(WsCodec, HandshakePreservesBufferedFrameData)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50));

  const unsigned char masked_text_frame[] = {
      0x81u, 0x81u, 0x01u, 0x02u, 0x03u, 0x04u, 0x40u,
  };

  std::string request(kValidHandshakeRequest);
  request.append(reinterpret_cast<const char*>(masked_text_frame),
                 sizeof(masked_text_frame));
  WriteAll(sockets.peer(), request.data(), request.size());

  EXPECT_NO_THROW(codec.Handshake());
  const auto response = ReadSome(sockets.peer());
  EXPECT_NE(response.find("HTTP/1.1 101 Switching Protocols"),
            std::string::npos);
  EXPECT_EQ(codec.RecvMessage(), "A");
}

TEST(WsCodec, RejectsOversizedSingleFramePayload)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50), 8, 16);

  const auto oversized = BuildMaskedFrame(0x1u, "123456789");
  WriteAll(sockets.peer(), oversized.data(), oversized.size());

  EXPECT_THROW(codec.RecvMessage(), std::runtime_error);
}

TEST(WsCodec, RejectsOversizedFragmentedMessage)
{
  SocketPair sockets;
  WsCodec codec(sockets.local(), std::chrono::milliseconds(50),
                std::chrono::milliseconds(50), 8, 6);

  const auto first = BuildMaskedFrame(0x1u, "abcd", false);
  const auto second = BuildMaskedFrame(0x0u, "efg");
  WriteAll(sockets.peer(), first.data(), first.size());
  WriteAll(sockets.peer(), second.data(), second.size());

  EXPECT_THROW(codec.RecvMessage(), std::runtime_error);
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
