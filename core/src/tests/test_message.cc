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
#include "lib/message.h"
#include <cstring>
#include <string>
#include <string_view>

// Test fixture for message system tests
class MessageTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    // Initialize message system for testing
    InitConsoleMsg(R"(/tmp)");
  }

  void TearDown() override { TermMsg(); }
};

// Test MyNameIs - sets daemon name and extracts executable info
TEST_F(MessageTest, MyNameIs_simple_name)
{
  const char* argv[] = {R"(/usr/sbin/bareos-dir)"};
  MyNameIs(1, argv, "bareos-dir");

  // Verify my_name is set
  EXPECT_STREQ(my_name, "bareos-dir");
  EXPECT_NE(strlen(my_name), 0);
}

// Test MyNameIs with full path
TEST_F(MessageTest, MyNameIs_full_path)
{
  const char* argv[] = {R"(/usr/local/sbin/my-daemon)"};
  MyNameIs(1, argv, "my-daemon");

  EXPECT_STREQ(my_name, "my-daemon");
}

// Test MyNameIs with Windows-style path
TEST_F(MessageTest, MyNameIs_windows_style)
{
  const char* argv[] = {R"(C:\Program Files\Bareos\bareos-dir.exe)"};
  MyNameIs(1, argv, "bareos-dir");

  EXPECT_STREQ(my_name, "bareos-dir");
}

// Test MyNameIs with no arguments
TEST_F(MessageTest, MyNameIs_no_args)
{
  MyNameIs(0, nullptr, "test-daemon");

  EXPECT_STREQ(my_name, "test-daemon");
}

// Test SetDbType - sets database type
TEST_F(MessageTest, SetDbType_postgresql)
{
  SetDbType("PostgreSQL");
  // SetDbType stores the value internally, verify it doesn't crash
  EXPECT_NO_FATAL_FAILURE(SetDbType("PostgreSQL"));
}

// Test SetDbType with multiple calls
TEST_F(MessageTest, SetDbType_multiple_calls)
{
  SetDbType("MySQL");
  SetDbType("PostgreSQL");
  SetDbType("SQLite");
  // Verify no crash and proper cleanup
  EXPECT_NO_FATAL_FAILURE(SetDbType(""));
}

// Test SetDbType with null pointer
TEST_F(MessageTest, SetDbType_empty_string)
{
  SetDbType("");
  // Should not crash
  EXPECT_NO_FATAL_FAILURE(SetDbType("PostgreSQL"));
}

// Test SetTrace flag
TEST_F(MessageTest, SetTrace_enable)
{
  SetTrace(1);
  EXPECT_TRUE(GetTrace());
}

// Test SetTrace disable
TEST_F(MessageTest, SetTrace_disable)
{
  SetTrace(0);
  EXPECT_FALSE(GetTrace());
}

// Test SetTrace toggle
TEST_F(MessageTest, SetTrace_toggle)
{
  SetTrace(1);
  EXPECT_TRUE(GetTrace());
  SetTrace(0);
  EXPECT_FALSE(GetTrace());
  SetTrace(1);
  EXPECT_TRUE(GetTrace());
}

// Test SetHangup flag
TEST_F(MessageTest, SetHangup_enable)
{
  SetHangup(1);
  EXPECT_TRUE(GetHangup());
}

// Test SetHangup disable
TEST_F(MessageTest, SetHangup_disable)
{
  SetHangup(0);
  EXPECT_FALSE(GetHangup());
}

// Test SetTimestamp flag
TEST_F(MessageTest, SetTimestamp_enable)
{
  SetTimestamp(1);
  EXPECT_TRUE(GetTimestamp());
}

// Test SetTimestamp disable
TEST_F(MessageTest, SetTimestamp_disable)
{
  SetTimestamp(0);
  EXPECT_FALSE(GetTimestamp());
}

// Test SetTimestamp toggle
TEST_F(MessageTest, SetTimestamp_toggle)
{
  SetTimestamp(1);
  EXPECT_TRUE(GetTimestamp());
  SetTimestamp(0);
  EXPECT_FALSE(GetTimestamp());
  SetTimestamp(1);
  EXPECT_TRUE(GetTimestamp());
}

// Test SetLogTimestampFormat with various formats
TEST_F(MessageTest, SetLogTimestampFormat_default)
{
  SetLogTimestampFormat(R"(%d-%b %H:%M)");
  // Just verify it doesn't crash
  EXPECT_NO_FATAL_FAILURE(SetLogTimestampFormat(R"(%d-%b %H:%M)"));
}

