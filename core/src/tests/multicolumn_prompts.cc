/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2024 Bareos GmbH & Co. KG

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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "dird/ua_select.h"
#include "include/jcr.h"

using namespace directordaemon;

class PromptsFormatting : public ::testing::Test {
 protected:
  void SetUp() override { ua = new_ua_context(&jcr); }

  void TearDown() override { FreeUaContext(ua); }

  void PopulateUaWithPrompts(UaContext* t_ua, const char** list)
  {
    StartPrompt(t_ua, "start");
    for (int i = 0; list[i]; ++i) { AddPrompt(t_ua, list[i]); }
  }

  int window_width{80};
  int lines_threshold{20};
  JobControlRecord jcr{};
  UaContext* ua{};
};

TEST_F(PromptsFormatting, ReturnsNothingOnAnEmptyList)
{
  const char* list[] = {nullptr};
  PopulateUaWithPrompts(ua, list);

  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      "");
  /* clang-format on */
}

TEST_F(PromptsFormatting, ReturnsSingleElementWhenOnlyOnePromptIsAvailable)
{
  const char* list[] = {T_("bareos1"), nullptr};
  PopulateUaWithPrompts(ua, list);

  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      "1: bareos1\n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting10Elements_StandardWidthNoThreshold)
{
  const char* list[]
      = {T_("bareos1"), T_("bareos2"),  T_("bareos3"), T_("bareos4"),
         T_("bareos5"), T_("bareos6"),  T_("bareos7"), T_("bareos8"),
         T_("bareos9"), T_("bareos10"), nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   3: bareos3   5: bareos5   7: bareos7   9: bareos9  \n"
      " 2: bareos2   4: bareos4   6: bareos6   8: bareos8  10: bareos10 \n"
     );
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting15Elements_StandardWidthNoThreshold)
{
  const char* list[]
      = {T_("bareos1"),  T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"),  T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"),  T_("bareos10"), T_("bareos11"), T_("bareos12"),
         T_("bareos13"), T_("bareos14"), T_("bareos15"),

         nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);
  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   4: bareos4   7: bareos7  10: bareos10 13: bareos13 \n"
      " 2: bareos2   5: bareos5   8: bareos8  11: bareos11 14: bareos14 \n"
      " 3: bareos3   6: bareos6   9: bareos9  12: bareos12 15: bareos15 \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting16Elements_StandardWidthNoThreshold)
{
  const char* list[]
      = {T_("bareos1"),  T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"),  T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"),  T_("bareos10"), T_("bareos11"), T_("bareos12"),
         T_("bareos13"), T_("bareos14"), T_("bareos15"), T_("bareos16"),
         nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   4: bareos4   7: bareos7  10: bareos10 13: bareos13 16: bareos16 \n"
      " 2: bareos2   5: bareos5   8: bareos8  11: bareos11 14: bareos14 \n"
      " 3: bareos3   6: bareos6   9: bareos9  12: bareos12 15: bareos15 \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting21Elements_StandardWidthNoThreshold)
{
  const char* list[]
      = {T_("bareos1"),  T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"),  T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"),  T_("bareos10"), T_("bareos11"), T_("bareos12"),
         T_("bareos13"), T_("bareos14"), T_("bareos15"), T_("bareos16"),
         T_("bareos17"), T_("bareos18"), T_("bareos19"), T_("bareos20"),
         T_("bareos21"), nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   5: bareos5   9: bareos9  13: bareos13 17: bareos17 21: bareos21 \n"
      " 2: bareos2   6: bareos6  10: bareos10 14: bareos14 18: bareos18 \n"
      " 3: bareos3   7: bareos7  11: bareos11 15: bareos15 19: bareos19 \n"
      " 4: bareos4   8: bareos8  12: bareos12 16: bareos16 20: bareos20 \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting,
       NoMulticolumnformattingWhenNumberOfElementsLessThanThreshold)
{
  const char* list[] = {T_("List last 20 Jobs run"), T_("Cancel"), nullptr};

  PopulateUaWithPrompts(ua, list);

  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      "1: List last 20 Jobs run\n"
      "2: Cancel\n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, FormatsForVeryLargeWidth)
{
  const char* list[] = {
      T_("List last 20 Jobs run"),
      T_("List Jobs where a given File is saved"),
      T_("Enter list of comma separated JobIds to select"),
      T_("Enter SQL list command"),
      T_("Select the most recent backup for a client"),
      T_("Select backup for a client before a specified time"),
      T_("Enter a list of files to restore"),
      T_("Enter a list of files to restore before a specified time"),
      T_("Find the JobIds of the most recent backup for a client"),
      T_("Find the JobIds for a backup for a client before a specified time"),
      T_("Enter a list of directories to restore for found JobIds"),
      T_("Select full restore to a specified Job date"),
      T_("Cancel"),
      nullptr};

  PopulateUaWithPrompts(ua, list);

  window_width = 5000;
  lines_threshold = 10;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: List last 20 Jobs run                                             "
      " 2: List Jobs where a given File is saved                             "
      " 3: Enter list of comma separated JobIds to select                    "
      " 4: Enter SQL list command                                            "
      " 5: Select the most recent backup for a client                        "
      " 6: Select backup for a client before a specified time                "
      " 7: Enter a list of files to restore                                  "
      " 8: Enter a list of files to restore before a specified time          "
      " 9: Find the JobIds of the most recent backup for a client            "
      "10: Find the JobIds for a backup for a client before a specified time "
      "11: Enter a list of directories to restore for found JobIds           "
      "12: Select full restore to a specified Job date                       "
      "13: Cancel                                                            \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Format15Elements_SmallWidth10LineThreshold)
{
  const char* list[]
      = {T_("bareos1"),  T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"),  T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"),  T_("bareos10"), T_("bareos11"), T_("bareos12"),
         T_("bareos13"), T_("bareos14"), T_("bareos15"),

         nullptr};

  PopulateUaWithPrompts(ua, list);

  window_width = 60;
  lines_threshold = 10;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(output.c_str(),
               " 1: bareos1   5: bareos5   9: bareos9  13: bareos13 \n"
               " 2: bareos2   6: bareos6  10: bareos10 14: bareos14 \n"
               " 3: bareos3   7: bareos7  11: bareos11 15: bareos15 \n"
               " 4: bareos4   8: bareos8  12: bareos12 \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting_NoWidth)
{
  const char* list[]
      = {T_("bareos1"), T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"), T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"), T_("bareos10"), T_("bareos11"), nullptr};

  PopulateUaWithPrompts(ua, list);

  window_width = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(output.c_str(),
               " 1: bareos1\n"
               " 2: bareos2\n"
               " 3: bareos3\n"
               " 4: bareos4\n"
               " 5: bareos5\n"
               " 6: bareos6\n"
               " 7: bareos7\n"
               " 8: bareos8\n"
               " 9: bareos9\n"
               "10: bareos10\n"
               "11: bareos11\n");
  /* clang-format on */
}

TEST_F(PromptsFormatting,
       FormatPromptsContainingSpacesAndRegularPrompts_StandartWidthNoThreshold)
{
  const char* list[] = {T_(""), T_("Listsaved"), T_("Cancel"), nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(output.c_str(),
               "1:           "
               "2: Listsaved "
               "3: Cancel    \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting,
       FormatPromptsContainingOnlySpacesPrompts_StandartWidthNoThreshold)
{
  const char* list[] = {T_(""), T_(" "), T_("  "), nullptr};

  PopulateUaWithPrompts(ua, list);

  std::string output = FormatPrompts(ua, 80, 20);

  /* clang-format off */
  EXPECT_STREQ(output.c_str(),
               "1: \n"
               "2:  \n"
               "3:   \n");
  /* clang-format on */
}
