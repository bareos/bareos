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

#ifndef BAREOS_LIB_BAUTH_OIDC_VERIFIER_H_
#define BAREOS_LIB_BAUTH_OIDC_VERIFIER_H_

#include "lib/bauth.h"
#include "lib/bauth/oidc/common.h"

#include <chrono>

namespace auth::Oidc {

// TODO: document that with entra id you need to set
// api.requestedAccessTokenVersion = 2 in the manifest

struct config {
  // we look at <oidc_url>/.well-known/openid-configuration
  std::string oidc_url{};
  std::string client_id{};
  std::string roles_name{};


  bool require_not_before{false};
  std::chrono::steady_clock::duration allowed_clock_skew
      = std::chrono::seconds{30};
};

struct Verifier;

struct site_metadata {
  std::string token_endpoint{};
  std::string jwks_uri{};
  std::string issuer{};
  std::string device_authorization_endpoint{};
};

struct UserClaims {
  std::string user_name;
  std::string preferred_name;
  std::vector<std::string> roles;
};

// each flow should actually be its own "type"
struct AuthenticationFlow {
  virtual auth::Status step(Verifier* verifier,
                            std::vector<char>& response,
                            std::span<const char> received)
      = 0;
  virtual ~AuthenticationFlow() = default;

  UserClaims verified_claims;

 protected:
  http::client* curl(Verifier* verifier);
  site_metadata& meta(Verifier* verifier);
  config& conf(Verifier* verifier);
};

struct Verifier : public auth::Verifier {
  using Base = auth::Verifier;

  Verifier(config conf) : cfg{std::move(conf)} {}

  std::string_view name() const override { return "oidc"; };
  std::vector<char> generate_challenge() override;
  auth::Status step(std::vector<char>&,
                    std::span<const char> received) override;
  const char* err() const override { return error.c_str(); }

  virtual ~Verifier() = default;

  std::string user_name;
  std::vector<std::string> roles;

  friend struct AuthenticationFlow;

 private:
  auth::Status initiate_flow(std::vector<char>& response,
                             std::span<const char> received);
  auth::Status check_token();

  enum
  {
    INITIAL = 0,
    FLOW_STARTED,
    CHECKED_TOKEN,
    ERROR,
  } current_state{INITIAL};


  config cfg;


  std::string error;
  site_metadata meta;
  std::unique_ptr<http::client> curl;
  std::unique_ptr<AuthenticationFlow> current_flow;

  Status Error(std::string message)
  {
    if (error.empty()) {
      error = std::move(message);
    } else {
      ASSERT(current_state == ERROR);
    }
    current_state = ERROR;
    return Status::Error;
  }
};
};  // namespace auth::Oidc


#endif  // BAREOS_LIB_BAUTH_OIDC_VERIFIER_H_
