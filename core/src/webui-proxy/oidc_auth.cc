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
#include "oidc_auth.h"

#include "auth_session.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <arpa/inet.h>
#include <jansson.h>
#include <netdb.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

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

std::vector<unsigned char> Base64UrlDecode(std::string value)
{
  std::replace(value.begin(), value.end(), '-', '+');
  std::replace(value.begin(), value.end(), '_', '/');
  while ((value.size() % 4) != 0) { value.push_back('='); }

  std::vector<unsigned char> decoded((value.size() / 4) * 3, '\0');
  const int length = EVP_DecodeBlock(
      decoded.data(), reinterpret_cast<const unsigned char*>(value.data()),
      static_cast<int>(value.size()));
  if (length < 0) {
    throw std::runtime_error("Proxy OIDC: invalid base64url data");
  }

  size_t padding = 0;
  while (!value.empty() && value.back() == '=') {
    value.pop_back();
    ++padding;
  }
  decoded.resize(static_cast<size_t>(length) - padding);
  return decoded;
}

std::string Sha256Base64Url(const std::string& value)
{
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_len = 0;
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if (!ctx) {
    throw std::runtime_error("Proxy OIDC: failed to allocate digest context");
  }
  if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1
      || EVP_DigestUpdate(ctx, value.data(), value.size()) != 1
      || EVP_DigestFinal_ex(ctx, digest.data(), &digest_len) != 1) {
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("Proxy OIDC: failed to hash PKCE verifier");
  }
  EVP_MD_CTX_free(ctx);
  return Base64UrlEncode(digest.data(), digest_len);
}

std::string PercentEncode(const std::string& value)
{
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size() * 3);
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
      encoded.push_back(static_cast<char>(ch));
      continue;
    }
    encoded.push_back('%');
    encoded.push_back(hex[(ch >> 4) & 0x0F]);
    encoded.push_back(hex[ch & 0x0F]);
  }
  return encoded;
}

std::string JoinScopes(const std::vector<std::string>& scopes)
{
  std::ostringstream output;
  bool first = true;
  for (const auto& scope : scopes) {
    if (scope.empty()) { continue; }
    if (!first) { output << ' '; }
    output << scope;
    first = false;
  }
  return output.str();
}

std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  return value;
}

std::string Trim(std::string value)
{
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(),
              std::find_if_not(value.begin(), value.end(), is_space));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(),
              value.end());
  return value;
}

struct ParsedUrl {
  std::string scheme;
  std::string host;
  int port{0};
  std::string target;
};

ParsedUrl ParseUrl(const std::string& url)
{
  const auto scheme_pos = url.find("://");
  if (scheme_pos == std::string::npos) {
    throw std::runtime_error("Proxy OIDC: unsupported URL '" + url + "'");
  }

  ParsedUrl parsed;
  parsed.scheme = ToLower(url.substr(0, scheme_pos));
  const bool https = parsed.scheme == "https";
  if (!https && parsed.scheme != "http") {
    throw std::runtime_error("Proxy OIDC: unsupported URL scheme '"
                             + parsed.scheme + "'");
  }

  const size_t authority_start = scheme_pos + 3;
  const size_t path_pos = url.find('/', authority_start);
  const std::string authority
      = url.substr(authority_start, path_pos == std::string::npos
                                        ? std::string::npos
                                        : path_pos - authority_start);
  if (authority.empty()) {
    throw std::runtime_error("Proxy OIDC: URL is missing host");
  }

  const size_t colon_pos = authority.rfind(':');
  if (colon_pos != std::string::npos
      && authority.find(']') == std::string::npos) {
    parsed.host = authority.substr(0, colon_pos);
    parsed.port = std::stoi(authority.substr(colon_pos + 1));
  } else {
    parsed.host = authority;
    parsed.port = https ? 443 : 80;
  }
  parsed.target = path_pos == std::string::npos ? "/" : url.substr(path_pos);
  return parsed;
}

