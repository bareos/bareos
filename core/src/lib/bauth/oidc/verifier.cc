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

#include "verifier.h"

#include <curl/curl.h>
#include <curl/easy.h>
#include <jansson.h>
#include "lib/base64.h"
#include <iostream>
#include <thread>

#include <openssl/core_names.h>

#include <sstream>
#include <string_view>

#include "common.h"
#include "lib/bauth/oidc/curl_http.h"

namespace auth::Oidc {
std::vector<char> Verifier::generate_challenge()
{
  json_error_t err;
  json_ptr chal{json_pack_ex(&err, JSON_COMPACT, "{s:[s]}",
                             "supported_auth_flows",
                             oidc::DeviceAuthenticationFlow::flow_name)};

  if (!chal) { return {}; }

  std::vector<char> result;

  dump_json(&result, chal.get());

  return result;
}

Status Verifier::initiate_flow(std::vector<char>& response,
                               std::span<const char> received)
{
  std::string_view received_view{received.begin(), received.end()};

  if (received_view != oidc::DeviceAuthenticationFlow::flow_name) {
    return Error("client requested unknown flow");
  }

  if (cfg.oidc_url.empty()) { return Error("empty oidc url"); }

  client = std::make_unique<http::curl_client>();
  if (!client) { return Error("could not create curl object"); }

  oidc::client_config oidc_cfg = {
      .client = client.get(),
      .client_id = cfg.client_id,
      .oidc_url = cfg.oidc_url,
  };

  oidc = oidc::client::fetch_meta_data(std::move(oidc_cfg));

  keys = oidc->fetch_keys_from_jwks();

  current_flow
      = oidc::DeviceAuthenticationFlow::start(&*oidc, "openid profile");

  auto& uri = current_flow->verification_uri();
  auto& code = current_flow->user_code();
  auto& uri_complete = current_flow->verification_uri_complete();

  json_error_t err;
  json_ptr result_json{json_pack_ex(
      &err, 0, "{s:s, s:s, s:s*}", "url", uri.c_str(), "code", code.c_str(),
      "complete_url", uri_complete ? uri_complete->c_str() : nullptr)};

  dump_json(&response, result_json.get());

  current_state = FLOW_STARTED;
  return Status::InProgress;
}

Status Verifier::step(std::vector<char>& response,
                      std::span<const char> received)
{
  try {
    switch (current_state) {
      case INITIAL: {
        return initiate_flow(response, received);
      } break;
      case FLOW_STARTED: {
        if (!received.empty()
            && std::string_view{received.data(), received.size()} != "pong") {
          return Error("received unexpected input from client");
        }
        if (!current_flow) {
          return Error("authentication flow was not setup");
        }

        std::string token = {};

        if (!current_flow->fetch_token(&token)) {
          static constexpr std::string_view ping = "ping";
          response.clear();
          response.insert(response.end(), ping.begin(), ping.end());
          return Status::InProgress;
        }

        auto claims
            = oidc::VerifyToken(token, cfg.oidc_url, cfg.client_id,
                                cfg.allowed_clock_skew, cfg.roles_name, keys);

        roles = std::move(claims.roles);
        if (claims.preferred_name.empty()) {
          user_name = std::move(claims.user_name);
        } else {
          user_name = std::move(claims.preferred_name);
        }
        return Status::Done;
      } break;
      default: {
        return Error("entered with bad state");
      } break;
    }
  } catch (const std::exception& ex) {
    return Error(ex.what());
  }
}
};  // namespace auth::Oidc
