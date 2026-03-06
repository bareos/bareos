/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2026 Bareos GmbH & Co. KG

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

// Unit tests option_parser::parse()

#include "include/bareos.h"
#include "gtest/gtest.h"

#include "../plugins/python/common.h"  // option_parser, plugin_argument

#include <cstdlib>
#include <cstring>
#include <memory>

// ---------------------------------------------------------------------------
// Helper – parse one char* argument "val" from a plugin-definition string.
// Returns the heap-allocated C string (caller must free()), or nullptr on
// parse failure.
// ---------------------------------------------------------------------------

template <typename T> struct c_ptr {
  c_ptr() = default;
  c_ptr(T* ptr_) : ptr{ptr_} {}

  c_ptr(const c_ptr&) = delete;
  c_ptr& operator=(const c_ptr&) = delete;

  c_ptr(c_ptr&& other) { *this = std::move(other); }
  c_ptr& operator=(c_ptr&& other)
  {
    std::swap(ptr, other.ptr);
    return *this;
  }

  T*& addr() { return ptr; }

  T* get() { return ptr; }

  operator bool() const noexcept { return ptr != nullptr; }

  ~c_ptr() { free(ptr); }

 private:
  T* ptr{nullptr};
};

static c_ptr<char> parse_string_arg(const char* plugin_def)
{
  char* dest = nullptr;
  plugin_argument args[] = {{"val", &dest}};
  option_parser::parse(plugin_def, args);

  return c_ptr<char>{dest};
}

// ---------------------------------------------------------------------------
// StripBackSlashes – basic correctness
// ---------------------------------------------------------------------------

TEST(StripBackSlashes, NoBackslash_StringIsUnchanged)
{
  auto result = parse_string_arg("plugin:val=hello");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "hello");
}

TEST(StripBackSlashes, EmptyValue_ReturnsEmptyString)
{
  auto result = parse_string_arg("plugin:val=");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "");
}

TEST(StripBackSlashes, SingleBackslashInMiddle_IsRemoved)
{
  // "a\b" → "ab"
  auto result = parse_string_arg("plugin:val=a\\b");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "ab");
}

TEST(StripBackSlashes, MultipleBackslashes_AllRemoved)
{
  // "a\b\c\d" → "abcd"
  auto result = parse_string_arg("plugin:val=a\\b\\c\\d");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "abcd");
}

TEST(StripBackSlashes, ConsecutiveBackslashes_AllRemoved)
{
  // "a\\b" (two consecutive backslashes) → "ab"
  auto result = parse_string_arg("plugin:val=a\\\\b");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "ab");
}

TEST(StripBackSlashes, LeadingBackslash_IsRemoved)
{
  // "\hello" → "hello"
  auto result = parse_string_arg("plugin:val=\\hello");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "hello");
}

TEST(StripBackSlashes, TrailingBackslash_IsRemoved)
{
  // "hello\" → "hello"
  auto result = parse_string_arg("plugin:val=hello\\");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "hello");
}

TEST(StripBackSlashes, OnlyBackslashes_ReturnsEmptyString)
{
  // "\\\" (three backslashes) → ""
  auto result = parse_string_arg("plugin:val=\\\\\\");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "");
}

// ---------------------------------------------------------------------------
// Null-termination regression test.
//
// Without the "*out = '\0'" fix, the characters after the last written
// position are not erased.  For example "a\b" would read as "abb" because
// the original 'b' at position 2 is never overwritten.
// ---------------------------------------------------------------------------

TEST(StripBackSlashes, NullTerminationIsCorrect_SingleBackslash)
{
  // "a\b" must have length 2, not 3.
  auto result = parse_string_arg("plugin:val=a\\b");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "ab");
}

TEST(StripBackSlashes, NullTerminationIsCorrect_MultipleBackslashes)
{
  // "path\to\file" → "pathtofile" (length 10, not 12).
  auto result = parse_string_arg("plugin:val=path\\to\\file");
  ASSERT_TRUE(result);
  EXPECT_STREQ(result.get(), "pathtofile");
  EXPECT_EQ(strlen(result.get()), 10u) << "Got: \"" << result << "\"";
}

