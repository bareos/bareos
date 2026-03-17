/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.
*/

#include <gtest/gtest.h>
#include "include/bareos.h"
#include "include/jcr.h"
#include "include/ch.h"
#include "lib/compression.h"
#include "include/streams.h"

class CompressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    jcr = new JobControlRecord();
  }

  void TearDown() override {
    if (jcr) {
      CleanupCompression(jcr);
      delete jcr;
      jcr = nullptr;
    }
  }

  JobControlRecord* jcr = nullptr;
};

// CompressorName tests
TEST(CompressionNameTest, CompressorName_GZIP) {
  EXPECT_EQ(CompressorName(COMPRESS_GZIP), "GZIP");
}

#ifdef HAVE_LZO
TEST(CompressionNameTest, CompressorName_LZO) {
  EXPECT_EQ(CompressorName(COMPRESS_LZO1X), "LZO");
}
#endif  // HAVE_LZO

TEST(CompressionNameTest, CompressorName_FASTLZ) {
  EXPECT_EQ(CompressorName(COMPRESS_FZFZ), "FASTLZ");
}

TEST(CompressionNameTest, CompressorName_LZ4) {
  EXPECT_EQ(CompressorName(COMPRESS_FZ4L), "LZ4");
}

TEST(CompressionNameTest, CompressorName_LZ4HC) {
  EXPECT_EQ(CompressorName(COMPRESS_FZ4H), "LZ4HC");
}

TEST(CompressionNameTest, CompressorName_Unknown) {
  EXPECT_EQ(CompressorName(999), "Unknown");
}

// RequiredCompressionOutputBufferSize tests
TEST(CompressionOutputSizeTest, GZIP_small_input) {
  std::size_t size = RequiredCompressionOutputBufferSize(COMPRESS_GZIP, 100);
  EXPECT_GT(size, 100);
}

TEST(CompressionOutputSizeTest, GZIP_large_input) {
  std::size_t size = RequiredCompressionOutputBufferSize(COMPRESS_GZIP, 1000000);
  EXPECT_GT(size, 1000000);
}

#ifdef HAVE_LZO
TEST(CompressionOutputSizeTest, LZO_small_input) {
  std::size_t size = RequiredCompressionOutputBufferSize(COMPRESS_LZO1X, 100);
  EXPECT_GT(size, 100);
}

TEST(CompressionOutputSizeTest, LZO_large_input) {
  std::size_t size = RequiredCompressionOutputBufferSize(COMPRESS_LZO1X, 100000);
  EXPECT_GT(size, 100000);
}
#endif  // HAVE_LZO

TEST(CompressionOutputSizeTest, FASTLZ_small_input) {
  std::size_t size = RequiredCompressionOutputBufferSize(COMPRESS_FZFZ, 100);
  EXPECT_GT(size, 100);
}

TEST(CompressionOutputSizeTest, LZ4_small_input) {
  std::size_t size = RequiredCompressionOutputBufferSize(COMPRESS_FZ4L, 100);
  EXPECT_GT(size, 100);
}

TEST(CompressionOutputSizeTest, LZ4HC_small_input) {
  std::size_t size = RequiredCompressionOutputBufferSize(COMPRESS_FZ4H, 100);
  EXPECT_GT(size, 100);
}

// SetupCompressionBuffers tests
TEST_F(CompressionTest, SetupCompressionBuffers_GZIP) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}

#ifdef HAVE_LZO
TEST_F(CompressionTest, SetupCompressionBuffers_LZO) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_LZO1X, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}
#endif  // HAVE_LZO

TEST_F(CompressionTest, SetupCompressionBuffers_FASTLZ) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_FZFZ, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}

TEST_F(CompressionTest, SetupCompressionBuffers_LZ4) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_FZ4L, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}

TEST_F(CompressionTest, SetupCompressionBuffers_LZ4HC) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_FZ4H, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}

TEST_F(CompressionTest, SetupCompressionBuffers_invalid_algo) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, 999, &buf_size);
  EXPECT_FALSE(result);
}

// SetupDecompressionBuffers tests
TEST_F(CompressionTest, SetupDecompressionBuffers) {
  uint32_t buf_size = 0;
  bool result = SetupDecompressionBuffers(jcr, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}

// SetupSpecificCompressionContext tests
TEST_F(CompressionTest, SetupSpecificCompressionContext_GZIP_level1) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));
  bool result = SetupSpecificCompressionContext(*jcr, COMPRESS_GZIP, 1);
  EXPECT_TRUE(result);
}

TEST_F(CompressionTest, SetupSpecificCompressionContext_GZIP_level6) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));
  bool result = SetupSpecificCompressionContext(*jcr, COMPRESS_GZIP, 6);
  EXPECT_TRUE(result);
}

TEST_F(CompressionTest, SetupSpecificCompressionContext_GZIP_level9) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));
  bool result = SetupSpecificCompressionContext(*jcr, COMPRESS_GZIP, 9);
  EXPECT_TRUE(result);
}

