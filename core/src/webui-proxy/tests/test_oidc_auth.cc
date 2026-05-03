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

#include "../oidc_auth.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include <openssl/core_names.h>
#include <openssl/evp.h>
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
  EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr),
                    &EVP_PKEY_CTX_free);
  EXPECT_TRUE(ctx != nullptr);
  EXPECT_EQ(EVP_PKEY_keygen_init(ctx.get()), 1);
  EXPECT_EQ(EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048), 1);
  EVP_PKEY* key = nullptr;
  EXPECT_EQ(EVP_PKEY_keygen(ctx.get(), &key), 1);
  return EvpPkeyPtr(key, &EVP_PKEY_free);
}

std::string GetBnParamBase64Url(EVP_PKEY* key, const char* param)
{
  BIGNUM* bn = nullptr;
  EXPECT_EQ(EVP_PKEY_get_bn_param(key, param, &bn), 1);
  BnPtr holder(bn, &BN_free);
  std::vector<unsigned char> bytes(BN_num_bytes(holder.get()));
  BN_bn2bin(holder.get(), bytes.data());
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
         + GetBnParamBase64Url(key, OSSL_PKEY_PARAM_RSA_N)
         + R"json(","e":")json"
         + GetBnParamBase64Url(key, OSSL_PKEY_PARAM_RSA_E) + R"json("}]})json";
}

}  // namespace

TEST(OidcAuth, CreatesAndConsumesPendingAuthorizationState)
{
  std::time_t now = 100;
  OidcPendingAuthStore store(std::chrono::minutes(10), [&]() { return now; });

  const auto pending = store.CreatePendingAuth("example");

  EXPECT_EQ(pending.provider_id, "example");
  EXPECT_FALSE(pending.state.empty());
  EXPECT_FALSE(pending.nonce.empty());
  EXPECT_FALSE(pending.code_verifier.empty());

  const auto restored = store.ConsumePendingAuth(pending.state);
  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->state, pending.state);
  EXPECT_FALSE(store.ConsumePendingAuth(pending.state).has_value());
}

TEST(OidcAuth, ExpiresPendingAuthorizationState)
{
  std::time_t now = 100;
  OidcPendingAuthStore store(std::chrono::seconds(5), [&]() { return now; });

  const auto pending = store.CreatePendingAuth("example");
  now += 6;

  EXPECT_FALSE(store.ConsumePendingAuth(pending.state).has_value());
}

TEST(OidcAuth, BuildsAuthorizationUrlWithPkceAndNonce)
{
  OidcAuthProvider provider;
  provider.id = "example";
  provider.authorization_endpoint = "https://id.example.test/oauth2/authorize";
  provider.client_id = "bareos-webui";
  provider.redirect_uri
      = "https://webui.example.test/api/v1/auth/oidc/example/callback";
  provider.scopes = {"openid", "profile", "email"};

  OidcPendingAuth pending;
  pending.provider_id = "example";
  pending.state = "state-token";
  pending.nonce = "nonce-token";
  pending.code_verifier = "code-verifier";

  const auto url = BuildOidcAuthorizationUrl(provider, pending);

  EXPECT_NE(url.find("response_type=code"), std::string::npos);
  EXPECT_NE(url.find("client_id=bareos-webui"), std::string::npos);
  EXPECT_NE(url.find("scope=openid%20profile%20email"), std::string::npos);
  EXPECT_NE(url.find("state=state-token"), std::string::npos);
  EXPECT_NE(url.find("nonce=nonce-token"), std::string::npos);
  EXPECT_NE(url.find("code_challenge_method=S256"), std::string::npos);
  EXPECT_NE(url.find("code_challenge="), std::string::npos);
}

TEST(OidcAuth, CompletesAuthorizationCodeExchangeIntoIdentity)
{
  const auto key = GenerateRsaKey();
  const std::string kid = "test-key";

  OidcAuthProvider provider;
  provider.id = "example";
  provider.issuer = "https://id.example.test";
  provider.token_endpoint = "https://id.example.test/oauth2/token";
  provider.jwks_uri = "https://id.example.test/keys";
  provider.client_id = "bareos-webui";
  provider.client_secret = "top-secret";
  provider.redirect_uri
      = "https://webui.example.test/api/v1/auth/oidc/example/callback";

  OidcPendingAuth pending;
  pending.provider_id = "example";
  pending.state = "state-token";
  pending.nonce = "nonce-token";
  pending.code_verifier = "verifier-token";

  const std::string payload
      = R"json({"iss":"https://id.example.test","sub":"248289761001","aud":"bareos-webui","exp":4102444800,"nonce":"nonce-token","email":"demo@example.test","preferred_username":"demo-admin","groups":["backup-admins"],"roles":["operator"]})json";
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

  const std::time_t now = 1000;
  const auto result = CompleteOidcAuthorization(provider, pending, "demo-code",
                                                client, [&]() { return now; });

  EXPECT_EQ(result.identity.provider, "oidc");
  EXPECT_EQ(result.identity.subject, "248289761001");
  EXPECT_EQ(result.identity.username, "demo-admin");
  EXPECT_EQ(result.identity.email, "demo@example.test");
  EXPECT_EQ(result.identity.groups,
            std::vector<std::string>({"backup-admins"}));
  EXPECT_EQ(result.identity.roles, std::vector<std::string>({"operator"}));
  ASSERT_TRUE(result.expires_at.has_value());
  EXPECT_EQ(*result.expires_at, now + 3600);

  ASSERT_EQ(client.requests.size(), 2U);
  EXPECT_NE(client.requests.front().body.find("grant_type=authorization_code"),
            std::string::npos);
  EXPECT_NE(client.requests.front().body.find("code=demo-code"),
            std::string::npos);
  EXPECT_NE(client.requests.front().body.find("code_verifier=verifier-token"),
            std::string::npos);
}

TEST(OidcAuth, RejectsIdTokenWithWrongNonce)
{
  const auto key = GenerateRsaKey();
  const std::string kid = "test-key";

  OidcAuthProvider provider;
  provider.id = "example";
  provider.issuer = "https://id.example.test";
  provider.token_endpoint = "https://id.example.test/oauth2/token";
  provider.jwks_uri = "https://id.example.test/keys";
  provider.client_id = "bareos-webui";
  provider.redirect_uri
      = "https://webui.example.test/api/v1/auth/oidc/example/callback";

  OidcPendingAuth pending;
  pending.provider_id = "example";
  pending.state = "state-token";
  pending.nonce = "expected-nonce";
  pending.code_verifier = "verifier-token";

  const std::string payload
      = R"json({"iss":"https://id.example.test","sub":"248289761001","aud":"bareos-webui","exp":4102444800,"nonce":"wrong-nonce"})json";
  const std::string id_token = SignJwt(key.get(), kid, payload);

  FakeOidcHttpClient client;
  client.responses.emplace(
      "POST https://id.example.test/oauth2/token",
      OidcHttpResponse{
          .status_code = 200,
          .headers = {},
          .body = std::string(
                      R"json({"access_token":"access-token","id_token":")json")
                  + id_token + R"json("})json"});
  client.responses.emplace("GET https://id.example.test/keys",
                           OidcHttpResponse{.status_code = 200,
                                            .headers = {},
                                            .body = BuildJwks(key.get(), kid)});

  EXPECT_THROW(CompleteOidcAuthorization(
                   provider, pending, "demo-code", client,
                   []() { return static_cast<std::time_t>(1000); }),
               std::runtime_error);
}
