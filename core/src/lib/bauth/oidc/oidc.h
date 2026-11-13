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

#ifndef BAREOS_LIB_BAUTH_OIDC_OIDC_H_
#define BAREOS_LIB_BAUTH_OIDC_OIDC_H_

#include <chrono>
#include <sstream>
#include "lib/bauth/oidc/common.h"
#include "lib/bauth/oidc/http.h"

namespace auth::Oidc::oidc {

struct DeviceAuthRequest {
  std::string user_code;
  std::string device_code;
  std::string verification_uri;

  // this is an optional part of RFC 8628.  Its a verification uri that
  // has the device code encoded inside of it.
  std::optional<std::string> verification_uri_complete;
  std::chrono::seconds expires_in;
  std::chrono::seconds poll_interval;
  std::string message;
};

struct client_config {
  http::client* client;
  std::string client_id;
  std::string oidc_url;
};

struct UserClaims {
  std::string user_name;
  std::string preferred_name;
  std::vector<std::string> roles;
};

using keyset = std::unordered_map<std::string, evp_pkey>;

struct client {
  static client fetch_meta_data(client_config cfg);

  keyset fetch_keys_from_jwks();

  // void add_keys(keyset keys);
  // const keyset& keys() const;

  const std::string& jwks_endpoint() const { return ep_jwks; }
  const std::string& device_auth_endpoint() const { return ep_device_auth; }
  const std::string& token_endpoint() const { return ep_token; }

  const std::string base_url() const { return oidc_url; }
  const std::string client_id() const { return my_id; }

  http::client* http;

 private:
  std::string my_id;
  std::string oidc_url;

  std::string ep_token;
  std::string ep_device_auth;
  std::string ep_jwks;
};

class DeviceAuthenticationFlow {
 public:
  static constexpr const char* flow_name = "device";

  static DeviceAuthenticationFlow start(oidc::client* client,
                                        const std::string& scopes);

  // return true if the token was received, false otherwise
  // will throw on error
  bool fetch_token(std::string* buffer);

  const std::string& user_code() const { return ucode; }
  const std::string& verification_uri() const { return vuri; }

  const std::optional<std::string>& verification_uri_complete() const
  {
    return vuri_complete;
  }

 private:
  using Clock = std::chrono::steady_clock;

  DeviceAuthenticationFlow(oidc::client* client,
                           std::string user_code,
                           std::string device_code,
                           std::string verification_uri,
                           std::optional<std::string> verification_uri_complete,
                           Clock::time_point expiration_point,
                           Clock::duration poll_interval) noexcept
      : oidc{client}
      , ucode{std::move(user_code)}
      , dcode{std::move(device_code)}
      , vuri{std::move(verification_uri)}
      , vuri_complete{std::move(verification_uri_complete)}
      , expiry{expiration_point}
      , pinterval{poll_interval}
  {
  }

  oidc::client* oidc;

  std::string ucode;
  std::string dcode;
  std::string vuri;
  std::optional<std::string> vuri_complete;

  Clock::time_point expiry;
  Clock::duration pinterval;
};

UserClaims VerifyToken(std::string_view token,
                       const std::string& issuer,
                       const std::string& audience,
                       std::chrono::seconds allowed_clock_skew,
                       const std::string& roles_name,
                       const keyset& keys);

};  // namespace auth::Oidc::oidc


#endif  // BAREOS_LIB_BAUTH_OIDC_OIDC_H_