#ifdef HAVE_LZO
TEST_F(CompressionTest, SetupSpecificCompressionContext_LZO) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_LZO1X, &buf_size));
  bool result = SetupSpecificCompressionContext(*jcr, COMPRESS_LZO1X, 1);
  EXPECT_TRUE(result);
}
#endif  // HAVE_LZO

TEST_F(CompressionTest, SetupSpecificCompressionContext_FASTLZ) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_FZFZ, &buf_size));
  bool result = SetupSpecificCompressionContext(*jcr, COMPRESS_FZFZ, 1);
  EXPECT_TRUE(result);
}

TEST_F(CompressionTest, SetupSpecificCompressionContext_LZ4) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_FZ4L, &buf_size));
  bool result = SetupSpecificCompressionContext(*jcr, COMPRESS_FZ4L, 1);
  EXPECT_TRUE(result);
}

TEST_F(CompressionTest, SetupSpecificCompressionContext_LZ4HC) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_FZ4H, &buf_size));
  bool result = SetupSpecificCompressionContext(*jcr, COMPRESS_FZ4H, 9);
  EXPECT_TRUE(result);
}

// ThreadlocalCompress tests
TEST_F(CompressionTest, ThreadlocalCompress_GZIP_simple) {
  const char* input = "Hello, World!";
  std::size_t input_len = strlen(input);
  std::size_t buf_size =
      RequiredCompressionOutputBufferSize(COMPRESS_GZIP, input_len);
  std::vector<char> output(buf_size);

  auto result =
      ThreadlocalCompress(COMPRESS_GZIP, 6, input, input_len,
                          output.data(), output.size());
  EXPECT_FALSE(result.holds_error());
  EXPECT_GT(*result.value(), 0);
}

TEST_F(CompressionTest, ThreadlocalCompress_GZIP_empty) {
  std::size_t buf_size = RequiredCompressionOutputBufferSize(COMPRESS_GZIP, 1);
  std::vector<char> output(buf_size);

  auto result = ThreadlocalCompress(COMPRESS_GZIP, 6, "", 0, output.data(),
                                    output.size());
  EXPECT_FALSE(result.holds_error());
}

TEST_F(CompressionTest, ThreadlocalCompress_GZIP_large_data) {
  std::string input(100000, 'A');
  std::size_t buf_size =
      RequiredCompressionOutputBufferSize(COMPRESS_GZIP, input.size());
  std::vector<char> output(buf_size);

  auto result = ThreadlocalCompress(COMPRESS_GZIP, 6, input.c_str(),
                                    input.size(), output.data(), output.size());
  EXPECT_FALSE(result.holds_error());
  EXPECT_LT(*result.value(), input.size());
}

TEST_F(CompressionTest, ThreadlocalCompress_GZIP_various_levels) {
  const char* input = "Repetitive data: " "The quick brown fox ";
  std::size_t input_len = strlen(input);
  std::size_t buf_size =
      RequiredCompressionOutputBufferSize(COMPRESS_GZIP, input_len);

  for (uint32_t level = 1; level <= 9; ++level) {
    std::vector<char> output(buf_size);
    auto result =
        ThreadlocalCompress(COMPRESS_GZIP, level, input, input_len,
                            output.data(), output.size());
    EXPECT_FALSE(result.holds_error()) << "Level " << level << " failed";
    EXPECT_GT(*result.value(), 0);
  }
}

#ifdef HAVE_LZO
TEST_F(CompressionTest, ThreadlocalCompress_LZO_simple) {
  const char* input = "Hello, World!";
  std::size_t input_len = strlen(input);
  std::size_t buf_size =
      RequiredCompressionOutputBufferSize(COMPRESS_LZO1X, input_len);
  std::vector<char> output(buf_size);

  auto result = ThreadlocalCompress(COMPRESS_LZO1X, 1, input, input_len,
                                    output.data(), output.size());
  EXPECT_FALSE(result.holds_error());
  EXPECT_GT(*result.value(), 0);
}
#endif  // HAVE_LZO

TEST_F(CompressionTest, ThreadlocalCompress_FASTLZ_simple) {
  const char* input = "Hello, World!";
  std::size_t input_len = strlen(input);
  std::size_t buf_size =
      RequiredCompressionOutputBufferSize(COMPRESS_FZFZ, input_len);
  std::vector<char> output(buf_size);

  auto result = ThreadlocalCompress(COMPRESS_FZFZ, 1, input, input_len,
                                    output.data(), output.size());
  EXPECT_FALSE(result.holds_error());
  EXPECT_GT(*result.value(), 0);
}

