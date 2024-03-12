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
#include <openssl/aes.h>
#include "lib/crypto_wrap.h"
#include "include/allow_deprecated.h"

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @plain: plaintext key to be wrapped, n * 64 bit
 * @cipher: wrapped key, (n + 1) * 64 bit
 */
void OldAesWrap(const uint8_t* kek,
                int n,
                const uint8_t* plain,
                uint8_t* cipher)
{
  uint8_t *a, *r, b[16];
  int i, j;
  AES_KEY key;

  a = cipher;
  r = cipher + 8;

  // 1) Initialize variables.
  memset(a, 0xa6, 8);
  memcpy(r, plain, 8 * n);

  ALLOW_DEPRECATED(AES_set_encrypt_key(kek, 128, &key));

  /* 2) Calculate intermediate values.
   * For j = 0 to 5
   *     For i=1 to n
   *      B = AES(K, A | R[i])
   *      A = MSB(64, B) ^ t where t = (n*j)+i
   *      R[i] = LSB(64, B) */
  for (j = 0; j <= 5; j++) {
    r = cipher + 8;
    for (i = 1; i <= n; i++) {
      memcpy(b, a, 8);
      memcpy(b + 8, r, 8);
      ALLOW_DEPRECATED(AES_encrypt(b, b, &key));
      memcpy(a, b, 8);
      a[7] ^= n * j + i;
      memcpy(r, b + 8, 8);
      r += 8;
    }
  }

  /* 3) Output the results.
   *
   * These are already in @cipher due to the location of temporary
   * variables.
   */
}

/*
 * @kek: key encryption key (KEK)
 * @n: length of the wrapped key in 64-bit units; e.g., 2 = 128-bit = 16 bytes
 * @cipher: wrapped key to be unwrapped, (n + 1) * 64 bit
 * @plain: plaintext key, n * 64 bit
 */
int OldAesUnwrap(const uint8_t* kek,
                 int n,
                 const uint8_t* cipher,
                 uint8_t* plain)
{
  uint8_t a[8], *r, b[16];
  int i, j;
  AES_KEY key;

  // 1) Initialize variables.
  memcpy(a, cipher, 8);
  r = plain;
  memcpy(r, cipher + 8, 8 * n);

  ALLOW_DEPRECATED(AES_set_decrypt_key(kek, 128, &key));

  /* 2) Compute intermediate values.
   * For j = 5 to 0
   *     For i = n to 1
   *      B = AES-1(K, (A ^ t) | R[i]) where t = n*j+i
   *      A = MSB(64, B)
   *      R[i] = LSB(64, B) */
  for (j = 5; j >= 0; j--) {
    r = plain + (n - 1) * 8;
    for (i = n; i >= 1; i--) {
      memcpy(b, a, 8);
      b[7] ^= n * j + i;

      memcpy(b + 8, r, 8);
      ALLOW_DEPRECATED(AES_decrypt(b, b, &key));
      memcpy(a, b, 8);
      memcpy(r, b + 8, 8);
      r -= 8;
    }
  }

  /* 3) Output results.
   *
   * These are already in @plain due to the location of temporary
   * variables. Just verify that the IV matches with the expected value. */
  for (i = 0; i < 8; i++) {
    if (a[i] != 0xa6) { return -1; }
  }

  return 0;
}

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

std::vector<uint8_t> MakePayload()
{
  std::vector<uint8_t> payload;

  // needs to be divisible by 8
  payload.resize(64);

  if (RAND_bytes(payload.data(), payload.size()) != 1) {
    OpenSSLPostErrors("RAND_bytes()");
    return {};
  }

  std::printf("--- BEGIN PAYLOAD ---\n");
  for (std::size_t i = 0; i < payload.size(); i += 8) {
    std::printf("%02x%02x%02x%02x%02x%02x%02x%02x\n", payload[i + 0],
                payload[i + 1], payload[i + 2], payload[i + 3], payload[i + 4],
                payload[i + 5], payload[i + 6], payload[i + 7]);
  }
  std::printf("--- END PAYLOAD ---\n");

  return payload;
}

