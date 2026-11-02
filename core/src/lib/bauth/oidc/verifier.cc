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

#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/core_names.h>

#include <sstream>
#include <string_view>

#include "common.h"

#define UNIQUE_PTR(Type)                           \
  std::unique_ptr<Type, decltype([](Type* val) {   \
                    if (val) { Type##_free(val); } \
                  })>

using evp_pkey = UNIQUE_PTR(EVP_PKEY);
using evp_pkey_ctx = UNIQUE_PTR(EVP_PKEY_CTX);
using evp_md_ctx = UNIQUE_PTR(EVP_MD_CTX);

#undef UNIQUE_PTR

namespace auth::Oidc {
namespace {

template <typename T> void dump_json(T* result, json_t* json)
{
  json_dump_callback(
      json,
      +[](const char* buffer, size_t size, void* arg) -> int {
        auto* output = static_cast<T*>(arg);
        output->insert(output->end(), buffer, buffer + size);
        return 0;
      },
      result, JSON_COMPACT);
}


// see rfc 7519
struct jwt_header {
  std::string algorithm;
  std::string key_id;
};

struct jwt {
  jwt_header header;
  jwt_payload payload;
};

std::pair<std::string, std::vector<char>> ExtractSignature(
    std::string_view to_parse)
{
  auto end = to_parse.find_last_of('.');
  if (end == to_parse.npos || end == to_parse.size() - 1) {
    throw std::runtime_error{"no dots or bad dot at end"};
  }

  auto signature64 = to_parse.substr(end + 1);
  std::vector<char> signature;
  signature.resize(signature64.size() + 1);
  auto actual_size = Base64ToBin(signature.data(), signature.size(),
                                 signature64.data(), signature64.size());

  if (actual_size <= 0) {
    throw std::runtime_error{"bad base64 encoded signature"};
  }

  signature.resize(actual_size);

  return {std::string{to_parse.substr(0, end)}, std::move(signature)};
}

// std does not include the signature
jwt ExtractJwt(std::string_view token, const char* roles_name)
{
  auto to_parse = token;
  auto end = to_parse.find('.');
  if (end == to_parse.npos) { throw std::runtime_error{"no dots"}; }

  auto header64 = to_parse.substr(0, end);
  to_parse.remove_prefix(end);

  std::string header;
  header.resize(header64.size());
  int actual_size = Base64ToBin(header.data(), header.size(), header64.data(),
                                header64.size());
  header.resize(actual_size);

  // check if "alg" is "RS256"; we do not accept anything else
  json_error_t err = {};
  json_ptr jheader{json_loadb(header.data(), header.size(), 0, &err)};
  if (!jheader || !json_is_object(jheader)) {
    throw std::runtime_error{"bad jwt header"};
  }

  const char* header_type{};
  const char* header_algo{};
  const char* header_key_id{};
  const char* header_nonce{};
  const char* header_x5t{};

  // we reject the key if there are any additional/unknown keys
  if (json_unpack_ex(jheader.get(), &err, JSON_STRICT,
                     "{s:s, s:s, s:s, s?s, s?s}", "typ", &header_type, "alg",
                     &header_algo, "kid", &header_key_id, "nonce",
                     &header_nonce, "x5t", &header_x5t)
      < 0) {
    std::string error = "jwt header error: ";
    error += err.text;
    throw std::runtime_error{error};
  }

  if (std::string_view{"JWT"} != header_type) {
    throw std::runtime_error{"bad jwt header (bad type)"};
  }
  if (std::string_view{"RS256"} != header_algo) {
    throw std::runtime_error{"bad jwt header (bad algo)"};
  }

  Dmsg0(300, "JWT is signed by '%s'\n", header_key_id);

  auto payload64 = to_parse.substr(1);

  std::string payload;
  payload.resize(payload64.size());
  actual_size = Base64ToBin(payload.data(), payload.size(), payload64.data(),
                            payload64.size());
  payload.resize(actual_size);

  Dmsg0(300, "payload size %zu (b64) -> %zu (binary)\n", payload64.size(),
        payload.size());

  json_ptr jpayload{json_loadb(payload.data(), payload.size(), 0, &err)};
  if (!jpayload || !json_is_object(jpayload)) {
    throw std::runtime_error{"bad jwt payload"};
  }


  json_t* roles{};
  const char* issuer{};
  const char* subject{};
  json_t* audience_json{};
  const char* authorized_party{};
  json_int_t expiry{};
  json_int_t not_before{};
  json_int_t issued_at{};
  const char* user_name{};
  const char* preferred_name{};
  // first parse required fields
  if (json_unpack_ex(jpayload.get(), &err, 0,
                     "{s:s, s:s, s:s, s:o, s:i, s?i, s:i, s:o, s?s, s?s}",
                     "name", &user_name, "iss", &issuer, "sub", &subject, "aud",
                     &audience_json, "exp", &expiry, "nbf", &not_before, "iat",
                     &issued_at, roles_name, &roles, "preferred_username",
                     &preferred_name, "azp", &authorized_party)
      < 0) {
    throw std::runtime_error{std::string{"missing values in jwt payload: "}
                             + err.text};
  }

  if (!json_is_array(roles)) {
    throw std::runtime_error{"bad roles value in jwt payload"};
  }

  const char* audience = [&] {
    if (authorized_party) {
      std::string_view azp{authorized_party};

      // make sure azp and aud are compatible!
      if (json_is_array(audience_json)) {
        json_t* aud{};
        json_int_t idx{};
        json_array_foreach(audience_json, idx, aud)
        {
          if (!json_is_string(aud)) {
            throw std::runtime_error{"non-string value in 'aud' array"};
          }

          if (azp == json_string_value(aud)) { return authorized_party; }
        }
        throw std::runtime_error{"'azp' is not contained in the 'aud' array"};

      } else if (json_is_string(audience_json)) {
        if (azp != json_string_value(audience_json)) {
          throw std::runtime_error{
              "'azp' and 'aud' are both strings, but are different"};
        }

        return authorized_party;
      } else {
        throw std::runtime_error{"'aud' is neither array nor string"};
      }


    } else {
      if (!json_is_string(audience_json)) {
        throw std::runtime_error{"'aud' is not a string, but 'azp' is unset"};
      }

      return json_string_value(audience_json);
    }
  }();

  jwt_payload payload_jwt{issuer,    subject, audience,   expiry, issued_at,
                          user_name, {},      not_before, ""};
  if (preferred_name) { payload_jwt.preferred_name = preferred_name; }

  json_t* role{};
  json_int_t idx{};
  json_array_foreach(roles, idx, role)
  {
    if (!json_is_string(role)) {
      throw std::runtime_error{"bad role value in jwt payload"};
    }

    payload_jwt.roles.push_back(json_string_value(role));
  }

  return {{header_algo, header_key_id}, payload_jwt};
}

struct rsa_key {
  std::vector<char> exponent;
  std::vector<char> modulus;
};

using keyset = std::unordered_map<std::string, rsa_key>;

keyset get_keys(CURL* curl, const std::string& key_uri, std::string_view issuer)
{
  Dmsg0(300, "fetching keys from %s\n", key_uri.c_str());

  curl_easy_setopt(curl, CURLOPT_URL, key_uri.c_str());

  std::string response;
  auto res = curl_easy_write_into_str(curl, &response);

  if (res != CURLE_OK) { throw std::runtime_error{"could not fetch keys"}; }

  json_error_t err = {};
  json_ptr keys_json{json_loadb(response.data(), response.size(), 0, &err)};

  if (!keys_json || !json_is_object(keys_json)) {
    throw std::runtime_error{"bad json keys"};
  }

  json_t* key_array = json_object_get(keys_json.get(), "keys");
  if (!key_array || !json_is_array(key_array)) {
    throw std::runtime_error{"bad json key array"};
  }

  keyset keys;

  json_int_t index{};
  json_t* key{};
  json_array_foreach(key_array, index, key)
  {
    if (!key || !json_is_object(key)) {
      throw std::runtime_error{"bad json key"};
    }

    const char* key_type{};
    const char* key_usage{};
    const char* key_id{};
    const char* key_issuer{};  // this is optional
    const char* key_modulus64{};
    const char* key_exponent64{};
    if (json_unpack_ex(key, &err, 0, "{s:s, s:s, s:s, s?s, s:s, s:s}", "kty",
                       &key_type, "use", &key_usage, "kid", &key_id, "issuer",
                       &key_issuer, "n", &key_modulus64, "e", &key_exponent64)
        < 0) {
      continue;
    }

    // we only care about signing keys
    if (std::string_view{"sig"} != key_usage) {
      Dmsg0(300, "key %s has does not have the right usage: %s ---> Skipped\n",
            key_id, key_usage);
      continue;
    }

    // we only care about keys from the issuer
    // but it is optional, so we only check it if it actually exists
    if (key_issuer && std::string_view{key_issuer} != issuer) {
      Dmsg0(300, "key %s has does not have the right issuer: %s ---> Skipped\n",
            key_id, key_issuer);
      continue;
    }

    if (std::string_view{"RSA"} != key_type) {
      Dmsg0(300, "key %s has does not have the right type: %s ---> Skipped\n",
            key_id, key_type);
      continue;
    }

    rsa_key rsa;
    std::string_view m64{key_modulus64};
    rsa.modulus.resize(m64.size() + 1);

    auto actual_size = Base64ToBin(rsa.modulus.data(), rsa.modulus.size(),
                                   m64.data(), m64.size());

    if (actual_size <= 0) {
      Dmsg0(300, "Could not decode modulus for key %s ---> Skipped\n", key_id);
      continue;
    }

    rsa.modulus.resize(actual_size);

    std::string_view e64{key_exponent64};
    rsa.exponent.resize(e64.size() + 1);

    actual_size = Base64ToBin(rsa.exponent.data(), rsa.exponent.size(),
                              e64.data(), e64.size());

    if (actual_size <= 0) {
      Dmsg0(300, "Could not decode exponent for key %s ---> Skipped\n", key_id);
      continue;
    }

    rsa.exponent.resize(actual_size);

    assert(keys.find(key_id) == keys.end());

    Dmsg0(300, "Adding key %s\n", key_id);
    keys.emplace(key_id, std::move(rsa));
  }

  return keys;
}

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

bool verify_signature(std::string_view token,
                      std::span<const char> signature,
                      const rsa_key& key)
{
  evp_pkey public_key = create_public_key(key.modulus, key.exponent);
  if (!public_key) { return false; }

  evp_md_ctx ctx{EVP_MD_CTX_new()};
  if (!ctx) { return false; }

  EVP_PKEY_CTX* kctx{};  // this is owned by ctx, so we use a non-owning ptr
  if (EVP_DigestVerifyInit(ctx.get(), &kctx, EVP_sha256(), nullptr,
                           public_key.get())
      != 1) {
    Dmsg0(100, "Could not init verify digest\n");
    return false;
  }

  if (!kctx) {
    Dmsg0(100, "No kctx was provided\n");
    return false;
  }

  if (EVP_PKEY_CTX_set_rsa_padding(kctx, RSA_PKCS1_PADDING) <= 0) {
    Dmsg0(100, "Could not set rsa padding\n");
    return false;
  }

  if (EVP_PKEY_CTX_set_signature_md(kctx, EVP_sha256()) <= 0) {
    Dmsg0(100, "Could not set signature md\n");
    return false;
  }

  if (EVP_DigestVerifyUpdate(ctx.get(), token.data(), token.size()) != 1) {
    Dmsg0(100, "Could not compute digest\n");
    return false;
  }

  if (EVP_DigestVerifyFinal(
          ctx.get(), reinterpret_cast<const unsigned char*>(signature.data()),
          signature.size())
      != 1) {
    Dmsg0(100, "Could not get digest\n");
    return false;
  }

  return true;
}

jwt_payload VerifyToken(const auth::Oidc::config& cfg,
                        const keyset& keys,
                        std::string_view token)
{
  auto [value, signature] = ExtractSignature(token);
  auto [header, payload] = ExtractJwt(value, cfg.roles_name.c_str());

  if (payload.issuer != cfg.oidc_url) {
    throw std::runtime_error{
        "received token with bad issuer: expected:'" + cfg.oidc_url + "' got:'"
            + payload.issuer + "'",
    };
  }

  std::stringstream msg;
  msg << "Checking token for " << payload.user_name << " (" << payload.subject
      << ")\n"
      << "with roles:\n";
  for (auto& role : payload.roles) { msg << " - " << role << "\n"; }

  Dmsg0(100, "%s\n", msg.str().c_str());

  if (auto iter = keys.find(header.key_id); iter != keys.end()) {
    const auto& used_key = iter->second;
    // we need to check that value matches signature via used_key

    if (!verify_signature(value, signature, used_key)) {
      throw std::runtime_error{"bad signature! rejecting jwt"};
    }

  } else {
    throw std::runtime_error{"unknown key id! rejecting jwt"};
  }

  auto allowed_clock_skew = cfg.allowed_clock_skew;

  auto now = std::chrono::system_clock::now();

  if ((now - allowed_clock_skew).time_since_epoch().count()
      < payload.issued_at) {
    throw std::runtime_error{"token does not exist yet"};
  }

  if (payload.not_before == 0) {
    if (cfg.require_not_before) {
      throw std::runtime_error{"token does not have required not-before (nbf)"};
    }
  } else if ((now + allowed_clock_skew).time_since_epoch().count()
             < payload.not_before) {
    throw std::runtime_error{"token not usable yet"};
  }

  if (payload.audience != cfg.client_id) {
    throw std::runtime_error{"this token is not for me"};
  }

  return std::move(payload);
}
};  // namespace

struct UserClaims {
  std::string user_name;
  std::string preferred_name;
  std::vector<std::string> roles;

  static UserClaims from_json(const config& cfg, json_t* claims)
  {
    auto* roles_json = json_object_get(claims, cfg.roles_name.c_str());
    auto* user_name_json = json_object_get(claims, "name");
    auto* preferred_json = json_object_get(claims, "preferred_username");

    if (!roles_json || !json_is_array(roles_json)) {
      throw std::runtime_error{"no or bad roles in id token"};
    }

    std::vector<std::string> roles;

    json_t* role{};
    json_int_t idx{};
    json_array_foreach(roles_json, idx, role)
    {
      if (!json_is_string(role)) {
        throw std::runtime_error{"bad role value in jwt payload"};
      }

      roles.push_back(json_string_value(role));
    }

    if (!user_name_json || !json_is_string(user_name_json)) {
      throw std::runtime_error{"no or bad user name in id token"};
    }

    std::string user_name{json_string_value(user_name_json)};
    std::string preferred_name;

    if (preferred_json) {
      if (!json_is_string(preferred_json)) {
        throw std::runtime_error{"bad preferred user name in id token"};
      }
      preferred_name = json_string_value(preferred_json);
    } else {
      preferred_name = user_name;
    }

    return UserClaims{std::move(user_name), std::move(preferred_name),
                      std::move(roles)};
  }
};

struct DeviceAuthRequest {
  std::string client_id;

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

DeviceAuthRequest start_device_auth(CURL* curl,
                                    const std::string& client_id,
                                    const std::string& scope,
                                    const site_metadata& meta)
{
  Dmsg0(200, "Starting device auth via %s\n",
        meta.device_authorization_endpoint.c_str());

  curl_easy_setopt(curl, CURLOPT_URL,
                   meta.device_authorization_endpoint.c_str());
  std::string response;
  auto res = [&] {
    url_encoder encoder{};

    encoder.add_kv("client_id", client_id.c_str());

    std::string scopes = scope;
    // if (!refresh_scope.empty()) { scopes += " " + refresh_scope; }

    encoder.add_kv("scope", scopes.c_str());

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, encoder.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    return curl_easy_write_into_str(curl, &response);
  }();

  curl_easy_reset(curl);

  if (res != CURLE_OK) {
    throw std::runtime_error{(std::stringstream{}
                              << "could not start device auth: "
                              << curl_easy_strerror(res) << " (" << res << ")")
                                 .str()};
  }

  Dmsg0(200, "GET was successful\n");

  json_error_t err = {};
  json_ptr json{json_loadb(response.data(), response.size(), 0, &err)};
  if (!json) {
    throw std::runtime_error{std::string{"Could not parse response as json: "}
                             + err.text};
  }


  const char* user_code;
  const char* device_code;
  const char* verification_uri;
  json_int_t expires_in;
  const char* message{""};
  json_int_t poll_interval{5};  // optional - default MUST be 5 per RFC
  const char* verification_uri_complete_{nullptr};  // optional

  if (json_unpack_ex(json.get(), &err, 0, "{s:s, s:s, s:s, s:i, s?s, s?i, s?s}",
                     "user_code", &user_code, "device_code", &device_code,
                     "verification_uri", &verification_uri, "expires_in",
                     &expires_in, "message", &message, "interval",
                     &poll_interval, "verification_uri_complete",
                     &verification_uri_complete_)
      < 0) {
    throw std::runtime_error{
        std::string{"missing values in device auth response: "} + err.text};
  }

  Dmsg0(200, "Unpack of GET was successful\n");

  std::optional<std::string> verification_uri_complete;
  if (verification_uri_complete_) {
    verification_uri_complete.emplace(verification_uri_complete_);
  }

  return DeviceAuthRequest{client_id,
                           user_code,
                           device_code,
                           verification_uri,
                           std::move(verification_uri_complete),
                           std::chrono::seconds{expires_in},
                           std::chrono::seconds{poll_interval},
                           message};
}

CURL* AuthenticationFlow::curl(Verifier* verifier)
{
  return verifier->curl.get();
}

site_metadata& AuthenticationFlow::meta(Verifier* verifier)
{
  return verifier->meta;
}

config& AuthenticationFlow::conf(Verifier* verifier) { return verifier->cfg; }

// See RFC 6749 Section 5.2 & RFC 8628 Section 3.5
enum class FetchResult
{
  Ok,
  Pending,
  SlowDown,
  AccessDenied,
  Expired,
  InvalidRequest,
  InvalidClient,
  InvalidGrant,
  UnauthorizedClient,
  UnsupportedGrantType,
  InvalidScope,
  Error,
};

static constexpr std::pair<FetchResult, std::string_view> error_table[] = {
    // RFC 8628 §3.5 — device-flow-specific errors
    {FetchResult::Pending, "authorization_pending"},
    {FetchResult::SlowDown, "slow_down"},
    {FetchResult::AccessDenied, "access_denied"},
    {FetchResult::Expired, "expired_token"},

    // RFC 6749 §5.2 — standard token-endpoint errors
    {FetchResult::InvalidRequest, "invalid_request"},
    {FetchResult::InvalidClient, "invalid_client"},
    {FetchResult::InvalidGrant, "invalid_grant"},
    {FetchResult::UnauthorizedClient, "unauthorized_client"},
    {FetchResult::UnsupportedGrantType, "unsupported_grant_type"},
    {FetchResult::InvalidScope, "invalid_scope"},

    // It seems some providers deviate from the standard.
    // We can add them below once we know of them
};

FetchResult handle_error_response(std::string_view buffer,
                                  std::string* err_desc)
{
  json_error_t err = {};
  json_ptr buffer_json{json_loadb(buffer.data(), buffer.size(), 0, &err)};

  if (!buffer_json) {
    *err_desc = "Error response was not json: ";
    *err_desc += err.text;
    return FetchResult::Error;
  }

  // These are required
  const char* error;

  // These are optional
  const char* error_desc{nullptr};
  const char* error_uri{nullptr};

  if (json_unpack_ex(buffer_json.get(), &err, 0, "{s:s, s?s, s?s}", "error",
                     &error, "error_description", &error_desc, "error_uri",
                     &error_uri)
      < 0) {
    *err_desc = "Error response could not be unpacked: ";
    *err_desc += err.text;
    return FetchResult::Error;
  }

  auto error_code = FetchResult::Error;

  Dmsg0(100, "Received error %s from token provider\n", error);

  for (auto [code, name] : error_table) {
    if (name == error) {
      error_code = code;
      break;
    }
  }

  if (error_desc) {
    err_desc->assign(error_desc);
  } else {
    err_desc->clear();
  }

  return error_code;
}

FetchResult handle_token_response(std::string_view buffer, std::string* token)
{
  json_error_t err = {};
  json_ptr buffer_json{json_loadb(buffer.data(), buffer.size(), 0, &err)};

  if (!buffer_json) {
    *token = "Token response was not json: ";
    *token += err.text;
    return FetchResult::Error;
  }

  // These are required
  const char* access_token;
  const char* token_type;

  // These are optional
  json_int_t expires_in{0};
  const char* refresh_token{nullptr};
  const char* scope{nullptr};

  if (json_unpack_ex(buffer_json.get(), &err, 0, "{s:s, s:s, s?i, s?s, s?s}",
                     "id_token", &access_token, "token_type", &token_type,
                     "expires_in", &expires_in, "refresh_token", &refresh_token,
                     "scope", &scope)
      < 0) {
    *token = "Error response could not be unpacked: ";
    *token += err.text;
    return FetchResult::Error;
  }

  if (!Bstrcasecmp(token_type, "Bearer")) {
    *token = "Bad token type: ";
    *token += token_type;
    return FetchResult::Error;
  }

  *token = access_token;
  return FetchResult::Ok;
}

struct DeviceAuthenticationFlow : AuthenticationFlow {
  static constexpr const char* name = "device";


  std::chrono::seconds expires_in;
  std::chrono::seconds poll_interval;
  std::string client_id;
  std::string device_code;

  DeviceAuthenticationFlow(DeviceAuthRequest req)
      : expires_in{req.expires_in}
      , poll_interval{req.poll_interval}
      , client_id{std::move(req.client_id)}
      , device_code{std::move(req.device_code)}
  {
  }

  Status step(auth::Oidc::Verifier* verifier,
              std::vector<char>& response,
              std::span<const char> received) override
  {
    switch (state) {
      case INITIAL: {
        std::string key_str;

        bool done = false;

        auto now = std::chrono::steady_clock::now;
        auto start_time = now();

        while (!done) {
          auto delta = now() - start_time;

          if (delta >= expires_in) {
            Dmsg0(
                100, "request expired: %zu ns vs max %zu ns\n",
                std::chrono::duration_cast<std::chrono::nanoseconds>(delta)
                    .count(),
                std::chrono::duration_cast<std::chrono::nanoseconds>(expires_in)
                    .count());

            throw std::runtime_error{"Device auth expired"};
          }

          /* sleeping before fetching the token makes it more likely to
           * get the token on the first try - if - people generally manage
           * to login in <5 seconds. If they take 5-10 seconds instead, then
           * its better to fetch first. */
          std::this_thread::sleep_for(poll_interval);

          switch (fetch_token(curl(verifier), meta(verifier), &key_str)) {
            case FetchResult::Ok: {
              Dmsg0(200, "token was fetched sucessfully\n");
              done = true;
            } break;
            case FetchResult::Pending: {
              Dmsg0(300, "token is not ready yet\n");
            } break;
            case FetchResult::SlowDown: {
              Dmsg0(300, "token is not ready yet; we need to slow down\n");

              // we are required by the standard to (permanently) add 5 seconds
              poll_interval += std::chrono::seconds(5);
            } break;
            default: {
              if (key_str.empty()) { key_str = "unknown error"; }

              Dmsg0(100, "We received an error during token fetch: %s\n",
                    key_str.c_str());

              throw std::runtime_error{std::move(key_str)};
            } break;
          }
        }

        auto keys = get_keys(curl(verifier), meta(verifier).jwks_uri,
                             meta(verifier).issuer);

        verified_payload = VerifyToken(conf(verifier), keys, key_str);

        return Status::Done;
      } break;
      default: {
        throw std::runtime_error{
            "entered DeviceAuthenticationFlow in unexpected state"};
      } break;
    }
  }

 private:
  enum State
  {
    INITIAL,
    DONE,
  };

  State state{INITIAL};


  FetchResult fetch_token(CURL* curl,
                          const site_metadata& meta,
                          std::string* response)
  {
    curl_easy_setopt(curl, CURLOPT_URL, meta.token_endpoint.c_str());

    url_encoder encoder{};
    encoder.add_kv("client_id", client_id);
    encoder.add_kv("device_code", device_code);
    encoder.add_kv("grant_type",
                   "urn:ietf:params:oauth:grant-type:device_code");

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, encoder.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    std::string buffer;
    auto res = curl_easy_write_into_str(curl, &buffer);

    if (res != CURLE_OK) {
      Dmsg0(100, "Could not fetch from %s: %s (%d)\n",
            meta.token_endpoint.c_str(), curl_easy_strerror(res), res);
      curl_easy_reset(curl);
      return FetchResult::Error;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_reset(curl);

    if (http_code >= 400) {
      Dmsg0(500, "Got an error response\n");
      return handle_error_response(buffer, response);
    } else {
      Dmsg0(500, "Got a success response\n");
      return handle_token_response(buffer, response);
    }
  }
};

std::vector<char> Verifier::generate_challenge()
{
  json_error_t err;
  json_ptr chal{json_pack_ex(&err, JSON_COMPACT, "{s:[s]}",
                             "supported_auth_flows",
                             DeviceAuthenticationFlow::name)};

  if (!chal) { return {}; }

  std::vector<char> result;

  dump_json(&result, chal.get());

  return result;
}


Status Verifier::initiate_flow(std::vector<char>& response,
                               std::span<const char> received)
{
  std::string_view received_view{received.begin(), received.end()};

  if (received_view != DeviceAuthenticationFlow::name) {
    return Error("client requested unknown flow");
  }

  if (cfg.oidc_url.empty()) { return Error("empty oidc url"); }

  curl.reset(curl_easy_init());
  if (!curl) { return Error("could not create curl object"); }
  std::string config_url = cfg.oidc_url;
  if (config_url.back() != '/') { config_url.push_back('/'); }

  config_url += ".well-known/openid-configuration";

  meta = fetch_metadata(curl.get(), config_url);

  auto device_auth
      = start_device_auth(curl.get(), cfg.client_id, "openid profile", meta);

  json_error_t err;

  json_ptr result_json{json_pack_ex(
      &err, 0, "{s:s, s:s, s:s*}", "url", device_auth.verification_uri.c_str(),
      "code", device_auth.user_code.c_str(), "complete_url",
      device_auth.verification_uri_complete
          ? device_auth.verification_uri_complete->c_str()
          : nullptr)};

  dump_json(&response, result_json.get());

  current_flow
      = std::make_unique<DeviceAuthenticationFlow>(std::move(device_auth));
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
        auto res = current_flow->step(this, response, received);
        if (res == Status::Done) {
          roles = std::move(current_flow->verified_payload.roles);
          if (current_flow->verified_payload.preferred_name.empty()) {
            user_name = std::move(current_flow->verified_payload.user_name);
          } else {
            user_name
                = std::move(current_flow->verified_payload.preferred_name);
          }
        }

        return res;
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