TEST(OptionParser, TwoOptions)
{
  c_ptr<char> a{};
  c_ptr<char> b{};
  plugin_argument args[] = {
      {"a", &a.addr()},
      {"b", &b.addr()},
  };

  auto result = option_parser::parse("plugin:a=1:b=2", args);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.unparsed_options(), "");

  EXPECT_STREQ(a.get(), "1");
  EXPECT_STREQ(b.get(), "2");
}

TEST(OptionParser, NoOptions)
{
  c_ptr<char> a{};
  c_ptr<char> b{};
  plugin_argument args[] = {
      {"a", &a.addr()},
      {"b", &b.addr()},
  };

  auto result = option_parser::parse("plugin:", args);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.unparsed_options(), "");
}

TEST(OptionParser, NoOptionsWithColon)
{
  c_ptr<char> a{};
  c_ptr<char> b{};
  plugin_argument args[] = {
      {"a", &a.addr()},
      {"b", &b.addr()},
  };

  auto result = option_parser::parse("plugin:", args);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.unparsed_options(), "");
}


TEST(OptionParser, MissingOption)
{
  c_ptr<char> a{};
  c_ptr<char> b{};
  plugin_argument args[] = {
      {"a", &a.addr()},
      {"b", &b.addr()},
  };

  auto result = option_parser::parse("plugin:a=1", args);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.unparsed_options(), "");
  EXPECT_STREQ(a.get(), "1");
  EXPECT_FALSE(b);
}


TEST(OptionParser, ExtraOption)
{
  c_ptr<char> a{};
  plugin_argument args[] = {
      {"a", &a.addr()},
  };

  auto result = option_parser::parse("plugin:a=1:b=2", args);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.unparsed_options(), "b=2:");
  EXPECT_STREQ(a.get(), "1");
}

TEST(OptionParser, TwoExtraOption)
{
  c_ptr<char> a;
  plugin_argument args[] = {
      {"a", &a.addr()},
  };

  auto result = option_parser::parse("plugin:a=1:b=2:c=3", args);

  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result.unparsed_options(), "b=2:c=3:");
  EXPECT_STREQ(a.get(), "1");
}

TEST(OptionParser, MissingValue)
{
  c_ptr<char> a{};

  plugin_argument args[] = {
      {"a", &a.addr()},
  };


  auto result = option_parser::parse("plugin:a=:b=2", args);

  EXPECT_TRUE(result.ok());
  EXPECT_STREQ(a.get(), "");
}

TEST(OptionParser, MissingValueNoOtherOption)
{
  c_ptr<char> a{};
  plugin_argument args[] = {
      {"a", &a.addr()},
  };

  auto result = option_parser::parse("plugin:a=", args);

  EXPECT_TRUE(result.ok());
  EXPECT_STREQ(a.get(), "");
}


TEST(OptionParser, MissingKey)
{
  c_ptr<char> a{};
  plugin_argument args[] = {
      {"a", &a.addr()},
  };

  auto result = option_parser::parse("plugin:=value", args);

  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.error_string(), "");
  EXPECT_EQ(a.get(), nullptr);
}


TEST(OptionParser, MissingEq)
{
  c_ptr<char> a{};
  plugin_argument args[] = {
      {"a", &a.addr()},
  };

  auto result = option_parser::parse("plugin:avalue", args);

  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.error_string().find("without value"), std::string::npos);
  EXPECT_EQ(a.get(), nullptr);
}

TEST(OptionParser, MissingColon)
{
  c_ptr<char> a{};
  plugin_argument args[] = {
      {"a", &a.addr()},
  };

  auto result = option_parser::parse("plugina=value", args);

  EXPECT_FALSE(result.ok());
  EXPECT_NE(result.error_string().find("missing ':'"), std::string::npos);
  EXPECT_EQ(a.get(), nullptr);
}
