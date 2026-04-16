/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
 * Minimal RFC 6455 WebSocket server codec shared by multiple frontends.
 *
 * Handles:
 *  - HTTP upgrade handshake (SHA-1 accept key via OpenSSL)
 *  - Text frames (opcode 0x1) — used for all JSON messages
 *  - Ping/pong frames for keep-alive
 *  - Close frame (opcode 0x8) — triggers graceful shutdown
 *  - Masked inbound frames (browser always masks)
 *  - Unmasked outbound frames (server never masks per RFC 6455)
 *  - 7-bit, 16-bit, and 64-bit payload length encoding
 */
#ifndef BAREOS_LIB_WS_CODEC_H_
#define BAREOS_LIB_WS_CODEC_H_

#include <cstddef>
#include <cstdint>
#include <string>

/** Opaque handle wrapping a connected file descriptor. */
class WsCodec {
 public:
  explicit WsCodec(int fd) : fd_(fd) {}

  /* Perform the HTTP/WebSocket upgrade handshake.
   * Reads the HTTP GET request and sends the 101 Switching Protocols reply.
   * Throws std::runtime_error on failure. */
  void Handshake();

  /* Read one complete WebSocket message (text frame, possibly fragmented).
   * Transparently handles ping/pong and close frames.
   * Returns the text payload, or an empty string on clean close.
   * Throws std::runtime_error on I/O error or protocol violation. */
  std::string RecvMessage();

  /* Send a text frame (opcode 0x1, FIN=1, unmasked).
   * Throws std::runtime_error on I/O error. */
  void SendText(const std::string& payload);

  /* Send a close frame and mark the connection as closed. */
  void SendClose();

  bool IsClosed() const { return closed_; }

 private:
  static constexpr std::size_t kMaxMessageSize = 16 * 1024 * 1024;

  int fd_;
  bool closed_ = false;

  // Low-level I/O helpers
  void WriteAll(const void* buf, size_t len);
  void ReadAll(void* buf, size_t len);

  // Frame encode/decode
  struct Frame {
    uint8_t opcode = 0;
    bool fin = true;
    std::string payload;
  };

  Frame RecvFrame();
  void SendFrame(uint8_t opcode, const std::string& payload, bool fin = true);
};

#endif  // BAREOS_LIB_WS_CODEC_H_
