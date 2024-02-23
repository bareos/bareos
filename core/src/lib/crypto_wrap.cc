/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "lib/crypto_wrap.h"
#include "include/allow_deprecated.h"

#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <memory>

namespace {
/* A 64 bit IV, chosen according to spec:
 * https://datatracker.ietf.org/doc/html/rfc3394.html#section-2.2.3.1 */
constexpr const unsigned char iv[]
    = {0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6};

std::string OpenSSLErrors(const char* errstring)
{
  std::string res{errstring};
  res += ": ";
  unsigned long sslerr;
  char buf[512];

  bool first = true;
  /* Pop errors off of the per-thread queue */
  while ((sslerr = ERR_get_error()) != 0) {
    /* Acquire the human readable string */
    ERR_error_string_n(sslerr, buf, sizeof(buf));
    res += buf;
    if (first) {
      first = false;
    } else {
      res += ", ";
    }
  }
  return res;
}

struct evp_ctx_free {
  void operator()(EVP_CIPHER_CTX* ctx) const { EVP_CIPHER_CTX_free(ctx); }
};

using evp_ptr = std::unique_ptr<EVP_CIPHER_CTX, evp_ctx_free>;
}  // namespace

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @plain: plaintext key to be wrapped, n * 64 bit
 * @cipher: wrapped key, (n + 1) * 64 bit
 */
std::optional<std::string> AesWrap(const uint8_t* kek,
                                   int n,
                                   const uint8_t* plain,
                                   uint8_t* cipher)
{
  evp_ptr ctx{EVP_CIPHER_CTX_new()};

  if (!ctx) { return OpenSSLErrors("EVP_CIPHER_CTX_new()"); }

  EVP_CIPHER_CTX_set_flags(ctx.get(), EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

  if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_wrap(), NULL, kek, iv) != 1) {
    return OpenSSLErrors("EVP_EncryptInit_ex()");
  }

  int total_len = 0;
  int len;
  if (EVP_EncryptUpdate(ctx.get(), cipher, &len, plain, n * 8) != 1) {
    return OpenSSLErrors("EVP_EncryptUpdate()");
  }
  total_len += len;

  if (EVP_EncryptFinal(ctx.get(), cipher + len, &len) != 1) {
    return OpenSSLErrors("EVP_EncryptFinal()");
  }
  total_len += len;

  ASSERT(total_len <= (n + 1) * 8);

  return std::nullopt;
}

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @cipher: wrapped key to be unwrapped, (n + 1) * 64 bit
 * @plain: plaintext key, n * 64 bit
 */
std::optional<std::string> AesUnwrap(const uint8_t* kek,
                                     int n,
                                     const uint8_t* cipher,
                                     uint8_t* plain)
{
  evp_ptr ctx{EVP_CIPHER_CTX_new()};

  if (!ctx) { return OpenSSLErrors("EVP_CIPHER_CTX_new()"); }

  EVP_CIPHER_CTX_set_flags(ctx.get(), EVP_CIPHER_CTX_FLAG_WRAP_ALLOW);

  if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_128_wrap(), NULL, kek, iv) != 1) {
    return OpenSSLErrors("EVP_EncryptInit_ex()");
  }

  int total_len = 0;
  int len;
  if (EVP_DecryptUpdate(ctx.get(), plain, &len, cipher, (n + 1) * 8) != 1) {
    return OpenSSLErrors("EVP_EncryptUpdate()");
  }
  total_len += len;

  if (EVP_DecryptFinal_ex(ctx.get(), plain + len, &len) != 1) {
    return OpenSSLErrors("EVP_EncryptFinal()");
  }
  total_len += len;

  ASSERT(total_len <= (n * 8));

  return std::nullopt;
}
