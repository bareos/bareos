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

#include "lib/bauth/oidc/oidc.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "lib/bauth/oidc/verifier.h"
#include "lib/bauth/oidc/http.h"

#include "lib/base64.h"

#include <jansson.h>
#include <openssl/ec.h>
#include <chrono>
#include <string>
#include <span>
#include <sstream>

using namespace auth::Oidc;

config default_test_config = {
    .oidc_url = "non existant",
    .client_id = "bareos",
    .roles_name = "roles",
};

std::string json_to_str(json_t* json)
{
  std::string result;
  json_dump_callback(
      json,
      +[](const char* buffer, size_t size, void* arg) -> int {
        auto* output = static_cast<std::string*>(arg);
        output->insert(output->end(), buffer, buffer + size);
        return 0;
      },
      &result, JSON_COMPACT);

  return result;
}

template <typename T> json_ptr as_json(T) = delete;

template <> json_ptr as_json<std::string_view>(std::string_view s)
{
  return json_ptr{json_stringn(s.data(), s.size())};
}

template <> json_ptr as_json<const char*>(const char* s)
{
  return json_ptr{json_string(s)};
}

template <> json_ptr as_json<json_int_t>(json_int_t i)
{
  return json_ptr{json_integer(i)};
}

template <> json_ptr as_json<int>(int i) { return json_ptr{json_integer(i)}; }

// base case for below
void json_set(json_t*) {}

template <typename T, typename... Ts>
void json_set(json_t* obj, std::string_view name, T value, Ts... args)
{
  json_ptr val = as_json(value);

  if (json_object_setn_new(obj, name.data(), name.size(), val.get()) != 0) {
    throw std::runtime_error{"bad json set"};
  }

  auto* ignored = val.release();

  json_set(obj, std::forward<Ts>(args)...);
}

template <typename... Ts>
  requires(sizeof...(Ts) % 2 == 0)
json_ptr json(Ts&&... args)
{
  json_ptr obj{json_object()};
  json_set(obj.get(), std::forward<Ts>(args)...);
  return obj;
}

template <typename... Ts> std::string format_json(Ts&&... args)
{
  json_ptr obj = json(std::forward<Ts>(args)...);

  return json_to_str(obj.get());
}

class MockHttp : public http::client {
 public:
  MOCK_METHOD1(get, bool(const std::string&));
  MOCK_METHOD2(post,
               bool(const std::string&, std::span<const http::post_field>));

  std::string error() override { return error_value; }

  std::string result() override { return result_value; }

  std::uint32_t return_code() override { return http_code; }

  void set_result(std::uint32_t code, std::string content)
  {
    result_value = std::move(content);
    http_code = code;
  }

  MockHttp(config cfg) : conf{std::move(cfg)} {}

  std::string get_endpoints() { return {}; }

 private:
  config conf;

  std::string error_value;
  std::string result_value;
  std::uint32_t http_code;
};

using Clock = std::chrono::utc_clock;

struct signing_key {
  std::string name;
  EVP_PKEY* keypair;

  std::string sign(std::string_view input) const
  {
    evp_md_ctx mdctx{EVP_MD_CTX_new()};
    if (!mdctx) { throw std::runtime_error("md_ctx construction failed"); }
    EVP_PKEY_CTX* pctx = nullptr;
    // Initialize SHA-256 signing
    if (EVP_DigestSignInit(mdctx.get(), &pctx, EVP_sha256(), nullptr, keypair)
        <= 0) {
      throw std::runtime_error("DigestSignInit failed");
    }

    // Configure RSA PKCS#1 v1.5 padding
    if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) <= 0) {
      throw std::runtime_error("RSA padding configuration failed");
    }

    // Feed signing input
    if (EVP_DigestSignUpdate(mdctx.get(), input.data(), input.size()) <= 0) {
      throw std::runtime_error("DigestSignUpdate failed");
    }

    // Determine signature size
    size_t length = 0;

    if (EVP_DigestSignFinal(mdctx.get(), nullptr, &length) <= 0) {
      throw std::runtime_error("Cannot determine signature size");
    }

    std::string signature;
    signature.resize(length);

    // Generate signature
    if (EVP_DigestSignFinal(mdctx.get(),
                            reinterpret_cast<unsigned char*>(signature.data()),
                            &length)
        <= 0) {
      throw std::runtime_error("Signing failed");
    }

    assert(length <= signature.size());

    signature.resize(length);

    return signature;
  }
};

