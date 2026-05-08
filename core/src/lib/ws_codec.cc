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
#include "ws_codec.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>

#include <sys/socket.h>
#include <unistd.h>

#include <openssl/evp.h>

// RFC 6455 magic GUID appended to Sec-WebSocket-Key before SHA-1
static constexpr const char kWsMagic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// WebSocket opcodes
static constexpr uint8_t kOpContinuation = 0x0;
static constexpr uint8_t kOpText = 0x1;
static constexpr uint8_t kOpBinary = 0x2;
static constexpr uint8_t kOpClose = 0x8;
static constexpr uint8_t kOpPing = 0x9;
static constexpr uint8_t kOpPong = 0xA;

// ---------------------------------------------------------------------------
// Low-level I/O
// ---------------------------------------------------------------------------

namespace {

using Clock = std::chrono::steady_clock;

int RemainingTimeoutMs(Clock::time_point deadline)
{
  const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
      deadline - Clock::now());
  return remaining.count() > 0 ? static_cast<int>(remaining.count()) : 0;
}

void WaitForSocket(int fd,
                   short events,
                   Clock::time_point deadline,
                   std::string_view action)
{
  while (true) {
    struct pollfd pfd{fd, events, 0};
    const int rc = ::poll(&pfd, 1, RemainingTimeoutMs(deadline));
    if (rc > 0) {
      if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        throw std::runtime_error("WebSocket: socket error while waiting to "
                                 + std::string(action));
      }
      if (pfd.revents & events) { return; }
      continue;
    }
    if (rc == 0) {
      throw std::runtime_error("WebSocket: timeout while waiting to "
                               + std::string(action));
    }
    if (errno == EINTR) { continue; }
    throw std::runtime_error("WebSocket: poll failed while waiting to "
                             + std::string(action));
  }
}

void WriteAll(int fd, const void* buf, size_t len, Clock::time_point deadline)
{
  const auto* p = static_cast<const uint8_t*>(buf);
  while (len > 0) {
    WaitForSocket(fd, POLLOUT, deadline, "write to peer");
    const ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      throw std::runtime_error("WebSocket: send failed");
    }
    if (n == 0) { throw std::runtime_error("WebSocket: send failed"); }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

void ReadAll(int fd,
             void* buf,
             size_t len,
             Clock::time_point deadline,
             std::string_view action)
{
  auto* p = static_cast<uint8_t*>(buf);
  while (len > 0) {
    WaitForSocket(fd, POLLIN, deadline, action);
    const ssize_t n = ::recv(fd, p, len, 0);
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      throw std::runtime_error("WebSocket: recv failed");
    }
    if (n == 0) {
      throw std::runtime_error("WebSocket: connection closed by peer");
    }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

Clock::time_point MakeDeadline(std::chrono::milliseconds timeout)
{
  return Clock::now() + timeout;
}

}  // namespace

// ---------------------------------------------------------------------------
// Handshake
// ---------------------------------------------------------------------------

static std::string Base64Encode(const uint8_t* data, size_t len)
{
  std::string encoded(4 * ((len + 2) / 3), '\0');
  const int encoded_len
      = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()), data,
                        static_cast<int>(len));
  if (encoded_len < 0) {
    throw std::runtime_error("WebSocket: base64 encode failed");
  }
  encoded.resize(static_cast<size_t>(encoded_len));
  return encoded;
}

static std::string ComputeAcceptKey(std::string_view client_key)
{
  uint8_t sha1[EVP_MAX_MD_SIZE];
  unsigned int sha1_len = 0;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
  EVP_DigestUpdate(ctx, client_key.data(), client_key.size());
  EVP_DigestUpdate(ctx, kWsMagic, sizeof(kWsMagic) - 1);
  EVP_DigestFinal_ex(ctx, sha1, &sha1_len);
  EVP_MD_CTX_free(ctx);

  return Base64Encode(sha1, sha1_len);
}

