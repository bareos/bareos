/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/crypto.h"
#include "lib/crypto_openssl.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// Test fixture for crypto operations
class CryptoTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_EQ(InitCrypto(), 0); }

  void TearDown() override { ASSERT_EQ(CleanupCrypto(), 0); }
};

// Test InitCrypto and CleanupCrypto
TEST(crypto, InitCrypto_CleanupCrypto)
{
  EXPECT_EQ(InitCrypto(), 0);
  EXPECT_EQ(CleanupCrypto(), 0);
}

// Test InitCrypto and CleanupCrypto multiple times
TEST(crypto, InitCrypto_CleanupCrypto_multiple_cycles)
{
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(InitCrypto(), 0);
    EXPECT_EQ(CleanupCrypto(), 0);
  }
}

// Test OpenSSL initialization
TEST(crypto, OpensslInitThreads)
{
  ASSERT_EQ(InitCrypto(), 0);
  EXPECT_EQ(OpensslInitThreads(), 0);
  ASSERT_EQ(CleanupCrypto(), 0);
}

// Test crypto_keypair_new and CryptoKeypairFree
TEST_F(CryptoTest, crypto_keypair_new_and_free)
{
  X509_KEYPAIR* keypair = crypto_keypair_new();
  ASSERT_NE(keypair, nullptr);
  CryptoKeypairFree(keypair);
}

// Test crypto_keypair_new creates independent instances
TEST_F(CryptoTest, crypto_keypair_new_multiple_instances)
{
  X509_KEYPAIR* kp1 = crypto_keypair_new();
  X509_KEYPAIR* kp2 = crypto_keypair_new();

  ASSERT_NE(kp1, nullptr);
  ASSERT_NE(kp2, nullptr);
  EXPECT_NE(kp1, kp2);

  CryptoKeypairFree(kp1);
  CryptoKeypairFree(kp2);
}

// Test crypto_keypair_dup
TEST_F(CryptoTest, crypto_keypair_dup)
{
  X509_KEYPAIR* original = crypto_keypair_new();
  ASSERT_NE(original, nullptr);

  X509_KEYPAIR* duplicate = crypto_keypair_dup(original);
  ASSERT_NE(duplicate, nullptr);
  EXPECT_NE(original, duplicate);

  CryptoKeypairFree(original);
  CryptoKeypairFree(duplicate);
}

// Test OpensslDigestNew with MD5
TEST_F(CryptoTest, OpensslDigestNew_MD5)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_MD5);
  ASSERT_NE(digest, nullptr);
  CryptoDigestFree(digest);
}

// Test OpensslDigestNew with SHA1
TEST_F(CryptoTest, OpensslDigestNew_SHA1)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA1);
  ASSERT_NE(digest, nullptr);
  CryptoDigestFree(digest);
}

// Test OpensslDigestNew with SHA256
TEST_F(CryptoTest, OpensslDigestNew_SHA256)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA256);
  ASSERT_NE(digest, nullptr);
  CryptoDigestFree(digest);
}

// Test OpensslDigestNew with SHA512
TEST_F(CryptoTest, OpensslDigestNew_SHA512)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA512);
  ASSERT_NE(digest, nullptr);
  CryptoDigestFree(digest);
}

// Test digest update with simple data
TEST_F(CryptoTest, CryptoDigestUpdate_simple)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA256);
  ASSERT_NE(digest, nullptr);

  const uint8_t data[] = R"(Hello, World!)";
  EXPECT_TRUE(CryptoDigestUpdate(digest, data, sizeof(data) - 1));

  CryptoDigestFree(digest);
}

// Test digest finalize
TEST_F(CryptoTest, CryptoDigestFinalize_SHA256)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA256);
  ASSERT_NE(digest, nullptr);

  const uint8_t data[] = R"(test data)";
  EXPECT_TRUE(CryptoDigestUpdate(digest, data, sizeof(data) - 1));

  uint8_t result[CRYPTO_DIGEST_MAX_SIZE];
  uint32_t result_len = sizeof(result);
  EXPECT_TRUE(CryptoDigestFinalize(digest, result, &result_len));
  EXPECT_EQ(result_len, CRYPTO_DIGEST_SHA256_SIZE);

  CryptoDigestFree(digest);
}

// Test digest multiple updates
TEST_F(CryptoTest, CryptoDigestUpdate_multiple)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA256);
  ASSERT_NE(digest, nullptr);

  const uint8_t data1[] = R"(Hello, )";
  const uint8_t data2[] = R"(World!)";

  EXPECT_TRUE(CryptoDigestUpdate(digest, data1, sizeof(data1) - 1));
  EXPECT_TRUE(CryptoDigestUpdate(digest, data2, sizeof(data2) - 1));

  uint8_t result[CRYPTO_DIGEST_MAX_SIZE];
  uint32_t result_len = sizeof(result);
  EXPECT_TRUE(CryptoDigestFinalize(digest, result, &result_len));
  EXPECT_GT(result_len, 0);

  CryptoDigestFree(digest);
}

