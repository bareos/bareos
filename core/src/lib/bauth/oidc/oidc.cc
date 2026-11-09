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

#include "oidc.h"
#include "verifier.h"
#include "common.h"
#include "http.h"

#include "lib/base64.h"

#include <jwt-cpp/jwt.h>
#include <openssl/core_names.h>
#include <thread>

namespace auth::Oidc::oidc {

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

FetchResult try_fetch_token(oidc::client* client,
                            const std::string& device_code,
                            std::string* response)
{
  http::post_field fields[]
      = {{"client_id", client->client_id()},
         {"device_code", device_code},
         {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}};

  auto* http = client->http;

  auto endpoint = client->token_endpoint();
  if (!http->post(endpoint.c_str(), fields)) {
    Dmsg0(100, "Could not fetch from %s: %s\n", endpoint.c_str(),
          http->error().c_str());
    return FetchResult::Error;
  }

  auto buffer = http->result();

  auto http_code = http->return_code();

  if (http_code >= 400) {
    Dmsg0(500, "Got an error response\n");
    return handle_error_response(buffer, response);
  } else {
    Dmsg0(500, "Got a success response\n");
    return handle_token_response(buffer, response);
  }
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
    // dump_json(&result, val.wrapped);
    return result;
  }
};

}  // namespace

UserClaims VerifyToken(std::string_view token,
                       const std::string& issuer,
                       const std::string& audience,
                       std::chrono::seconds allowed_clock_skew,
                       const std::string& roles_name,
                       const keyset& keys)
{
  auto decoded = ::jwt::decode<jansson_traits>(std::string{token});

  // TODO: check openid scope!
  auto verifier = ::jwt::verify<jansson_traits>()
                      .with_issuer(issuer)
                      // todo: make sure this is correct
                      .leeway(allowed_clock_skew.count())
                      .with_audience(audience);

  auto itr = keys.find(decoded.get_key_id());
  if (itr == keys.end()) {
    throw std::runtime_error{"unknown key id! rejecting jwt"};
  }

  auto pem = pem_from_public_key(itr->second.get());

  // todo: make pem out of used_key
  verifier.allow_algorithm(::jwt::algorithm::rs256{pem});

  verifier.verify(decoded);

  auto roles_json = decoded.get_payload_claim(roles_name).to_json();
  auto user_json = decoded.get_payload_claim("name").to_json();
  jansson_traits::json_wrapper preferred_json{};
  if (decoded.has_payload_claim("preferred_username")) {
    preferred_json = decoded.get_payload_claim("preferred_username").to_json();
  }

  return from_json(roles_json.wrapped, user_json.wrapped,
                   preferred_json.wrapped);
}

client client::fetch_meta_data(client_config cfg)
{
  auto http = cfg.client;

  std::string config_url = cfg.oidc_url;
  if (config_url.back() != '/') { config_url.push_back('/'); }

  config_url += ".well-known/openid-configuration";

  if (!http->get(config_url)) {
    auto error = http->error();
    throw std::runtime_error{"could not fetch metadata" + error};
  }

  std::string response = http->result();

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

  client result{};

  result.http = http;
  result.my_id = cfg.client_id;
  result.oidc_url = cfg.oidc_url;
  result.ep_device_auth = device_authorization_endpoint;
  result.ep_jwks = jwks_uri;
  result.ep_token = token_endpoint;

  return result;
}