TEST_F(CompressionTest, ThreadlocalCompress_LZ4_simple) {
  const char* input = "Hello, World!";
  std::size_t input_len = strlen(input);
  std::size_t buf_size =
      RequiredCompressionOutputBufferSize(COMPRESS_FZ4L, input_len);
  std::vector<char> output(buf_size);

  auto result = ThreadlocalCompress(COMPRESS_FZ4L, 1, input, input_len,
                                    output.data(), output.size());
  EXPECT_FALSE(result.holds_error());
  EXPECT_GT(*result.value(), 0);
}

TEST_F(CompressionTest, ThreadlocalCompress_LZ4HC_simple) {
  const char* input = "Hello, World!";
  std::size_t input_len = strlen(input);
  std::size_t buf_size =
      RequiredCompressionOutputBufferSize(COMPRESS_FZ4H, input_len);
  std::vector<char> output(buf_size);

  auto result = ThreadlocalCompress(COMPRESS_FZ4H, 9, input, input_len,
                                    output.data(), output.size());
  EXPECT_FALSE(result.holds_error());
  EXPECT_GT(*result.value(), 0);
}

TEST_F(CompressionTest, ThreadlocalCompress_insufficient_buffer) {
  const char* input = "Hello, World!";
  std::size_t input_len = strlen(input);
  std::vector<char> output(1);

  auto result = ThreadlocalCompress(COMPRESS_GZIP, 6, input, input_len,
                                    output.data(), output.size());
  EXPECT_TRUE(result.holds_error());
}

TEST_F(CompressionTest, ThreadlocalCompress_repetitive_data_compresses) {
  std::string input(1000, 'A');
  std::size_t buf_size =
      RequiredCompressionOutputBufferSize(COMPRESS_GZIP, input.size());
  std::vector<char> output(buf_size);

  auto result = ThreadlocalCompress(COMPRESS_GZIP, 6, input.c_str(),
                                    input.size(), output.data(), output.size());
  EXPECT_FALSE(result.holds_error());
  EXPECT_LT(*result.value(), input.size());
}

TEST_F(CompressionTest, ThreadlocalCompress_binary_data) {
  unsigned char binary_data[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
  std::size_t buf_size = RequiredCompressionOutputBufferSize(
      COMPRESS_GZIP, sizeof(binary_data));
  std::vector<char> output(buf_size);

  auto result = ThreadlocalCompress(COMPRESS_GZIP, 6,
                                    reinterpret_cast<const char*>(binary_data),
                                    sizeof(binary_data), output.data(),
                                    output.size());
  EXPECT_FALSE(result.holds_error());
}

// CompressData and DecompressData integration tests
TEST_F(CompressionTest, CompressData_setup_validates) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));
  EXPECT_GT(buf_size, 0);
}

TEST_F(CompressionTest, CompressData_GZIP_repetitive) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));

  std::string test_data(1000, 'A');
  std::vector<unsigned char> compress_buf(buf_size);
  uint32_t compress_len = 0;

  bool result = CompressData(jcr, COMPRESS_GZIP,
                             const_cast<char*>(test_data.c_str()),
                             test_data.size(), compress_buf.data(), buf_size,
                             &compress_len);
  EXPECT_TRUE(result);
  EXPECT_GT(compress_len, 0);
  EXPECT_LT(compress_len, test_data.size());
}

#ifdef HAVE_LZO
TEST_F(CompressionTest, CompressData_LZO_setup) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_LZO1X, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}
#endif  // HAVE_LZO

TEST_F(CompressionTest, CompressData_FASTLZ_setup) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_FZFZ, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}

TEST_F(CompressionTest, CompressData_LZ4_setup) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_FZ4L, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}

TEST_F(CompressionTest, CompressData_LZ4HC_setup) {
  uint32_t buf_size = 0;
  bool result = SetupCompressionBuffers(jcr, COMPRESS_FZ4H, &buf_size);
  EXPECT_TRUE(result);
  EXPECT_GT(buf_size, 0);
}

TEST_F(CompressionTest, CompressData_empty_data) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));

  std::vector<unsigned char> compress_buf(buf_size);
  uint32_t compress_len = 0;

  bool result = CompressData(jcr, COMPRESS_GZIP, nullptr, 0,
                             compress_buf.data(), buf_size, &compress_len);
  EXPECT_TRUE(result);
}

TEST_F(CompressionTest, CompressData_large_data) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));
  EXPECT_GT(buf_size, 0);
}

// CleanupCompression tests
TEST_F(CompressionTest, CleanupCompression_after_setup) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));

  CleanupCompression(jcr);
  EXPECT_TRUE(true);
}

TEST_F(CompressionTest, CleanupCompression_multiple_setups) {
  uint32_t buf_size = 0;
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_GZIP, &buf_size));
#ifdef HAVE_LZO
  ASSERT_TRUE(SetupCompressionBuffers(jcr, COMPRESS_LZO1X, &buf_size));
#endif

  CleanupCompression(jcr);
  EXPECT_TRUE(true);
}

TEST_F(CompressionTest, CleanupCompression_without_setup) {
  CleanupCompression(jcr);
  EXPECT_TRUE(true);
}