// Test crypto_sign_new and CryptoSignFree
TEST_F(CryptoTest, crypto_sign_new_and_free)
{
  SIGNATURE* sig = crypto_sign_new(nullptr);
  ASSERT_NE(sig, nullptr);
  CryptoSignFree(sig);
}

// Test crypto_sign_new creates independent instances
TEST_F(CryptoTest, crypto_sign_new_multiple_instances)
{
  SIGNATURE* sig1 = crypto_sign_new(nullptr);
  SIGNATURE* sig2 = crypto_sign_new(nullptr);

  ASSERT_NE(sig1, nullptr);
  ASSERT_NE(sig2, nullptr);
  EXPECT_NE(sig1, sig2);

  CryptoSignFree(sig1);
  CryptoSignFree(sig2);
}

// Test crypto_session_new with AES_128_CBC
TEST_F(CryptoTest, crypto_session_new_AES_128_CBC)
{
  CRYPTO_SESSION* session = crypto_session_new(CRYPTO_CIPHER_AES_128_CBC, nullptr);
  ASSERT_NE(session, nullptr);
  CryptoSessionFree(session);
}

// Test crypto_session_new with AES_256_CBC
TEST_F(CryptoTest, crypto_session_new_AES_256_CBC)
{
  CRYPTO_SESSION* session = crypto_session_new(CRYPTO_CIPHER_AES_256_CBC, nullptr);
  ASSERT_NE(session, nullptr);
  CryptoSessionFree(session);
}

// Test crypto_session_new with multiple instances
TEST_F(CryptoTest, crypto_session_new_multiple_instances)
{
  CRYPTO_SESSION* session1 = crypto_session_new(CRYPTO_CIPHER_AES_128_CBC, nullptr);
  CRYPTO_SESSION* session2 = crypto_session_new(CRYPTO_CIPHER_AES_256_CBC, nullptr);

  ASSERT_NE(session1, nullptr);
  ASSERT_NE(session2, nullptr);
  EXPECT_NE(session1, session2);

  CryptoSessionFree(session1);
  CryptoSessionFree(session2);
}

// Test digest with empty data
TEST_F(CryptoTest, CryptoDigestUpdate_empty)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA256);
  ASSERT_NE(digest, nullptr);

  const uint8_t* data = nullptr;
  EXPECT_TRUE(CryptoDigestUpdate(digest, data, 0));

  uint8_t result[CRYPTO_DIGEST_MAX_SIZE];
  uint32_t result_len = sizeof(result);
  EXPECT_TRUE(CryptoDigestFinalize(digest, result, &result_len));
  EXPECT_GT(result_len, 0);

  CryptoDigestFree(digest);
}

// Test multiple digest types
TEST_F(CryptoTest, OpensslDigestNew_all_types)
{
  constexpr std::array digest_types = {CRYPTO_DIGEST_MD5, CRYPTO_DIGEST_SHA1,
                                       CRYPTO_DIGEST_SHA256, CRYPTO_DIGEST_SHA512};

  for (crypto_digest_t type : digest_types) {
    DIGEST* digest = OpensslDigestNew(nullptr, type);
    ASSERT_NE(digest, nullptr);

    const uint8_t data[] = R"(test)";
    EXPECT_TRUE(CryptoDigestUpdate(digest, data, sizeof(data) - 1));

    uint8_t result[CRYPTO_DIGEST_MAX_SIZE];
    uint32_t result_len = sizeof(result);
    EXPECT_TRUE(CryptoDigestFinalize(digest, result, &result_len));
    EXPECT_GT(result_len, 0);

    CryptoDigestFree(digest);
  }
}

// Test digest lifecycle
TEST_F(CryptoTest, digest_lifecycle)
{
  for (int i = 0; i < 5; ++i) {
    DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA256);
    ASSERT_NE(digest, nullptr);

    const uint8_t data[] = R"(lifecycle test)";
    EXPECT_TRUE(CryptoDigestUpdate(digest, data, sizeof(data) - 1));

    uint8_t result[CRYPTO_DIGEST_MAX_SIZE];
    uint32_t result_len = sizeof(result);
    EXPECT_TRUE(CryptoDigestFinalize(digest, result, &result_len));

    CryptoDigestFree(digest);
  }
}

// Test large data digesting
TEST_F(CryptoTest, CryptoDigestUpdate_large_data)
{
  DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA256);
  ASSERT_NE(digest, nullptr);

  // Create large data buffer (1MB)
  constexpr size_t large_size = 1024 * 1024;
  std::vector<uint8_t> large_data(large_size, 0xAB);

  EXPECT_TRUE(CryptoDigestUpdate(digest, large_data.data(), large_data.size()));

  uint8_t result[CRYPTO_DIGEST_MAX_SIZE];
  uint32_t result_len = sizeof(result);
  EXPECT_TRUE(CryptoDigestFinalize(digest, result, &result_len));
  EXPECT_EQ(result_len, CRYPTO_DIGEST_SHA256_SIZE);

  CryptoDigestFree(digest);
}

