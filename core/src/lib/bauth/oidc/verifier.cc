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
namespace {
evp_pkey create_public_key(std::span<const char> modulus,
                           std::span<const char> exponent)
{
  // modulus is big endian encoded, but openssl expects the bytes in
  // native format
  std::vector<unsigned char> native(modulus.size());
  for (size_t i = 0; i < modulus.size(); ++i) {
    native[i] = modulus[modulus.size() - i - 1];
  }

  evp_pkey_ctx ctx{EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr)};
  if (!ctx) { return {}; }


  if (EVP_PKEY_fromdata_init(ctx.get()) != 1) { return {}; }

  OSSL_PARAM params[3]{};
  params[0] = OSSL_PARAM_construct_BN(OSSL_PKEY_PARAM_RSA_N, native.data(),
                                      static_cast<int>(modulus.size()));

  // todo: this also needs to get converted to native
  params[1] = OSSL_PARAM_construct_BN(
      OSSL_PKEY_PARAM_RSA_E,
      const_cast<unsigned char*>(
          reinterpret_cast<const unsigned char*>(exponent.data())),
      static_cast<int>(exponent.size()));
  params[2] = OSSL_PARAM_construct_end();

  EVP_PKEY* result{};

  if (EVP_PKEY_fromdata(ctx.get(), &result, EVP_PKEY_PUBLIC_KEY, params) != 1) {
    return {};
  }

  return evp_pkey{result};
}


};  // namespace

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
  // if (!oidc->fetch_meta_data()) { return Error("could not fetch meta data");
  // }

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
        if (!received.empty()) {
          return Error("received unexpected input from client");
        }
        if (!current_flow) {
          return Error("authentication flow was not setup");
        }

        auto token = current_flow->fetch_token();

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
