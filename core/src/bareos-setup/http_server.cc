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
#include "http_server.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstring>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/sha.h>

#include "embedded_assets.h"
#include "setup_steps.h"

// ---- helpers ---------------------------------------------------------------

namespace {

std::atomic_bool g_shutdown_requested{false};
std::atomic_int g_server_fd{-1};

bool IsLoopbackPeer(const sockaddr_storage& peer)
{
  if (peer.ss_family == AF_INET) {
    const auto* addr = reinterpret_cast<const sockaddr_in*>(&peer);
    return (ntohl(addr->sin_addr.s_addr) & 0xff000000U) == 0x7f000000U;
  }
  return false;
}

}  // namespace

/** Read bytes from fd until we see "\r\n\r\n". Returns the full header block.
 */
static std::string ReadHttpHeaders(int fd)
{
  std::string buf;
  buf.reserve(2048);
  char c;
  while (true) {
    ssize_t n = recv(fd, &c, 1, 0);
    if (n <= 0) return buf;
    buf += c;
    if (buf.size() >= 4 && buf.compare(buf.size() - 4, 4, "\r\n\r\n") == 0)
      break;
    if (buf.size() > 65536) break;  // safety limit
  }
  return buf;
}

/** Case-insensitive header value lookup from raw HTTP header block. */
static std::string GetHeader(const std::string& headers,
                             const std::string& name)
{
  // Search for "name:" (case-insensitive)
  std::string search = name + ":";
  auto it = std::search(
      headers.begin(), headers.end(), search.begin(), search.end(),
      [](char a, char b) { return std::tolower(a) == std::tolower(b); });
  if (it == headers.end()) return {};
  it += static_cast<std::ptrdiff_t>(search.size());
  // skip leading whitespace
  while (it != headers.end() && (*it == ' ' || *it == '\t')) ++it;
  // collect until \r\n
  std::string val;
  while (it != headers.end() && *it != '\r' && *it != '\n') val += *it++;
  return val;
}

/** Extract the request path from the first line of an HTTP request. */
static std::string GetRequestPath(const std::string& headers)
{
  auto nl = headers.find("\r\n");
  if (nl == std::string::npos) return "/";
  std::string first = headers.substr(0, nl);
  // "GET /path HTTP/1.1"
  auto s1 = first.find(' ');
  if (s1 == std::string::npos) return "/";
  auto s2 = first.find(' ', s1 + 1);
  if (s2 == std::string::npos) return first.substr(s1 + 1);
  return first.substr(s1 + 1, s2 - s1 - 1);
}

/** Decode percent-encoded octets (and '+' as space) in a URL query
 * component, as produced by JavaScript's encodeURIComponent(). */
static std::string UrlDecode(const std::string& value)
{
  std::string result;
  result.reserve(value.size());
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()
        && std::isxdigit(static_cast<unsigned char>(value[i + 1]))
        && std::isxdigit(static_cast<unsigned char>(value[i + 2]))) {
      std::string hex = value.substr(i + 1, 2);
      result += static_cast<char>(std::stoi(hex, nullptr, 16));
      i += 2;
    } else if (value[i] == '+') {
      result += ' ';
    } else {
      result += value[i];
    }
  }
  return result;
}

// ---- WebSocket upgrade key computation ------------------------------------

