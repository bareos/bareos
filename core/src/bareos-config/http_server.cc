/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#include <atomic>
#include <cctype>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
constexpr int kHeaderReadTimeoutSeconds = 10;
constexpr int kMaxConcurrentWorkers = 64;

std::string ReadHttpHeaders(int fd)
{
  std::string buffer;
  buffer.reserve(2048);
  char c;
  while (true) {
    const auto n = recv(fd, &c, 1, 0);
    if (n <= 0) return buffer;
    buffer += c;
    if (buffer.size() >= 4
        && buffer.compare(buffer.size() - 4, 4, "\r\n\r\n") == 0) {
      break;
    }
    if (buffer.size() > 65536) break;
  }
  return buffer;
}

std::string GetRequestLinePart(const std::string& headers, int index)
{
  const auto line_end = headers.find("\r\n");
  if (line_end == std::string::npos) return {};
  const std::string first_line = headers.substr(0, line_end);

  size_t token_start = 0;
  for (int token_index = 0; token_index <= index; ++token_index) {
    const auto token_end = first_line.find(' ', token_start);
    if (token_end == std::string::npos) {
      return token_index == index ? first_line.substr(token_start) : "";
    }
    if (token_index == index) {
      return first_line.substr(token_start, token_end - token_start);
    }
    token_start = token_end + 1;
  }
  return {};
}

std::string GetRequestMethod(const std::string& headers)
{
  return GetRequestLinePart(headers, 0);
}

std::string GetRequestPath(const std::string& headers)
{
  auto path = GetRequestLinePart(headers, 1);
  const auto query_start = path.find('?');
  if (query_start != std::string::npos) path = path.substr(0, query_start);
  if (path.empty()) return "/";
  return path;
}

std::string Lowercase(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string Trim(std::string value)
{
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  return value;
}

std::string GetHeaderValue(const std::string& headers, const std::string& name)
{
  const auto lower_name = Lowercase(name);
  size_t line_start = headers.find("\r\n");
  if (line_start == std::string::npos) return {};
  line_start += 2;

  while (line_start < headers.size()) {
    const auto line_end = headers.find("\r\n", line_start);
    if (line_end == std::string::npos || line_end == line_start) break;

    const auto separator = headers.find(':', line_start);
    if (separator != std::string::npos && separator < line_end) {
      const auto header_name = Lowercase(
          headers.substr(line_start, separator - line_start));
      if (header_name == lower_name) {
        return Trim(headers.substr(separator + 1, line_end - separator - 1));
      }
    }

    line_start = line_end + 2;
  }

  return {};
}

size_t ParseContentLength(const std::string& headers)
{
  const auto value = GetHeaderValue(headers, "Content-Length");
  if (value.empty()) return 0;

  size_t content_length = 0;
  for (char c : value) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return 0;
    content_length = content_length * 10 + static_cast<size_t>(c - '0');
  }
  return content_length;
}

std::string ReadHttpBody(int fd, size_t content_length)
{
  std::string body;
  body.resize(content_length);
  size_t offset = 0;

  while (offset < content_length) {
    const auto n = recv(fd, body.data() + offset, content_length - offset, 0);
    if (n <= 0) {
      body.resize(offset);
      return body;
    }
    offset += static_cast<size_t>(n);
  }

  return body;
}

void WriteAll(int fd, const std::string& data)
{
  const char* current = data.data();
  size_t remaining = data.size();
  while (remaining > 0) {
    const auto n = send(fd, current, remaining, MSG_NOSIGNAL);
    if (n <= 0) return;
    current += n;
    remaining -= static_cast<size_t>(n);
  }
}

std::string StatusText(int status_code)
{
  switch (status_code) {
    case 200:
      return "OK";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 503:
      return "Service Unavailable";
    default:
      return "Internal Server Error";
  }
}

std::string BuildHttpResponse(const HttpResponse& response)
{
  std::string payload;
  payload += "HTTP/1.1 " + std::to_string(response.status_code) + " "
             + StatusText(response.status_code) + "\r\n";
  payload += "Content-Type: " + response.mime_type + "\r\n";
  payload += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
  payload += "Cache-Control: no-cache\r\n";
  payload += "Connection: close\r\n\r\n";
  payload += response.body;
  return payload;
}
}  // namespace

void RunHttpServer(const std::string& bind_address,
                   int port,
                   const HttpHandler& handler)
{
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) throw std::runtime_error("socket() failed");

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr) != 1) {
    close(server_fd);
    throw std::runtime_error("invalid bind address: " + bind_address);
  }

  if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    close(server_fd);
    throw std::runtime_error("bind() failed on " + bind_address + ":"
                             + std::to_string(port));
  }
  if (listen(server_fd, 16) < 0) {
    close(server_fd);
    throw std::runtime_error("listen() failed");
  }

  std::cout << "bareos-config listening on http://" << bind_address << ":"
            << port << "/\n"
            << std::flush;

  std::atomic<int> active_workers{0};
  while (true) {
    const int fd = accept(server_fd, nullptr, nullptr);
    if (fd < 0) continue;

    if (active_workers.load(std::memory_order_relaxed)
        >= kMaxConcurrentWorkers) {
      const HttpResponse busy{503, "text/plain; charset=utf-8",
                              "Service Unavailable"};
      WriteAll(fd, BuildHttpResponse(busy));
      close(fd);
      continue;
    }

    active_workers.fetch_add(1, std::memory_order_relaxed);
    std::thread([fd, &active_workers, handler]() {
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

      const auto headers = ReadHttpHeaders(fd);
      if (headers.empty()) return;

      const HttpRequest request{GetRequestMethod(headers),
                                GetRequestPath(headers),
                                GetHeaderValue(headers, "Content-Type"),
                                ReadHttpBody(fd, ParseContentLength(headers))};
      const auto response = handler(request);
      WriteAll(fd, BuildHttpResponse(response));
    }).detach();
  }
}
