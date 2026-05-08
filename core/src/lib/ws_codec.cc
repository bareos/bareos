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
#include <optional>
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
static constexpr size_t kMaxHttpHeaderSize = 16384;
static constexpr size_t kHandshakeReadChunkSize = 1024;
static constexpr std::string_view kSupportedWsVersion = "13";

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
             std::string& pending_input,
             void* buf,
             size_t len,
             Clock::time_point deadline,
             std::string_view action)
{
  auto* p = static_cast<uint8_t*>(buf);
  if (!pending_input.empty()) {
    const auto buffered = std::min(len, pending_input.size());
    std::memcpy(p, pending_input.data(), buffered);
    pending_input.erase(0, buffered);
    p += buffered;
    len -= buffered;
  }

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

size_t ReadChunk(int fd,
                 std::string& target,
                 Clock::time_point deadline,
                 std::string_view action,
                 size_t chunk_size = kHandshakeReadChunkSize)
{
  WaitForSocket(fd, POLLIN, deadline, action);
  const auto original_size = target.size();
  target.resize(original_size + chunk_size);

  while (true) {
    const ssize_t n = ::recv(fd, target.data() + original_size, chunk_size, 0);
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      target.resize(original_size);
      throw std::runtime_error("WebSocket: recv failed");
    }
    if (n == 0) {
      target.resize(original_size);
      throw std::runtime_error("WebSocket: connection closed by peer");
    }
    target.resize(original_size + static_cast<size_t>(n));
    return static_cast<size_t>(n);
  }
}

Clock::time_point MakeDeadline(std::chrono::milliseconds timeout)
{
  return Clock::now() + timeout;
}

bool EqualsIgnoreCase(std::string_view lhs, std::string_view rhs)
{
  return lhs.size() == rhs.size()
         && std::equal(
             lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
             [](char left, char right) {
               return std::tolower(static_cast<unsigned char>(left))
                      == std::tolower(static_cast<unsigned char>(right));
             });
}

std::optional<std::string_view> FindHeaderValue(std::string_view headers,
                                                std::string_view name)
{
  size_t line_start = headers.find("\r\n");
  if (line_start == std::string_view::npos) { return std::nullopt; }
  line_start += 2;

  while (line_start < headers.size()) {
    const auto line_end = headers.find("\r\n", line_start);
    if (line_end == std::string_view::npos || line_end == line_start) {
      return std::nullopt;
    }

    const auto line = headers.substr(line_start, line_end - line_start);
    const auto colon = line.find(':');
    if (colon != std::string_view::npos
        && EqualsIgnoreCase(line.substr(0, colon), name)) {
      auto value = line.substr(colon + 1);
      while (!value.empty()
             && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
      }
      while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
      }
      return value;
    }

    line_start = line_end + 2;
  }

  return std::nullopt;
}

bool HeaderHasToken(std::string_view value, std::string_view expected)
{
  while (!value.empty()) {
    const auto comma = value.find(',');
    auto token = value.substr(0, comma);
    while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) {
      token.remove_prefix(1);
    }
    while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) {
      token.remove_suffix(1);
    }
    if (EqualsIgnoreCase(token, expected)) { return true; }
    if (comma == std::string_view::npos) { break; }
    value.remove_prefix(comma + 1);
  }
  return false;
}

void SendHttpResponse(int fd,
                      Clock::time_point deadline,
                      std::string_view status_line,
                      std::string_view headers = {})
{
  std::string response;
  response.reserve(status_line.size() + headers.size() + 32);
  response += status_line;
  response += "\r\n";
  if (!headers.empty()) {
    response += headers;
    if (!headers.ends_with("\r\n")) { response += "\r\n"; }
  }
  response += "Content-Length: 0\r\n\r\n";
  WriteAll(fd, response.data(), response.size(), deadline);
}