std::string base64url(std::string input)
{
  std::string result;

  // this is way more than enough
  result.resize(Base64LengthUnpadded(input.size()) + 20);
  auto count = BinToBase64(result.data(), result.size(), input.data(),
                           input.size(), true);

  if (count < 0 || (size_t)count > result.size()) {
    throw std::runtime_error{"bad base64 result"};
  }

  result.resize(count);

  // now convert it to url encoding
  std::replace(result.begin(), result.end(), '+', '-');
  std::replace(result.begin(), result.end(), '/', '_');

  return result;
}

std::string as_jwt(json_t* json, const signing_key& key)
{
  std::string payload = json_to_str(json);

  // std::string header = (std::ostringstream{} << "{ \"alg\": \"RS256\",
  // \"typ\": \"JWT\", \"kid\": \"" << key.name << "\" }").str();

  json_t* h = json_pack("{s:s,s:s,s:s}", "alg", "RS256", "typ", "JWT", "kid",
                        key.name.c_str());
  std::string header = json_to_str(h);

  auto payload64 = base64url(payload);
  auto header64 = base64url(header);

  auto to_sign = header64 + "." + payload64;

  std::string signature = key.sign(to_sign);
  std::string signature64 = base64url(signature);

  return to_sign + "." + signature64;
}

json_ptr default_token(const config& cfg)
{
  auto tp = Clock::now();
  auto un = tp.time_since_epoch();
  auto now = std::chrono::duration_cast<std::chrono::seconds>(un).count();
  json_t* token
      = json_pack("{s:s,s:s,s:s,s:s,s:s,s:I,s:I}", "aud", cfg.client_id.c_str(),
                  "iss", cfg.oidc_url.c_str(), "sub", "user", "name",
                  "user@oidc", cfg.roles_name.c_str(), "", "iat",
                  (json_int_t)now, "exp", (json_int_t)now + 30);

  return json_ptr{token};
}

oidc::keyset default_keys()
{
  evp_pkey_ctx ctx{EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr)};
  if (!ctx) { throw std::runtime_error{"could not create key ctx"}; }

  if (EVP_PKEY_keygen_init(ctx.get()) <= 0) {
    throw std::runtime_error{"could not init keygen"};
  }

  if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 3072) <= 0) {
    throw std::runtime_error{"could not set rsa bit count"};
  }

  EVP_PKEY* pkey = nullptr;
  if (EVP_PKEY_generate(ctx.get(), &pkey) <= 0) {
    throw std::runtime_error{"could not generate rsa key"};
  }

  oidc::keyset result;
  result["test"] = evp_pkey{pkey};

  return result;
}

TEST(jwt, roles_test)
{
  config cfg = default_test_config;
  auto keys = default_keys();

  auto& [name, pkey] = *keys.begin();

  signing_key key;
  key.name = name;
  key.keypair = pkey.get();

  auto token_json = default_token(cfg);
  ASSERT_NE(token_json, nullptr);

  {
    json_ptr roles{json_pack("[s]", "admin")};
    ASSERT_NE(roles, nullptr);
    ASSERT_EQ(json_object_set_new(token_json.get(), cfg.roles_name.c_str(),
                                  roles.release()),
              0);
    auto token_str = as_jwt(token_json.get(), key);
    auto claims
        = oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                            std::chrono::seconds{30}, cfg.roles_name, keys);
    EXPECT_THAT(claims.roles, testing::ElementsAre("admin"));
  }

  {
    json_ptr roles{json_pack("[ss]", "admin", "test")};
    ASSERT_NE(roles, nullptr);
    ASSERT_EQ(json_object_set_new(token_json.get(), cfg.roles_name.c_str(),
                                  roles.release()),
              0);
    auto token_str = as_jwt(token_json.get(), key);
    auto claims
        = oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                            std::chrono::seconds{30}, cfg.roles_name, keys);
    EXPECT_THAT(claims.roles, testing::ElementsAre("admin", "test"));
  }

  {
    json_ptr roles{json_pack("[]")};
    ASSERT_NE(roles, nullptr);
    ASSERT_EQ(json_object_set_new(token_json.get(), cfg.roles_name.c_str(),
                                  roles.release()),
              0);
    auto token_str = as_jwt(token_json.get(), key);
    auto claims
        = oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                            std::chrono::seconds{30}, cfg.roles_name, keys);
    EXPECT_THAT(claims.roles, testing::ElementsAre());
  }
}