void WsCodec::Handshake()
{
  // Read HTTP request line by line until blank line
  const auto deadline = MakeDeadline(handshake_timeout_);
  std::string headers;
  headers.reserve(1024);
  char ch;
  while (true) {
    ReadAll(fd_, &ch, 1, deadline, "read handshake data");
    headers.push_back(ch);
    if (headers.ends_with("\r\n\r\n")) { break; }
    if (headers.size() > 16384) {
      throw std::runtime_error("WebSocket: HTTP headers too large");
    }
  }

  // Extract Sec-WebSocket-Key
  constexpr std::string_view kKeyHeader = "Sec-WebSocket-Key:";
  const auto pos_it
      = std::search(headers.begin(), headers.end(), kKeyHeader.begin(),
                    kKeyHeader.end(), [](char lhs, char rhs) {
                      return std::tolower(static_cast<unsigned char>(lhs))
                             == std::tolower(static_cast<unsigned char>(rhs));
                    });
  if (pos_it == headers.end()) {
    throw std::runtime_error("WebSocket: missing Sec-WebSocket-Key header");
  }

  const auto pos = static_cast<size_t>(std::distance(headers.begin(), pos_it))
                   + kKeyHeader.size();
  const std::string_view headers_view(headers);
  const auto end = headers_view.find("\r\n", pos);
  if (end == std::string::npos) {
    throw std::runtime_error("WebSocket: malformed Sec-WebSocket-Key header");
  }
  std::string_view key = headers_view.substr(pos, end - pos);
  while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) {
    key.remove_prefix(1);
  }
  while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
    key.remove_suffix(1);
  }

  const std::string accept = ComputeAcceptKey(key);

  std::string response
      = "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: "
        + accept + "\r\n\r\n";

  WriteAll(fd_, response.data(), response.size(), deadline);
}

// ---------------------------------------------------------------------------
// Frame encode / decode
// ---------------------------------------------------------------------------

WsCodec::Frame WsCodec::RecvFrame()
{
  const auto deadline = MakeDeadline(io_timeout_);
  Frame f;

  uint8_t b0;
  ReadAll(fd_, &b0, 1, deadline, "read websocket frame");
  f.fin = (b0 & 0x80u) != 0;
  f.opcode = b0 & 0x0Fu;

  uint8_t b1;
  ReadAll(fd_, &b1, 1, deadline, "read websocket frame");
  bool masked = (b1 & 0x80u) != 0;
  uint64_t payload_len = b1 & 0x7Fu;

  if (payload_len == 126) {
    uint8_t ext[2];
    ReadAll(fd_, ext, 2, deadline, "read websocket frame");
    payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
  } else if (payload_len == 127) {
    uint8_t ext[8];
    ReadAll(fd_, ext, 8, deadline, "read websocket frame");
    payload_len = 0;
    for (int i = 0; i < 8; ++i) { payload_len = (payload_len << 8) | ext[i]; }
  }

  uint8_t mask[4] = {};
  if (masked) { ReadAll(fd_, mask, 4, deadline, "read websocket frame"); }

  if (payload_len > 0) {
    f.payload.resize(static_cast<size_t>(payload_len));
    ReadAll(fd_, f.payload.data(), static_cast<size_t>(payload_len), deadline,
            "read websocket frame payload");
    if (masked) {
      for (size_t i = 0; i < f.payload.size(); ++i) {
        f.payload[i] ^= static_cast<char>(mask[i % 4]);
      }
    }
  }

  return f;
}

void WsCodec::SendFrame(uint8_t opcode, std::string_view payload, bool fin)
{
  const auto deadline = MakeDeadline(io_timeout_);
  std::string frame;
  frame.reserve(10 + payload.size());

  frame += static_cast<char>((fin ? 0x80u : 0u) | (opcode & 0x0Fu));

  size_t len = payload.size();
  if (len <= 125) {
    frame += static_cast<char>(len);
  } else if (len <= 65535) {
    frame += static_cast<char>(126);
    frame += static_cast<char>((len >> 8) & 0xFFu);
    frame += static_cast<char>(len & 0xFFu);
  } else {
    frame += static_cast<char>(127);
    for (int i = 7; i >= 0; --i) {
      frame += static_cast<char>((len >> (i * 8)) & 0xFFu);
    }
  }

  frame += payload;
  WriteAll(fd_, frame.data(), frame.size(), deadline);
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

std::string WsCodec::RecvMessage()
{
  std::string message;

  while (true) {
    Frame f = RecvFrame();

    switch (f.opcode) {
      case kOpPing:
        SendFrame(kOpPong, f.payload);
        break;

      case kOpPong:
        break;

      case kOpClose:
        SendClose();
        return {};

      case kOpText:
      case kOpBinary:
        message += f.payload;
        if (f.fin) { return message; }
        break;

      case kOpContinuation:
        message += f.payload;
        if (f.fin) { return message; }
        break;

      default:
        throw std::runtime_error("WebSocket: unknown opcode");
    }
  }
}

void WsCodec::SendText(std::string_view payload)
{
  SendFrame(kOpText, payload);
}

void WsCodec::SendClose()
{
  if (!closed_) {
    closed_ = true;
    SendFrame(kOpClose, {});
  }
}
