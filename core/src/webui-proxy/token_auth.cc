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
#include "token_auth.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

#include <openssl/crypto.h>
#include <openssl/sha.h>

namespace {

std::string ToHex(const unsigned char* data, size_t size)
{
  static constexpr char hex[] = "0123456789abcdef";
  std::string encoded(size * 2, '\0');
  for (size_t i = 0; i < size; ++i) {
    encoded[i * 2] = hex[(data[i] >> 4) & 0x0f];
    encoded[i * 2 + 1] = hex[data[i] & 0x0f];
  }
  return encoded;
}

std::optional<unsigned char> HexValue(char ch)
{
  if (ch >= '0' && ch <= '9') { return static_cast<unsigned char>(ch - '0'); }
  ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  if (ch >= 'a' && ch <= 'f') {
    return static_cast<unsigned char>(10 + (ch - 'a'));
  }
  return std::nullopt;
}

std::optional<std::vector<unsigned char>> DecodeHex(const std::string& hex)
{
  if (hex.size() % 2 != 0) { return std::nullopt; }

  std::vector<unsigned char> decoded;
  decoded.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    const auto upper = HexValue(hex[i]);
    const auto lower = HexValue(hex[i + 1]);
    if (!upper || !lower) { return std::nullopt; }
    decoded.push_back(static_cast<unsigned char>((*upper << 4) | *lower));
  }
  return decoded;
}

std::optional<std::array<std::string, 2>> ParseTokenHash(
    const std::string& token_hash)
{
  std::array<std::string, 2> parts{};
  std::istringstream input(token_hash);
  std::string part;
  size_t index = 0;
  while (std::getline(input, part, '$')) {
    if (index >= parts.size()) { return std::nullopt; }
    parts[index++] = part;
  }
  if (index != parts.size()) { return std::nullopt; }
  return parts;
}

}  // namespace

std::string CreateTokenAuthHash(const std::string& token)
{
  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(),
         digest.data());
  return "sha256$" + ToHex(digest.data(), digest.size());
}

bool VerifyTokenAuthToken(const std::string& token, const std::string& token_hash)
{
  const auto parts = ParseTokenHash(token_hash);
  if (!parts || (*parts)[0] != "sha256") { return false; }

  const auto expected = DecodeHex((*parts)[1]);
  if (!expected || expected->size() != SHA256_DIGEST_LENGTH) { return false; }

  std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
  SHA256(reinterpret_cast<const unsigned char*>(token.data()), token.size(),
         digest.data());

  return CRYPTO_memcmp(digest.data(), expected->data(), digest.size()) == 0;
}

std::optional<AuthResult> AuthenticateToken(const std::vector<TokenAuthEntry>& tokens,
                                            const std::string& access_token,
                                            std::time_t now)
{
  auto it = std::find_if(tokens.begin(), tokens.end(), [&](const auto& token) {
    return VerifyTokenAuthToken(access_token, token.token_hash);
  });
  if (it == tokens.end()) { return std::nullopt; }
  if (it->expires_at && *it->expires_at <= now) { return std::nullopt; }

  AuthResult result;
  result.identity.provider = "token";
  result.identity.subject = !it->subject.empty()
                                ? it->subject
                                : (!it->username.empty() ? it->username : it->id);
  result.identity.username = it->username;
  result.identity.email = it->email;
  result.identity.groups = it->groups;
  result.identity.roles = it->roles;
  result.expires_at = it->expires_at;
  return result;
}
