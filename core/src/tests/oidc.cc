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
#include <openssl/asn1.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace auth::Oidc;

config default_test_config = {
    .oidc_url = "my/url",
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

template <> json_ptr as_json<std::string>(std::string s)
{
  return json_ptr{json_stringn(s.data(), s.size())};
}

template <> json_ptr as_json<json_int_t>(json_int_t i)
{
  return json_ptr{json_integer(i)};
}

template <> json_ptr as_json<int>(int i) { return json_ptr{json_integer(i)}; }

template <> json_ptr as_json<bool>(bool b) { return json_ptr{json_boolean(b)}; }

template <> json_ptr as_json<json_ptr>(json_ptr p) { return p; }

// base case for below
void json_set(json_t*) {}

template <typename T, typename... Ts>
void json_set(json_t* obj, std::string_view name, T value, Ts... args)
{
  json_ptr val = as_json(std::move(value));

  if (json_object_setn_new(obj, name.data(), name.size(), val.get()) != 0) {
    throw std::runtime_error{"bad json set"};
  }

  (void)val.release();

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
  MOCK_METHOD(bool, get, (const std::string&), (override));
  MOCK_METHOD(bool,
              post,
              (const std::string&, std::span<const http::post_field>),
              (override));

  std::string error() override { return error_value; }

  std::string result() override { return result_value; }

  std::uint32_t return_code() override { return http_code; }

  void set_result(std::uint32_t code, std::string content)
  {
    result_value = std::move(content);
    http_code = code;
  }

  void set_error(std::string err) { error_value = std::move(err); }

  MockHttp(config cfg = default_test_config) : conf{std::move(cfg)} {}

  std::string get_endpoints() { return {}; }

 private:
  config conf;

  std::string error_value;
  std::string result_value;
  std::uint32_t http_code{200};
};

// JWT NumericDate (RFC 7519 Section 2) uses Unix epoch seconds (system_clock),
// without leap seconds.
using Clock = std::chrono::system_clock;

json_int_t unix_now()
{
  return std::chrono::duration_cast<std::chrono::seconds>(
             Clock::now().time_since_epoch())
      .count();
}

struct signing_key {
  std::string name;
  EVP_PKEY* keypair;

  std::string sign(std::string_view input,
                   const EVP_MD* md = EVP_sha256()) const
  {
    evp_md_ctx mdctx{EVP_MD_CTX_new()};
    if (!mdctx) { throw std::runtime_error("md_ctx construction failed"); }
    EVP_PKEY_CTX* pctx = nullptr;
    if (EVP_DigestSignInit(mdctx.get(), &pctx, md, nullptr, keypair) <= 0) {
      throw std::runtime_error("DigestSignInit failed");
    }

    if (EVP_PKEY_is_a(keypair, "RSA")) {
      // Configure RSA PKCS#1 v1.5 padding
      if (EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) <= 0) {
        throw std::runtime_error("RSA padding configuration failed");
      }
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

std::string base64url(std::string_view input)
{
  if (input.empty()) { return {}; }

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

std::string base64_standard(std::string_view input)
{
  if (input.empty()) { return {}; }
  std::string out(4 * ((input.size() + 2) / 3), '\0');
  int len
      = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                        reinterpret_cast<const unsigned char*>(input.data()),
                        static_cast<int>(input.size()));
  if (len < 0) { throw std::runtime_error{"EVP_EncodeBlock failed"}; }
  out.resize(static_cast<size_t>(len));
  return out;
}

std::string as_jwt_with_header(json_t* header_json,
                               json_t* payload_json,
                               const signing_key& key,
                               const EVP_MD* md = EVP_sha256())
{
  std::string header = json_to_str(header_json);
  std::string payload = json_to_str(payload_json);

  auto header64 = base64url(header);
  auto payload64 = base64url(payload);

  auto to_sign = header64 + "." + payload64;

  std::string signature = key.sign(to_sign, md);
  std::string signature64 = base64url(signature);

  return to_sign + "." + signature64;
}

std::string as_jwt(json_t* payload_json, const signing_key& key)
{
  json_ptr h{json_pack("{s:s,s:s,s:s}", "alg", "RS256", "typ", "JWT", "kid",
                       key.name.c_str())};
  return as_jwt_with_header(h.get(), payload_json, key);
}

json_ptr default_token(const config& cfg)
{
  auto now = unix_now();
  json_t* token = json_pack(
      "{s:s,s:s,s:s,s:s,s:[s],s:I,s:I}", "aud", cfg.client_id.c_str(), "iss",
      cfg.oidc_url.c_str(), "sub", "user", "name", "user@oidc",
      cfg.roles_name.c_str(), "operator", "iat", now, "exp", now + 30);

  return json_ptr{token};
}

evp_pkey generate_rsa_key(int bits = 3072, unsigned long pubexp = RSA_F4)
{
  evp_pkey_ctx ctx{EVP_PKEY_CTX_new_from_name(nullptr, "RSA", nullptr)};
  if (!ctx) { throw std::runtime_error{"could not create key ctx"}; }

  if (EVP_PKEY_keygen_init(ctx.get()) <= 0) {
    throw std::runtime_error{"could not init keygen"};
  }

  if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), bits) <= 0) {
    throw std::runtime_error{"could not set rsa bit count"};
  }

  BIGNUM* bn_e = BN_new();
  if (!bn_e || BN_set_word(bn_e, pubexp) != 1) {
    BN_free(bn_e);
    throw std::runtime_error{"could not create pubexp bignum"};
  }
  int rc = EVP_PKEY_CTX_set1_rsa_keygen_pubexp(ctx.get(), bn_e);
  BN_free(bn_e);
  if (rc <= 0) { throw std::runtime_error{"could not set rsa pubexp"}; }

  EVP_PKEY* pkey = nullptr;
  if (EVP_PKEY_generate(ctx.get(), &pkey) <= 0) {
    throw std::runtime_error{"could not generate rsa key"};
  }

  return evp_pkey{pkey};
}

evp_pkey generate_ec_key()
{
  EVP_PKEY* pkey = EVP_EC_gen("P-256");
  if (!pkey) { throw std::runtime_error{"could not generate ec key"}; }
  return evp_pkey{pkey};
}

oidc::keyset default_keys()
{
  oidc::keyset result;
  result["test"] = generate_rsa_key();
  return result;
}

std::string bn_param_to_base64url(const EVP_PKEY* pkey, const char* param_name)
{
  BIGNUM* bn = nullptr;
  if (EVP_PKEY_get_bn_param(pkey, param_name, &bn) != 1 || !bn) {
    throw std::runtime_error{std::string{"could not get bn param "}
                             + param_name};
  }
  std::string bytes(static_cast<size_t>(BN_num_bytes(bn)), '\0');
  BN_bn2bin(bn, reinterpret_cast<unsigned char*>(bytes.data()));
  BN_free(bn);
  return base64url(bytes);
}

json_ptr make_rsa_jwk(const EVP_PKEY* pkey,
                      std::string_view kid,
                      std::string_view use = "sig",
                      std::optional<std::string_view> issuer = std::nullopt)
{
  auto n64 = bn_param_to_base64url(pkey, OSSL_PKEY_PARAM_RSA_N);
  auto e64 = bn_param_to_base64url(pkey, OSSL_PKEY_PARAM_RSA_E);

  auto jwk = json("kty", "RSA", "use", use, "kid", kid, "n",
                  std::string_view{n64}, "e", std::string_view{e64});
  if (issuer) {
    json_object_set_new(jwk.get(), "issuer",
                        json_stringn(issuer->data(), issuer->size()));
  }
  return jwk;
}

std::string make_self_signed_cert_der(EVP_PKEY* pkey,
                                      long not_before_offset_sec = -3600,
                                      long not_after_offset_sec = 3600,
                                      const char* common_name = "oidc")
{
  x509 cert{X509_new()};
  if (!cert) { throw std::runtime_error{"X509_new failed"}; }

  if (X509_set_version(cert.get(), 2) != 1) {
    throw std::runtime_error{"X509_set_version failed"};
  }
  if (ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1) != 1) {
    throw std::runtime_error{"ASN1_INTEGER_set failed"};
  }
  if (!X509_gmtime_adj(X509_getm_notBefore(cert.get()),
                       not_before_offset_sec)) {
    throw std::runtime_error{"X509_gmtime_adj notBefore failed"};
  }
  if (!X509_gmtime_adj(X509_getm_notAfter(cert.get()), not_after_offset_sec)) {
    throw std::runtime_error{"X509_gmtime_adj notAfter failed"};
  }
  if (X509_set_pubkey(cert.get(), pkey) != 1) {
    throw std::runtime_error{"X509_set_pubkey failed"};
  }

  X509_NAME* name = X509_get_subject_name(cert.get());
  if (X509_NAME_add_entry_by_txt(
          name, "CN", MBSTRING_ASC,
          reinterpret_cast<const unsigned char*>(common_name), -1, -1, 0)
      != 1) {
    throw std::runtime_error{"X509_NAME_add_entry_by_txt failed"};
  }
  if (X509_set_issuer_name(cert.get(), name) != 1) {
    throw std::runtime_error{"X509_set_issuer_name failed"};
  }

  if (X509_sign(cert.get(), pkey, EVP_sha256()) <= 0) {
    throw std::runtime_error{"X509_sign failed"};
  }

  unsigned char* der_buf = nullptr;
  int der_len = i2d_X509(cert.get(), &der_buf);
  if (der_len <= 0 || !der_buf) { throw std::runtime_error{"i2d_X509 failed"}; }
  std::string der(reinterpret_cast<const char*>(der_buf),
                  static_cast<size_t>(der_len));
  OPENSSL_free(der_buf);
  return der;
}