std::vector<uint8_t> MakeWrappedPayload(uint8_t* key)
{
  std::vector<uint8_t> payload = MakePayload();

  if (payload.size() == 0 || payload.size() % 8 != 0) { return {}; }

  std::vector<uint8_t> wrapped;
  wrapped.resize(payload.size() + 8);

  if (auto error
      = AesWrap(key, payload.size() / 8, payload.data(), wrapped.data())) {
    std::fprintf(stderr, "MakeWrappedPayload error: %s\n", error->c_str());
    return {};
  }

  return wrapped;
}

/* A 256 bit key */
uint8_t key[]
    = {0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
       0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31,
       0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x31};

TEST(Aes, old)
{
  auto payload = MakePayload();

  ASSERT_NE(payload.size(), 0);
  ASSERT_EQ(payload.size() % 8, 0);

  auto our_cipher = std::make_unique<uint8_t[]>(payload.size() + 8);
  auto our_decipher = std::make_unique<uint8_t[]>(payload.size());
  OldAesWrap(key, payload.size() / 8, payload.data(), our_cipher.get());
  ASSERT_NE(OldAesUnwrap(key, payload.size() / 8, our_cipher.get(),
                         our_decipher.get()),
            -1);

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
  {
    auto result = AesWrap(key, payload.size() / 8, payload.data(),
                          openssl_cipher.get());
    ASSERT_FALSE(!!result) << result->c_str();
  }
  {
    auto result = AesUnwrap(key, payload.size() / 8, openssl_cipher.get(),
                            openssl_decipher.get());
    ASSERT_FALSE(!!result) << result->c_str();
  }

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

  {
    auto result = AesWrap(key, payload.size() / 8, payload.data(),
                          openssl_cipher.get());
    ASSERT_FALSE(!!result) << result->c_str();
  }
  OldAesWrap(key, payload.size() / 8, payload.data(), our_cipher.get());

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

  {
    auto result = AesUnwrap(key, payload_size / 8, wrapped.data(),
                            openssl_decipher.get());
    ASSERT_FALSE(!!result) << result->c_str();
  }
  ASSERT_EQ(
      OldAesUnwrap(key, payload_size / 8, wrapped.data(), our_decipher.get()),
      0);

  for (std::size_t i = 0; i < payload_size; ++i) {
    EXPECT_EQ((unsigned)openssl_decipher[i], (unsigned)our_decipher[i])
        << "at index " << i << ".";
  }
}

TEST(Aes, test_val)
{
  uint8_t test_kek[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  };

  uint8_t test_key[] = {
      0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
      0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
  };

  uint8_t test_wrapped[] = {
      0x1F, 0xA6, 0x8B, 0x0A, 0x81, 0x12, 0xB4, 0x47, 0xAE, 0xF3, 0x4B, 0xD8,
      0xFB, 0x5A, 0x7B, 0x82, 0x9D, 0x3E, 0x86, 0x23, 0x71, 0xD2, 0xCF, 0xE5,
  };

  uint8_t out[sizeof(test_wrapped)];
  uint8_t out2[sizeof(test_key)];

  auto payload_size = sizeof(test_key);

  OldAesWrap(test_kek, payload_size / 8, test_key, out);

  for (std::size_t i = 0; i < (payload_size + 8); ++i) {
    EXPECT_EQ((unsigned)test_wrapped[i], (unsigned)out[i])
        << "at index " << i << ".";
  }

  OldAesUnwrap(test_kek, payload_size / 8, out, out2);

  for (std::size_t i = 0; i < payload_size; ++i) {
    EXPECT_EQ((unsigned)test_key[i], (unsigned)out2[i])
        << "at index " << i << ".";
  }
}