[[noreturn]] void FailHandshake(int fd,
                                Clock::time_point deadline,
                                std::string_view status_line,
                                std::string_view message,
                                std::string_view headers = {})
{
  SendHttpResponse(fd, deadline, status_line, headers);
  throw std::runtime_error(std::string(message));
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

WsCodec::WsCodec(int fd,
                 std::chrono::milliseconds io_timeout,
                 std::chrono::milliseconds handshake_timeout,
                 size_t max_frame_payload_size,
                 size_t max_message_size)
    : fd_(fd)
    , io_timeout_(io_timeout)
    , handshake_timeout_(handshake_timeout)
    , max_frame_payload_size_(max_frame_payload_size)
    , max_message_size_(max_message_size)
{
}

WsCodec WsCodec::Accept(int fd,
                        std::chrono::milliseconds io_timeout,
                        std::chrono::milliseconds handshake_timeout,
                        size_t max_frame_payload_size,
                        size_t max_message_size)
{
  WsCodec codec(fd, io_timeout, handshake_timeout, max_frame_payload_size,
                max_message_size);
  codec.Handshake();
  return codec;
}

void WsCodec::Handshake()
{
  const auto deadline = MakeDeadline(handshake_timeout_);
  std::string headers;
  headers.reserve(1024);
  while (true) {
    ReadChunk(fd_, pending_input_, deadline, "read handshake data");
    const auto header_end = pending_input_.find("\r\n\r\n");
    if (header_end != std::string::npos) {
      headers.assign(pending_input_.data(), header_end + 4);
      pending_input_.erase(0, header_end + 4);
      break;
    }
    if (pending_input_.size() > kMaxHttpHeaderSize) {
      throw std::runtime_error("WebSocket: HTTP headers too large");
    }
  }

  const std::string_view headers_view(headers);
  const auto request_line_end = headers_view.find("\r\n");
  if (request_line_end == std::string_view::npos) {
    FailHandshake(fd_, deadline, "HTTP/1.1 400 Bad Request",
                  "WebSocket: malformed HTTP request line");
  }
  const auto request_line = headers_view.substr(0, request_line_end);
  if (!request_line.starts_with("GET ")
      || request_line.find(" HTTP/1.1") == std::string_view::npos) {
    FailHandshake(fd_, deadline, "HTTP/1.1 400 Bad Request",
                  "WebSocket: invalid HTTP upgrade request");
  }

  const auto upgrade = FindHeaderValue(headers_view, "Upgrade");
  if (!upgrade || !EqualsIgnoreCase(*upgrade, "websocket")) {
    FailHandshake(fd_, deadline, "HTTP/1.1 400 Bad Request",
                  "WebSocket: missing Upgrade: websocket header");
  }

  const auto connection = FindHeaderValue(headers_view, "Connection");
  if (!connection || !HeaderHasToken(*connection, "Upgrade")) {
    FailHandshake(fd_, deadline, "HTTP/1.1 400 Bad Request",
                  "WebSocket: missing Connection: Upgrade header");
  }

  const auto version = FindHeaderValue(headers_view, "Sec-WebSocket-Version");
  if (!version || *version != kSupportedWsVersion) {
    FailHandshake(fd_, deadline, "HTTP/1.1 426 Upgrade Required",
                  "WebSocket: unsupported WebSocket version",
                  "Sec-WebSocket-Version: 13\r\n");
  }

  const auto key = FindHeaderValue(headers_view, "Sec-WebSocket-Key");
  if (!key || key->empty()) {
    FailHandshake(fd_, deadline, "HTTP/1.1 400 Bad Request",
                  "WebSocket: missing Sec-WebSocket-Key header");
  }

  const std::string accept = ComputeAcceptKey(*key);

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

  // RFC 6455 §5.2 frame header layout:
  // https://datatracker.ietf.org/doc/html/rfc6455#section-5.2
  // - byte 0 carries FIN and the opcode
  // - byte 1 carries MASK and the 7-bit payload length
  // - payload lengths 126 and 127 add 16-bit or 64-bit extended lengths
  // - client-to-server frames must include the masking key
  uint8_t b0;
  ReadAll(fd_, pending_input_, &b0, 1, deadline, "read websocket frame");
  f.fin = (b0 & 0x80u) != 0;
  f.opcode = b0 & 0x0Fu;

  uint8_t b1;
  ReadAll(fd_, pending_input_, &b1, 1, deadline, "read websocket frame");
  bool masked = (b1 & 0x80u) != 0;
  uint64_t payload_len = b1 & 0x7Fu;

  if (payload_len == 126) {
    uint8_t ext[2];
    ReadAll(fd_, pending_input_, ext, 2, deadline, "read websocket frame");
    payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
  } else if (payload_len == 127) {
    uint8_t ext[8];
    ReadAll(fd_, pending_input_, ext, 8, deadline, "read websocket frame");
    payload_len = 0;
    for (int i = 0; i < 8; ++i) { payload_len = (payload_len << 8) | ext[i]; }
  }

  uint8_t mask[4] = {};
  if (masked) {
    ReadAll(fd_, pending_input_, mask, 4, deadline, "read websocket frame");
  }

  if (payload_len > max_frame_payload_size_) {
    throw std::runtime_error(
        "WebSocket: frame payload exceeds configured limit");
  }

  if (payload_len > 0) {
    f.payload.resize(static_cast<size_t>(payload_len));
    ReadAll(fd_, pending_input_, f.payload.data(),
            static_cast<size_t>(payload_len), deadline,
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
        if (f.payload.size() > max_message_size_ - message.size()) {
          throw std::runtime_error(
              "WebSocket: message payload exceeds configured limit");
        }
        message += f.payload;
        if (f.fin) { return message; }
        break;

      case kOpContinuation:
        if (f.payload.size() > max_message_size_ - message.size()) {
          throw std::runtime_error(
              "WebSocket: message payload exceeds configured limit");
        }
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