std::string public_key_pem(EVP_PKEY* pkey)
{
  bio mem{BIO_new(BIO_s_mem())};
  if (!mem || PEM_write_bio_PUBKEY(mem.get(), pkey) != 1) {
    throw std::runtime_error{"PEM_write_bio_PUBKEY failed"};
  }
  BUF_MEM* ptr = nullptr;
  BIO_get_mem_ptr(mem.get(), &ptr);
  return {ptr->data, ptr->length};
}

std::string hmac_sha256(std::string_view secret, std::string_view data)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int md_len = 0;
  if (!HMAC(EVP_sha256(), secret.data(), static_cast<int>(secret.size()),
            reinterpret_cast<const unsigned char*>(data.data()), data.size(),
            md, &md_len)) {
    throw std::runtime_error{"HMAC failed"};
  }
  return {reinterpret_cast<const char*>(md), md_len};
}

// Shared key material so tests do not pay repeated RSA keygen cost.
class JwtTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite()
  {
    primary_rsa = generate_rsa_key(2048);
    secondary_rsa = generate_rsa_key(2048);
    ec_p256 = generate_ec_key();
  }

  static void TearDownTestSuite()
  {
    primary_rsa.reset();
    secondary_rsa.reset();
    ec_p256.reset();
  }

  static oidc::keyset make_keyset(std::string_view kid = "test",
                                  EVP_PKEY* pkey = nullptr)
  {
    if (!pkey) { pkey = primary_rsa.get(); }
    if (EVP_PKEY_up_ref(pkey) != 1) {
      throw std::runtime_error{"EVP_PKEY_up_ref failed"};
    }
    oidc::keyset ks;
    ks.emplace(std::string{kid}, evp_pkey{pkey});
    return ks;
  }

  static signing_key primary_signer(std::string name = "test")
  {
    return signing_key{std::move(name), primary_rsa.get()};
  }

  static signing_key secondary_signer(std::string name = "test")
  {
    return signing_key{std::move(name), secondary_rsa.get()};
  }

  inline static evp_pkey primary_rsa{};
  inline static evp_pkey secondary_rsa{};
  inline static evp_pkey ec_p256{};
};

oidc::client make_client_with_jwks(MockHttp& http,
                                   const config& cfg,
                                   std::string jwks_body,
                                   bool jwks_http_ok = true)
{
  std::string config_url = cfg.oidc_url;
  if (config_url.back() != '/') { config_url.push_back('/'); }
  config_url += ".well-known/openid-configuration";

  EXPECT_CALL(http, get(config_url)).WillOnce([&](const std::string&) {
    auto meta = format_json("issuer", cfg.oidc_url.c_str(), "token_endpoint",
                            "/token", "jwks_uri", "/jwks",
                            "device_authorization_endpoint", "/device");
    http.set_result(200, std::move(meta));
    return true;
  });

  EXPECT_CALL(http, get("/jwks"))
      .WillOnce([&, body = std::move(jwks_body)](const std::string&) {
        if (!jwks_http_ok) {
          http.set_error("connection refused");
          return false;
        }
        http.set_result(200, body);
        return true;
      });

  return oidc::client::fetch_meta_data({
      .client = &http,
      .client_id = cfg.client_id,
      .oidc_url = cfg.oidc_url,
  });
}

TEST(JwtTest, roles_test)
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

// ============================================================================
// JWT Timing & Expiration Edge Cases
// ============================================================================

TEST_F(JwtTest, expired_token_beyond_clock_skew_rejected)
{
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto token_json = default_token(cfg);
  auto now = unix_now();

  json_object_set_new(token_json.get(), "iat", json_integer(now - 120));
  json_object_set_new(token_json.get(), "exp", json_integer(now - 60));

  auto token_str = as_jwt(token_json.get(), primary_signer());
  EXPECT_THROW(
      oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                        std::chrono::seconds{30}, cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, expired_token_within_clock_skew_accepted)
{
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto token_json = default_token(cfg);
  auto now = unix_now();

  json_object_set_new(token_json.get(), "iat", json_integer(now - 60));
  json_object_set_new(token_json.get(), "exp", json_integer(now - 10));

  auto token_str = as_jwt(token_json.get(), primary_signer());

  // Accepted when clock skew leeway covers the 10s expiration delta.
  auto claims
      = oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys);
  EXPECT_EQ(claims.user_name, "user");

  // Rejected when zero clock skew is allowed.
  EXPECT_THROW(oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                                 std::chrono::seconds{0}, cfg.roles_name, keys),
               std::exception);
}

TEST_F(JwtTest, missing_exp_claim_rejected)
{
  // OIDC Core 1.0 Section 2: "exp" is REQUIRED in ID Tokens.
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto token_json = default_token(cfg);
  ASSERT_EQ(json_object_del(token_json.get(), "exp"), 0);

  auto token_str = as_jwt(token_json.get(), primary_signer());
  EXPECT_THROW(
      oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                        std::chrono::seconds{30}, cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, missing_iat_claim_rejected)
{
  // OIDC Core 1.0 Section 2: "iat" is REQUIRED in ID Tokens.
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto token_json = default_token(cfg);
  ASSERT_EQ(json_object_del(token_json.get(), "iat"), 0);

  auto token_str = as_jwt(token_json.get(), primary_signer());
  EXPECT_THROW(
      oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                        std::chrono::seconds{30}, cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, not_before_claim_handling)
{
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto now = unix_now();

  // nbf far in the future (beyond 10s skew) -> rejected.
  {
    auto token_json = default_token(cfg);
    json_object_set_new(token_json.get(), "nbf", json_integer(now + 60));
    json_object_set_new(token_json.get(), "exp", json_integer(now + 120));
    auto token_str = as_jwt(token_json.get(), primary_signer());
    EXPECT_THROW(
        oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{10}, cfg.roles_name, keys),
        std::exception);
  }

  // nbf slightly in the future (within 30s skew) -> accepted.
  {
    auto token_json = default_token(cfg);
    json_object_set_new(token_json.get(), "nbf", json_integer(now + 10));
    json_object_set_new(token_json.get(), "exp", json_integer(now + 120));
    auto token_str = as_jwt(token_json.get(), primary_signer());
    EXPECT_NO_THROW(oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                                      std::chrono::seconds{30}, cfg.roles_name,
                                      keys));
  }

  // nbf in the past with zero skew -> accepted.
  {
    auto token_json = default_token(cfg);
    json_object_set_new(token_json.get(), "nbf", json_integer(now - 10));
    auto token_str = as_jwt(token_json.get(), primary_signer());
    EXPECT_NO_THROW(oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                                      std::chrono::seconds{0}, cfg.roles_name,
                                      keys));
  }
}

TEST_F(JwtTest, issued_at_in_future_handling)
{
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto now = unix_now();

  // iat far in the future beyond clock skew -> rejected.
  {
    auto token_json = default_token(cfg);
    json_object_set_new(token_json.get(), "iat", json_integer(now + 60));
    json_object_set_new(token_json.get(), "exp", json_integer(now + 120));
    auto token_str = as_jwt(token_json.get(), primary_signer());
    EXPECT_THROW(
        oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{10}, cfg.roles_name, keys),
        std::exception);
  }

  // iat slightly in the future within clock skew -> accepted.
  {
    auto token_json = default_token(cfg);
    json_object_set_new(token_json.get(), "iat", json_integer(now + 10));
    json_object_set_new(token_json.get(), "exp", json_integer(now + 120));
    auto token_str = as_jwt(token_json.get(), primary_signer());
    EXPECT_NO_THROW(oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                                      std::chrono::seconds{30}, cfg.roles_name,
                                      keys));
  }
}

TEST_F(JwtTest, non_numeric_time_claims_rejected)
{
  config cfg = default_test_config;
  auto keys = make_keyset();

  // Notably, a string timestamp far in the future like "99999999999" for nbf or
  // iat must NOT be coerced to 0 (epoch 1970) and silently accepted.
  for (const char* claim : {"exp", "nbf", "iat"}) {
    auto token_json = default_token(cfg);
    json_object_set_new(token_json.get(), claim, json_string("99999999999"));
    auto token_str = as_jwt(token_json.get(), primary_signer());
    EXPECT_THROW(
        oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception)
        << "Expected non-numeric " << claim << " to fail";
  }
}

TEST_F(JwtTest, fractional_numeric_date_claims_accepted)
{
  // RFC 7519 Section 2: NumericDate is a JSON numeric value; non-integer values
  // (e.g. sub-second floats emitted by Python/Ruby IdPs) are valid.
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto now = static_cast<double>(unix_now());

  auto token_json = default_token(cfg);
  json_object_set_new(token_json.get(), "iat", json_real(now - 1.25));
  json_object_set_new(token_json.get(), "nbf", json_real(now - 0.5));
  json_object_set_new(token_json.get(), "exp", json_real(now + 30.75));

  EXPECT_NO_THROW(oidc::VerifyToken(
      as_jwt(token_json.get(), primary_signer()), cfg.oidc_url, cfg.client_id,
      std::chrono::seconds{5}, cfg.roles_name, keys));
}

// ============================================================================
// JWT Keys, Key Rotation, Signatures & Algorithm Confusion Edge Cases
// ============================================================================

