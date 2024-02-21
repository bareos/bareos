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

#include <cstdio>
#include <cinttypes>
#include <vector>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
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

  if (total_len > (n + 1) * 8) {
    fprintf(stderr,
            "Written to much data to cipher (%d written vs %d capacity)\n",
            total_len, (n + 1) * 8);
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

  if (total_len > n * 8) {
    fprintf(stderr,
            "Written to much data to plain (%d written vs %d capacity)\n",
            total_len, n * 8);
    return false;
  }

  EVP_CIPHER_CTX_free(ctx);

  return true;
}

std::vector<uint8_t> MakePayload()
{
  std::vector<uint8_t> payload;

  // needs to be divisible by 8
  payload.resize(64);

  if (RAND_bytes(payload.data(), payload.size()) != 1) {
    OpenSSLPostErrors("RAND_bytes()");
    return {};
  }

  printf("--- BEGIN PAYLOAD ---\n");
  for (std::size_t i = 0; i < payload.size(); i += 8) {
    printf("%02x%02x%02x%02x%02x%02x%02x%02x\n", payload[i + 0], payload[i + 1],
           payload[i + 2], payload[i + 3], payload[i + 4], payload[i + 5],
           payload[i + 6], payload[i + 7]);
  }
  printf("--- END PAYLOAD ---\n");

  return payload;
}

std::vector<uint8_t> MakeWrappedPayload(uint8_t* key)
{
  std::vector<uint8_t> payload = MakePayload();

  if (payload.size() == 0 || payload.size() % 8 != 0) { return {}; }

  std::vector<uint8_t> wrapped;
  wrapped.resize(payload.size() + 8);

  if (!OpenSSLAesWrap(key, payload.size() / 8, payload.data(),
                      wrapped.data())) {
    return {};
  }

  return wrapped;
}

/* A 256 bit key */
uint8_t key[]
    = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
       0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
       0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31};

TEST(Aes, our)
{
  auto payload = MakePayload();

  ASSERT_NE(payload.size(), 0);
  ASSERT_EQ(payload.size() % 8, 0);

  auto our_cipher = std::make_unique<uint8_t[]>(payload.size() + 8);
  auto our_decipher = std::make_unique<uint8_t[]>(payload.size());
  AesWrap(key, payload.size() / 8, payload.data(), our_cipher.get());
  AesUnwrap(key, payload.size() / 8, our_cipher.get(), our_decipher.get());

  for (std::size_t i = 0; i < payload.size(); ++i) {
    EXPECT_EQ((unsigned)our_decipher[i], (unsigned)payload[i])
        << "at index " << i << ".";
  }
}

TEST(Aes, openssl)
{
  auto payload = MakePayload();

  ASSERT_NE(payload.size(), 0);
  ASSERT_EQ(payload.size() % 8, 0);

  auto openssl_cipher = std::make_unique<uint8_t[]>(payload.size() + 8);
  auto openssl_decipher = std::make_unique<uint8_t[]>(payload.size());
  AesWrap(key, payload.size() / 8, payload.data(), openssl_cipher.get());
  AesUnwrap(key, payload.size() / 8, openssl_cipher.get(),
            openssl_decipher.get());

  for (std::size_t i = 0; i < payload.size(); ++i) {
    EXPECT_EQ((unsigned)openssl_decipher[i], (unsigned)payload[i])
        << "at index " << i << ".";
  }
}

TEST(Aes, wrap)
{
  auto payload = MakePayload();

  ASSERT_NE(payload.size(), 0);
  ASSERT_EQ(payload.size() % 8, 0);

  auto openssl_cipher = std::make_unique<uint8_t[]>(payload.size() + 8);
  auto our_cipher = std::make_unique<uint8_t[]>(payload.size() + 8);

  ASSERT_TRUE(OpenSSLAesWrap(key, payload.size() / 8, payload.data(),
                             openssl_cipher.get()));
  AesWrap(key, payload.size() / 8, payload.data(), our_cipher.get());

  for (std::size_t i = 0; i < payload.size() + 8; ++i) {
    EXPECT_EQ((unsigned)openssl_cipher[i], (unsigned)our_cipher[i])
        << "at index " << i << ".";
  }
}

TEST(Aes, unwrap)
{
  auto wrapped = MakeWrappedPayload(key);

  ASSERT_NE(wrapped.size(), 0);
  ASSERT_EQ(wrapped.size() % 8, 0);

  auto payload_size = wrapped.size() - 8;

  auto openssl_decipher = std::make_unique<uint8_t[]>(payload_size);
  auto our_decipher = std::make_unique<uint8_t[]>(payload_size);


  ASSERT_TRUE(OpenSSLAesUnwrap(key, payload_size / 8, wrapped.data(),
                               openssl_decipher.get()));
  ASSERT_EQ(
      AesUnwrap(key, payload_size / 8, wrapped.data(), our_decipher.get()), 0);

  for (std::size_t i = 0; i < payload_size; ++i) {
    EXPECT_EQ((unsigned)openssl_decipher[i], (unsigned)our_decipher[i])
        << "at index " << i << ".";
  }
}
