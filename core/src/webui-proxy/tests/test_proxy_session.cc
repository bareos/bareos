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

#include "../proxy_session.h"

#include <gtest/gtest.h>

TEST(ProxySession, TrimsWhitespaceAroundCommands)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("  list jobs \r\n"), "list jobs");
}

TEST(ProxySession, PreservesTrailingTabForCompletion)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("  list cl\t \r\n"), "list cl\t");
}

TEST(ProxySession, PreservesStandaloneTabCompletion)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("\t"), "\t");
}

TEST(ProxySession, TreatsQuitAsExpectedMainPromptExit)
{
  EXPECT_TRUE(IsExpectedConsoleExitCommand(true, "quit"));
  EXPECT_TRUE(IsExpectedConsoleExitCommand(true, "exit"));
  EXPECT_TRUE(IsExpectedConsoleExitCommand(true, ".quit"));
  EXPECT_TRUE(IsExpectedConsoleExitCommand(true, ".exit"));
}

TEST(ProxySession, DoesNotTreatSubPromptExitAsConsoleClose)
{
  EXPECT_FALSE(IsExpectedConsoleExitCommand(false, "quit"));
  EXPECT_FALSE(IsExpectedConsoleExitCommand(false, "exit"));
  EXPECT_FALSE(IsExpectedConsoleExitCommand(true, "done"));
}