TEST_F(JwtTest, key_rotation_and_unknown_kid)
{
  config cfg = default_test_config;
  auto keys = make_keyset("key-v1", primary_rsa.get());
  ASSERT_EQ(EVP_PKEY_up_ref(secondary_rsa.get()), 1);
  keys.emplace("key-v2", evp_pkey{secondary_rsa.get()});

  auto token_json = default_token(cfg);

  // Both active keys in the keyset verify their own tokens.
  EXPECT_NO_THROW(oidc::VerifyToken(
      as_jwt(token_json.get(), primary_signer("key-v1")), cfg.oidc_url,
      cfg.client_id, std::chrono::seconds{5}, cfg.roles_name, keys));
  EXPECT_NO_THROW(oidc::VerifyToken(
      as_jwt(token_json.get(), secondary_signer("key-v2")), cfg.oidc_url,
      cfg.client_id, std::chrono::seconds{5}, cfg.roles_name, keys));

  // Token signed by retired key whose kid is no longer in JWKS -> rejected.
  EXPECT_THROW(oidc::VerifyToken(
                   as_jwt(token_json.get(), primary_signer("retired-key-v0")),
                   cfg.oidc_url, cfg.client_id, std::chrono::seconds{5},
                   cfg.roles_name, keys),
               std::exception);

  // Token claiming kid "key-v1" but signed with key-v2's private key ->
  // rejected.
  EXPECT_THROW(
      oidc::VerifyToken(as_jwt(token_json.get(), secondary_signer("key-v1")),
                        cfg.oidc_url, cfg.client_id, std::chrono::seconds{5},
                        cfg.roles_name, keys),
      std::exception);

  // Empty keyset rejects all tokens.
  oidc::keyset empty_keys;
  EXPECT_THROW(
      oidc::VerifyToken(as_jwt(token_json.get(), primary_signer("key-v1")),
                        cfg.oidc_url, cfg.client_id, std::chrono::seconds{5},
                        cfg.roles_name, empty_keys),
      std::exception);
}

