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

#include <jwt-cpp/jwt.h>

#include "common.h"
#include "lib/bauth/oidc/curl_http.h"

#define UNIQUE_PTR(Type)                           \
  std::unique_ptr<Type, decltype([](Type* val) {   \
                    if (val) { Type##_free(val); } \
                  })>

using evp_pkey = UNIQUE_PTR(EVP_PKEY);
using evp_pkey_ctx = UNIQUE_PTR(EVP_PKEY_CTX);
using evp_md_ctx = UNIQUE_PTR(EVP_MD_CTX);
using bio = UNIQUE_PTR(BIO);

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

struct jansson_traits {
  struct json_wrapper {
    json_t* wrapped{};
    json_wrapper() = default;
    json_wrapper(json_t* towrap) : wrapped{towrap} {}
    json_wrapper(json_wrapper const& other)
    {
      json_incref(other.wrapped);
      wrapped = other.wrapped;
    }
    json_wrapper(json_wrapper&& other)
    {
      auto* n = other.wrapped;
      other.wrapped = nullptr;
      wrapped = n;
    }
    json_wrapper(std::string const& str)
        : wrapped{json_stringn(str.data(), str.size())}
    {
    }

    template <typename T> json_wrapper(T const&)
    {
      // necessary to make it compile, but should never be used
      ASSERT(false);
    }
    json_wrapper& operator=(json_wrapper const& other)
    {
      json_incref(other.wrapped);
      if (wrapped) json_decref(wrapped);
      wrapped = other.wrapped;
      return *this;
    }
    json_wrapper& operator=(json_wrapper&& other)
    {
      auto* n = other.wrapped;
      other.wrapped = nullptr;

      if (wrapped) json_decref(wrapped);

      wrapped = n;
      return *this;
    }

    ~json_wrapper()
    {
      if (wrapped) json_decref(wrapped);
      wrapped = nullptr;
    }
  };

  using value_type = json_wrapper;
  using string_type = std::string;
  using number_type = double;
  using integer_type = json_int_t;
  using boolean_type = bool;
  using object_type = std::unordered_map<string_type, value_type>;
  using array_type = std::vector<value_type>;

  static_assert(std::is_constructible_v<value_type, object_type>);

  static jwt::json::type get_type(const value_type& val)
  {
    using jwt::json::type;

    if (json_is_object(val.wrapped)) return type::object;
    if (json_is_array(val.wrapped)) return type::array;
    if (json_is_string(val.wrapped)) return type::string;
    if (json_is_real(val.wrapped)) return type::number;
    if (json_is_integer(val.wrapped)) return type::integer;
    if (json_is_boolean(val.wrapped)) return type::boolean;

    throw std::logic_error("invalid type");
  }

  static object_type as_object(const value_type& val)
  {
    json_t* elem;
    const char* key;
    object_type result;
    json_object_foreach(val.wrapped, key, elem)
    {
      json_incref(elem);
      result[key] = elem;
    }
    return result;
  }
  static array_type as_array(const value_type& val)
  {
    json_t* elem;
    json_int_t index;
    array_type result;
    json_array_foreach(val.wrapped, index, elem)
    {
      json_incref(elem);
      result.emplace_back(elem);
    }
    return result;
  }
  static string_type as_string(const value_type& val)
  {
    auto* str = json_string_value(val.wrapped);
    auto len = json_string_length(val.wrapped);
    return {str, len};
  }
  static number_type as_number(const value_type& val)
  {
    return json_real_value(val.wrapped);
  }
  static integer_type as_integer(const value_type& val)
  {
    return json_integer_value(val.wrapped);
  }
  static boolean_type as_boolean(const value_type& val)
  {
    return json_boolean_value(val.wrapped);
  }

  static bool parse(value_type& val, string_type str)
  {
    json_error_t err;
    val = json_wrapper{json_loadb(str.data(), str.size(), 0, &err)};
    if (!val.wrapped) return false;

    return true;
  }

  static string_type serialize(const value_type& val)
  {
    string_type result;
    dump_json(&result, val.wrapped);
    return result;
  }
};

site_metadata fetch_metadata(http::client* client, const std::string& url)
{
  if (!client->get(url)) {
    auto error = client->error();
    throw std::runtime_error{"could not fetch metadata" + error};
  }

  std::string response = client->result();

  json_error_t err = {};
  json_ptr json{json_loadb(response.data(), response.size(), 0, &err)};
  if (!json || !json_is_object(json.get())) {
    throw std::runtime_error{"bad json"};
  }

  const char* token_endpoint{};
  const char* jwks_uri{};
  const char* issuer{};
  const char* device_authorization_endpoint{};

  if (json_unpack_ex(json.get(), &err, 0, "{s:s, s:s, s:s, s:s}",
                     "token_endpoint", &token_endpoint, "jwks_uri", &jwks_uri,
                     "issuer", &issuer, "device_authorization_endpoint",
                     &device_authorization_endpoint)
      < 0) {
    throw std::runtime_error{std::string{"missing values in json object: "}
                             + response};
  }

  return {token_endpoint, jwks_uri, issuer, device_authorization_endpoint};
}

struct rsa_key {
  std::vector<char> exponent;
  std::vector<char> modulus;
};

using keyset = std::unordered_map<std::string, rsa_key>;

keyset get_keys(http::client* client,
                const std::string& key_uri,
                std::string_view issuer)
{
  Dmsg0(300, "fetching keys from %s\n", key_uri.c_str());

  if (!client->get(key_uri)) {
    auto error = client->error();
    throw std::runtime_error{"could not fetch keys"};
  }

  std::string response = client->result();

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

std::string pem_from_public_key(EVP_PKEY* pkey)
{
  // we print the PEM data to memory,  in the future we should print it to a
  // local file, so we can load it again next time without having to fetch it!
  bio mem{BIO_new(BIO_s_mem())};

  if (!mem) { throw std::runtime_error{"Could not allocate new memory BIO"}; }

  if (PEM_write_bio_PUBKEY(mem.get(), pkey) != 1) {
    throw std::runtime_error{"Could not write out PEM to mem BIO"};
  }

  BUF_MEM* ptr{};
  BIO_get_mem_ptr(mem.get(), &ptr);

  return {ptr->data, ptr->length};
}

UserClaims from_json(json_t* roles_json,
                     json_t* user_name_json,
                     json_t* preferred_json)
{
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

UserClaims VerifyToken(const auth::Oidc::config& cfg,
                       const keyset& keys,
                       std::string_view token)
{
  auto decoded = ::jwt::decode<jansson_traits>(std::string{token});

  auto verifier = ::jwt::verify<jansson_traits>()
                      .with_issuer(cfg.oidc_url)
                      .leeway(std::chrono::duration_cast<std::chrono::seconds>(
                                  cfg.allowed_clock_skew)
                                  .count())  // todo: make sure this is correct
                      .with_audience(cfg.client_id);

  auto itr = keys.find(decoded.get_key_id());
  if (itr == keys.end()) {
    throw std::runtime_error{"unknown key id! rejecting jwt"};
  }
  auto pkey = create_public_key(itr->second.modulus, itr->second.exponent);

  auto pem = pem_from_public_key(pkey.get());

  // todo: make pem out of used_key
  verifier.allow_algorithm(::jwt::algorithm::rs256{pem});

  verifier.verify(decoded);
  auto roles_json = decoded.get_payload_claim(cfg.roles_name).to_json();
  auto user_json = decoded.get_payload_claim("name").to_json();
  auto preferred_json
      = decoded.get_payload_claim("preferred_username").to_json();

  return from_json(roles_json.wrapped, user_json.wrapped,
                   preferred_json.wrapped);
}
};  // namespace

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

DeviceAuthRequest start_device_auth(http::client* client,
                                    const std::string& client_id,
                                    const std::string& scope,
                                    const site_metadata& meta)
{
  Dmsg0(200, "Starting device auth via %s\n",
        meta.device_authorization_endpoint.c_str());

  std::vector<http::post_field> fields{
      {"client_id", client_id},
      {"scope", scope},
  };

  if (!client->post(meta.device_authorization_endpoint, fields)) {
    throw std::runtime_error{(std::stringstream{}
                              << "could not start device auth: "
                              << client->error())
                                 .str()};
  }

  std::string response = client->result();

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

http::client* AuthenticationFlow::curl(Verifier* verifier)
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

        verified_claims = VerifyToken(conf(verifier), keys, key_str);

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


  FetchResult fetch_token(http::client* client,
                          const site_metadata& meta,
                          std::string* response)
  {
    http::post_field fields[]
        = {{"client_id", client_id},
           {"device_code", device_code},
           {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}};

    if (!client->post(meta.token_endpoint, fields)) {
      Dmsg0(100, "Could not fetch from %s: %s\n", meta.token_endpoint.c_str(),
            client->error().c_str());
      return FetchResult::Error;
    }

    auto buffer = client->result();

    auto http_code = client->return_code();

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

  curl = std::make_unique<http::curl_client>();
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
          roles = std::move(current_flow->verified_claims.roles);
          if (current_flow->verified_claims.preferred_name.empty()) {
            user_name = std::move(current_flow->verified_claims.user_name);
          } else {
            user_name = std::move(current_flow->verified_claims.preferred_name);
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