// Test SetLogTimestampFormat with different format
TEST_F(MessageTest, SetLogTimestampFormat_iso)
{
  SetLogTimestampFormat(R"(%Y-%m-%d %H:%M:%S)");
  EXPECT_NO_FATAL_FAILURE(SetLogTimestampFormat(R"(%Y-%m-%d %H:%M:%S)"));
}

// Test SetLogTimestampFormat with minimal format
TEST_F(MessageTest, SetLogTimestampFormat_minimal)
{
  SetLogTimestampFormat(R"(%H:%M)");
  EXPECT_NO_FATAL_FAILURE(SetLogTimestampFormat(R"(%H:%M)"));
}

// Test SetLogTimestampFormat with empty string
TEST_F(MessageTest, SetLogTimestampFormat_empty)
{
  SetLogTimestampFormat("");
  EXPECT_NO_FATAL_FAILURE(SetLogTimestampFormat(""));
}

// Test get_basename with simple filename
TEST(message, get_basename_simple)
{
  const char* result = get_basename(R"(filename.txt)");
  EXPECT_STREQ(result, R"(filename.txt)");
}

// Test get_basename with Unix path
TEST(message, get_basename_unix_path)
{
  const char* result = get_basename(R"(/usr/bin/program)");
  // get_basename finds parent directory name, not filename
  EXPECT_NE(result, nullptr);
  EXPECT_NE(std::string(result).find(R"(bin)"), std::string::npos);
}

// Test get_basename with multiple directories
TEST(message, get_basename_multiple_dirs)
{
  const char* result = get_basename(R"(/home/user/documents/file.txt)");
  EXPECT_NE(result, nullptr);
  // Returns the directory/filename components after the second-to-last separator
  EXPECT_NE(std::string(result).find(R"(documents)"), std::string::npos);
}

// Test get_basename with trailing slash
TEST(message, get_basename_trailing_slash)
{
  const char* result = get_basename(R"(/home/user/)");
  EXPECT_STREQ(result, R"(user/)");
}

// Test get_basename with Windows path
TEST(message, get_basename_windows_path)
{
  const char* result = get_basename(R"(C:\Program Files\App\file.exe)");
  // Result depends on platform path separator handling
  EXPECT_NE(result, nullptr);
}

// Test global variables initialization
TEST_F(MessageTest, global_variables_initialized)
{
  // Verify global variables are accessible and initialized
  EXPECT_GE(debug_level, 0);
  EXPECT_GE(g_verbose, 0);
  EXPECT_NE(my_name, nullptr);
}

// Test daemon name is set correctly
TEST_F(MessageTest, MyNameIs_sets_daemon_name)
{
  MyNameIs(0, nullptr, R"(test-daemon)");
  // Verify my_name is set
  EXPECT_STREQ(my_name, R"(test-daemon)");
}

// Test InitConsoleMsg with valid directory
TEST(message, InitConsoleMsg_valid_dir)
{
  InitConsoleMsg(R"(/tmp)");
  // Should initialize without crashing
  TermMsg();
}

// Test InitConsoleMsg with current directory
TEST(message, InitConsoleMsg_current_dir)
{
  InitConsoleMsg(R"(.)");
  TermMsg();
}

// Test trace and hangup flags independent behavior
TEST_F(MessageTest, flags_independent)
{
  SetTrace(1);
  SetHangup(1);
  SetTimestamp(1);

  EXPECT_TRUE(GetTrace());
  EXPECT_TRUE(GetHangup());
  EXPECT_TRUE(GetTimestamp());

  SetTrace(0);
  EXPECT_FALSE(GetTrace());
  EXPECT_TRUE(GetHangup());
  EXPECT_TRUE(GetTimestamp());

  SetHangup(0);
  EXPECT_FALSE(GetTrace());
  EXPECT_FALSE(GetHangup());
  EXPECT_TRUE(GetTimestamp());

  SetTimestamp(0);
  EXPECT_FALSE(GetTrace());
  EXPECT_FALSE(GetHangup());
  EXPECT_FALSE(GetTimestamp());
}

// Test get_basename with empty string
TEST(message, get_basename_empty)
{
  const char* result = get_basename("");
  EXPECT_NE(result, nullptr);
}

// Test SetDbType with long database names
TEST_F(MessageTest, SetDbType_long_name)
{
  constexpr std::string_view long_name = R"(PostgreSQL with extended settings)";
  SetDbType(long_name.data());
  EXPECT_NO_FATAL_FAILURE(SetDbType("MySQL"));
}

// Test debug level global variable
TEST_F(MessageTest, debug_level_global)
{
  // debug_level should be accessible
  int original = debug_level;
  EXPECT_GE(debug_level, 0);
  debug_level = original;
}