class SocketConnection {
 public:
  explicit SocketConnection(const ParsedUrl& url)
  {
    const std::string port = std::to_string(url.port);
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    const int rc = getaddrinfo(url.host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
      throw std::runtime_error(std::string("Proxy OIDC: getaddrinfo failed: ")
                               + gai_strerror(rc));
    }

    for (const struct addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
      fd_ = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd_ < 0) { continue; }
      if (::connect(fd_, ai->ai_addr, ai->ai_addrlen) == 0) { break; }
      ::close(fd_);
      fd_ = -1;
    }
    freeaddrinfo(res);

    if (fd_ < 0) {
      throw std::runtime_error("Proxy OIDC: could not connect to " + url.host);
    }

    if (url.scheme == "https") {
      ssl_ctx_.reset(SSL_CTX_new(TLS_client_method()));
      if (!ssl_ctx_) {
        throw std::runtime_error("Proxy OIDC: failed to create TLS context");
      }
      SSL_CTX_set_default_verify_paths(ssl_ctx_.get());
      SSL_CTX_set_verify(ssl_ctx_.get(), SSL_VERIFY_PEER, nullptr);

      ssl_.reset(SSL_new(ssl_ctx_.get()));
      if (!ssl_) {
        throw std::runtime_error("Proxy OIDC: failed to create TLS session");
      }
      SSL_set_tlsext_host_name(ssl_.get(), url.host.c_str());
#ifdef SSL_set1_host
      SSL_set1_host(ssl_.get(), url.host.c_str());
#endif
      SSL_set_fd(ssl_.get(), fd_);
      if (SSL_connect(ssl_.get()) != 1) {
        throw std::runtime_error("Proxy OIDC: TLS connect failed");
      }
    }
  }

  ~SocketConnection()
  {
    ssl_.reset();
    ssl_ctx_.reset();
    if (fd_ >= 0) { ::close(fd_); }
  }

  void WriteAll(const std::string& data)
  {
    size_t offset = 0;
    while (offset < data.size()) {
      int written = 0;
      if (ssl_) {
        written = SSL_write(ssl_.get(), data.data() + offset,
                            static_cast<int>(data.size() - offset));
      } else {
        written = static_cast<int>(::send(fd_, data.data() + offset,
                                          data.size() - offset, MSG_NOSIGNAL));
      }
      if (written <= 0) {
        throw std::runtime_error("Proxy OIDC: upstream write failed");
      }
      offset += static_cast<size_t>(written);
    }
  }

  std::string ReadAll()
  {
    std::string data;
    std::array<char, 4096> buffer{};
    while (true) {
      int n = 0;
      if (ssl_) {
        n = SSL_read(ssl_.get(), buffer.data(),
                     static_cast<int>(buffer.size()));
      } else {
        n = static_cast<int>(::recv(fd_, buffer.data(), buffer.size(), 0));
      }
      if (n == 0) { break; }
      if (n < 0) {
        throw std::runtime_error("Proxy OIDC: upstream read failed");
      }
      data.append(buffer.data(), static_cast<size_t>(n));
    }
    return data;
  }

 private:
  struct SslCtxDeleter {
    void operator()(SSL_CTX* ctx) const { SSL_CTX_free(ctx); }
  };
  struct SslDeleter {
    void operator()(SSL* ssl) const { SSL_free(ssl); }
  };

  int fd_{-1};
  std::unique_ptr<SSL_CTX, SslCtxDeleter> ssl_ctx_;
  std::unique_ptr<SSL, SslDeleter> ssl_;
};

OidcHttpResponse ParseHttpResponse(const std::string& raw)
{
  const auto split = raw.find("\r\n\r\n");
  if (split == std::string::npos) {
    throw std::runtime_error("Proxy OIDC: invalid upstream HTTP response");
  }

  std::istringstream input(raw.substr(0, split));
  std::string line;
  if (!std::getline(input, line)) {
    throw std::runtime_error("Proxy OIDC: invalid upstream status line");
  }
  if (!line.empty() && line.back() == '\r') { line.pop_back(); }

  std::istringstream status_line(line);
  std::string http_version;
  OidcHttpResponse response;
  if (!(status_line >> http_version >> response.status_code)) {
    throw std::runtime_error("Proxy OIDC: invalid upstream status line");
  }

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') { line.pop_back(); }
    if (line.empty()) { break; }
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      throw std::runtime_error("Proxy OIDC: invalid upstream header");
    }
    response.headers.emplace(ToLower(Trim(line.substr(0, colon))),
                             Trim(line.substr(colon + 1)));
  }
  response.body = raw.substr(split + 4);
  return response;
}

