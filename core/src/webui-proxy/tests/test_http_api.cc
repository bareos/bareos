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

#include "../http_api.h"
#include "../local_auth.h"
#include "../oidc_auth.h"
#include "../token_auth.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include <jansson.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/rsa.h>

namespace {

std::string Base64UrlEncode(const unsigned char* data, size_t size)
{
  std::string encoded(((size + 2) / 3) * 4, '\0');
  const int length
      = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()), data,
                        static_cast<int>(size));
  encoded.resize(length > 0 ? static_cast<size_t>(length) : 0);
  std::replace(encoded.begin(), encoded.end(), '+', '-');
  std::replace(encoded.begin(), encoded.end(), '/', '_');
  while (!encoded.empty() && encoded.back() == '=') { encoded.pop_back(); }
  return encoded;
}

std::string Base64UrlEncode(const std::string& value)
{
  return Base64UrlEncode(reinterpret_cast<const unsigned char*>(value.data()),
                         value.size());
}

class FakeOidcHttpClient : public OidcHttpClient {
 public:
  OidcHttpResponse Request(const OidcHttpRequest& request) const override
  {
    requests.push_back(request);
    const auto it = responses.find(request.method + " " + request.url);
    EXPECT_NE(it, responses.end());
    return it->second;
  }

  mutable std::vector<OidcHttpRequest> requests;
  std::map<std::string, OidcHttpResponse> responses;
};

using EvpPkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using EvpPkeyCtxPtr
    = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using EvpMdCtxPtr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using BnPtr = std::unique_ptr<BIGNUM, decltype(&BN_free)>;

EvpPkeyPtr GenerateRsaKey()
{
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr),
                    &EVP_PKEY_CTX_free);
  EXPECT_TRUE(ctx != nullptr);
  EXPECT_EQ(EVP_PKEY_keygen_init(ctx.get()), 1);
  EXPECT_EQ(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048), 1);
  EVP_PKEY* key = nullptr;
  EXPECT_EQ(EVP_PKEY_keygen(ctx.get(), &key), 1);
  return EvpPkeyPtr(key, &EVP_PKEY_free);
#else
  BnPtr exponent(BN_new(), &BN_free);
  EXPECT_TRUE(exponent != nullptr);
  EXPECT_EQ(BN_set_word(exponent.get(), RSA_F4), 1);

#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  RSA* rsa = RSA_new();
  EXPECT_TRUE(rsa != nullptr);
  EXPECT_EQ(RSA_generate_key_ex(rsa, 2048, exponent.get(), nullptr), 1);

  EVP_PKEY* key = EVP_PKEY_new();
  EXPECT_TRUE(key != nullptr);
  EXPECT_EQ(EVP_PKEY_assign_RSA(key, rsa), 1);
  rsa = nullptr;
#  pragma GCC diagnostic pop

  return EvpPkeyPtr(key, &EVP_PKEY_free);
#endif
}

std::string GetRsaComponentBase64Url(EVP_PKEY* key, bool modulus)
{
  const BIGNUM* component = nullptr;
  BnPtr holder(nullptr, &BN_free);

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
  BIGNUM* bn = nullptr;
  EXPECT_EQ(EVP_PKEY_get_bn_param(key, modulus ? "n" : "e", &bn), 1);
  holder.reset(bn);
  component = holder.get();
#else
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  std::unique_ptr<RSA, decltype(&RSA_free)> rsa(EVP_PKEY_get1_RSA(key),
                                                &RSA_free);
  EXPECT_TRUE(rsa != nullptr);
  if (!rsa) { return {}; }

  const BIGNUM* modulus_bn = nullptr;
  const BIGNUM* exponent_bn = nullptr;
  RSA_get0_key(rsa.get(), &modulus_bn, &exponent_bn, nullptr);
  component = modulus ? modulus_bn : exponent_bn;
#  pragma GCC diagnostic pop
#endif

  EXPECT_NE(component, nullptr);
  if (!component) { return {}; }

  std::vector<unsigned char> bytes(BN_num_bytes(component));
  BN_bn2bin(component, bytes.data());
  return Base64UrlEncode(bytes.data(), bytes.size());
}

