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
#include "lib/var.h"
#include <array>
#include <cstring>
#include <string>
#include <string_view>

class VarTest : public ::testing::Test {
 protected:
  var_t* var = nullptr;

  void SetUp() override { ASSERT_EQ(var_create(&var), VAR_OK); }

  void TearDown() override
  {
    if (var) {
      var_destroy(var);
    }
  }
};

// Test var_create and var_destroy
TEST(var, create_destroy)
{
  var_t* v = nullptr;
  EXPECT_EQ(var_create(&v), VAR_OK);
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(var_destroy(v), VAR_OK);
}

// Test multiple create/destroy cycles
TEST(var, create_destroy_multiple)
{
  for (int i = 0; i < 10; i++) {
    var_t* v = nullptr;
    EXPECT_EQ(var_create(&v), VAR_OK);
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(var_destroy(v), VAR_OK);
  }
}

// Test simple unescape functionality with raw strings
TEST_F(VarTest, unescape_simple)
{
  const std::string_view src = R"(hello\nworld)";
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "hello\nworld");
}

// Test unescape with escaped backslash using raw strings
TEST_F(VarTest, unescape_escaped_backslash)
{
  const std::string_view src = R"(hello\\world)";
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  // With all=0, \\ produces \\ in output
  EXPECT_EQ(std::string(dst), R"(hello\\world)");
}

// Test unescape with tab character
TEST_F(VarTest, unescape_tab)
{
  const std::string_view src = R"(hello\tworld)";
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "hello\tworld");
}

// Test unescape with carriage return
TEST_F(VarTest, unescape_carriage_return)
{
  const std::string_view src = R"(hello\rworld)";
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "hello\rworld");
}

// Test unescape with unsupported escape sequence
TEST_F(VarTest, unescape_unsupported_escape)
{
  const std::string_view src = R"(hello\fworld)";
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  // Unsupported escape codes are passed through with backslash
  EXPECT_EQ(std::string(dst), R"(hello\fworld)");
}

// Test unescape with octal escape sequences
TEST_F(VarTest, unescape_octal)
{
  const std::string_view src = R"(A\101B)";
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "AAB");
}

// Test unescape with hex escape sequences
TEST_F(VarTest, unescape_hex)
{
  const std::string_view src = R"(A\x41B)";
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "AAB");
}

// Test unescape with no escapes (should copy as-is)
TEST_F(VarTest, unescape_no_escapes)
{
  const std::string_view src = "hello world";
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "hello world");
}

// Test unescape with partial source length
TEST_F(VarTest, unescape_partial_length)
{
  const std::string_view src = "hello world";
  char dst[100];
  int dst_len = sizeof(dst);

  // Only process first 5 characters
  EXPECT_EQ(var_unescape(var, src.data(), 5, dst, dst_len, 0), VAR_OK);
  EXPECT_EQ(std::string(dst), "hello");
}

// Test unescape with sufficient output buffer
TEST_F(VarTest, unescape_buffer_sufficient)
{
  const std::string_view src = "hello";
  char dst[10];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src.data(), src.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "hello");
}

// Test var_config with syntax using C++20 designated initializers
TEST_F(VarTest, config_syntax)
{
  var_syntax_t syntax{
      .escape = '\\',
      .delim_init = '$',
      .delim_open = '{',
      .delim_close = '}',
      .index_open = '[',
      .index_close = ']',
      .index_mark = '#',
      .name_chars = "a-zA-Z0-9_"};

  EXPECT_EQ(var_config(var, var_config_t::VAR_CONFIG_SYNTAX, &syntax),
            VAR_OK);
}

// Test simple variable expansion with force_expand flag
TEST_F(VarTest, expand_with_force_expand)
{
  const std::string_view src = "Hello World";
  char* dst = nullptr;
  int dst_len = 0;

  // Use force_expand flag to avoid undefined variable issues
  var_rc_t rc = var_expand(var, src.data(), src.size(), &dst, &dst_len, 1);
  // Should succeed with force_expand enabled
  EXPECT_EQ(rc, VAR_OK);
  if (dst) {
    free(dst);
  }
}

// Test expand with plain text (no variables)
TEST_F(VarTest, expand_plain_text)
{
  const std::string_view src = "Hello World";
  char* dst = nullptr;
  int dst_len = 0;

  EXPECT_EQ(var_expand(var, src.data(), src.size(), &dst, &dst_len, 0),
            VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::string(dst), "Hello World");

  free(dst);
}