class DefaultOidcHttpClientImpl : public OidcHttpClient {
 public:
  OidcHttpResponse Request(const OidcHttpRequest& request) const override
  {
    const ParsedUrl url = ParseUrl(request.url);
    SocketConnection connection(url);

    std::ostringstream http;
    http << request.method << ' ' << url.target << " HTTP/1.1\r\n";
    http << "Host: " << url.host << "\r\n";
    http << "Connection: close\r\n";
    for (const auto& [key, value] : request.headers) {
      http << key << ": " << value << "\r\n";
    }
    if (!request.body.empty()) {
      http << "Content-Length: " << request.body.size() << "\r\n";
    }
    http << "\r\n" << request.body;

    connection.WriteAll(http.str());
    return ParseHttpResponse(connection.ReadAll());
  }
};

json_t* LoadJsonObject(const std::string& text, const char* context)
{
  json_error_t err{};
  json_t* root = json_loads(text.c_str(), 0, &err);
  if (!root || !json_is_object(root)) {
    if (root) { json_decref(root); }
    throw std::runtime_error(std::string("Proxy OIDC: invalid ") + context
                             + " JSON");
  }
  return root;
}

std::string RequireString(json_t* obj,
                          const char* key,
                          const char* context,
                          bool allow_empty = false)
{
  const char* value = json_string_value(json_object_get(obj, key));
  if (!value || (!allow_empty && *value == '\0')) {
    throw std::runtime_error(std::string("Proxy OIDC: missing ") + context
                             + " field '" + key + "'");
  }
  return value;
}

std::optional<std::string> OptionalString(json_t* obj, const char* key)
{
  const char* value = json_string_value(json_object_get(obj, key));
  if (!value || *value == '\0') { return std::nullopt; }
  return std::string(value);
}

std::vector<std::string> JsonStringArray(json_t* obj, const char* key)
{
  std::vector<std::string> values;
  json_t* array = json_object_get(obj, key);
  if (!array) { return values; }
  if (json_is_string(array)) {
    const char* value = json_string_value(array);
    if (value && *value) { values.emplace_back(value); }
    return values;
  }
  if (!json_is_array(array)) { return values; }

  const size_t size = json_array_size(array);
  values.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    const char* value = json_string_value(json_array_get(array, i));
    if (value && *value) { values.emplace_back(value); }
  }
  return values;
}

std::optional<std::time_t> ParseExpiry(json_t* obj, OidcClock clock)
{
  json_t* expires_in = json_object_get(obj, "expires_in");
  if (!expires_in || !json_is_integer(expires_in)) { return std::nullopt; }
  return clock() + static_cast<std::time_t>(json_integer_value(expires_in));
}

struct OidcTokenSet {
  std::string access_token;
  std::string id_token;
  std::optional<std::time_t> expires_at;
};

OidcTokenSet ExchangeAuthorizationCode(const OidcAuthProvider& provider,
                                       const OidcPendingAuth& pending_auth,
                                       const std::string& code,
                                       const OidcHttpClient& http_client,
                                       OidcClock clock)
{
  if (provider.token_endpoint.empty()) {
    throw std::runtime_error("Proxy OIDC: provider is missing token_endpoint");
  }

  const std::string body
      = "grant_type=authorization_code&code=" + PercentEncode(code)
        + "&redirect_uri=" + PercentEncode(provider.redirect_uri)
        + "&client_id=" + PercentEncode(provider.client_id)
        + "&client_secret=" + PercentEncode(provider.client_secret)
        + "&code_verifier=" + PercentEncode(pending_auth.code_verifier);

  const auto response = http_client.Request(OidcHttpRequest{
      .method = "POST",
      .url = provider.token_endpoint,
      .headers = {{"Content-Type", "application/x-www-form-urlencoded"},
                  {"Accept", "application/json"}},
      .body = body});
  if (response.status_code != 200) {
    throw std::runtime_error("Proxy OIDC: token endpoint returned HTTP "
                             + std::to_string(response.status_code));
  }

  json_t* root = LoadJsonObject(response.body, "token response");
  OidcTokenSet token_set;
  token_set.access_token
      = RequireString(root, "access_token", "token response");
  token_set.id_token = RequireString(root, "id_token", "token response");
  token_set.expires_at = ParseExpiry(root, clock);
  json_decref(root);
  return token_set;
}