std::string SignJwt(EVP_PKEY* key,
                    const std::string& kid,
                    const std::string& payload_json)
{
  const std::string header_json
      = R"json({"alg":"RS256","kid":")json" + kid + R"json(","typ":"JWT"})json";
  const std::string signing_input
      = Base64UrlEncode(header_json) + "." + Base64UrlEncode(payload_json);

  EvpMdCtxPtr ctx(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
  EXPECT_TRUE(ctx != nullptr);
  EXPECT_EQ(EVP_DigestSignInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key),
            1);
  EXPECT_EQ(EVP_DigestSignUpdate(ctx.get(), signing_input.data(),
                                 signing_input.size()),
            1);

  size_t signature_size = 0;
  EXPECT_EQ(EVP_DigestSignFinal(ctx.get(), nullptr, &signature_size), 1);
  std::vector<unsigned char> signature(signature_size, '\0');
  EXPECT_EQ(EVP_DigestSignFinal(ctx.get(), signature.data(), &signature_size),
            1);
  signature.resize(signature_size);
  return signing_input + "."
         + Base64UrlEncode(signature.data(), signature.size());
}

std::string BuildJwks(EVP_PKEY* key, const std::string& kid)
{
  return R"json({"keys":[{"kty":"RSA","use":"sig","kid":")json" + kid
         + R"json(","alg":"RS256","n":")json"
         + GetRsaComponentBase64Url(key, true) + R"json(","e":")json"
         + GetRsaComponentBase64Url(key, false) + R"json("}]})json";
}

HttpRequest MakeRequest(std::string method,
                        std::string path,
                        std::string body = "",
                        std::map<std::string, std::string> headers = {})
{
  HttpRequest request;
  request.method = std::move(method);
  request.target = path;
  request.path = path.substr(0, path.find('?'));
  request.body = std::move(body);
  request.headers = std::move(headers);
  return request;
}

ProxyConfig DemoConfig()
{
  ProxyConfig cfg;
  cfg.allowed_directors.emplace("bareos-dir",
                                DirectorTargetConfig{.id = "bareos-dir",
                                                     .host = "127.0.0.1",
                                                     .port = 9101,
                                                     .name = "bareos-dir",
                                                     .tls_psk_disable = true,
                                                     .tls_psk_require = false});

  LocalAuthUser user;
  user.id = "demo-admin";
  user.username = "demo-admin";
  user.password_hash = CreateLocalAuthPasswordHash(
      "secret", "00112233445566778899aabbccddeeff", 600000);
  user.groups = {"backup-admins"};
  cfg.local_auth_users.push_back(std::move(user));

  TokenAuthEntry token;
  token.id = "ci-bot";
  token.token_hash = CreateTokenAuthHash("demo-token");
  token.subject = "ci-bot";
  token.roles = {"operator"};
  cfg.token_auth_entries.push_back(std::move(token));

  OidcAuthProvider oidc;
  oidc.id = "example";
  oidc.display_name = "Example Identity";
  oidc.issuer = "https://id.example.test";
  oidc.authorization_endpoint = "https://id.example.test/oauth2/authorize";
  oidc.token_endpoint = "https://id.example.test/oauth2/token";
  oidc.jwks_uri = "https://id.example.test/keys";
  oidc.client_id = "bareos-webui";
  oidc.client_secret = "top-secret";
  oidc.redirect_uri
      = "https://webui.example.test/api/v1/auth/oidc/example/callback";
  oidc.scopes = {"openid", "profile", "email"};
  cfg.oidc_auth_providers.push_back(std::move(oidc));

  IdentityMappingRule local_rule;
  local_rule.id = "local-admin";
  local_rule.provider = "local";
  local_rule.username = "demo-admin";
  local_rule.director_username = "admin-notls";
  local_rule.director_password = "secret";
  local_rule.preferred_director_id = "bareos-dir";
  cfg.identity_mappings.push_back(std::move(local_rule));

  IdentityMappingRule token_rule;
  token_rule.id = "token-bot";
  token_rule.provider = "token";
  token_rule.subject = "ci-bot";
  token_rule.director_username = "limited-operator";
  token_rule.director_password = "secret";
  token_rule.preferred_director_id = "bareos-dir";
  cfg.identity_mappings.push_back(std::move(token_rule));

  IdentityMappingRule oidc_rule;
  oidc_rule.id = "oidc-admin";
  oidc_rule.provider = "oidc";
  oidc_rule.subject = "248289761001";
  oidc_rule.director_username = "admin-notls";
  oidc_rule.director_password = "secret";
  oidc_rule.preferred_director_id = "bareos-dir";
  cfg.identity_mappings.push_back(std::move(oidc_rule));

  return cfg;
}