keyset client::fetch_keys_from_jwks()
{
  Dmsg0(300, "fetching keys from %s\n", ep_jwks.c_str());

  if (!http->get(ep_jwks)) {
    auto error = http->error();

    std::ostringstream message;
    message << "could not fetch keys: " << error;

    throw std::runtime_error{std::move(message).str()};
  }

  std::string response = http->result();

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
    const char* key_der64{};

    /* 'x5c' is a certificate chain
     * The first certificate is always the certificate that holds the
     * public key to verify the jwts. */
    if (json_unpack_ex(key, &err, 0, "{s:s, s:s, s:s, s?s, s?s, s?s, s?[s]}",
                       "kty", &key_type, "use", &key_usage, "kid", &key_id,
                       "issuer", &key_issuer, "n", &key_modulus64, "e",
                       &key_exponent64, "x5c", &key_der64)
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
    if (key_issuer && std::string_view{key_issuer} != oidc_url) {
      Dmsg0(300, "key %s has does not have the right issuer: %s ---> Skipped\n",
            key_id, key_issuer);
      continue;
    }

    if (std::string_view{"RSA"} != key_type) {
      Dmsg0(300, "key %s has does not have the right type: %s ---> Skipped\n",
            key_id, key_type);
      continue;
    }

    if (!key_der64 && (!key_modulus64 || !key_exponent64)) {
      Dmsg0(300,
            "key %s has does contain a full key: der:%s mod:%s exp:%s ---> "
            "Skipped\n",
            key_id, key_der64 ? "YES" : "NO", key_modulus64 ? "YES" : "NO",
            key_exponent64 ? "YES" : "NO");
      continue;
    }


    if (keys.find(key_id) != keys.end()) {
      Dmsg0(300, "key %s was already added ?! ---> Skipped\n", key_id);
      continue;
    }

    if (key_der64) {
      std::string_view der64{key_der64};
      std::string key_der(der64.size(), '\0');
      auto actual_size = Base64ToBin(key_der.data(), key_der.size(),
                                     der64.data(), der64.size());
      if (actual_size <= 0) {
        Dmsg0(300, "Could not decode modulus for key %s ---> Skipped\n",
              key_id);
        continue;
      }

      key_der.resize(actual_size);
      auto* ptr = reinterpret_cast<const unsigned char*>(key_der.data());
      auto* begin = ptr;
      x509 cert{d2i_X509(nullptr, &ptr, key_der.size())};

      if (!cert) {
        Dmsg0(300,
              "OpenSSL could not create cert from DER for %s ---> Skipped\n",
              key_id);
        continue;
      }

      // make sure that we actually parsed the whole der
      ASSERT(ptr <= begin + key_der.size());
      bool not_completely_parsed = false;
      for (auto* cur = ptr; cur < begin + key_der.size(); ++cur) {
        // we allow zeroes at the end, but nothing else
        if (*cur != 0) {
          not_completely_parsed = true;
          break;
        }
      }
      if (not_completely_parsed) {
        Dmsg0(300, "OpenSSL did not consume complete DER for %s ---> Skipped\n",
              key_id);
        continue;
      }

      evp_pkey public_key{X509_get_pubkey(cert.get())};

      if (!public_key) {
        Dmsg0(300,
              "OpenSSL could not extract key from certificate for %s ---> "
              "Skipped\n",
              key_id);
        continue;
      }

      Dmsg0(300, "Adding key %s\n", key_id);
      keys.emplace(key_id, std::move(public_key));
    } else if (key_modulus64 && key_exponent64) {
      std::vector<char> modulus, exponent;
      std::string_view m64{key_modulus64};
      modulus.resize(m64.size() + 1);

      auto actual_size
          = Base64ToBin(modulus.data(), modulus.size(), m64.data(), m64.size());

      if (actual_size <= 0) {
        Dmsg0(300, "Could not decode modulus for key %s ---> Skipped\n",
              key_id);
        continue;
      }

      modulus.resize(actual_size);

      std::string_view e64{key_exponent64};
      exponent.resize(e64.size() + 1);

      actual_size = Base64ToBin(exponent.data(), exponent.size(), e64.data(),
                                e64.size());

      if (actual_size <= 0) {
        Dmsg0(300, "Could not decode exponent for key %s ---> Skipped\n",
              key_id);
        continue;
      }

      exponent.resize(actual_size);

      Dmsg0(300, "Adding key %s\n", key_id);
      auto public_key = create_public_key(modulus, exponent);
      keys.emplace(key_id, std::move(public_key));
    } else {
      // we should never get here, as this is checked above
      ASSERT(!"No key data included");
    }
  }

  return keys;
}

DeviceAuthenticationFlow DeviceAuthenticationFlow::start(
    oidc::client* client,
    const std::string& scopes)
{
  auto* http = client->http;
  auto endpoint = client->device_auth_endpoint();

  Dmsg0(200, "Starting device auth via %s\n", endpoint.c_str());

  std::vector<http::post_field> fields{
      {"client_id", client->client_id()},
      {"scope", scopes},
  };

  if (!http->post(endpoint, fields)) {
    throw std::runtime_error{(std::stringstream{}
                              << "could not start device auth: "
                              << http->error())
                                 .str()};
  }

  std::string response = http->result();

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

  auto expiration_ts = Clock::now() + std::chrono::seconds{expires_in};

  return DeviceAuthenticationFlow{
      client,
      user_code,
      device_code,
      verification_uri,
      std::move(verification_uri_complete),
      expiration_ts,
      std::chrono::seconds{poll_interval},
  };
}

std::string DeviceAuthenticationFlow::fetch_token()
{
  std::string key_str;

  bool done = false;

  while (!done) {
    if (auto now = Clock::now(); now >= expiry) {
      Dmsg0(100, "request expired: %zu ns vs max %zu ns\n",

            std::chrono::duration_cast<std::chrono::nanoseconds>(
                now.time_since_epoch())
                .count(),
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                expiry.time_since_epoch())
                .count());

      throw std::runtime_error{"Device auth expired"};
    }

    /* sleeping before fetching the token makes it more likely to
     * get the token on the first try - if - people generally manage
     * to login in <5 seconds. If they take 5-10 seconds instead, then
     * its better to fetch first. */
    std::this_thread::sleep_for(pinterval);

    switch (try_fetch_token(oidc, dcode, &key_str)) {
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
        pinterval += std::chrono::seconds(5);
      } break;
      default: {
        if (key_str.empty()) { key_str = "unknown error"; }

        Dmsg0(100, "We received an error during token fetch: %s\n",
              key_str.c_str());

        throw std::runtime_error{std::move(key_str)};
      } break;
    }
  }

  return key_str;
}
};  // namespace auth::Oidc::oidc
