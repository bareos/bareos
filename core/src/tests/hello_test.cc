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
#include "gtest/gtest.h"
#include "include/bareos.h"

#include "lib/hello.h"

using global_resource::Type;

/* These tests exercise parse_hello() with hello messages as they were
 * actually sent by pre-26 (and, for the console, pre-18.2) Bareos
 * components, to make sure old peers are still recognized correctly
 * by the unified BareosAccept()/parse_hello() implementation. */

TEST(ParseHello, OldStyleClientToDirector)
{
  // pre-51 File daemon: no FdProtocolVersion=, no Version="..."
  auto result = parse_hello(Type::Director, "Hello Client Bareos-fd calling");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->from, Type::Client);
  EXPECT_EQ(result->type, Type::Client);
  EXPECT_EQ(result->name, "Bareos-fd");
}

TEST(ParseHello, OldStyleClientToDirectorWithProtocolVersionOnly)
{
  // File daemon that sends FdProtocolVersion= but no Version="..."
  auto result = parse_hello(Type::Director,
                            "Hello Client Bareos-fd FdProtocolVersion=51 "
                            "calling");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->from, Type::Client);
  EXPECT_EQ(result->type, Type::Client);
  EXPECT_EQ(result->name, "Bareos-fd");
  EXPECT_EQ(result->fd_protocol_version, 51);
}

TEST(ParseHello, OldStyleDirectorToClient)
{
  // pre-26 Director: no Version="..." suffix
  auto result = parse_hello(Type::Client, "Hello Director my-dir calling");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->from, Type::Director);
  EXPECT_EQ(result->type, Type::Director);
  EXPECT_EQ(result->name, "my-dir");
}

TEST(ParseHello, OldStyleDirectorToStorage)
{
  auto result = parse_hello(Type::Storage, "Hello Director my-dir calling");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->from, Type::Director);
  EXPECT_EQ(result->type, Type::Director);
  EXPECT_EQ(result->name, "my-dir");
}

TEST(ParseHello, OldStyleClientToStorage)
{
  // pre-26 File daemon connecting to Storage for a backup/restore job
  auto result = parse_hello(Type::Storage, "Hello Start Job my-job-name");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->from, Type::Client);
  EXPECT_EQ(result->type, Type::Job);
  EXPECT_EQ(result->name, "my-job-name");
}

TEST(ParseHello, OldStyleStorageToStorage)
{
  // pre-26 Storage daemon replicating to another Storage daemon
  auto result
      = parse_hello(Type::Storage, "Hello Start Storage Job my-job-name");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->from, Type::Storage);
  EXPECT_EQ(result->type, Type::Job);
  EXPECT_EQ(result->name, "my-job-name");
}

TEST(ParseHello, OldStyleStorageToClient)
{
  auto result
      = parse_hello(Type::Client, "Hello Storage calling Start Job my-job");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->from, Type::Storage);
  EXPECT_EQ(result->type, Type::Job);
  EXPECT_EQ(result->name, "my-job");
}

// -- Console hello variants: this is the console-to-director path where
// -- the sscanf return value bug used to make every versioned old
// -- console hello fall through to the bare-name branch, which
// -- unconditionally set old_console = true.

TEST(ParseHello, BareNameConsoleIsOldConsole)
{
  // pre-13.2 console: only sends its (bashed) name, nothing else
  auto result = parse_hello(Type::Director, "Hello *UserAgent* calling");
  EXPECT_EQ(result, std::nullopt);
}

TEST(ParseHello, ConsoleWithVersionStringNoSemver)
{
  // console that sends its version string, but not the
  // Version="major.minor.patch" suffix (pre-18.2 style)
  auto result
      = parse_hello(Type::Director, "Hello *UserAgent* calling version 17.2.4");
  EXPECT_EQ(result, std::nullopt);
}

TEST(ParseHello, ConsoleWithOldSemverIsOldConsole)
{
  // console that sends both a version string and a Version="..." suffix,
  // but the semver itself is < 18.2
  auto result = parse_hello(
      Type::Director,
      "Hello *UserAgent* calling version 17.2.4 Version=\"17.2.4\"");
  EXPECT_EQ(result, std::nullopt);
}

TEST(ParseHello, ConsoleWithNewSemverIsNotOldConsole)
{
  // a current console always sends both a version string and a
  // Version="major.minor.patch" suffix with a semver >= 18.2
  auto result = parse_hello(
      Type::Director,
      "Hello *UserAgent* calling version 26.0.0 Version=\"26.0.0\"");
  ASSERT_TRUE(result);
  EXPECT_EQ(result->from, Type::Console);
  EXPECT_EQ(result->type, Type::Console);
}

TEST(ParseHello, UnrecognizedHelloIsRejected)
{
  // must not even start with the literal "Hello " prefix, otherwise
  // sscanf's lenient matching (it returns the count of specifiers
  // successfully assigned *before* a literal mismatch) can still
  // report a "successful" partial match
  auto result = parse_hello(Type::Director, "Goodbye, this is not a hello");
  EXPECT_FALSE(result);
}