json_t* ParseJson(const std::string& body)
{
  json_error_t err{};
  json_t* obj = json_loads(body.c_str(), 0, &err);
  EXPECT_NE(obj, nullptr) << err.text;
  return obj;
}

}  // namespace

TEST(HttpApi, ListsConfiguredProviders)
{
  const auto response
      = HandleHttpApiRequest(DemoConfig(), nullptr, nullptr,
                             MakeRequest("GET", "/api/v1/auth/providers"));

  EXPECT_EQ(response.status_code, 200);
  json_t* body = ParseJson(response.body);
  json_t* providers = json_object_get(body, "providers");
  ASSERT_TRUE(json_is_array(providers));
  EXPECT_EQ(json_array_size(providers), 3U);
  json_decref(body);
}

TEST(HttpApi, LogsInLocalUserAndReturnsSession)
{
  const auto store = std::make_shared<ProxySessionStore>(std::chrono::hours(8));
  const auto response = HandleHttpApiRequest(
      DemoConfig(), store, nullptr,
      MakeRequest(
          "POST", "/api/v1/auth/login",
          R"json({"username":"demo-admin","password":"secret","director":"bareos-dir"})json",
          {{"content-type", "application/json"}}));

  EXPECT_EQ(response.status_code, 200);
  json_t* body = ParseJson(response.body);
  const char* director_username
      = json_string_value(json_object_get(body, "director_username"));
  EXPECT_STREQ(director_username, "admin-notls");
  json_t* audit = json_object_get(body, "audit");
  ASSERT_TRUE(json_is_object(audit));
  EXPECT_STREQ(json_string_value(json_object_get(audit, "provider")), "local");
  EXPECT_STREQ(json_string_value(json_object_get(audit, "username")),
               "demo-admin");
  EXPECT_STREQ(
      json_string_value(json_object_get(audit, "mapped_director_username")),
      "admin-notls");
  const char* session_token
      = json_string_value(json_object_get(body, "session_token"));
  ASSERT_NE(session_token, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(audit, "proxy_session_token")),
               session_token);
  EXPECT_TRUE(store->GetSession(session_token).has_value());
  json_decref(body);
}

TEST(HttpApi, InspectsAndLogsOutSession)
{
  const auto store = std::make_shared<ProxySessionStore>(std::chrono::hours(8));
  AuthResult result;
  result.identity.provider = "token";
  result.identity.subject = "ci-bot";
  const auto session = store->CreateSession(result, "limited-operator",
                                            "secret", "bareos-dir");

  const auto session_response = HandleHttpApiRequest(
      DemoConfig(), store, nullptr,
      MakeRequest("GET", "/api/v1/auth/session", "",
                  {{"authorization", "Bearer " + session.token}}));
  EXPECT_EQ(session_response.status_code, 200);
  json_t* body = ParseJson(session_response.body);
  json_t* audit = json_object_get(body, "audit");
  ASSERT_TRUE(json_is_object(audit));
  EXPECT_STREQ(json_string_value(json_object_get(audit, "provider")), "token");
  EXPECT_STREQ(json_string_value(json_object_get(audit, "subject")), "ci-bot");
  EXPECT_STREQ(json_string_value(json_object_get(audit, "proxy_session_token")),
               session.token.c_str());
  json_decref(body);

  const auto logout_response = HandleHttpApiRequest(
      DemoConfig(), store, nullptr,
      MakeRequest("POST", "/api/v1/auth/logout", "",
                  {{"authorization", "Bearer " + session.token}}));
  EXPECT_EQ(logout_response.status_code, 204);
  EXPECT_FALSE(store->GetSession(session.token).has_value());
}

