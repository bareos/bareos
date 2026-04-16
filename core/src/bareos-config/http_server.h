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
#ifndef BAREOS_BAREOS_CONFIG_HTTP_SERVER_H_
#define BAREOS_BAREOS_CONFIG_HTTP_SERVER_H_

#include <functional>
#include <string>

struct HttpResponse {
  int status_code = 200;
  std::string mime_type = "text/plain; charset=utf-8";
  std::string body;
};

struct HttpRequest {
  std::string method;
  std::string path;
  std::string content_type;
  std::string body;
};

using HttpHandler = std::function<HttpResponse(const HttpRequest& request)>;

void RunHttpServer(const std::string& bind_address,
                   int port,
                   const HttpHandler& handler);

#endif  // BAREOS_BAREOS_CONFIG_HTTP_SERVER_H_
