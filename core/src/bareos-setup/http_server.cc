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

static constexpr int kMaxConcurrentWorkers = 64;
static constexpr int kHeaderReadTimeoutSeconds = 10;

// ---- helpers ---------------------------------------------------------------

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

void RunHttpServer(int port, WsHandler ws_handler)
{
  int srv = socket(AF_INET, SOCK_STREAM, 0);
  if (srv < 0) throw std::runtime_error("socket() failed");

  int opt = 1;
  setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // localhost only

  if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    throw std::runtime_error("bind() failed on port " + std::to_string(port));
  if (listen(srv, 16) < 0) throw std::runtime_error("listen() failed");

  std::cout << "bareos-setup listening on http://localhost:" << port << "/\n"
            << std::flush;

  std::atomic<int> active_workers{0};
  while (true) {
    int fd = accept(srv, nullptr, nullptr);
    if (fd < 0) continue;

    if (active_workers.load(std::memory_order_relaxed)
        >= kMaxConcurrentWorkers) {
      const char* r503
          = "HTTP/1.1 503 Service Unavailable\r\n"
            "Connection: close\r\n"
            "Content-Length: 19\r\n\r\nService Unavailable";
      WriteAll(fd, r503, strlen(r503));
      close(fd);
      continue;
    }

    active_workers.fetch_add(1, std::memory_order_relaxed);

    // Handle each connection in its own thread
    std::thread([fd, ws_handler, &active_workers]() {
      struct WorkerGuard {
        int fd;
        std::atomic<int>& active_workers;
        ~WorkerGuard()
        {
          close(fd);
          active_workers.fetch_sub(1, std::memory_order_relaxed);
        }
      } guard{fd, active_workers};

      const timeval header_timeout{kHeaderReadTimeoutSeconds, 0};
      setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &header_timeout,
                 sizeof(header_timeout));

      std::string headers = ReadHttpHeaders(fd);
      if (headers.empty()) {
        return;
      }

      std::string upgrade = GetHeader(headers, "Upgrade");
      bool is_ws = (upgrade.find("websocket") != std::string::npos
                    || upgrade.find("WebSocket") != std::string::npos);

      if (is_ws) {
        const timeval no_timeout{0, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &no_timeout,
                   sizeof(no_timeout));

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
        ws_handler(fd);
      } else {
        std::string path = GetRequestPath(headers);
        // Strip query string
        auto q = path.find('?');
        if (q != std::string::npos) path = path.substr(0, q);
        ServeStaticFile(fd, path);
      }
    }).detach();
  }
}