TEST(HttpApi, RedirectsToOidcAuthorizationEndpoint)
{
  const auto oidc_store = std::make_shared<OidcPendingAuthStore>();
  const auto response = HandleHttpApiRequest(
      DemoConfig(), nullptr, oidc_store,
      MakeRequest("GET", "/api/v1/auth/oidc/example/login"));

  EXPECT_EQ(response.status_code, 302);
  const auto location = response.headers.find("location");
  ASSERT_NE(location, response.headers.end());
  EXPECT_NE(location->second.find("https://id.example.test/oauth2/authorize"),
            std::string::npos);
  EXPECT_NE(location->second.find("response_type=code"), std::string::npos);
  EXPECT_NE(location->second.find("client_id=bareos-webui"), std::string::npos);
  EXPECT_NE(location->second.find("scope=openid%20profile%20email"),
            std::string::npos);
  EXPECT_NE(location->second.find("state="), std::string::npos);
  EXPECT_NE(location->second.find("nonce="), std::string::npos);
  EXPECT_NE(location->second.find("code_challenge="), std::string::npos);
}

TEST(HttpApi, CompletesOidcCallbackIntoProxySession)
{
  const auto session_store
      = std::make_shared<ProxySessionStore>(std::chrono::hours(8));
  const auto oidc_store = std::make_shared<OidcPendingAuthStore>(
      std::chrono::minutes(10),
      []() { return static_cast<std::time_t>(1000); });

  const auto pending = oidc_store->CreatePendingAuth("example");
  const auto key = GenerateRsaKey();
  const std::string kid = "test-key";
  const std::string payload
      = R"json({"iss":"https://id.example.test","sub":"248289761001","aud":"bareos-webui","exp":4102444800,"nonce":")json"
        + pending.nonce
        + R"json(","email":"demo@example.test","preferred_username":"demo-admin","groups":["backup-admins"],"roles":["operator"]})json";
  const std::string id_token = SignJwt(key.get(), kid, payload);

  FakeOidcHttpClient client;
  client.responses.emplace(
      "POST https://id.example.test/oauth2/token",
      OidcHttpResponse{
          .status_code = 200,
          .headers = {},
          .body = std::string(
                      R"json({"access_token":"access-token","id_token":")json")
                  + id_token + R"json(","expires_in":3600})json"});
  client.responses.emplace("GET https://id.example.test/keys",
                           OidcHttpResponse{.status_code = 200,
                                            .headers = {},
                                            .body = BuildJwks(key.get(), kid)});

  const auto response = HandleHttpApiRequest(
      DemoConfig(), session_store, oidc_store,
      MakeRequest("GET",
                  "/api/v1/auth/oidc/example/callback?code=demo-code&state="
                      + pending.state),
      &client);

  EXPECT_EQ(response.status_code, 200);
  json_t* body = ParseJson(response.body);
  const char* director_username
      = json_string_value(json_object_get(body, "director_username"));
  EXPECT_STREQ(director_username, "admin-notls");
  json_t* identity = json_object_get(body, "identity");
  ASSERT_TRUE(json_is_object(identity));
  EXPECT_STREQ(json_string_value(json_object_get(identity, "provider")),
               "oidc");
  EXPECT_STREQ(json_string_value(json_object_get(identity, "subject")),
               "248289761001");
  json_t* audit = json_object_get(body, "audit");
  ASSERT_TRUE(json_is_object(audit));
  EXPECT_STREQ(json_string_value(json_object_get(audit, "provider")), "oidc");
  EXPECT_STREQ(json_string_value(json_object_get(audit, "subject")),
               "248289761001");
  EXPECT_STREQ(
      json_string_value(json_object_get(audit, "mapped_director_username")),
      "admin-notls");
  const char* session_token
      = json_string_value(json_object_get(body, "session_token"));
  ASSERT_NE(session_token, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(audit, "proxy_session_token")),
               session_token);
  EXPECT_TRUE(session_store->GetSession(session_token).has_value());
  json_decref(body);
}
