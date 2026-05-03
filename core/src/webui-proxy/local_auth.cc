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
#include "local_auth.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <stdexcept>

#include <openssl/crypto.h>
#include <openssl/evp.h>

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

std::optional<std::array<std::string, 4>> ParsePasswordHash(
    const std::string& password_hash)
{
  std::array<std::string, 4> parts{};
  std::istringstream input(password_hash);
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

std::string CreateLocalAuthPasswordHash(const std::string& password,
                                        const std::string& salt_hex,
                                        int iterations)
{
  const auto salt = DecodeHex(salt_hex);
  if (!salt || salt->empty()) {
    throw std::runtime_error("Proxy local auth: invalid salt hex");
  }
  if (iterations <= 0) {
    throw std::runtime_error("Proxy local auth: iterations must be positive");
  }

  std::array<unsigned char, 32> digest{};
  if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                        salt->data(), static_cast<int>(salt->size()),
                        iterations, EVP_sha256(),
                        static_cast<int>(digest.size()), digest.data())
      != 1) {
    throw std::runtime_error("Proxy local auth: password hashing failed");
  }

  return "pbkdf2-sha256$" + std::to_string(iterations) + "$" + salt_hex + "$"
         + ToHex(digest.data(), digest.size());
}

bool VerifyLocalAuthPassword(const std::string& password,
                             const std::string& password_hash)
{
  const auto parts = ParsePasswordHash(password_hash);
  if (!parts || (*parts)[0] != "pbkdf2-sha256") { return false; }

  int iterations = 0;
  try {
    size_t pos = 0;
    iterations = std::stoi((*parts)[1], &pos, 10);
    if (pos != (*parts)[1].size()) { return false; }
  } catch (...) {
    return false;
  }

  const auto salt = DecodeHex((*parts)[2]);
  const auto expected = DecodeHex((*parts)[3]);
  if (!salt || !expected || expected->size() != 32 || iterations <= 0) {
    return false;
  }

  std::array<unsigned char, 32> digest{};
  if (PKCS5_PBKDF2_HMAC(password.c_str(), static_cast<int>(password.size()),
                        salt->data(), static_cast<int>(salt->size()),
                        iterations, EVP_sha256(),
                        static_cast<int>(digest.size()), digest.data())
      != 1) {
    return false;
  }

  return CRYPTO_memcmp(digest.data(), expected->data(), digest.size()) == 0;
}

std::optional<AuthResult> AuthenticateLocalUser(
    const std::vector<LocalAuthUser>& users,
    const std::string& username,
    const std::string& password)
{
  auto it = std::find_if(users.begin(), users.end(), [&](const auto& user) {
    return user.username == username;
  });
  if (it == users.end()) { return std::nullopt; }
  if (!VerifyLocalAuthPassword(password, it->password_hash)) {
    return std::nullopt;
  }

  AuthResult result;
  result.identity.provider = "local";
  result.identity.subject = it->subject.empty() ? it->username : it->subject;
  result.identity.username = it->username;
  result.identity.email = it->email;
  result.identity.groups = it->groups;
  result.identity.roles = it->roles;
  return result;
}
