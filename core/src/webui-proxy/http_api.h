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
#ifndef BAREOS_WEBUI_PROXY_HTTP_API_H_
#define BAREOS_WEBUI_PROXY_HTTP_API_H_

#include "auth_session.h"
#include "oidc_auth.h"
#include "proxy_config.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

struct HttpRequest {
  std::string method;
  std::string target;
  std::string path;
  std::map<std::string, std::string> headers;
  std::string body;
};

struct HttpResponse {
  int status_code{200};
  std::string reason{"OK"};
  std::string body;
  std::map<std::string, std::string> headers;
};

HttpRequest ParseHttpRequest(const std::string& request_head, std::string body);

bool IsWebSocketUpgradeRequest(const HttpRequest& request);

HttpResponse HandleHttpApiRequest(
    const ProxyConfig& config,
    const std::shared_ptr<ProxySessionStore>& session_store,
    const std::shared_ptr<OidcPendingAuthStore>& oidc_store,
    const HttpRequest& request,
    const OidcHttpClient* oidc_http_client = nullptr);

std::string BuildHttpResponse(const HttpResponse& response);

#endif  // BAREOS_WEBUI_PROXY_HTTP_API_H_