EVP_PKEY* BuildRsaPublicKey(const std::string& modulus_b64,
                            const std::string& exponent_b64)
{
  std::vector<unsigned char> modulus = Base64UrlDecode(modulus_b64);
  std::vector<unsigned char> exponent = Base64UrlDecode(exponent_b64);

  BIGNUM* modulus_bn
      = BN_bin2bn(modulus.data(), static_cast<int>(modulus.size()), nullptr);
  BIGNUM* exponent_bn
      = BN_bin2bn(exponent.data(), static_cast<int>(exponent.size()), nullptr);
  if (!modulus_bn || !exponent_bn) {
    BN_free(modulus_bn);
    BN_free(exponent_bn);
    throw std::runtime_error("Proxy OIDC: failed to decode RSA public key");
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  RSA* rsa = RSA_new();
  if (!rsa || RSA_set0_key(rsa, modulus_bn, exponent_bn, nullptr) != 1) {
    RSA_free(rsa);
    BN_free(modulus_bn);
    BN_free(exponent_bn);
    throw std::runtime_error("Proxy OIDC: failed to construct RSA public key");
  }

  EVP_PKEY* key = EVP_PKEY_new();
  if (!key || EVP_PKEY_assign_RSA(key, rsa) != 1) {
    EVP_PKEY_free(key);
    RSA_free(rsa);
    throw std::runtime_error("Proxy OIDC: failed to import RSA public key");
  }
#pragma GCC diagnostic pop
  return key;
}

void VerifyJwtSignature(const std::string& signing_input,
                        const std::vector<unsigned char>& signature,
                        const std::string& modulus_b64,
                        const std::string& exponent_b64)
{
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
      BuildRsaPublicKey(modulus_b64, exponent_b64), &EVP_PKEY_free);
  std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(),
                                                              &EVP_MD_CTX_free);
  if (!ctx) {
    throw std::runtime_error("Proxy OIDC: failed to allocate verify context");
  }
  if (EVP_DigestVerifyInit(ctx.get(), nullptr, EVP_sha256(), nullptr, key.get())
          != 1
      || EVP_DigestVerifyUpdate(ctx.get(), signing_input.data(),
                                signing_input.size())
             != 1
      || EVP_DigestVerifyFinal(ctx.get(), signature.data(), signature.size())
             != 1) {
    throw std::runtime_error("Proxy OIDC: invalid ID token signature");
  }
}

std::pair<std::string, std::string> FindJwkKey(
    const OidcAuthProvider& provider,
    const std::optional<std::string>& key_id,
    const OidcHttpClient& http_client)
{
  if (provider.jwks_uri.empty()) {
    throw std::runtime_error("Proxy OIDC: provider is missing jwks_uri");
  }

  const auto response = http_client.Request(
      OidcHttpRequest{.method = "GET",
                      .url = provider.jwks_uri,
                      .headers = {{"Accept", "application/json"}},
                      .body = ""});
  if (response.status_code != 200) {
    throw std::runtime_error("Proxy OIDC: JWKS endpoint returned HTTP "
                             + std::to_string(response.status_code));
  }

  json_t* root = LoadJsonObject(response.body, "JWKS");
  json_t* keys = json_object_get(root, "keys");
  if (!keys || !json_is_array(keys)) {
    json_decref(root);
    throw std::runtime_error("Proxy OIDC: JWKS response is missing keys");
  }

  for (size_t i = 0; i < json_array_size(keys); ++i) {
    json_t* key = json_array_get(keys, i);
    if (!json_is_object(key)) { continue; }
    const auto kty = OptionalString(key, "kty");
    const auto kid = OptionalString(key, "kid");
    if (!kty || *kty != "RSA") { continue; }
    if (key_id && kid && *kid != *key_id) { continue; }
    const auto use = OptionalString(key, "use");
    if (use && *use != "sig") { continue; }

    const auto modulus = OptionalString(key, "n");
    const auto exponent = OptionalString(key, "e");
    if (modulus && exponent) {
      const auto result = std::make_pair(*modulus, *exponent);
      json_decref(root);
      return result;
    }
  }

  json_decref(root);
  throw std::runtime_error("Proxy OIDC: no matching RSA signing key found");
}