static constexpr const char kWsMagic[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

static std::string Base64Encode(const unsigned char* data, size_t len)
{
  BIO* b64 = BIO_new(BIO_f_base64());
  BIO* bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
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
  unsigned char sha1[SHA_DIGEST_LENGTH];
  SHA1(reinterpret_cast<const unsigned char*>(combined.data()), combined.size(),
       sha1);
  return Base64Encode(sha1, SHA_DIGEST_LENGTH);
}

// ---- Static file serving --------------------------------------------------

static void WriteAll(int fd, const void* buf, size_t len)
{
  const char* p = static_cast<const char*>(buf);
  while (len > 0) {
    ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
    if (n <= 0) return;
    p += n;
    len -= static_cast<size_t>(n);
  }
}

/** Find an embedded asset by URL path. Returns nullptr if not found. */
static const EmbeddedFile* FindAsset(const std::string& path)
{
  for (size_t i = 0; i < kEmbeddedFilesCount; ++i) {
    if (path == kEmbeddedFiles[i].path) return &kEmbeddedFiles[i];
  }
  return nullptr;
}

/** Find /index.html (SPA fallback). */
static const EmbeddedFile* FindIndexHtml() { return FindAsset("/index.html"); }

static void ServeStaticFile(int fd, const std::string& path)
{
  const EmbeddedFile* ef = FindAsset(path);
  if (!ef) ef = FindIndexHtml();  // SPA fallback
  if (!ef) {
    const char* r404
        = "HTTP/1.1 404 Not Found\r\n"
          "Content-Length: 9\r\n\r\nNot Found";
    WriteAll(fd, r404, strlen(r404));
    return;
  }

  std::ostringstream hdr;
  hdr << "HTTP/1.1 200 OK\r\n"
      << "Content-Type: " << ef->mime << "\r\n"
      << "Content-Length: " << ef->size << "\r\n";
  if (ef->gzipped) hdr << "Content-Encoding: gzip\r\n";
  hdr << "Cache-Control: no-cache\r\n"
      << "\r\n";
  std::string h = hdr.str();
  WriteAll(fd, h.data(), h.size());
  WriteAll(fd, ef->data, ef->size);
}

// ---- Main server loop -----------------------------------------------------

void RunHttpServer(const std::string& bind_address,
                   int port,
                   const std::string& setup_token,
                   WsHandler ws_handler)
{
  g_shutdown_requested = false;
  int srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) throw std::runtime_error("socket() failed");
  g_server_fd = srv;

  int opt = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr) != 1) {
    throw std::runtime_error("Invalid listen address: " + bind_address);
  }

  if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    throw std::runtime_error("bind() failed on " + bind_address + ":"
                             + std::to_string(port));
  if (listen(srv, 16) < 0) throw std::runtime_error("listen() failed");

  std::cout << "bareos-setup listening on http://" << bind_address << ":"
            << port << "/\n"
            << std::flush;

  while (!g_shutdown_requested) {
    sockaddr_storage peer{};
    socklen_t peer_len = sizeof(peer);
    int fd = accept(srv, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (fd < 0) {
      if (g_shutdown_requested) break;
      continue;
    }

    // Handle each connection in its own thread
    const bool peer_is_loopback = IsLoopbackPeer(peer);
    std::thread([fd, setup_token, ws_handler, peer_is_loopback]() {
      std::string headers = ReadHttpHeaders(fd);
      if (headers.empty()) {
        close(fd);
        return;
      }

      std::string upgrade = GetHeader(headers, "Upgrade");
      bool is_ws = (upgrade.find("websocket") != std::string::npos
                    || upgrade.find("WebSocket") != std::string::npos);

      if (is_ws) {
        const std::string origin = GetHeader(headers, "Origin");
        const std::string host_header = GetHeader(headers, "Host");
        const std::string path = GetRequestPath(headers);
        const auto path_end = path.find('?');
        const std::string request_path
            = path_end == std::string::npos ? path : path.substr(0, path_end);
        const auto query = path.find('?');
        const std::string query_string
            = query == std::string::npos ? "" : path.substr(query + 1);
        bool token_valid = false;
        std::istringstream query_stream(query_string);
        std::string parameter;
        while (std::getline(query_stream, parameter, '&')) {
          constexpr const char kTokenPrefix[] = "token=";
          constexpr size_t kTokenPrefixLen = sizeof(kTokenPrefix) - 1;
          if (parameter.compare(0, kTokenPrefixLen, kTokenPrefix) == 0
              && UrlDecode(parameter.substr(kTokenPrefixLen)) == setup_token) {
            token_valid = true;
            break;
          }
        }
        if (request_path != "/ws" || !IsValidSetupOrigin(origin, host_header)
            || !token_valid) {
          const char response[]
              = "HTTP/1.1 403 Forbidden\r\nContent-Length: 10\r\n"
                "Connection: close\r\n\r\nForbidden\n";
          WriteAll(fd, response, sizeof(response) - 1);
          close(fd);
          return;
        }
        // Send 101 Switching Protocols
        std::string key = GetHeader(headers, "Sec-WebSocket-Key");
        std::string accept = ComputeAcceptKey(key);

        std::string resp =
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: " + accept + "\r\n"
            "\r\n";
        WriteAll(fd, resp.data(), resp.size());

        // Delegate to WebSocket handler (blocks until connection closes)
        ws_handler(fd, peer_is_loopback);
      } else {
        std::string path = GetRequestPath(headers);
        // Strip query string
        auto q = path.find('?');
        if (q != std::string::npos) path = path.substr(0, q);
        ServeStaticFile(fd, path);
      }
      close(fd);
    }).detach();
  }

  const int fd = g_server_fd.exchange(-1);
  if (fd >= 0) close(fd);
}

void RequestHttpServerShutdown()
{
  g_shutdown_requested = true;
  const int fd = g_server_fd.exchange(-1);
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
  }
}