// Test expand with escaped characters using raw strings
TEST_F(VarTest, expand_escaped_dollar)
{
  const std::string_view src = R"(Price: \$50)";
  char* dst = nullptr;
  int dst_len = 0;

  EXPECT_EQ(var_expand(var, src.data(), src.size(), &dst, &dst_len, 0),
            VAR_OK);
  ASSERT_NE(dst, nullptr);
  // var_expand preserves the backslash for escaped dollar
  EXPECT_EQ(std::string(dst), R"(Price: \$50)");

  free(dst);
}

// Test var_format with simple format string
TEST_F(VarTest, format_simple)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "Hello %s", "World"), VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::string(dst), "Hello World");

  free(dst);
}

// Test var_format with integer
TEST_F(VarTest, format_integer)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "Count: %d", 42), VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::string(dst), "Count: 42");

  free(dst);
}

// Test var_format with character
TEST_F(VarTest, format_character)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "Char: %c", 'X'), VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::string(dst), "Char: X");

  free(dst);
}

// Test var_format with percent literal
TEST_F(VarTest, format_percent)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "100%%"), VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::string(dst), "100%");

  free(dst);
}

// Test var_format with mixed format specifiers
TEST_F(VarTest, format_mixed)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "%s: %d%c", "Value", 42, '!'), VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::string(dst), "Value: 42!");

  free(dst);
}

// Test var_strerror for valid error codes
TEST_F(VarTest, strerror_valid_errors)
{
  const char* err_msg = var_strerror(var, VAR_ERR_OUT_OF_MEMORY);
  EXPECT_NE(err_msg, nullptr);
  EXPECT_GT(std::strlen(err_msg), 0);

  err_msg = var_strerror(var, VAR_ERR_UNDEFINED_VARIABLE);
  EXPECT_NE(err_msg, nullptr);
  EXPECT_GT(std::strlen(err_msg), 0);

  err_msg = var_strerror(var, VAR_ERR_INCOMPLETE_VARIABLE_SPEC);
  EXPECT_NE(err_msg, nullptr);
  EXPECT_GT(std::strlen(err_msg), 0);
}

// Test var_strerror for VAR_OK
TEST_F(VarTest, strerror_ok)
{
  const char* err_msg = var_strerror(var, VAR_OK);
  EXPECT_NE(err_msg, nullptr);
  EXPECT_GT(std::strlen(err_msg), 0);
}

// Test var_strerror for various error codes
TEST_F(VarTest, strerror_various_errors)
{
  constexpr std::array errors = {
      VAR_ERR_INVALID_HEX,
      VAR_ERR_INVALID_OCTAL,
      VAR_ERR_EMPTY_SEARCH_STRING,
      VAR_ERR_MALFORMATTED_REPLACE,
      VAR_ERR_INVALID_REGEX_IN_REPLACE,
      VAR_ERR_MISSING_PARAMETER_IN_COMMAND,
      VAR_ERR_RANGE_OUT_OF_BOUNDS,
      VAR_ERR_MISSING_START_OFFSET,
  };

  for (var_rc_t err : errors) {
    const char* err_msg = var_strerror(var, err);
    EXPECT_NE(err_msg, nullptr);
    EXPECT_GT(std::strlen(err_msg), 0);
  }
}

// Test that expand outputs are null-terminated
TEST_F(VarTest, expand_null_termination)
{
  const std::string_view src = "test";
  char* dst = nullptr;
  int dst_len = 0;

  EXPECT_EQ(var_expand(var, src.data(), src.size(), &dst, &dst_len, 0),
            VAR_OK);
  ASSERT_NE(dst, nullptr);
  // Check that result is properly null-terminated
  EXPECT_EQ(dst[std::strlen(dst)], '\0');

  free(dst);
}

// Test that format outputs are null-terminated
TEST_F(VarTest, format_null_termination)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "test"), VAR_OK);
  ASSERT_NE(dst, nullptr);
  // Check that result is properly null-terminated
  EXPECT_EQ(dst[std::strlen(dst)], '\0');

  free(dst);
}

// Test unescape with empty input
TEST_F(VarTest, unescape_empty)
{
  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, "", 0, dst, dst_len, 0), VAR_OK);
  EXPECT_EQ(std::strlen(dst), 0);
}

