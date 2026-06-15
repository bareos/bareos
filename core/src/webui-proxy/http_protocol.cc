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
#include "http_protocol.h"

#include "lib/bpoll.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

#include <poll.h>
#include <sys/socket.h>

namespace {

using Clock = std::chrono::steady_clock;
constexpr size_t kReadChunkSize = 1024;

int RemainingTimeoutMs(Clock::time_point deadline)
{
  const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
      deadline - Clock::now());
  return remaining.count() > 0 ? static_cast<int>(remaining.count()) : 0;
}

Clock::time_point MakeDeadline(std::chrono::milliseconds timeout)
{
  return Clock::now() + timeout;
}

void WriteAll(int fd, const void* buf, size_t len, Clock::time_point deadline)
{
  const auto* p = static_cast<const uint8_t*>(buf);
  while (len > 0) {
    WaitForSocket(fd, POLLOUT, deadline, "write HTTP response");
    const ssize_t n = ::send(fd, p, len, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      throw std::runtime_error("HTTP: send failed");
    }
    if (n == 0) { throw std::runtime_error("HTTP: send failed"); }
    p += n;
    len -= static_cast<size_t>(n);
  }
}

size_t ReadChunk(int fd,
                 std::string& target,
                 Clock::time_point deadline,
                 std::string_view action)
{
  WaitForSocket(fd, POLLIN, deadline, action);
  const auto original_size = target.size();
  target.resize(original_size + kReadChunkSize);

  while (true) {
    const ssize_t n = ::recv(fd, target.data() + original_size, kReadChunkSize, 0);
    if (n < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        WaitForSocket(fd, POLLIN, deadline, action);
        continue;
      }
      target.resize(original_size);
      throw std::runtime_error("HTTP: recv failed");
    }
    if (n == 0) {
      target.resize(original_size);
      throw std::runtime_error("HTTP: connection closed by peer");
    }
    target.resize(original_size + static_cast<size_t>(n));
    return static_cast<size_t>(n);
  }
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

size_t ParseContentLength(const HttpRequest& request, size_t max_body_size)
{
  const auto content_length = request.HeaderValue("Content-Length");
  if (!content_length) {
    return 0;
  }

  size_t body_size = 0;
  for (const char ch : *content_length) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      throw std::runtime_error("HTTP: invalid Content-Length header");
    }
    body_size = body_size * 10 + static_cast<size_t>(ch - '0');
    if (body_size > max_body_size) {
      throw std::runtime_error("HTTP: request body too large");
    }
  }
  return body_size;
}

}  // namespace

void WaitForSocket(int fd,
                   short events,
                   Clock::time_point deadline,
                   std::string_view action)
{
  const auto wait_for_fd = [fd, events](int timeout_ms) {
    if (events == POLLIN) { return WaitForReadableFd(fd, timeout_ms, true); }
    if (events == POLLOUT) { return WaitForWritableFd(fd, timeout_ms, true); }
    throw std::runtime_error("HTTP: unsupported wait event");
  };

  while (true) {
    switch (wait_for_fd(RemainingTimeoutMs(deadline))) {
      case 1:
        return;
      case 0:
        throw std::runtime_error("HTTP: timeout while waiting to "
                                 + std::string(action));
      case -1:
        throw std::runtime_error("HTTP: socket error while waiting to "
                                 + std::string(action));
      default:
        throw std::runtime_error("HTTP: invalid wait result");
    }
  }
}

std::optional<std::string_view> HttpRequest::HeaderValue(std::string_view name) const
{
  return FindHeaderValue(raw_headers, name);
}

HttpRequest ReadHttpRequest(int fd,
                            std::chrono::milliseconds timeout,
                            size_t max_header_size,
                            size_t max_body_size)
{
  const auto deadline = MakeDeadline(timeout);
  std::string input;
  input.reserve(1024);

  size_t header_end = std::string::npos;
  while (true) {
    ReadChunk(fd, input, deadline, "read HTTP request");
    header_end = input.find("\r\n\r\n");
    if (header_end != std::string::npos) {
      header_end += 4;
      break;
    }
    if (input.size() > max_header_size) {
      throw std::runtime_error("HTTP: headers too large");
    }
  }

  HttpRequest request;
  request.raw_headers.assign(input.data(), header_end);
  request.pending_input.assign(input.data() + header_end, input.size() - header_end);

  const std::string_view headers_view(request.raw_headers);
  const auto request_line_end = headers_view.find("\r\n");
  if (request_line_end == std::string_view::npos) {
    throw std::runtime_error("HTTP: malformed request line");
  }

  const auto request_line = headers_view.substr(0, request_line_end);
  const auto first_space = request_line.find(' ');
  const auto second_space = request_line.rfind(' ');
  if (first_space == std::string_view::npos || second_space == std::string_view::npos
      || first_space == second_space) {
    throw std::runtime_error("HTTP: malformed request line");
  }

  request.method = std::string(request_line.substr(0, first_space));
  request.target = std::string(
      request_line.substr(first_space + 1, second_space - first_space - 1));
  request.version = std::string(request_line.substr(second_space + 1));

  const auto body_size = ParseContentLength(request, max_body_size);
  while (request.pending_input.size() < body_size) {
    ReadChunk(fd, request.pending_input, deadline, "read HTTP request body");
  }
  request.body.assign(request.pending_input.data(), body_size);
  request.pending_input.erase(0, body_size);

  return request;
}

bool IsWebSocketUpgradeRequest(const HttpRequest& request)
{
  const auto upgrade = request.HeaderValue("Upgrade");
  const auto connection = request.HeaderValue("Connection");
  return request.method == "GET" && upgrade && connection
         && EqualsIgnoreCase(*upgrade, "websocket")
         && HeaderHasToken(*connection, "Upgrade");
}

void SendHttpResponse(
    int fd,
    std::chrono::milliseconds timeout,
    std::string_view status_line,
    std::string_view body,
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers)
{
  const auto deadline = MakeDeadline(timeout);
  std::string response;
  response.reserve(status_line.size() + body.size() + 256);
  response += status_line;
  response += "\r\n";
  for (const auto& [name, value] : headers) {
    response += std::string(name);
    response += ": ";
    response += std::string(value);
    response += "\r\n";
  }
  response += "Content-Length: ";
  response += std::to_string(body.size());
  response += "\r\nConnection: close\r\n\r\n";
  response += body;

  WriteAll(fd, response.data(), response.size(), deadline);
}

std::optional<std::string_view> FindCookieValue(std::string_view cookie_header,
                                                std::string_view name)
{
  while (!cookie_header.empty()) {
    const auto separator = cookie_header.find(';');
    auto cookie = cookie_header.substr(0, separator);
    while (!cookie.empty() && cookie.front() == ' ') {
      cookie.remove_prefix(1);
    }
    const auto equals = cookie.find('=');
    if (equals != std::string_view::npos
        && cookie.substr(0, equals) == name) {
      return cookie.substr(equals + 1);
    }
    if (separator == std::string_view::npos) { break; }
    cookie_header.remove_prefix(separator + 1);
  }

  return std::nullopt;
}