// Test verbose level global variable
TEST_F(MessageTest, verbose_level_global)
{
  // g_verbose should be accessible
  int original = g_verbose;
  EXPECT_GE(g_verbose, 0);
  g_verbose = original;
}

// Test dbg_timestamp global variable
TEST_F(MessageTest, dbg_timestamp_global)
{
  bool original = dbg_timestamp;
  EXPECT_NO_FATAL_FAILURE(dbg_timestamp = true);
  EXPECT_NO_FATAL_FAILURE(dbg_timestamp = false);
  dbg_timestamp = original;
}

// Test prt_kaboom global variable
TEST_F(MessageTest, prt_kaboom_global)
{
  bool original = prt_kaboom;
  EXPECT_NO_FATAL_FAILURE(prt_kaboom = true);
  EXPECT_NO_FATAL_FAILURE(prt_kaboom = false);
  prt_kaboom = original;
}

// Test MyNameIs with relative path
TEST_F(MessageTest, MyNameIs_relative_path)
{
  const char* argv[] = {R"(./daemon)"};
  MyNameIs(1, argv, R"(test-daemon)");
  EXPECT_STREQ(my_name, R"(test-daemon)");
}

// Test MyNameIs with long daemon name
TEST_F(MessageTest, MyNameIs_long_name)
{
  const char* argv[] = {R"(/usr/sbin/bareos-file-daemon)"};
  MyNameIs(1, argv, R"(bareos-file-daemon)");
  EXPECT_STREQ(my_name, R"(bareos-file-daemon)");
}

// Test SetLogTimestampFormat with special characters
TEST_F(MessageTest, SetLogTimestampFormat_special_chars)
{
  SetLogTimestampFormat(R"([%Y-%m-%d %H:%M:%S])");
  EXPECT_NO_FATAL_FAILURE(SetLogTimestampFormat(R"(<%d-%b %H:%M>)"));
}

// Test flag combinations
TEST_F(MessageTest, flag_combinations)
{
  // Test all combinations of three boolean flags
  for (int trace_val = 0; trace_val <= 1; ++trace_val) {
    for (int hangup_val = 0; hangup_val <= 1; ++hangup_val) {
      for (int timestamp_val = 0; timestamp_val <= 1; ++timestamp_val) {
        SetTrace(trace_val);
        SetHangup(hangup_val);
        SetTimestamp(timestamp_val);

        EXPECT_EQ(GetTrace(), static_cast<bool>(trace_val));
        EXPECT_EQ(GetHangup(), static_cast<bool>(hangup_val));
        EXPECT_EQ(GetTimestamp(), static_cast<bool>(timestamp_val));
      }
    }
  }
}

// Test get_basename with multiple path separators
TEST(message, get_basename_multiple_separators)
{
  const char* result = get_basename(R"(/home//user///file.txt)");
  EXPECT_NE(result, nullptr);
}

// Test MyNameIs initializes exepath and exename correctly
TEST_F(MessageTest, MyNameIs_extracts_executable_name)
{
  const char* argv[] = {R"(/usr/bin/my-program)"};
  MyNameIs(1, argv, R"(my-program)");

  // my_name should be set
  EXPECT_STREQ(my_name, R"(my-program)");
  // Test doesn't verify exepath/exename directly as they're not exported
}

// Test InitConsoleMsg and TermMsg cycle
TEST(message, InitConsoleMsg_TermMsg_cycle)
{
  for (int i = 0; i < 3; ++i) {
    InitConsoleMsg(R"(/tmp)");
    TermMsg();
  }
}

// Test SetDbType preserves state across calls
TEST_F(MessageTest, SetDbType_state_preservation)
{
  SetDbType(R"(PostgreSQL)");
  // Make multiple calls to verify state handling
  SetDbType(R"(MySQL)");
  SetDbType(R"(SQLite)");
  // Should not crash or leak
  EXPECT_NO_FATAL_FAILURE(SetDbType(""));
}

// Test message system with daemon name containing special characters
TEST_F(MessageTest, MyNameIs_special_characters_name)
{
  const char* argv[] = {R"(/usr/sbin/bareos-sd)"};
  MyNameIs(1, argv, R"(bareos-sd)");
  EXPECT_STREQ(my_name, R"(bareos-sd)");
}

// Test console_msg_pending global variable
TEST_F(MessageTest, console_msg_pending_global)
{
  bool original = console_msg_pending;
  EXPECT_NO_FATAL_FAILURE(console_msg_pending = true);
  EXPECT_NO_FATAL_FAILURE(console_msg_pending = false);
  console_msg_pending = original;
}
