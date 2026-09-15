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

#include "console/console_output.h"

#include "gtest/gtest.h"

TEST(ConsoleOutput, ColorPolicy)
{
  EXPECT_TRUE(console::ShouldUseColor(true, true, "xterm-256color", nullptr));
  EXPECT_FALSE(console::ShouldUseColor(false, true, "xterm", nullptr));
  EXPECT_FALSE(console::ShouldUseColor(true, false, "xterm", nullptr));
  EXPECT_FALSE(console::ShouldUseColor(true, true, "dumb", nullptr));
  EXPECT_FALSE(console::ShouldUseColor(true, true, "xterm", ""));
}

TEST(ConsoleOutput, StylesSemanticMessages)
{
  EXPECT_EQ(console::ApplyStyle("failed\n", ConsoleOutputStyle::kError, true),
            "\033[1;31mfailed\n\033[0m");
  EXPECT_EQ(
      console::ApplyStyle("warning\n", ConsoleOutputStyle::kWarning, true),
      "\033[1;33mwarning\n\033[0m");
  EXPECT_EQ(console::ApplyStyle("info\n", ConsoleOutputStyle::kInfo, true),
            "\033[36minfo\n\033[0m");
  EXPECT_EQ(console::ApplyStyle("plain", ConsoleOutputStyle::kError, false),
            "plain");
}

TEST(ConsoleOutput, StylesReadlinePromptWithoutChangingDisplayWidth)
{
  EXPECT_EQ(console::ReadlinePrompt("*", true),
            "\001\033[1;36m\002*\001\033[0m\002");
  EXPECT_EQ(console::ReadlinePrompt("*", false), "*");
  EXPECT_EQ(console::ReadlinePrompt("", true), "");
}

TEST(ConsoleOutput, StripsAnsiForPlainSinks)
{
  EXPECT_EQ(console::StripAnsiEscapeSequences(
                "\033[1;31merror\033[0m \033[7;36mselected\033[0m"),
            "error selected");
  EXPECT_EQ(console::StripAnsiEscapeSequences(
                "before\033]52;c;Y2xpcGJvYXJkBw==\aafter"),
            "beforeafter");
  EXPECT_EQ(console::StripAnsiEscapeSequences(
                "before\033Pmalicious\033\\after"),
            "beforeafter");
  EXPECT_EQ(console::StripAnsiEscapeSequences(
                "line\r\b\a\x7f C1:\xc2\x9b" "31mred"),
            "line C1:31mred");
  EXPECT_EQ(console::StripAnsiEscapeSequences("plain"), "plain");
}

TEST(ConsoleOutput, StripsAnsiFromFileSink)
{
  FILE* file = tmpfile();
  ASSERT_NE(file, nullptr);
  SetTeeFile(file);
  ConsoleSetAnsiPassthrough(true);

  ConsoleOutput("\033[1;31merror\033[0m\n");
  ASSERT_EQ(fseek(file, 0, SEEK_SET), 0);
  char buffer[32]{};
  ASSERT_NE(fgets(buffer, sizeof(buffer), file), nullptr);
  EXPECT_STREQ(buffer, "error\n");

  SetTeeFile(stdout);
  fclose(file);
}