json_t* DecodeJwtSegmentToJson(const std::string& segment, const char* context)
{
  const auto data = Base64UrlDecode(segment);
  return LoadJsonObject(
      std::string(reinterpret_cast<const char*>(data.data()), data.size()),
      context);
}

void ValidateAudience(json_t* payload, const std::string& client_id)
{
  json_t* aud = json_object_get(payload, "aud");
  if (!aud) { throw std::runtime_error("Proxy OIDC: ID token is missing aud"); }
  if (json_is_string(aud)) {
    const char* value = json_string_value(aud);
    if (!value || client_id != value) {
      throw std::runtime_error(
          "Proxy OIDC: ID token aud does not match client_id");
    }
    return;
  }
  if (json_is_array(aud)) {
    for (size_t i = 0; i < json_array_size(aud); ++i) {
      const char* value = json_string_value(json_array_get(aud, i));
      if (value && client_id == value) { return; }
    }
  }
  throw std::runtime_error("Proxy OIDC: ID token aud does not match client_id");
}

AuthIdentity BuildIdentityFromIdToken(const OidcAuthProvider& provider,
                                      const OidcPendingAuth& pending_auth,
                                      const std::string& id_token,
                                      const OidcHttpClient& http_client,
                                      OidcClock clock)
{
  const size_t dot1 = id_token.find('.');
  const size_t dot2 = dot1 == std::string::npos ? std::string::npos
                                                : id_token.find('.', dot1 + 1);
  if (dot1 == std::string::npos || dot2 == std::string::npos) {
    throw std::runtime_error("Proxy OIDC: invalid ID token format");
  }

  const std::string header_b64 = id_token.substr(0, dot1);
  const std::string payload_b64 = id_token.substr(dot1 + 1, dot2 - dot1 - 1);
  const std::string signature_b64 = id_token.substr(dot2 + 1);

  json_t* header = DecodeJwtSegmentToJson(header_b64, "ID token header");
  json_t* payload = DecodeJwtSegmentToJson(payload_b64, "ID token payload");

  const std::string alg = RequireString(header, "alg", "ID token header");
  if (alg != "RS256") {
    json_decref(header);
    json_decref(payload);
    throw std::runtime_error("Proxy OIDC: unsupported ID token alg '" + alg
                             + "'");
  }

  const auto key_id = OptionalString(header, "kid");
  const auto [modulus, exponent] = FindJwkKey(provider, key_id, http_client);
  VerifyJwtSignature(id_token.substr(0, dot2), Base64UrlDecode(signature_b64),
                     modulus, exponent);

  if (!provider.issuer.empty()) {
    const auto issuer = OptionalString(payload, "iss");
    if (!issuer || *issuer != provider.issuer) {
      json_decref(header);
      json_decref(payload);
      throw std::runtime_error("Proxy OIDC: ID token issuer does not match");
    }
  }
  ValidateAudience(payload, provider.client_id);

  json_t* exp = json_object_get(payload, "exp");
  if (!exp || !json_is_integer(exp)
      || static_cast<std::time_t>(json_integer_value(exp)) <= clock()) {
    json_decref(header);
    json_decref(payload);
    throw std::runtime_error("Proxy OIDC: ID token is expired");
  }

  const auto nonce = OptionalString(payload, "nonce");
  if (!nonce || *nonce != pending_auth.nonce) {
    json_decref(header);
    json_decref(payload);
    throw std::runtime_error("Proxy OIDC: ID token nonce does not match");
  }

  AuthIdentity identity;
  identity.provider = "oidc";
  identity.subject = RequireString(payload, "sub", "ID token payload");
  identity.email = OptionalString(payload, "email").value_or("");
  identity.username
      = OptionalString(payload, "preferred_username")
            .value_or(OptionalString(payload, "name")
                          .value_or(identity.email.empty() ? identity.subject
                                                           : identity.email));
  identity.groups = JsonStringArray(payload, "groups");
  identity.roles = JsonStringArray(payload, "roles");

  for (const auto& key : {"iss", "name"}) {
    if (const auto value = OptionalString(payload, key)) {
      identity.attributes.emplace(key, *value);
    }
  }

  json_decref(header);
  json_decref(payload);
  return identity;
}

}  // namespace

