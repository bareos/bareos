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
/**
 * @file
 * Minimal RFC 6455 WebSocket server codec for the webui proxy.
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
#ifndef BAREOS_WEBUI_PROXY_WS_CODEC_H_
#define BAREOS_WEBUI_PROXY_WS_CODEC_H_

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

/** Opaque handle wrapping a connected WebSocket after HTTP upgrade. */
class WsCodec {
 public:
  /* Perform the HTTP/WebSocket upgrade handshake and return the upgraded
   * connection. Throws std::runtime_error on failure. */
  static WsCodec Accept(int fd,
                        std::chrono::milliseconds io_timeout
                        = std::chrono::seconds(30),
                        std::chrono::milliseconds handshake_timeout
                        = std::chrono::seconds(5),
                        size_t max_frame_payload_size = 1024 * 1024,
                        size_t max_message_size = 4 * 1024 * 1024);
  static WsCodec Accept(int fd,
                        std::string_view request_headers,
                        std::string pending_input,
                        std::chrono::milliseconds io_timeout
                        = std::chrono::seconds(30),
                        std::chrono::milliseconds handshake_timeout
                        = std::chrono::seconds(5),
                        size_t max_frame_payload_size = 1024 * 1024,
                        size_t max_message_size = 4 * 1024 * 1024);

  /* Read one complete WebSocket message (text frame, possibly fragmented).
   * Transparently handles ping/pong and close frames.
   * Returns the text payload, or an empty string on clean close.
   * Throws std::runtime_error on I/O error or protocol violation. */
  std::string RecvMessage();

  /* Send a text frame (opcode 0x1, FIN=1, unmasked).
   * Throws std::runtime_error on I/O error. */
  void SendText(std::string_view payload);

  /* Send a close frame and mark the connection as closed. */
  void SendClose();

  bool IsClosed() const { return closed_; }
  const std::string& RequestTarget() const { return request_target_; }
  std::optional<std::string_view> RequestHeader(std::string_view name) const;

 private:
  WsCodec(int fd,
          std::chrono::milliseconds io_timeout,
          std::chrono::milliseconds handshake_timeout,
          size_t max_frame_payload_size,
          size_t max_message_size);
  void Handshake(std::optional<std::string_view> request_headers = std::nullopt);

  int fd_;
  bool closed_ = false;
  std::chrono::milliseconds io_timeout_;
  std::chrono::milliseconds handshake_timeout_;
  size_t max_frame_payload_size_;
  size_t max_message_size_;
  std::string pending_input_;
  std::string request_headers_;
  std::string request_target_;

  // Frame encode/decode
  struct Frame {
    uint8_t opcode = 0;
    bool fin = true;
    std::string payload;
  };

  Frame RecvFrame();
  void SendFrame(uint8_t opcode, std::string_view payload, bool fin = true);
};

#endif  // BAREOS_WEBUI_PROXY_WS_CODEC_H_
