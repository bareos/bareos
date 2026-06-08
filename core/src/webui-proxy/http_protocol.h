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
#ifndef BAREOS_WEBUI_PROXY_HTTP_PROTOCOL_H_
#define BAREOS_WEBUI_PROXY_HTTP_PROTOCOL_H_

#include <chrono>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct HttpRequest {
  std::string method;
  std::string target;
  std::string version;
  std::string raw_headers;
  std::string body;
  std::string pending_input;

  std::optional<std::string_view> HeaderValue(std::string_view name) const;
};

HttpRequest ReadHttpRequest(int fd,
                            std::chrono::milliseconds timeout
                            = std::chrono::seconds(5),
                            size_t max_header_size = 16 * 1024,
                            size_t max_body_size = 64 * 1024);

bool IsWebSocketUpgradeRequest(const HttpRequest& request);

void SendHttpResponse(
    int fd,
    std::chrono::milliseconds timeout,
    std::string_view status_line,
    std::string_view body,
    std::initializer_list<std::pair<std::string_view, std::string_view>> headers
    = {});

std::optional<std::string_view> FindCookieValue(std::string_view cookie_header,
                                                std::string_view name);

#endif  // BAREOS_WEBUI_PROXY_HTTP_PROTOCOL_H_