// Test signature creation lifecycle
TEST_F(CryptoTest, signature_lifecycle)
{
  for (int i = 0; i < 3; ++i) {
    SIGNATURE* sig = crypto_sign_new(nullptr);
    ASSERT_NE(sig, nullptr);
    CryptoSignFree(sig);
  }
}

// Test CryptoKeypairHasKey
TEST_F(CryptoTest, CryptoKeypairHasKey_nonexistent)
{
  bool has_key = CryptoKeypairHasKey(R"(/nonexistent/path/to/key.pem)");
  EXPECT_FALSE(has_key);
}

// Test LogSSLError doesn't crash
TEST_F(CryptoTest, LogSSLError_various_errors)
{
  // Test with various error codes
  LogSSLError(0);
  LogSSLError(1);
  LogSSLError(-1);
  LogSSLError(100);
}

// Test digest size constants
TEST(crypto, digest_size_constants)
{
  EXPECT_EQ(CRYPTO_DIGEST_MD5_SIZE, 16);
  EXPECT_EQ(CRYPTO_DIGEST_SHA1_SIZE, 20);
  EXPECT_EQ(CRYPTO_DIGEST_SHA256_SIZE, 32);
  EXPECT_EQ(CRYPTO_DIGEST_SHA512_SIZE, 64);
  EXPECT_EQ(CRYPTO_DIGEST_XXH128_SIZE, 16);
  EXPECT_EQ(CRYPTO_DIGEST_MAX_SIZE, 64);
}

// Test cipher block size constant
TEST(crypto, cipher_block_size_constant)
{
  EXPECT_GT(CRYPTO_CIPHER_MAX_BLOCK_SIZE, 0);
  EXPECT_LE(CRYPTO_CIPHER_MAX_BLOCK_SIZE, 32);
}

// Test keypair creation and duplication chain
TEST_F(CryptoTest, keypair_duplication_chain)
{
  X509_KEYPAIR* kp1 = crypto_keypair_new();
  ASSERT_NE(kp1, nullptr);

  X509_KEYPAIR* kp2 = crypto_keypair_dup(kp1);
  ASSERT_NE(kp2, nullptr);

  X509_KEYPAIR* kp3 = crypto_keypair_dup(kp2);
  ASSERT_NE(kp3, nullptr);

  EXPECT_NE(kp1, kp2);
  EXPECT_NE(kp2, kp3);
  EXPECT_NE(kp1, kp3);

  CryptoKeypairFree(kp1);
  CryptoKeypairFree(kp2);
  CryptoKeypairFree(kp3);
}

// Test CryptoDigestFree with nullptr doesn't crash
TEST_F(CryptoTest, CryptoDigestFree_nullptr)
{
  CryptoDigestFree(nullptr);
}

// Test CryptoSessionFree with nullptr doesn't crash
TEST_F(CryptoTest, CryptoSessionFree_nullptr)
{
  CryptoSessionFree(nullptr);
}

// Test CryptoKeypairFree with nullptr doesn't crash
TEST_F(CryptoTest, CryptoKeypairFree_nullptr)
{
  CryptoKeypairFree(nullptr);
}

// Test CryptoSignFree with nullptr doesn't crash
TEST_F(CryptoTest, CryptoSignFree_nullptr)
{
  CryptoSignFree(nullptr);
}

// Test digest with various data sizes
TEST_F(CryptoTest, CryptoDigestUpdate_various_sizes)
{
  constexpr std::array sizes = {1, 10, 64, 256, 1024, 4096};

  for (size_t size : sizes) {
    DIGEST* digest = OpensslDigestNew(nullptr, CRYPTO_DIGEST_SHA256);
    ASSERT_NE(digest, nullptr);

    std::vector<uint8_t> data(size, 0x42);
    EXPECT_TRUE(CryptoDigestUpdate(digest, data.data(), data.size()));

    uint8_t result[CRYPTO_DIGEST_MAX_SIZE];
    uint32_t result_len = sizeof(result);
    EXPECT_TRUE(CryptoDigestFinalize(digest, result, &result_len));
    EXPECT_EQ(result_len, CRYPTO_DIGEST_SHA256_SIZE);

    CryptoDigestFree(digest);
  }
}

// Test session lifecycle
TEST_F(CryptoTest, session_lifecycle)
{
  for (int i = 0; i < 5; ++i) {
    CRYPTO_SESSION* session = crypto_session_new(CRYPTO_CIPHER_AES_128_CBC, nullptr);
    ASSERT_NE(session, nullptr);
    CryptoSessionFree(session);
  }
}
