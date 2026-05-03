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

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace {

std::string Base64UrlEncode(const unsigned char* data, size_t size)
{
  std::string encoded(((size + 2) / 3) * 4, '\0');
  const int length = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()),
                                     data, static_cast<int>(size));
  encoded.resize(length > 0 ? static_cast<size_t>(length) : 0);
  std::replace(encoded.begin(), encoded.end(), '+', '-');
  std::replace(encoded.begin(), encoded.end(), '/', '_');
  while (!encoded.empty() && encoded.back() == '=') {
    encoded.pop_back();
  }
  return encoded;
}

std::string Sha256Base64Url(const std::string& value)
{
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_len = 0;
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  EVP_DigestUpdate(ctx, value.data(), value.size());
  EVP_DigestFinal_ex(ctx, digest.data(), &digest_len);
  EVP_MD_CTX_free(ctx);
  return Base64UrlEncode(digest.data(), digest_len);
}

std::string PercentEncode(const std::string& value)
{
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size() * 3);
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.'
        || ch == '~') {
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

}  // namespace

OidcPendingAuthStore::OidcPendingAuthStore(std::chrono::seconds ttl, OidcClock clock)
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
  output << provider.authorization_endpoint << separator
         << "response_type=code"
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
