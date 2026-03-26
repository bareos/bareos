/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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
/**
 * @file
 * Unit tests for WsCodec frame encode/decode.
 *
 * Uses a socketpair() to exercise the real I/O paths without a network.
 */
#include "../ws_codec.h"

#include <array>
#include <cstring>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

#include <gtest/gtest.h>

// Helper: write a raw WebSocket frame into a socket (simulating a browser).
// browser always sends masked frames.
static void SendMaskedTextFrame(int fd,
                                const std::string& payload,
                                bool fin = true)
{
  std::string frame;
  uint8_t b0 = static_cast<uint8_t>((fin ? 0x80u : 0u) | 0x01u);  // text
  frame += static_cast<char>(b0);

  size_t len = payload.size();
  // MASK bit set
  if (len <= 125) {
    frame += static_cast<char>(0x80u | len);
  } else if (len <= 65535) {
    frame += static_cast<char>(0x80u | 126);
    frame += static_cast<char>((len >> 8) & 0xFFu);
    frame += static_cast<char>(len & 0xFFu);
  } else {
    frame += static_cast<char>(0x80u | 127);
    for (int i = 7; i >= 0; --i) {
      frame += static_cast<char>((len >> (i * 8)) & 0xFFu);
    }
  }

  // Masking key (fixed for test)
  const uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
  frame += static_cast<char>(mask[0]);
  frame += static_cast<char>(mask[1]);
  frame += static_cast<char>(mask[2]);
  frame += static_cast<char>(mask[3]);

  for (size_t i = 0; i < payload.size(); ++i) {
    frame += static_cast<char>(payload[i] ^ static_cast<char>(mask[i % 4]));
  }

  const auto* p = reinterpret_cast<const uint8_t*>(frame.data());
  size_t remaining = frame.size();
  while (remaining > 0) {
    ssize_t n = ::send(fd, p, remaining, MSG_NOSIGNAL);
    if (n <= 0) { break; }
    p += n;
    remaining -= static_cast<size_t>(n);
  }
}

// Helper: read raw bytes from a socket into a string.
static std::string ReadBytes(int fd, size_t n)
{
  std::string buf(n, '\0');
  size_t off = 0;
  while (off < n) {
    ssize_t r = ::recv(fd, buf.data() + off, n - off, MSG_WAITALL);
    if (r <= 0) { break; }
    off += static_cast<size_t>(r);
  }
  return buf;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(WsCodec, SendTextFrame)
{
  int sv[2];
  ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

  WsCodec server(sv[0]);
  const std::string payload = "hello, world";
  server.SendText(payload);

  // Read raw frame from sv[1]
  std::string header = ReadBytes(sv[1], 2);
  ASSERT_EQ(header.size(), 2u);

  uint8_t b0 = static_cast<uint8_t>(header[0]);
  uint8_t b1 = static_cast<uint8_t>(header[1]);

  EXPECT_EQ(b0, 0x81u);  // FIN=1, opcode=text
  EXPECT_EQ(b1 & 0x80u, 0u);  // server must NOT mask
  EXPECT_EQ(b1 & 0x7Fu, payload.size());

  std::string recv_payload = ReadBytes(sv[1], payload.size());
  EXPECT_EQ(recv_payload, payload);

  ::close(sv[0]);
  ::close(sv[1]);
}

TEST(WsCodec, RecvMaskedTextFrame)
{
  int sv[2];
  ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

  const std::string payload = "test message";

  // Write masked frame into sv[1] (simulating browser)
  SendMaskedTextFrame(sv[1], payload);

  WsCodec server(sv[0]);
  std::string result = server.RecvMessage();
  EXPECT_EQ(result, payload);

  ::close(sv[0]);
  ::close(sv[1]);
}

TEST(WsCodec, PingPong)
{
  int sv[2];
  ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

  // Send ping frame followed by a text frame
  {
    // Ping (opcode 0x9), unmasked for simplicity (sv[1] is not a real browser)
    const std::string ping_data = "ping!";
    // Build masked ping frame
    uint8_t b0 = 0x80u | 0x09u;  // FIN + ping
    uint8_t b1 = 0x80u | static_cast<uint8_t>(ping_data.size());
    const uint8_t mask[4] = {0x01, 0x02, 0x03, 0x04};
    std::string frame;
    frame += static_cast<char>(b0);
    frame += static_cast<char>(b1);
    frame += static_cast<char>(mask[0]);
    frame += static_cast<char>(mask[1]);
    frame += static_cast<char>(mask[2]);
    frame += static_cast<char>(mask[3]);
    for (size_t i = 0; i < ping_data.size(); ++i) {
      frame += static_cast<char>(ping_data[i] ^ static_cast<char>(mask[i % 4]));
    }
    ::send(sv[1], frame.data(), frame.size(), MSG_NOSIGNAL);
  }

  // Now send a text frame
  SendMaskedTextFrame(sv[1], "after ping");

  WsCodec server(sv[0]);

  // RecvMessage should handle the ping internally and return the text message.
  // We also need to consume the pong that the server sends.
  std::thread reader([&] {
    // Read the pong from sv[1]
    // pong header: 0x8A (FIN+pong), length byte, payload
    std::string pong_hdr = ReadBytes(sv[1], 2);
    EXPECT_EQ(static_cast<uint8_t>(pong_hdr[0]), 0x8Au);  // FIN+pong
    uint8_t pong_len = static_cast<uint8_t>(pong_hdr[1]) & 0x7Fu;
    if (pong_len > 0) { ReadBytes(sv[1], pong_len); }
  });

  std::string result = server.RecvMessage();
  reader.join();

  EXPECT_EQ(result, "after ping");

  ::close(sv[0]);
  ::close(sv[1]);
}

TEST(WsCodec, LargePayload)
{
  int sv[2];
  ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

  // Build a 1000-byte payload (requires 16-bit length encoding)
  std::string payload(1000, 'X');
  SendMaskedTextFrame(sv[1], payload);

  WsCodec server(sv[0]);
  std::string result = server.RecvMessage();
  EXPECT_EQ(result, payload);

  ::close(sv[0]);
  ::close(sv[1]);
}