struct oidc_config {
  std::string base_url;
  std::string token_endpoint;
  std::string jwks_endpoint;
  std::string device_authorization_endpoint;
  std::string issuer;
};

void check_required_fields(std::string context,
                           std::span<const http::post_field> given,
                           std::span<const http::post_field> required)
{
  std::ostringstream message;
  message << context << ": ";

  for (auto& [req_name, req_value] : required) {
    if (auto found
        = std::find_if(given.begin(), given.end(),
                       [&](auto& val) { return val.key == req_name; });
        found == given.end()) {
      message << "required field missing: " << req_name;

      throw std::runtime_error{message.str()};
    } else if (found->value != req_value) {
      message << "required field " << req_name << " has wrong value: expected '"
              << req_value << "', got '" << found->value << "'";
      throw std::runtime_error{message.str()};
    }
  }
}


// Check that a login attempt returns the correct user code, verification uri,
// and token Make sure that verification_uri_complete is unset!
TEST(oidc, login_without_complete)
{
  config cfg = default_test_config;

  std::string user_code = "ABC";
  std::string uri = "my/site";
  std::string token = "my-token";
  std::string scope = "openid profile";

  std::string config_url
      = cfg.oidc_url + "/" + ".well-known/openid-configuration";

  MockHttp http{cfg};
  EXPECT_CALL(http, get(config_url)).WillRepeatedly([&](auto) {
    auto result = format_json("issuer", uri.c_str(), "token_endpoint", "/token",
                              "jwks_uri", "/jwks",
                              "device_authorization_endpoint", "/device");
    http.set_result(200, result);
    return true;
  });
  EXPECT_CALL(http, post("/device", testing::_))
      .WillRepeatedly([&](auto, std::span<const http::post_field> fields) {
        // first we check that we were called with the correct scope/client_id

        http::post_field required[]
            = {{"scope", scope}, {"client_id", cfg.client_id}};

        check_required_fields("device auth endpoint", fields, required);

        auto result = format_json("user_code", user_code.c_str(), "device_code",
                                  "dcode-123", "verification_uri", "/verify",
                                  "interval", 1, "expires_in", 15);

        http.set_result(200, result);
        return true;
      });

  EXPECT_CALL(http, post("/token", testing::_))
      .WillOnce([&](auto, std::span<const http::post_field> fields) {
        http::post_field required[] = {
            {"device_code", "dcode-123"},
            {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
            {"client_id", cfg.client_id},
        };

        check_required_fields("token endpoint", fields, required);

        auto result = format_json("id_token", token.c_str(), "token_type",
                                  "Bearer", "exprise_in", 15);
        http.set_result(200, result);
        return true;
      });

  auto client = oidc::client::fetch_meta_data({
      .client = &http,
      .client_id = cfg.client_id,
      .oidc_url = cfg.oidc_url,
  });

  auto flow = oidc::DeviceAuthenticationFlow::start(&client, scope);

  EXPECT_EQ(flow.user_code(), user_code);
  EXPECT_NE(flow.verification_uri(), "");
  EXPECT_EQ(flow.verification_uri_complete(), std::nullopt);

  auto fetched_token = flow.fetch_token();

  EXPECT_EQ(fetched_token, token);
}

#if 0
TEST(oidc, login_complete)
{
  std::string user_code = "ABC";
  std::string uri = "my/site";
  std::string uri_complete = "my/site";

  MockHttp http;

  config cfg = default_test_config;

  auto client = oidc::client::fetch_meta_data({
    .client = &http,
    .client_id = cfg.client_id,
    .oidc_url = cfg.oidc_url,
  });

  auto flow = oidc::DeviceAuthenticationFlow::start(&client, "admin");

  EXPECT_EQ(flow.user_code(), user_code);
  EXPECT_EQ(flow.user_code(), user_code);

  auto token flow.fetch_token();
}
#endif