TEST_F(JwtTest, wrong_signing_key_rejected)
{
  config cfg = default_test_config;
  auto keys = make_keyset("test", primary_rsa.get());
  auto token_json = default_token(cfg);

  // Header kid matches "test", but signed by secondary_rsa.
  auto token_str = as_jwt(token_json.get(), secondary_signer("test"));
  EXPECT_THROW(
      oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                        std::chrono::seconds{30}, cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, missing_or_non_string_kid_rejected)
{
  config cfg = default_test_config;
  auto keys = make_keyset("test", primary_rsa.get());
  auto token_json = default_token(cfg);

  // Header without kid
  {
    json_ptr header{json_pack("{s:s,s:s}", "alg", "RS256", "typ", "JWT")};
    auto token_str
        = as_jwt_with_header(header.get(), token_json.get(), primary_signer());
    EXPECT_THROW(
        oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }

  // Header with integer kid
  {
    json_ptr header{
        json_pack("{s:s,s:s,s:i}", "alg", "RS256", "typ", "JWT", "kid", 42)};
    auto token_str
        = as_jwt_with_header(header.get(), token_json.get(), primary_signer());
    EXPECT_THROW(
        oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }
}

TEST_F(JwtTest, non_string_kid_not_coerced_to_empty_kid)
{
  // Even if the keyset happens to contain an empty-string key id "", a
  // non-string "kid" (e.g. integer 42) must fail type validation rather than
  // silently coercing to "".
  config cfg = default_test_config;
  auto keys = make_keyset("", primary_rsa.get());
  auto token_json = default_token(cfg);

  json_ptr header{
      json_pack("{s:s,s:s,s:i}", "alg", "RS256", "typ", "JWT", "kid", 42)};
  auto token_str
      = as_jwt_with_header(header.get(), token_json.get(), primary_signer(""));
  EXPECT_THROW(
      oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                        std::chrono::seconds{30}, cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, bad_keys_in_keyset_rejected)
{
  config cfg = default_test_config;
  auto token_json = default_token(cfg);
  auto token_str = as_jwt(token_json.get(), primary_signer("test"));

  // Null EVP_PKEY in keyset
  {
    oidc::keyset null_keys;
    null_keys.emplace("test", evp_pkey{nullptr});
    EXPECT_THROW(
        oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, null_keys),
        std::exception);
  }

  // EC P-256 key in keyset where RSA is expected
  {
    auto ec_keys = make_keyset("test", ec_p256.get());
    EXPECT_THROW(
        oidc::VerifyToken(token_str, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, ec_keys),
        std::exception);
  }
}

TEST_F(JwtTest, alg_none_and_algorithm_confusion_rejected)
{
  config cfg = default_test_config;
  auto keys = make_keyset("test", primary_rsa.get());
  auto token_json = default_token(cfg);
  std::string payload64 = base64url(json_to_str(token_json.get()));

  // 1. "alg": "none" with empty signature
  {
    json_ptr hdr{
        json_pack("{s:s,s:s,s:s}", "alg", "none", "typ", "JWT", "kid", "test")};
    std::string unsigned_jwt
        = base64url(json_to_str(hdr.get())) + "." + payload64 + ".";
    EXPECT_THROW(
        oidc::VerifyToken(unsigned_jwt, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }

  // 2. Classic RS256 -> HS256 key-confusion attack: sign with HMAC-SHA256 using
  //    the server's RSA public key PEM bytes as the symmetric secret.
  {
    json_ptr hdr{json_pack("{s:s,s:s,s:s}", "alg", "HS256", "typ", "JWT", "kid",
                           "test")};
    std::string signing_input
        = base64url(json_to_str(hdr.get())) + "." + payload64;
    std::string pub_pem = public_key_pem(primary_rsa.get());
    std::string mac = hmac_sha256(pub_pem, signing_input);
    std::string confused_jwt = signing_input + "." + base64url(mac);

    EXPECT_THROW(
        oidc::VerifyToken(confused_jwt, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }

  // 3. RSA key signing with RS384 when only RS256 is permitted
  {
    json_ptr hdr{json_pack("{s:s,s:s,s:s}", "alg", "RS384", "typ", "JWT", "kid",
                           "test")};
    auto rs384_jwt = as_jwt_with_header(hdr.get(), token_json.get(),
                                        primary_signer("test"), EVP_sha384());
    EXPECT_THROW(
        oidc::VerifyToken(rs384_jwt, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }

  // 4. Header claims RS256, but signature was actually computed with SHA-384
  {
    json_ptr hdr{json_pack("{s:s,s:s,s:s}", "alg", "RS256", "typ", "JWT", "kid",
                           "test")};
    auto mismatched_digest_jwt = as_jwt_with_header(
        hdr.get(), token_json.get(), primary_signer("test"), EVP_sha384());
    EXPECT_THROW(
        oidc::VerifyToken(mismatched_digest_jwt, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }
}

TEST_F(JwtTest, tampered_token_segments_rejected)
{
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto token_json = default_token(cfg);
  auto valid_jwt = as_jwt(token_json.get(), primary_signer());

  auto dot1 = valid_jwt.find('.');
  auto dot2 = valid_jwt.find('.', dot1 + 1);
  ASSERT_NE(dot1, std::string::npos);
  ASSERT_NE(dot2, std::string::npos);

  std::string hdr_part = valid_jwt.substr(0, dot1);
  std::string payload_part = valid_jwt.substr(dot1 + 1, dot2 - dot1 - 1);
  std::string sig_part = valid_jwt.substr(dot2 + 1);

  // Tampered payload (privilege escalation to "admin" without re-signing)
  {
    auto escalated = default_token(cfg);
    json_object_set_new(escalated.get(), cfg.roles_name.c_str(),
                        json_pack("[s]", "admin"));
    std::string forged = hdr_part + "."
                         + base64url(json_to_str(escalated.get())) + "."
                         + sig_part;
    EXPECT_THROW(
        oidc::VerifyToken(forged, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }

  // Tampered header (extra field added after signing)
  {
    json_ptr hdr{json_pack("{s:s,s:s,s:s,s:s}", "alg", "RS256", "typ", "JWT",
                           "kid", "test", "cty", "JWT")};
    std::string forged = base64url(json_to_str(hdr.get())) + "." + payload_part
                         + "." + sig_part;
    EXPECT_THROW(
        oidc::VerifyToken(forged, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }

  // Corrupted byte in the middle of the signature
  {
    std::string bad_sig = sig_part;
    size_t mid = bad_sig.size() / 2;
    bad_sig[mid] = (bad_sig[mid] == 'A') ? 'B' : 'A';
    std::string forged = hdr_part + "." + payload_part + "." + bad_sig;
    EXPECT_THROW(
        oidc::VerifyToken(forged, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }

  // Empty signature with alg=RS256
  {
    std::string forged = hdr_part + "." + payload_part + ".";
    EXPECT_THROW(
        oidc::VerifyToken(forged, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }

  // Extra 4th dot-separated segment appended
  {
    std::string forged = valid_jwt + ".extrasegment";
    EXPECT_THROW(
        oidc::VerifyToken(forged, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception);
  }
}

TEST_F(JwtTest, malformed_jwt_inputs_rejected)
{
  config cfg = default_test_config;
  auto keys = make_keyset();

  const std::string malformed_tokens[] = {
      "",
      "not-a-jwt",
      "only.two",
      "..",
      "!!!.@@@.###",
      base64url("not json") + "." + base64url("{}") + ".c2ln",
      base64url("{}") + "." + base64url("not json") + ".c2ln",
      base64url("[1,2,3]") + "." + base64url("[4,5,6]") + ".c2ln",
  };

  for (const auto& bad : malformed_tokens) {
    EXPECT_THROW(
        oidc::VerifyToken(bad, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys),
        std::exception)
        << "Malformed token was not rejected: " << bad;
  }
}

// ============================================================================
// JWT Issuer, Audience, and Claims Edge Cases
// ============================================================================

TEST_F(JwtTest, issuer_and_audience_validation)
{
  config cfg = default_test_config;
  auto keys = make_keyset();

  // Wrong issuer
  {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), "iss", json_string("https://evil.idp"));
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Missing issuer
  {
    auto tok = default_token(cfg);
    json_object_del(tok.get(), "iss");
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Wrong audience string
  {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), "aud", json_string("other-service"));
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Missing audience
  {
    auto tok = default_token(cfg);
    json_object_del(tok.get(), "aud");
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Multi-valued audience array containing our client_id -> accepted (OIDC Core
  // Section 2)
  {
    auto tok = default_token(cfg);
    json_object_set_new(
        tok.get(), "aud",
        json_pack("[ss]", "other-service", cfg.client_id.c_str()));
    EXPECT_NO_THROW(oidc::VerifyToken(
        as_jwt(tok.get(), primary_signer()), cfg.oidc_url, cfg.client_id,
        std::chrono::seconds{30}, cfg.roles_name, keys));
  }

  // Multi-valued audience array NOT containing our client_id -> rejected
  {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), "aud",
                        json_pack("[ss]", "service-a", "service-b"));
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Empty audience array -> rejected
  {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), "aud", json_array());
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }
}

TEST_F(JwtTest, audience_array_with_non_string_element_rejected)
{
  // RFC 7519 Section 4.1.3: each element of an "aud" array MUST be a
  // StringOrURI value.
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto tok = default_token(cfg);
  json_object_set_new(tok.get(), "aud",
                      json_pack("[si]", cfg.client_id.c_str(), 12345));

  EXPECT_THROW(
      oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                        cfg.client_id, std::chrono::seconds{30}, cfg.roles_name,
                        keys),
      std::exception);
}

TEST_F(JwtTest, missing_or_empty_audience_rejected)
{
  // OIDC Core 1.0 Section 2: "aud" is REQUIRED. A token must also never be
  // accepted just because the claim exists but does not name us.
  config cfg = default_test_config;
  auto keys = make_keyset();

  auto verify = [&](json_t* aud) {
    auto tok = default_token(cfg);
    if (aud) {
      json_object_set_new(tok.get(), "aud", aud);
    } else {
      json_object_del(tok.get(), "aud");
    }
    return oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                             cfg.client_id, std::chrono::seconds{30},
                             cfg.roles_name, keys);
  };

  // No "aud" claim at all -> rejected
  EXPECT_THROW(verify(nullptr), std::exception);

  // "aud": null -> rejected
  EXPECT_THROW(verify(json_null()), std::exception);

  // "aud": "" -> rejected
  EXPECT_THROW(verify(json_string("")), std::exception);

  // "aud": [""] -> rejected
  EXPECT_THROW(verify(json_pack("[s]", "")), std::exception);

  // Sanity check: the very same token with a correct "aud" is accepted, so the
  // failures above are caused by the audience and nothing else.
  EXPECT_NO_THROW(verify(json_string(cfg.client_id.c_str())));
}

TEST_F(JwtTest, unrelated_null_optional_claims_accepted)
{
  // Real-world OIDC providers frequently include null for unset profile claims.
  config cfg = default_test_config;
  auto keys = make_keyset();
  auto tok = default_token(cfg);
  json_object_set_new(tok.get(), "middle_name", json_null());
  json_object_set_new(tok.get(), "picture", json_null());

  EXPECT_NO_THROW(oidc::VerifyToken(
      as_jwt(tok.get(), primary_signer()), cfg.oidc_url, cfg.client_id,
      std::chrono::seconds{30}, cfg.roles_name, keys));
}

TEST_F(JwtTest, user_and_role_claim_edge_cases)
{
  config cfg = default_test_config;
  auto keys = make_keyset();

  // preferred_username absent -> preferred_name defaults to user_name ("name")
  {
    auto tok = default_token(cfg);
    auto claims = oidc::VerifyToken(
        as_jwt(tok.get(), primary_signer()), cfg.oidc_url, cfg.client_id,
        std::chrono::seconds{30}, cfg.roles_name, keys);
    EXPECT_EQ(claims.preferred_name, "user@oidc");
  }

  // preferred_username present -> preferred_name uses preferred_username
  {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), "preferred_username", json_string("alice"));
    auto claims = oidc::VerifyToken(
        as_jwt(tok.get(), primary_signer()), cfg.oidc_url, cfg.client_id,
        std::chrono::seconds{30}, cfg.roles_name, keys);
    EXPECT_EQ(claims.preferred_name, "alice");
  }

  // Custom roles_name claim
  {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), "bareos_profiles",
                        json_pack("[ss]", "operator", "restore"));
    auto claims = oidc::VerifyToken(
        as_jwt(tok.get(), primary_signer()), cfg.oidc_url, cfg.client_id,
        std::chrono::seconds{30}, "bareos_profiles", keys);
    EXPECT_THAT(claims.roles, testing::ElementsAre("operator", "restore"));
  }

  // preferred_username has non-string types (null, integer, array) -> rejected
  for (json_t* bad_pref :
       {json_null(), json_integer(123), json_pack("[s]", "alice")}) {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), "preferred_username", bad_pref);
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Non-string "name" claim -> rejected
  {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), "name", json_integer(99));
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Missing roles claim -> rejected
  {
    auto tok = default_token(cfg);
    json_object_del(tok.get(), cfg.roles_name.c_str());
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Non-array roles claim (string, object, null) -> rejected
  for (json_t* bad_roles : {json_string("admin"), json_object(), json_null()}) {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), cfg.roles_name.c_str(), bad_roles);
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }

  // Roles array containing non-string element -> rejected
  {
    auto tok = default_token(cfg);
    json_object_set_new(tok.get(), cfg.roles_name.c_str(),
                        json_pack("[si]", "admin", 7));
    EXPECT_THROW(
        oidc::VerifyToken(as_jwt(tok.get(), primary_signer()), cfg.oidc_url,
                          cfg.client_id, std::chrono::seconds{30},
                          cfg.roles_name, keys),
        std::exception);
  }
}

// ============================================================================
// JWKS Key Discovery, Key Expiration, x5c & Bad Keys Edge Cases
// ============================================================================

TEST_F(JwtTest, jwks_valid_rsa_modulus_and_exponent)
{
  config cfg = default_test_config;
  auto jwk = make_rsa_jwk(primary_rsa.get(), "rsa-kid-1", "sig", cfg.oidc_url);
  json_ptr keys_arr{json_array()};
  json_array_append_new(keys_arr.get(), jwk.release());
  auto jwks_str = format_json("keys", std::move(keys_arr));

  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();
  ASSERT_EQ(keys.size(), 1U);
  ASSERT_TRUE(keys.contains("rsa-kid-1"));
  ASSERT_NE(keys.at("rsa-kid-1"), nullptr);

  auto tok = default_token(cfg);
  auto claims = oidc::VerifyToken(
      as_jwt(tok.get(), primary_signer("rsa-kid-1")), cfg.oidc_url,
      cfg.client_id, std::chrono::seconds{30}, cfg.roles_name, keys);
  EXPECT_THAT(claims.roles, testing::ElementsAre("operator"));
}

TEST_F(JwtTest, jwks_rsa_non_symmetric_public_exponent)
{
  // e = 65539 (0x010003) is a valid RSA exponent whose big-endian bytes
  // [0x01, 0x00, 0x03] differ from little-endian [0x03, 0x00, 0x01] (unlike
  // 65537 = [0x01, 0x00, 0x01], which is a byte palindrome).
  config cfg = default_test_config;
  auto custom_exp_key = generate_rsa_key(2048, 65539UL);

  auto jwk = make_rsa_jwk(custom_exp_key.get(), "exp-65539");
  json_ptr keys_arr{json_array()};
  json_array_append_new(keys_arr.get(), jwk.release());
  auto jwks_str = format_json("keys", std::move(keys_arr));

  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();
  ASSERT_EQ(keys.size(), 1U);

  auto tok = default_token(cfg);
  signing_key signer{"exp-65539", custom_exp_key.get()};
  auto jwt_str = as_jwt(tok.get(), signer);

  EXPECT_NO_THROW(oidc::VerifyToken(jwt_str, cfg.oidc_url, cfg.client_id,
                                    std::chrono::seconds{30}, cfg.roles_name,
                                    keys));
}

TEST_F(JwtTest, jwks_valid_x5c_certificate)
{
  config cfg = default_test_config;

  // Test CNs of varying lengths so DER byte size hits all mod-3 base64 padding
  // cases ("", "=", "==").
  for (const char* cn : {"a", "ab", "abc"}) {
    std::string cert_der
        = make_self_signed_cert_der(primary_rsa.get(), -3600, 3600, cn);
    std::string cert_b64 = base64_standard(cert_der);

    json_ptr jwk{json_pack("{s:s,s:s,s:s,s:[s]}", "kty", "RSA", "use", "sig",
                           "kid", "x5c-kid", "x5c", cert_b64.c_str())};
    json_ptr keys_arr{json_array()};
    json_array_append_new(keys_arr.get(), jwk.release());
    auto jwks_str = format_json("keys", std::move(keys_arr));

    MockHttp http{cfg};
    auto client = make_client_with_jwks(http, cfg, jwks_str);
    auto keys = client.fetch_keys_from_jwks();
    ASSERT_EQ(keys.size(), 1U);
    ASSERT_TRUE(keys.contains("x5c-kid"));

    auto tok = default_token(cfg);
    EXPECT_NO_THROW(oidc::VerifyToken(
        as_jwt(tok.get(), primary_signer("x5c-kid")), cfg.oidc_url,
        cfg.client_id, std::chrono::seconds{30}, cfg.roles_name, keys));
  }
}

TEST_F(JwtTest, jwks_x5c_certificate_chain_uses_first_cert)
{
  // RFC 7517 Section 4.7: "x5c" contains a chain of 1 or more PKIX
  // certificates; the first certificate MUST contain the public key value.
  config cfg = default_test_config;
  std::string leaf_der
      = make_self_signed_cert_der(primary_rsa.get(), -3600, 3600);
  std::string ca_der
      = make_self_signed_cert_der(secondary_rsa.get(), -3600, 7200);

  std::string leaf_b64 = base64_standard(leaf_der);
  std::string ca_b64 = base64_standard(ca_der);

  json_ptr jwk{json_pack("{s:s,s:s,s:s,s:[ss]}", "kty", "RSA", "use", "sig",
                         "kid", "chain-kid", "x5c", leaf_b64.c_str(),
                         ca_b64.c_str())};
  json_ptr keys_arr{json_array()};
  json_array_append_new(keys_arr.get(), jwk.release());
  auto jwks_str = format_json("keys", std::move(keys_arr));

  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();
  ASSERT_TRUE(keys.contains("chain-kid"));

  auto tok = default_token(cfg);
  EXPECT_NO_THROW(oidc::VerifyToken(
      as_jwt(tok.get(), primary_signer("chain-kid")), cfg.oidc_url,
      cfg.client_id, std::chrono::seconds{30}, cfg.roles_name, keys));
  // Signing with the CA key (second cert in x5c) must NOT verify as leaf.
  EXPECT_THROW(
      oidc::VerifyToken(as_jwt(tok.get(), secondary_signer("chain-kid")),
                        cfg.oidc_url, cfg.client_id, std::chrono::seconds{30},
                        cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, jwks_corrupted_rsa_modulus_rejected)
{
  config cfg = default_test_config;
  BIGNUM* bn_n = nullptr;
  ASSERT_EQ(
      EVP_PKEY_get_bn_param(primary_rsa.get(), OSSL_PKEY_PARAM_RSA_N, &bn_n),
      1);
  std::string n_bytes(static_cast<size_t>(BN_num_bytes(bn_n)), '\0');
  BN_bn2bin(bn_n, reinterpret_cast<unsigned char*>(n_bytes.data()));
  BN_free(bn_n);

  // Flip bits in the modulus so the published JWK public key does not match the
  // private signing key.
  n_bytes[0] ^= 0x04;
  auto bad_n64 = base64url(n_bytes);
  auto e64 = bn_param_to_base64url(primary_rsa.get(), OSSL_PKEY_PARAM_RSA_E);

  json_ptr jwk{json_pack("{s:s,s:s,s:s,s:s,s:s}", "kty", "RSA", "use", "sig",
                         "kid", "corrupt-n", "n", bad_n64.c_str(), "e",
                         e64.c_str())};
  json_ptr keys_arr{json_array()};
  json_array_append_new(keys_arr.get(), jwk.release());
  auto jwks_str = format_json("keys", std::move(keys_arr));

  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();

  auto tok = default_token(cfg);
  EXPECT_THROW(
      oidc::VerifyToken(as_jwt(tok.get(), primary_signer("corrupt-n")),
                        cfg.oidc_url, cfg.client_id, std::chrono::seconds{30},
                        cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, jwks_expired_x5c_certificate_rejected)
{
  // Certificate expired 1 hour ago (notBefore = -7200s, notAfter = -3600s).
  config cfg = default_test_config;
  std::string expired_der
      = make_self_signed_cert_der(primary_rsa.get(), -7200, -3600);
  std::string expired_b64 = base64_standard(expired_der);

  json_ptr jwk{json_pack("{s:s,s:s,s:s,s:[s]}", "kty", "RSA", "use", "sig",
                         "kid", "expired-cert", "x5c", expired_b64.c_str())};
  json_ptr keys_arr{json_array()};
  json_array_append_new(keys_arr.get(), jwk.release());
  auto jwks_str = format_json("keys", std::move(keys_arr));

  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();

  auto tok = default_token(cfg);
  auto jwt_str = as_jwt(tok.get(), primary_signer("expired-cert"));
  EXPECT_THROW(
      oidc::VerifyToken(jwt_str, cfg.oidc_url, cfg.client_id,
                        std::chrono::seconds{30}, cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, jwks_not_yet_valid_x5c_certificate_rejected)
{
  // Certificate is only valid starting 1 hour in the future.
  config cfg = default_test_config;
  std::string future_der
      = make_self_signed_cert_der(primary_rsa.get(), 3600, 7200);
  std::string future_b64 = base64_standard(future_der);

  json_ptr jwk{json_pack("{s:s,s:s,s:s,s:[s]}", "kty", "RSA", "use", "sig",
                         "kid", "future-cert", "x5c", future_b64.c_str())};
  json_ptr keys_arr{json_array()};
  json_array_append_new(keys_arr.get(), jwk.release());
  auto jwks_str = format_json("keys", std::move(keys_arr));

  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();

  auto tok = default_token(cfg);
  auto jwt_str = as_jwt(tok.get(), primary_signer("future-cert"));
  EXPECT_THROW(
      oidc::VerifyToken(jwt_str, cfg.oidc_url, cfg.client_id,
                        std::chrono::seconds{30}, cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, jwks_skips_bad_and_irrelevant_keys_but_keeps_valid_ones)
{
  config cfg = default_test_config;
  auto n64 = bn_param_to_base64url(primary_rsa.get(), OSSL_PKEY_PARAM_RSA_N);
  auto e64 = bn_param_to_base64url(primary_rsa.get(), OSSL_PKEY_PARAM_RSA_E);

  std::string cert_with_trailing_garbage
      = make_self_signed_cert_der(primary_rsa.get(), -3600, 3600)
        + std::string{"\x01\x02\x03\x04", 4};
  std::string trailing_b64 = base64_standard(cert_with_trailing_garbage);

  json_ptr keys_arr{json_array()};
  // 1. Encryption key ("use": "enc") -> skipped
  json_array_append_new(
      keys_arr.get(),
      make_rsa_jwk(primary_rsa.get(), "enc-key", "enc").release());
  // 2. Wrong key type ("kty": "EC") -> skipped
  json_array_append_new(
      keys_arr.get(),
      json_pack("{s:s,s:s,s:s,s:s,s:s}", "kty", "EC", "use", "sig", "kid",
                "ec-key", "n", n64.c_str(), "e", e64.c_str()));
  // 3. Mismatched issuer -> skipped
  json_array_append_new(
      keys_arr.get(),
      make_rsa_jwk(primary_rsa.get(), "wrong-iss", "sig", "https://other.idp")
          .release());
  // 4. Missing exponent "e" -> skipped
  json_array_append_new(
      keys_arr.get(), json_pack("{s:s,s:s,s:s,s:s}", "kty", "RSA", "use", "sig",
                                "kid", "no-exp", "n", n64.c_str()));
  // 5. Missing modulus "n" -> skipped
  json_array_append_new(
      keys_arr.get(), json_pack("{s:s,s:s,s:s,s:s}", "kty", "RSA", "use", "sig",
                                "kid", "no-mod", "e", e64.c_str()));
  // 6. Neither x5c nor (n, e) -> skipped
  json_array_append_new(keys_arr.get(),
                        json_pack("{s:s,s:s,s:s}", "kty", "RSA", "use", "sig",
                                  "kid", "bare-key"));
  // 7. Missing required "kid" -> skipped
  json_array_append_new(
      keys_arr.get(), json_pack("{s:s,s:s,s:s,s:s}", "kty", "RSA", "use", "sig",
                                "n", n64.c_str(), "e", e64.c_str()));
  // 8. Empty n / empty e -> skipped
  json_array_append_new(
      keys_arr.get(),
      json_pack("{s:s,s:s,s:s,s:s,s:s}", "kty", "RSA", "use", "sig", "kid",
                "empty-mod", "n", "", "e", e64.c_str()));
  json_array_append_new(
      keys_arr.get(),
      json_pack("{s:s,s:s,s:s,s:s,s:s}", "kty", "RSA", "use", "sig", "kid",
                "empty-exp", "n", n64.c_str(), "e", ""));
  // 9. Corrupted DER in x5c -> skipped
  json_array_append_new(
      keys_arr.get(),
      json_pack("{s:s,s:s,s:s,s:[s]}", "kty", "RSA", "use", "sig", "kid",
                "bad-der", "x5c", "Zm9vYmFyYmF6"));
  // 10. Empty x5c string / empty x5c array -> skipped
  json_array_append_new(keys_arr.get(),
                        json_pack("{s:s,s:s,s:s,s:[s]}", "kty", "RSA", "use",
                                  "sig", "kid", "empty-x5c", "x5c", ""));
  json_array_append_new(keys_arr.get(),
                        json_pack("{s:s,s:s,s:s,s:[]}", "kty", "RSA", "use",
                                  "sig", "kid", "empty-x5c-arr", "x5c"));
  // 11. Valid DER followed by unconsumed trailing non-zero bytes -> skipped
  json_array_append_new(
      keys_arr.get(),
      json_pack("{s:s,s:s,s:s,s:[s]}", "kty", "RSA", "use", "sig", "kid",
                "trailing-der", "x5c", trailing_b64.c_str()));
  // 12. Valid signing key -> kept!
  json_array_append_new(keys_arr.get(),
                        make_rsa_jwk(primary_rsa.get(), "good-kid").release());
  // 13. Duplicate "good-kid" with a different key -> skipped, first key kept!
  json_array_append_new(
      keys_arr.get(), make_rsa_jwk(secondary_rsa.get(), "good-kid").release());

  auto jwks_str = format_json("keys", std::move(keys_arr));
  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();

  EXPECT_EQ(keys.size(), 1U);
  ASSERT_TRUE(keys.contains("good-kid"));

  auto tok = default_token(cfg);
  // First key for "good-kid" succeeds; duplicate second key fails.
  EXPECT_NO_THROW(oidc::VerifyToken(
      as_jwt(tok.get(), primary_signer("good-kid")), cfg.oidc_url,
      cfg.client_id, std::chrono::seconds{30}, cfg.roles_name, keys));
  EXPECT_THROW(
      oidc::VerifyToken(as_jwt(tok.get(), secondary_signer("good-kid")),
                        cfg.oidc_url, cfg.client_id, std::chrono::seconds{30},
                        cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, jwks_ec_cert_disguised_as_rsa_rejected)
{
  config cfg = default_test_config;
  std::string ec_cert_der
      = make_self_signed_cert_der(ec_p256.get(), -3600, 3600);
  std::string ec_cert_b64 = base64_standard(ec_cert_der);

  json_ptr jwk{json_pack("{s:s,s:s,s:s,s:[s]}", "kty", "RSA", "use", "sig",
                         "kid", "ec-in-x5c", "x5c", ec_cert_b64.c_str())};
  json_ptr keys_arr{json_array()};
  json_array_append_new(keys_arr.get(), jwk.release());
  auto jwks_str = format_json("keys", std::move(keys_arr));

  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();

  auto tok = default_token(cfg);
  EXPECT_THROW(
      oidc::VerifyToken(as_jwt(tok.get(), primary_signer("ec-in-x5c")),
                        cfg.oidc_url, cfg.client_id, std::chrono::seconds{30},
                        cfg.roles_name, keys),
      std::exception);
}

TEST_F(JwtTest, jwks_degenerate_zero_rsa_parameters_rejected)
{
  // Modulus and exponent of all-zero bytes ("AAAA") cannot verify signatures.
  config cfg = default_test_config;
  json_ptr jwk{json_pack("{s:s,s:s,s:s,s:s,s:s}", "kty", "RSA", "use", "sig",
                         "kid", "zero-rsa", "n", "AAAA", "e", "AAAA")};
  json_ptr keys_arr{json_array()};
  json_array_append_new(keys_arr.get(), jwk.release());
  auto jwks_str = format_json("keys", std::move(keys_arr));

  MockHttp http{cfg};
  auto client = make_client_with_jwks(http, cfg, jwks_str);
  auto keys = client.fetch_keys_from_jwks();

  for (const auto& [kid, pkey] : keys) {
    EXPECT_NE(pkey, nullptr) << "Null EVP_PKEY stored in keyset for " << kid;
  }

  auto tok = default_token(cfg);
  EXPECT_THROW(
      oidc::VerifyToken(as_jwt(tok.get(), primary_signer("zero-rsa")),
                        cfg.oidc_url, cfg.client_id, std::chrono::seconds{30},
                        cfg.roles_name, keys),
      std::exception);
}

TEST(jwks, http_and_malformed_json_errors)
{
  config cfg = default_test_config;

  // HTTP error fetching JWKS
  {
    MockHttp http{cfg};
    auto client = make_client_with_jwks(http, cfg, "", false);
    EXPECT_THROW(client.fetch_keys_from_jwks(), std::runtime_error);
  }

  const std::string bad_payloads[] = {
      "not json at all",
      "[]",
      "{}",
      R"({"keys": "not-an-array"})",
      R"({"keys": {}})",
      R"({"keys": [123]})",
      R"({"keys": ["not-an-object"]})",
  };

  for (const auto& payload : bad_payloads) {
    MockHttp http{cfg};
    auto client = make_client_with_jwks(http, cfg, payload);
    EXPECT_THROW(client.fetch_keys_from_jwks(), std::runtime_error)
        << "Expected bad JWKS payload to throw: " << payload;
  }

  // Empty keys array succeeds and returns empty keyset.
  {
    MockHttp http{cfg};
    auto client = make_client_with_jwks(http, cfg, R"({"keys": []})");
    auto keys = client.fetch_keys_from_jwks();
    EXPECT_TRUE(keys.empty());
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
  std::string token = "my-token";
  std::string scope = "openid profile";

  std::string config_url
      = cfg.oidc_url + "/" + ".well-known/openid-configuration";

  MockHttp http{cfg};
  EXPECT_CALL(http, get(config_url)).WillRepeatedly([&](auto) {
    auto result = format_json("issuer", cfg.oidc_url.c_str(), "token_endpoint",
                              "/token", "jwks_uri", "/jwks",
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

TEST(oidc, login_complete)
{
  config cfg = default_test_config;

  std::string user_code = "ABC-123";
  std::string verify_uri = "https://idp.example.com/activate";
  std::string verify_uri_complete
      = "https://idp.example.com/activate?user_code=ABC-123";
  std::string token = "signed-id-token";
  std::string scope = "openid profile";
  std::string config_url = cfg.oidc_url + "/.well-known/openid-configuration";

  MockHttp http{cfg};
  EXPECT_CALL(http, get(config_url)).WillOnce([&](const std::string&) {
    http.set_result(
        200, format_json("issuer", cfg.oidc_url.c_str(), "token_endpoint",
                         "/token", "jwks_uri", "/jwks",
                         "device_authorization_endpoint", "/device"));
    return true;
  });

  EXPECT_CALL(http, post("/device", testing::_))
      .WillOnce([&](const std::string&,
                    std::span<const http::post_field> fields) {
        http::post_field required[]
            = {{"scope", scope}, {"client_id", cfg.client_id}};
        check_required_fields("device auth endpoint", fields, required);

        http.set_result(
            200, format_json("user_code", user_code, "device_code", "dcode-xyz",
                             "verification_uri", verify_uri,
                             "verification_uri_complete", verify_uri_complete,
                             "message", "Visit URL to sign in", "interval", 0,
                             "expires_in", 30));
        return true;
      });

  EXPECT_CALL(http, post("/token", testing::_))
      .WillOnce([&](const std::string&,
                    std::span<const http::post_field> fields) {
        http::post_field required[] = {
            {"device_code", "dcode-xyz"},
            {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
            {"client_id", cfg.client_id},
        };
        check_required_fields("token endpoint", fields, required);

        http.set_result(
            200,
            format_json("id_token", token, "token_type", "Bearer", "expires_in",
                        60, "refresh_token", "rt-1", "scope", scope));
        return true;
      });

  auto client = oidc::client::fetch_meta_data({
      .client = &http,
      .client_id = cfg.client_id,
      .oidc_url = cfg.oidc_url,
  });

  auto flow = oidc::DeviceAuthenticationFlow::start(&client, scope);

  EXPECT_EQ(flow.user_code(), user_code);
  EXPECT_EQ(flow.verification_uri(), verify_uri);
  ASSERT_TRUE(flow.verification_uri_complete().has_value());
  EXPECT_EQ(*flow.verification_uri_complete(), verify_uri_complete);

  EXPECT_EQ(flow.fetch_token(), token);
}

// Runs a minimal device flow whose token endpoint response carries the given
// "scope" field (nullopt means the field is omitted entirely) and checks
// whether the flow accepts the response.
void check_token_response_scope(std::optional<std::string> scope, bool accepted)
{
  config cfg = default_test_config;
  MockHttp http{cfg};

  EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
    http.set_result(
        200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/token",
                         "jwks_uri", "/jwks", "device_authorization_endpoint",
                         "/device"));
    return true;
  });

  EXPECT_CALL(http, post("/device", testing::_))
      .WillOnce([&](const std::string&, std::span<const http::post_field>) {
        http.set_result(
            200, format_json("user_code", "SCOPE", "device_code", "dc-scope",
                             "verification_uri", "/verify", "interval", 0,
                             "expires_in", 30));
        return true;
      });

  EXPECT_CALL(http, post("/token", testing::_))
      .WillOnce([&](const std::string&, std::span<const http::post_field>) {
        json_ptr body = json("id_token", "scoped-jwt", "token_type", "Bearer");
        if (scope) {
          json_object_set_new(body.get(), "scope", json_string(scope->c_str()));
        }
        http.set_result(200, json_to_str(body.get()));
        return true;
      });

  auto client = oidc::client::fetch_meta_data({
      .client = &http,
      .client_id = cfg.client_id,
      .oidc_url = cfg.oidc_url,
  });

  auto flow = oidc::DeviceAuthenticationFlow::start(&client, "openid profile");

  if (accepted) {
    EXPECT_EQ(flow.fetch_token(), "scoped-jwt")
        << "scope: " << (scope ? *scope : "<absent>");
  } else {
    EXPECT_THROW(flow.fetch_token(), std::exception)
        << "scope: " << (scope ? *scope : "<absent>");
  }
}

// The token endpoint's "scope" field tells us which scopes were actually
// granted. Since we asked for "openid profile" (and an ID token is only
// meaningful with the openid scope), a response that explicitly reports a
// different scope must be rejected instead of being used as a login.
TEST(oidc, token_response_scope_must_contain_openid)
{
  // Granted scopes containing openid -> accepted, no matter the order or the
  // additional scopes.
  check_token_response_scope("openid", true);
  check_token_response_scope("openid profile", true);
  check_token_response_scope("profile openid", true);
  check_token_response_scope("openid profile email", true);

  // RFC 6749 Section 5.1: an absent "scope" means the granted scope is
  // identical to the requested one, so this must stay valid.
  check_token_response_scope(std::nullopt, true);

  // openid was not granted -> no valid ID token, reject.
  check_token_response_scope("profile email", false);
  check_token_response_scope("", false);

  // Scope values are case sensitive (RFC 6749 Section 3.3), so these must not
  // be treated as openid.
  check_token_response_scope("OpenID profile", false);

  // A prefix/suffix match is not good enough.
  check_token_response_scope("openidx profile", false);
  check_token_response_scope("profile openid_extra", false);
}

// ============================================================================
// Metadata Discovery & Device Flow Edge Cases
// ============================================================================

TEST(oidc, metadata_discovery_url_normalization_and_errors)
{
  // Trailing slash in oidc_url should not produce "//.well-known/..."
  {
    config cfg = default_test_config;
    cfg.oidc_url = "https://idp.example.com/";

    MockHttp http{cfg};
    EXPECT_CALL(http,
                get("https://idp.example.com/.well-known/openid-configuration"))
        .WillOnce([&](const std::string&) {
          http.set_result(
              200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/tok",
                               "jwks_uri", "/keys",
                               "device_authorization_endpoint", "/dev"));
          return true;
        });

    auto client = oidc::client::fetch_meta_data({
        .client = &http,
        .client_id = cfg.client_id,
        .oidc_url = cfg.oidc_url,
    });
    EXPECT_EQ(client.token_endpoint(), "/tok");
    EXPECT_EQ(client.jwks_endpoint(), "/keys");
    EXPECT_EQ(client.device_auth_endpoint(), "/dev");
    EXPECT_EQ(client.client_id(), cfg.client_id);
    EXPECT_EQ(client.base_url(), cfg.oidc_url);
  }

  // HTTP failure during metadata fetch
  {
    config cfg = default_test_config;
    MockHttp http{cfg};
    EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
      http.set_error("DNS resolution failed");
      return false;
    });
    EXPECT_THROW(oidc::client::fetch_meta_data({
                     .client = &http,
                     .client_id = cfg.client_id,
                     .oidc_url = cfg.oidc_url,
                 }),
                 std::runtime_error);
  }

  // Malformed or incomplete metadata JSON responses
  const std::string bad_meta[] = {
      "not json",
      "[]",
      "{}",
      // Missing device_authorization_endpoint
      format_json("issuer", "x", "token_endpoint", "/t", "jwks_uri", "/j"),
      // Missing jwks_uri
      format_json("issuer", "x", "token_endpoint", "/t",
                  "device_authorization_endpoint", "/d"),
      // Missing token_endpoint
      format_json("issuer", "x", "jwks_uri", "/j",
                  "device_authorization_endpoint", "/d"),
      // Missing issuer
      format_json("token_endpoint", "/t", "jwks_uri", "/j",
                  "device_authorization_endpoint", "/d"),
      // Wrong field type (integer jwks_uri)
      format_json("issuer", "x", "token_endpoint", "/t", "jwks_uri", 123,
                  "device_authorization_endpoint", "/d"),
  };

  for (const auto& body : bad_meta) {
    config cfg = default_test_config;
    MockHttp http{cfg};
    EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
      http.set_result(200, body);
      return true;
    });
    EXPECT_THROW(oidc::client::fetch_meta_data({
                     .client = &http,
                     .client_id = cfg.client_id,
                     .oidc_url = cfg.oidc_url,
                 }),
                 std::runtime_error)
        << "Expected bad metadata to throw: " << body;
  }
}

TEST(oidc, metadata_issuer_mismatch_rejected)
{
  // OpenID Connect Discovery 1.0 Section 4.3: the "issuer" value returned MUST
  // be identical to the Issuer URL used to retrieve the configuration.
  config cfg = default_test_config;
  cfg.oidc_url = "https://expected-idp.example.com";

  MockHttp http{cfg};
  EXPECT_CALL(http, get("https://expected-idp.example.com/.well-known/"
                        "openid-configuration"))
      .WillOnce([&](const std::string&) {
        http.set_result(
            200, format_json("issuer", "https://attacker-idp.example.com",
                             "token_endpoint", "/token", "jwks_uri", "/jwks",
                             "device_authorization_endpoint", "/device"));
        return true;
      });

  EXPECT_THROW(oidc::client::fetch_meta_data({
                   .client = &http,
                   .client_id = cfg.client_id,
                   .oidc_url = cfg.oidc_url,
               }),
               std::runtime_error);
}

TEST(oidc, device_auth_start_errors)
{
  config cfg = default_test_config;

  auto make_meta_client = [&](MockHttp& http) {
    EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
      http.set_result(
          200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/token",
                           "jwks_uri", "/jwks", "device_authorization_endpoint",
                           "/device"));
      return true;
    });
    return oidc::client::fetch_meta_data({
        .client = &http,
        .client_id = cfg.client_id,
        .oidc_url = cfg.oidc_url,
    });
  };

  // HTTP POST failure
  {
    MockHttp http{cfg};
    auto client = make_meta_client(http);
    EXPECT_CALL(http, post("/device", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_error("TLS handshake failed");
          return false;
        });
    EXPECT_THROW(oidc::DeviceAuthenticationFlow::start(&client, "openid"),
                 std::runtime_error);
  }

  // Invalid / incomplete JSON responses from /device
  const std::string bad_device_responses[] = {
      "not json",
      "{}",
      // Missing user_code
      format_json("device_code", "dc", "verification_uri", "/v", "expires_in",
                  60),
      // Missing device_code
      format_json("user_code", "uc", "verification_uri", "/v", "expires_in",
                  60),
      // Missing verification_uri
      format_json("user_code", "uc", "device_code", "dc", "expires_in", 60),
      // Missing expires_in
      format_json("user_code", "uc", "device_code", "dc", "verification_uri",
                  "/v"),
  };

  for (const auto& body : bad_device_responses) {
    MockHttp http{cfg};
    auto client = make_meta_client(http);
    EXPECT_CALL(http, post("/device", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_result(200, body);
          return true;
        });
    EXPECT_THROW(oidc::DeviceAuthenticationFlow::start(&client, "openid"),
                 std::runtime_error)
        << "Expected bad /device response to throw: " << body;
  }
}

TEST(oidc, token_polling_authorization_pending_then_bearer_case_insensitive)
{
  config cfg = default_test_config;
  MockHttp http{cfg};

  EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
    http.set_result(
        200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/token",
                         "jwks_uri", "/jwks", "device_authorization_endpoint",
                         "/device"));
    return true;
  });

  EXPECT_CALL(http, post("/device", testing::_))
      .WillOnce([&](const std::string&, std::span<const http::post_field>) {
        http.set_result(
            200, format_json("user_code", "CODE1", "device_code", "dc-poll",
                             "verification_uri", "/verify", "interval", 0,
                             "expires_in", 30));
        return true;
      });

  int attempts = 0;
  EXPECT_CALL(http, post("/token", testing::_))
      .Times(3)
      .WillRepeatedly([&](const std::string&,
                          std::span<const http::post_field>) {
        ++attempts;
        if (attempts < 3) {
          http.set_result(
              400, format_json("error", "authorization_pending",
                               "error_description", "User has not completed"));
          return true;
        }
        // RFC 6749 token_type comparison is case-insensitive ("bearer").
        http.set_result(200, format_json("id_token", "final-jwt", "token_type",
                                         "bearer", "expires_in", 60));
        return true;
      });

  auto client = oidc::client::fetch_meta_data({
      .client = &http,
      .client_id = cfg.client_id,
      .oidc_url = cfg.oidc_url,
  });
  auto flow = oidc::DeviceAuthenticationFlow::start(&client, "openid");
  EXPECT_EQ(flow.fetch_token(), "final-jwt");
  EXPECT_EQ(attempts, 3);
}

TEST(oidc, token_polling_slow_down_recovers)
{
  config cfg = default_test_config;
  MockHttp http{cfg};

  EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
    http.set_result(
        200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/token",
                         "jwks_uri", "/jwks", "device_authorization_endpoint",
                         "/device"));
    return true;
  });

  EXPECT_CALL(http, post("/device", testing::_))
      .WillOnce([&](const std::string&, std::span<const http::post_field>) {
        http.set_result(
            200, format_json("user_code", "SLOW1", "device_code", "dc-slow",
                             "verification_uri", "/verify", "interval", 0,
                             "expires_in", 30));
        return true;
      });

  int attempts = 0;
  EXPECT_CALL(http, post("/token", testing::_))
      .Times(2)
      .WillRepeatedly(
          [&](const std::string&, std::span<const http::post_field>) {
            if (++attempts == 1) {
              http.set_result(400, format_json("error", "slow_down"));
              return true;
            }
            http.set_result(200, format_json("id_token", "slow-ok-jwt",
                                             "token_type", "Bearer"));
            return true;
          });

  auto client = oidc::client::fetch_meta_data({
      .client = &http,
      .client_id = cfg.client_id,
      .oidc_url = cfg.oidc_url,
  });
  auto flow = oidc::DeviceAuthenticationFlow::start(&client, "openid");
  EXPECT_EQ(flow.fetch_token(), "slow-ok-jwt");
  EXPECT_EQ(attempts, 2);
}

TEST(oidc, device_flow_expiration)
{
  config cfg = default_test_config;

  // Immediate local expiration (expires_in == 0) -> throws before polling
  // /token
  {
    MockHttp http{cfg};
    EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
      http.set_result(
          200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/token",
                           "jwks_uri", "/jwks", "device_authorization_endpoint",
                           "/device"));
      return true;
    });
    EXPECT_CALL(http, post("/device", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_result(
              200, format_json("user_code", "EXP0", "device_code", "dc-exp0",
                               "verification_uri", "/verify", "interval", 0,
                               "expires_in", 0));
          return true;
        });
    EXPECT_CALL(http, post("/token", testing::_)).Times(0);

    auto client = oidc::client::fetch_meta_data({
        .client = &http,
        .client_id = cfg.client_id,
        .oidc_url = cfg.oidc_url,
    });
    auto flow = oidc::DeviceAuthenticationFlow::start(&client, "openid");
    EXPECT_THROW(flow.fetch_token(), std::runtime_error);
  }

  // Expires while polling (slow_down pushes next poll past 1s expiry)
  {
    MockHttp http{cfg};
    EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
      http.set_result(
          200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/token",
                           "jwks_uri", "/jwks", "device_authorization_endpoint",
                           "/device"));
      return true;
    });
    EXPECT_CALL(http, post("/device", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_result(
              200, format_json("user_code", "EXP1", "device_code", "dc-exp1",
                               "verification_uri", "/verify", "interval", 1,
                               "expires_in", 1));
          return true;
        });
    EXPECT_CALL(http, post("/token", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_result(400, format_json("error", "slow_down"));
          return true;
        });

    auto client = oidc::client::fetch_meta_data({
        .client = &http,
        .client_id = cfg.client_id,
        .oidc_url = cfg.oidc_url,
    });
    auto flow = oidc::DeviceAuthenticationFlow::start(&client, "openid");
    EXPECT_THROW(flow.fetch_token(), std::runtime_error);
  }
}

TEST(oidc, token_endpoint_terminal_errors_abort_polling)
{
  config cfg = default_test_config;

  const std::vector<std::pair<std::uint32_t, std::string>> error_responses = {
      {400, format_json("error", "expired_token", "error_description",
                        "Device code has expired")},
      {400, format_json("error", "access_denied")},
      {400, format_json("error", "invalid_request")},
      {401, format_json("error", "invalid_client")},
      {400, format_json("error", "invalid_grant")},
      {400, format_json("error", "unauthorized_client")},
      {400, format_json("error", "unsupported_grant_type")},
      {400, format_json("error", "invalid_scope")},
      {400, format_json("error", "custom_provider_error", "error_uri",
                        "https://idp.example.com/err")},
      {500, "Internal Server Error (not JSON)"},
      {400, format_json("unexpected_field", "no error key")},
      // Malformed HTTP 200 responses:
      {200, "not json"},
      {200, format_json("access_token", "only-access-no-id-token", "token_type",
                        "Bearer")},
      {200, format_json("id_token", "tok-without-type")},
      {200, format_json("id_token", "tok-mac", "token_type", "MAC")},
  };

  for (const auto& [status_code, body] : error_responses) {
    MockHttp http{cfg};
    EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
      http.set_result(
          200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/token",
                           "jwks_uri", "/jwks", "device_authorization_endpoint",
                           "/device"));
      return true;
    });
    EXPECT_CALL(http, post("/device", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_result(
              200, format_json("user_code", "ERR", "device_code", "dc-err",
                               "verification_uri", "/verify", "interval", 0,
                               "expires_in", 30));
          return true;
        });
    // Must fail on the first /token response and NOT keep polling.
    EXPECT_CALL(http, post("/token", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_result(status_code, body);
          return true;
        });

    auto client = oidc::client::fetch_meta_data({
        .client = &http,
        .client_id = cfg.client_id,
        .oidc_url = cfg.oidc_url,
    });
    auto flow = oidc::DeviceAuthenticationFlow::start(&client, "openid");
    EXPECT_THROW(flow.fetch_token(), std::runtime_error)
        << "Expected /token response (" << status_code << ": " << body
        << ") to throw";
  }

  // Transport-level failure on /token
  {
    MockHttp http{cfg};
    EXPECT_CALL(http, get(testing::_)).WillOnce([&](const std::string&) {
      http.set_result(
          200, format_json("issuer", cfg.oidc_url, "token_endpoint", "/token",
                           "jwks_uri", "/jwks", "device_authorization_endpoint",
                           "/device"));
      return true;
    });
    EXPECT_CALL(http, post("/device", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_result(
              200, format_json("user_code", "ERR", "device_code", "dc-net",
                               "verification_uri", "/verify", "interval", 0,
                               "expires_in", 30));
          return true;
        });
    EXPECT_CALL(http, post("/token", testing::_))
        .WillOnce([&](const std::string&, std::span<const http::post_field>) {
          http.set_error("connection reset");
          return false;
        });

    auto client = oidc::client::fetch_meta_data({
        .client = &http,
        .client_id = cfg.client_id,
        .oidc_url = cfg.oidc_url,
    });
    auto flow = oidc::DeviceAuthenticationFlow::start(&client, "openid");
    EXPECT_THROW(flow.fetch_token(), std::runtime_error);
  }
}

