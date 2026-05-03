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
#ifndef BAREOS_WEBUI_PROXY_OIDC_AUTH_H_
#define BAREOS_WEBUI_PROXY_OIDC_AUTH_H_

#include <chrono>
#include <ctime>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct OidcAuthProvider {
  std::string id;
  std::string display_name;
  std::string issuer;
  std::string authorization_endpoint;
  std::string token_endpoint;
  std::string jwks_uri;
  std::string client_id;
  std::string client_secret;
  std::string redirect_uri;
  std::vector<std::string> scopes;
};

struct OidcPendingAuth {
  std::string provider_id;
  std::string state;
  std::string nonce;
  std::string code_verifier;
  std::time_t created_at{0};
  std::time_t expires_at{0};
};

using OidcClock = std::function<std::time_t()>;

class OidcPendingAuthStore {
 public:
  explicit OidcPendingAuthStore(
      std::chrono::seconds ttl = std::chrono::minutes(10),
      OidcClock clock = []() { return std::time(nullptr); });

  OidcPendingAuth CreatePendingAuth(const std::string& provider_id);
  std::optional<OidcPendingAuth> ConsumePendingAuth(const std::string& state);

 private:
  std::string GenerateOpaqueToken() const;
  void RemoveExpiredStatesLocked(std::time_t now);

  std::chrono::seconds ttl_;
  OidcClock clock_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, OidcPendingAuth> pending_;
};

std::string BuildOidcAuthorizationUrl(const OidcAuthProvider& provider,
                                      const OidcPendingAuth& pending_auth);

#endif  // BAREOS_WEBUI_PROXY_OIDC_AUTH_H_
