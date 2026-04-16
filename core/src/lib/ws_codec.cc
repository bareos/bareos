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
#include <cstring>
#include <stdexcept>
#include <string>

#include <sys/socket.h>
#include <unistd.h>

#include <openssl/bio.h>
#include <openssl/buffer.h>
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

void WsCodec::WriteAll(const void* buf, size_t len)
{
  const auto* p = static_cast<const uint8_t*>(buf);
  while (len > 0) {
    ssize_t n = ::send(fd_, p, len, MSG_NOSIGNAL);
    if (n <= 0) { throw std::runtime_error("WebSocket: send failed"); }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

void WsCodec::ReadAll(void* buf, size_t len)
{
  auto* p = static_cast<uint8_t*>(buf);
  while (len > 0) {
    ssize_t n = ::recv(fd_, p, len, MSG_WAITALL);
    if (n <= 0) {
      throw std::runtime_error("WebSocket: connection closed by peer");
    }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

// ---------------------------------------------------------------------------
// Handshake
// ---------------------------------------------------------------------------

static std::string Base64Encode(const uint8_t* data, size_t len)
{
  BIO* b64 = BIO_new(BIO_f_base64());
  BIO* mem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, mem);
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(b64, data, static_cast<int>(len));
  BIO_flush(b64);

  BUF_MEM* bptr = nullptr;
  BIO_get_mem_ptr(b64, &bptr);
  std::string result(bptr->data, bptr->length);
  BIO_free_all(b64);
  return result;
}

static std::string ComputeAcceptKey(const std::string& client_key)
{
  std::string combined = client_key + kWsMagic;

  uint8_t sha1[EVP_MAX_MD_SIZE];
  unsigned int sha1_len = 0;

  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
  EVP_DigestUpdate(ctx, combined.data(), combined.size());
  EVP_DigestFinal_ex(ctx, sha1, &sha1_len);
  EVP_MD_CTX_free(ctx);

  return Base64Encode(sha1, sha1_len);
}

void WsCodec::Handshake()
{
  // Read HTTP request line by line until blank line
  std::string headers;
  headers.reserve(1024);
  char ch;
  while (true) {
    ssize_t n = ::recv(fd_, &ch, 1, 0);
    if (n <= 0) {
      throw std::runtime_error("WebSocket: connection lost during handshake");
    }
    headers += ch;
    if (headers.size() >= 4
        && headers.compare(headers.size() - 4, 4, "\r\n\r\n") == 0) {
      break;
    }
    if (headers.size() > 16384) {
      throw std::runtime_error("WebSocket: HTTP headers too large");
    }
  }

  // Extract Sec-WebSocket-Key
  std::string key;
  const std::string key_header = "Sec-WebSocket-Key:";
  auto pos = headers.find(key_header);
  if (pos == std::string::npos) {
    // Try case-insensitive search
    std::string lower_headers = headers;
    std::transform(lower_headers.begin(), lower_headers.end(),
                   lower_headers.begin(), ::tolower);
    std::string lower_key_header = "sec-websocket-key:";
    pos = lower_headers.find(lower_key_header);
    if (pos == std::string::npos) {
      throw std::runtime_error("WebSocket: missing Sec-WebSocket-Key header");
    }
  }
  pos += key_header.size();
  auto end = headers.find("\r\n", pos);
  if (end == std::string::npos) {
    throw std::runtime_error("WebSocket: malformed Sec-WebSocket-Key header");
  }
  key = headers.substr(pos, end - pos);
  while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) {
    key.erase(key.begin());
  }
  while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) {
    key.pop_back();
  }

  std::string accept = ComputeAcceptKey(key);

  std::string response
      = "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: "
        + accept + "\r\n\r\n";

  WriteAll(response.data(), response.size());
}

// ---------------------------------------------------------------------------
// Frame encode / decode
// ---------------------------------------------------------------------------

WsCodec::Frame WsCodec::RecvFrame()
{
  Frame f;

  uint8_t b0;
  ReadAll(&b0, 1);
  f.fin = (b0 & 0x80u) != 0;
  f.opcode = b0 & 0x0Fu;

  uint8_t b1;
  ReadAll(&b1, 1);
  bool masked = (b1 & 0x80u) != 0;
  uint64_t payload_len = b1 & 0x7Fu;

  if (payload_len == 126) {
    uint8_t ext[2];
    ReadAll(ext, 2);
    payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
  } else if (payload_len == 127) {
    uint8_t ext[8];
    ReadAll(ext, 8);
    payload_len = 0;
    for (int i = 0; i < 8; ++i) { payload_len = (payload_len << 8) | ext[i]; }
  }

  uint8_t mask[4] = {};
  if (masked) { ReadAll(mask, 4); }

  if (payload_len > kMaxMessageSize) {
    throw std::runtime_error("WebSocket: frame too large");
  }

  if (payload_len > 0) {
    f.payload.resize(static_cast<size_t>(payload_len));
    ReadAll(f.payload.data(), static_cast<size_t>(payload_len));
    if (masked) {
      for (size_t i = 0; i < f.payload.size(); ++i) {
        f.payload[i] ^= static_cast<char>(mask[i % 4]);
      }
    }
  }

  return f;
}

void WsCodec::SendFrame(uint8_t opcode, const std::string& payload, bool fin)
{
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
  WriteAll(frame.data(), frame.size());
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
        if (message.size() + f.payload.size() > kMaxMessageSize) {
          throw std::runtime_error("WebSocket: message too large");
        }
        message += f.payload;
        if (f.fin) { return message; }
        break;

      case kOpContinuation:
        if (message.size() + f.payload.size() > kMaxMessageSize) {
          throw std::runtime_error("WebSocket: message too large");
        }
        message += f.payload;
        if (f.fin) { return message; }
        break;

      default:
        throw std::runtime_error("WebSocket: unknown opcode");
    }
  }
}

void WsCodec::SendText(const std::string& payload)
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