TEST_F(JwtTest, end_to_end_discovery_jwks_device_flow_and_token_verification)
{
  config cfg = {
      .oidc_url = "https://idp.bareos.test",
      .client_id = "bareos-dir",
      .roles_name = "bareos_roles",
  };

  // JWKS publishes two keys (simulating key rotation); token is signed by the
  // second key via x5c certificate.
  json_ptr keys_arr{json_array()};
  json_array_append_new(
      keys_arr.get(),
      make_rsa_jwk(primary_rsa.get(), "key-2025", "sig", cfg.oidc_url)
          .release());

  std::string cert_der
      = make_self_signed_cert_der(secondary_rsa.get(), -3600, 3600);
  std::string cert_b64 = base64_standard(cert_der);
  json_array_append_new(
      keys_arr.get(), json_pack("{s:s,s:s,s:s,s:s,s:[s]}", "kty", "RSA", "use",
                                "sig", "kid", "key-2026", "issuer",
                                cfg.oidc_url.c_str(), "x5c", cert_b64.c_str()));
  auto jwks_body = format_json("keys", std::move(keys_arr));

  auto tok_json = default_token(cfg);
  json_object_set_new(tok_json.get(), "preferred_username",
                      json_string("backup-admin"));
  json_object_set_new(tok_json.get(), cfg.roles_name.c_str(),
                      json_pack("[ss]", "admin", "operator"));
  std::string signed_id_token
      = as_jwt(tok_json.get(), secondary_signer("key-2026"));

  MockHttp http{cfg};
  EXPECT_CALL(http,
              get("https://idp.bareos.test/.well-known/openid-configuration"))
      .WillOnce([&](const std::string&) {
        http.set_result(
            200, format_json("issuer", cfg.oidc_url, "token_endpoint",
                             "https://idp.bareos.test/oauth/token", "jwks_uri",
                             "https://idp.bareos.test/oauth/jwks",
                             "device_authorization_endpoint",
                             "https://idp.bareos.test/oauth/device"));
        return true;
      });
  EXPECT_CALL(http, get("https://idp.bareos.test/oauth/jwks"))
      .WillOnce([&](const std::string&) {
        http.set_result(200, jwks_body);
        return true;
      });
  EXPECT_CALL(http, post("https://idp.bareos.test/oauth/device", testing::_))
      .WillOnce([&](const std::string&, std::span<const http::post_field>) {
        http.set_result(
            200,
            format_json("user_code", "WDJB-MJHT", "device_code", "dev-code-99",
                        "verification_uri", "https://idp.bareos.test/device",
                        "interval", 0, "expires_in", 60));
        return true;
      });

  int poll_count = 0;
  EXPECT_CALL(http, post("https://idp.bareos.test/oauth/token", testing::_))
      .Times(2)
      .WillRepeatedly([&](const std::string&,
                          std::span<const http::post_field>) {
        if (++poll_count == 1) {
          http.set_result(400, format_json("error", "authorization_pending"));
          return true;
        }
        http.set_result(200,
                        format_json("id_token", signed_id_token, "token_type",
                                    "Bearer", "expires_in", 300));
        return true;
      });

  auto client = oidc::client::fetch_meta_data({
      .client = &http,
      .client_id = cfg.client_id,
      .oidc_url = cfg.oidc_url,
  });
  auto keys = client.fetch_keys_from_jwks();
  ASSERT_EQ(keys.size(), 2U);

  auto flow = oidc::DeviceAuthenticationFlow::start(&client, "openid profile");
  auto raw_token = flow.fetch_token();
  auto claims
      = oidc::VerifyToken(raw_token, cfg.oidc_url, cfg.client_id,
                          std::chrono::seconds{30}, cfg.roles_name, keys);

  EXPECT_EQ(claims.preferred_name, "backup-admin");
  EXPECT_THAT(claims.roles, testing::ElementsAre("admin", "operator"));
}
