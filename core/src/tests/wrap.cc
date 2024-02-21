/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include <openssl/evp.h>
#include <openssl/err.h>
#include <cstdio>
#include "lib/crypto_wrap.h"

void OpenSSLPostErrors(const char* errstring)
{
  char buf[512];
  unsigned long sslerr;

  /* Pop errors off of the per-thread queue */
  while ((sslerr = ERR_get_error()) != 0) {
    /* Acquire the human readable string */
    ERR_error_string_n(sslerr, buf, sizeof(buf));
    std::fprintf(stderr, "%s: ERR=%s\n", errstring, buf);
  }
}

constexpr int cipher_size = 256;

/* A 64 bit IV */
unsigned char iv[] = {0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6};

bool OpenSSLAesWrap(const uint8_t* kek,
                    int n,
                    const uint8_t* plain,
                    uint8_t* cipher)
{
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

  if (ctx == nullptr) {
    OpenSSLPostErrors("EVP_CIPHER_CTX_new()");
    return false;
  }

  if (EVP_EncryptInit_ex(ctx, EVP_aes_128_wrap(), NULL, kek, iv) != 1) {
    OpenSSLPostErrors("EVP_EncryptInit_ex()");
    return false;
  }

  int total_len = 0;
  int len;
  if (EVP_EncryptUpdate(ctx, cipher, &len, plain, n * 8) != 1) {
    OpenSSLPostErrors("EVP_EncryptUpdate()");
    return false;
  }
  total_len += len;

  if (EVP_EncryptFinal(ctx, cipher + len, &len) != 1) {
    OpenSSLPostErrors("EVP_EncryptFinal()");
    return false;
  }
  total_len += len;

  if (total_len >= cipher_size) {
    fprintf(stderr,
            "Written to much data to cipher (%d written vs %d capacity)\n",
            total_len, cipher_size);
    return false;
  }

  EVP_CIPHER_CTX_free(ctx);

  return true;
}

bool OpenSSLAesUnwrap(const uint8_t* kek,
                      int n,
                      const uint8_t* cipher,
                      uint8_t* plain)
{
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

  if (ctx == nullptr) {
    OpenSSLPostErrors("EVP_CIPHER_CTX_new()");
    return false;
  }

  if (EVP_DecryptInit_ex(ctx, EVP_aes_128_wrap(), NULL, kek, iv) != 1) {
    OpenSSLPostErrors("EVP_EncryptInit_ex()");
    return false;
  }

  int total_len = 0;
  int len;
  if (EVP_DecryptUpdate(ctx, plain, &len, cipher, (n + 1) * 8) != 1) {
    OpenSSLPostErrors("EVP_EncryptUpdate()");
    return false;
  }
  total_len += len;

  if (EVP_DecryptFinal_ex(ctx, plain + len, &len) != 1) {
    OpenSSLPostErrors("EVP_EncryptFinal()");
    return false;
  }
  total_len += len;

  if (total_len >= cipher_size) {
    fprintf(stderr,
            "Written to much data to cipher (%d written vs %d capacity)\n",
            total_len, cipher_size);
    return false;
  }

  EVP_CIPHER_CTX_free(ctx);

  return true;
}

/* A 256 bit key */
uint8_t key[]
    = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
       0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
       0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31};

/* Message to be encrypted */
const uint8_t plaintext[] = "The quick brown fox jumps over the lazy";

static_assert((sizeof(plaintext) / 8) * 8 == sizeof(plaintext));
static_assert(sizeof(plaintext) + 8 <= cipher_size);

TEST(Aes, our)
{
  uint8_t our_cipher[cipher_size] = {};
  uint8_t our_plain[cipher_size] = {};
  AesWrap(key, sizeof(plaintext) / 8, plaintext, our_cipher);
  AesUnwrap(key, sizeof(plaintext) / 8, our_cipher, our_plain);

  for (int i = 0; i < (int)sizeof(plaintext); ++i) {
    EXPECT_EQ((unsigned)our_plain[i], (unsigned)plaintext[i])
        << "at index " << i << ".";
  }
}

TEST(Aes, openssl)
{
  uint8_t openssl_cipher[cipher_size] = {};
  uint8_t openssl_plain[cipher_size] = {};
  ASSERT_TRUE(
      OpenSSLAesWrap(key, sizeof(plaintext) / 8, plaintext, openssl_cipher));
  ASSERT_TRUE(OpenSSLAesUnwrap(key, sizeof(plaintext) / 8, openssl_cipher,
                               openssl_plain));

  for (int i = 0; i < (int)sizeof(plaintext); ++i) {
    EXPECT_EQ((unsigned)openssl_plain[i], (unsigned)plaintext[i])
        << "at index " << i << ".";
  }
}

TEST(Aes, wrap)
{
  uint8_t openssl_cipher[cipher_size] = {};
  uint8_t our_cipher[cipher_size] = {};

  ASSERT_TRUE(
      OpenSSLAesWrap(key, sizeof(plaintext) / 8, plaintext, openssl_cipher));
  AesWrap(key, sizeof(plaintext) / 8, plaintext, our_cipher);

  for (int i = 0; i < cipher_size; ++i) {
    EXPECT_EQ((unsigned)openssl_cipher[i], (unsigned)our_cipher[i])
        << "at index " << i << ".";
  }
}

TEST(Aes, unwrap)
{
  uint8_t cipher[cipher_size] = {};
  unsigned char openssl_plain[128] = {};
  unsigned char our_plain[128] = {};

  static_assert(sizeof(our_plain) >= sizeof(plaintext));

  ASSERT_TRUE(OpenSSLAesWrap(key, sizeof(plaintext) / 8, plaintext, cipher));

  ASSERT_TRUE(
      OpenSSLAesUnwrap(key, sizeof(plaintext) / 8, cipher, openssl_plain));
  ASSERT_EQ(AesUnwrap(key, sizeof(plaintext) / 8, cipher, our_plain), 0);

  for (int i = 0; i < 128; ++i) {
    EXPECT_EQ((unsigned)openssl_plain[i], (unsigned)our_plain[i])
        << "at index " << i << ".";
  }
}