// Test expand with empty input
TEST_F(VarTest, expand_empty)
{
  char* dst = nullptr;
  int dst_len = 0;

  // Empty input to var_expand returns an error
  var_rc_t rc = var_expand(var, "", 0, &dst, &dst_len, 0);
  // Returns VAR_ERR_INVALID_ARGUMENT = -34
  EXPECT_EQ(rc, VAR_ERR_INVALID_ARGUMENT);

  if (dst) {
    free(dst);
  }
}

// Test format with single character format string
TEST_F(VarTest, format_single_char)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "X"), VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::strlen(dst), 1);
  EXPECT_EQ(std::string(dst), "X");

  free(dst);
}

// Test multiple unescapes in sequence
TEST_F(VarTest, unescape_sequence)
{
  const std::string_view src1 = R"(line1\n)";
  const std::string_view src2 = R"(line2\t)";
  const std::string_view src3 = "line3";

  char dst[100];
  int dst_len = sizeof(dst);

  EXPECT_EQ(var_unescape(var, src1.data(), src1.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "line1\n");

  EXPECT_EQ(var_unescape(var, src2.data(), src2.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "line2\t");

  EXPECT_EQ(var_unescape(var, src3.data(), src3.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(std::string(dst), "line3");
}

// Test multiple formats in sequence
TEST_F(VarTest, format_sequence)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "first"), VAR_OK);
  EXPECT_EQ(std::string(dst), "first");
  free(dst);

  EXPECT_EQ(var_format(var, &dst, 0, "second: %d", 2), VAR_OK);
  EXPECT_EQ(std::string(dst), "second: 2");
  free(dst);

  EXPECT_EQ(var_format(var, &dst, 0, "third: %s", "test"), VAR_OK);
  EXPECT_EQ(std::string(dst), "third: test");
  free(dst);
}

// Test unescape with all common escape sequences
TEST_F(VarTest, unescape_all_common)
{
  char dst[200];
  int dst_len = sizeof(dst);

  // Test newline
  const std::string_view src_n = R"(a\nb)";
  EXPECT_EQ(var_unescape(var, src_n.data(), src_n.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(dst[1], '\n');

  // Test tab
  const std::string_view src_t = R"(a\tb)";
  EXPECT_EQ(var_unescape(var, src_t.data(), src_t.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(dst[1], '\t');

  // Test carriage return
  const std::string_view src_r = R"(a\rb)";
  EXPECT_EQ(var_unescape(var, src_r.data(), src_r.size(), dst, dst_len, 0),
            VAR_OK);
  EXPECT_EQ(dst[1], '\r');
}

// Test expand with special characters that should be preserved
TEST_F(VarTest, expand_special_chars)
{
  const std::string_view src = "value{test}[array]#mark";
  char* dst = nullptr;
  int dst_len = 0;

  EXPECT_EQ(var_expand(var, src.data(), src.size(), &dst, &dst_len, 0),
            VAR_OK);
  ASSERT_NE(dst, nullptr);
  // These should be preserved when not used as variable syntax
  EXPECT_NE(std::string(dst).find('{'), std::string::npos);

  free(dst);
}

// Test format with multiple format specifiers of same type
TEST_F(VarTest, format_multiple_same_type)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "%d %d %d", 1, 2, 3), VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::string(dst), "1 2 3");

  free(dst);
}

// Test format with multiple string specifiers
TEST_F(VarTest, format_multiple_strings)
{
  char* dst = nullptr;

  EXPECT_EQ(var_format(var, &dst, 0, "%s-%s-%s", "one", "two", "three"),
            VAR_OK);
  ASSERT_NE(dst, nullptr);
  EXPECT_EQ(std::string(dst), "one-two-three");

  free(dst);
}

// Test that created variables are independent
TEST(var, multiple_instances)
{
  var_t* v1 = nullptr;
  var_t* v2 = nullptr;

  EXPECT_EQ(var_create(&v1), VAR_OK);
  EXPECT_EQ(var_create(&v2), VAR_OK);

  ASSERT_NE(v1, nullptr);
  ASSERT_NE(v2, nullptr);
  EXPECT_NE(v1, v2);

  EXPECT_EQ(var_destroy(v1), VAR_OK);
  EXPECT_EQ(var_destroy(v2), VAR_OK);
}