OidcPendingAuthStore::OidcPendingAuthStore(std::chrono::seconds ttl,
                                           OidcClock clock)
    : ttl_(ttl), clock_(std::move(clock))
{
}

OidcPendingAuth OidcPendingAuthStore::CreatePendingAuth(
    const std::string& provider_id)
{
  const std::time_t now = clock_();

  OidcPendingAuth pending_auth;
  pending_auth.provider_id = provider_id;
  pending_auth.state = GenerateOpaqueToken();
  pending_auth.nonce = GenerateOpaqueToken();
  pending_auth.code_verifier = GenerateOpaqueToken();
  pending_auth.created_at = now;
  pending_auth.expires_at = (std::chrono::seconds(now) + ttl_).count();

  std::lock_guard<std::mutex> lock(mutex_);
  RemoveExpiredStatesLocked(now);
  pending_[pending_auth.state] = pending_auth;
  return pending_auth;
}

std::optional<OidcPendingAuth> OidcPendingAuthStore::ConsumePendingAuth(
    const std::string& state)
{
  const std::time_t now = clock_();
  std::lock_guard<std::mutex> lock(mutex_);
  RemoveExpiredStatesLocked(now);
  auto it = pending_.find(state);
  if (it == pending_.end()) { return std::nullopt; }
  OidcPendingAuth pending_auth = it->second;
  pending_.erase(it);
  return pending_auth;
}

std::string OidcPendingAuthStore::GenerateOpaqueToken() const
{
  std::array<unsigned char, 32> bytes{};
  if (RAND_bytes(bytes.data(), bytes.size()) != 1) {
    throw std::runtime_error("Proxy OIDC: failed to generate random state");
  }
  return Base64UrlEncode(bytes.data(), bytes.size());
}

void OidcPendingAuthStore::RemoveExpiredStatesLocked(std::time_t now)
{
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->second.expires_at <= now) {
      it = pending_.erase(it);
    } else {
      ++it;
    }
  }
}

std::string BuildOidcAuthorizationUrl(const OidcAuthProvider& provider,
                                      const OidcPendingAuth& pending_auth)
{
  const std::string separator
      = provider.authorization_endpoint.find('?') == std::string::npos ? "?"
                                                                       : "&";
  std::ostringstream output;
  output << provider.authorization_endpoint << separator << "response_type=code"
         << "&client_id=" << PercentEncode(provider.client_id)
         << "&redirect_uri=" << PercentEncode(provider.redirect_uri)
         << "&scope=" << PercentEncode(JoinScopes(provider.scopes))
         << "&state=" << PercentEncode(pending_auth.state)
         << "&nonce=" << PercentEncode(pending_auth.nonce)
         << "&code_challenge_method=S256"
         << "&code_challenge="
         << PercentEncode(Sha256Base64Url(pending_auth.code_verifier));
  return output.str();
}

const OidcHttpClient& DefaultOidcHttpClient()
{
  static const DefaultOidcHttpClientImpl client;
  return client;
}

AuthResult CompleteOidcAuthorization(const OidcAuthProvider& provider,
                                     const OidcPendingAuth& pending_auth,
                                     const std::string& code,
                                     const OidcHttpClient& http_client,
                                     OidcClock clock)
{
  const auto token_set = ExchangeAuthorizationCode(provider, pending_auth, code,
                                                   http_client, clock);
  AuthResult result;
  result.identity = BuildIdentityFromIdToken(
      provider, pending_auth, token_set.id_token, http_client, clock);
  result.expires_at = token_set.expires_at;
  return result;
}
