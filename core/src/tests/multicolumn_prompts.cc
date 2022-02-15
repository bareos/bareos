/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2022 Bareos GmbH & Co. KG

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

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

class MulticolumPrompts : public ::testing::Test {
 protected:
  void SetUp() override { ua = new_ua_context(&jcr); }

  void TearDown() override { FreeUaContext(ua); }

  JobControlRecord jcr{};
  UaContext* ua{};
};

TEST_F(MulticolumPrompts, FormatForAnyNumberOfLinesAndStandardWidth)
{
  const char* list[] = {
      _("bareos1"),  _("bareos2"),  _("bareos3"),  _("bareos4"),  _("bareos5"),
      _("bareos6"),  _("bareos7"),  _("bareos8"),  _("bareos9"),  _("bareos10"),
      _("bareos11"), _("bareos12"), _("bareos13"), _("bareos14"), _("bareos15"),

      nullptr};

  StartPrompt(ua, "start");
  for (int i = 0; list[i]; ++i) { AddPrompt(ua, list[i]); }

  std::string output = FormatMulticolumnPrompts(ua, 80, 1);

  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   4: bareos4   7: bareos7  10: bareos10 13: bareos13 \n"
      " 2: bareos2   5: bareos5   8: bareos8  11: bareos11 14: bareos14 \n"
      " 3: bareos3   6: bareos6   9: bareos9  12: bareos12 15: bareos15 ");
}

TEST_F(MulticolumPrompts, FormatForMoreThan10LinesAndStandardWidth)
{
  const char* list[]
      = {_("List last 20 Jobs run"),
         _("List Jobs where a given File is saved"),
         _("Enter list of comma separated JobIds to select"),
         _("Enter SQL list command"),
         _("Select the most recent backup for a client"),
         _("Select backup for a client before a specified time"),
         _("Enter a list of files to restore"),
         _("Enter a list of files to restore before a specified time"),
         _("Find the JobIds of the most recent backup for a client"),
         _("Find the JobIds for a backup for a client before a specified time"),
         _("Enter a list of directories to restore for found JobIds"),
         _("Select full restore to a specified Job date"),
         _("Cancel"),
         nullptr};
  StartPrompt(ua, "start");
  for (int i = 0; list[i]; ++i) { AddPrompt(ua, list[i]); }

  std::string output = FormatMulticolumnPrompts(ua, 80, 20);

  EXPECT_STREQ(
      output.c_str(),
      " 1: List last 20 Jobs run\n"
      " 2: List Jobs where a given File is saved\n"
      " 3: Enter list of comma separated JobIds to select\n"
      " 4: Enter SQL list command\n"
      " 5: Select the most recent backup for a client\n"
      " 6: Select backup for a client before a specified time\n"
      " 7: Enter a list of files to restore\n"
      " 8: Enter a list of files to restore before a specified time\n"
      " 9: Find the JobIds of the most recent backup for a client\n"
      "10: Find the JobIds for a backup for a client before a specified time\n"
      "11: Enter a list of directories to restore for found JobIds\n"
      "12: Select full restore to a specified Job date\n"
      "13: Cancel\n");
}

TEST_F(MulticolumPrompts, FormatForAnyNumberOfLinesAndHugeWidth)
{
  const char* list[]
      = {_("List last 20 Jobs run"),
         _("List Jobs where a given File is saved"),
         _("Enter list of comma separated JobIds to select"),
         _("Enter SQL list command"),
         _("Select the most recent backup for a client"),
         _("Select backup for a client before a specified time"),
         _("Enter a list of files to restore"),
         _("Enter a list of files to restore before a specified time"),
         _("Find the JobIds of the most recent backup for a client"),
         _("Find the JobIds for a backup for a client before a specified time"),
         _("Enter a list of directories to restore for found JobIds"),
         _("Select full restore to a specified Job date"),
         _("Cancel"),
         nullptr};
  StartPrompt(ua, "start");
  for (int i = 0; list[i]; ++i) { AddPrompt(ua, list[i]); }

  std::string output = FormatMulticolumnPrompts(ua, 5000, 1);

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
      "13: Cancel\n");
}

TEST_F(MulticolumPrompts, FormatForMoreThan10LinesAndSmallerThanStandardWidth)
{
  const char* list[] = {
      _("bareos1"),  _("bareos2"),  _("bareos3"),  _("bareos4"),  _("bareos5"),
      _("bareos6"),  _("bareos7"),  _("bareos8"),  _("bareos9"),  _("bareos10"),
      _("bareos11"), _("bareos12"), _("bareos13"), _("bareos14"), _("bareos15"),

      nullptr};

  StartPrompt(ua, "start");
  for (int i = 0; list[i]; ++i) { AddPrompt(ua, list[i]); }

  std::string output = FormatMulticolumnPrompts(ua, 60, 10);

  EXPECT_STREQ(output.c_str(),
               " 1: bareos1   5: bareos5   9: bareos9  13: bareos13\n"
               " 2: bareos2   6: bareos6  10: bareos10 14: bareos14\n"
               " 3: bareos3   7: bareos7  11: bareos11 15: bareos15\n"
               " 4: bareos4   8: bareos8  12: bareos12\n");
}

TEST_F(MulticolumPrompts, FormatPromptsContainingSpacesAndRegularPrompts)
{
  const char* list[] = {_(""), _("Listsaved"), _("Cancel"), nullptr};

  StartPrompt(ua, "start");
  for (int i = 0; list[i]; ++i) { AddPrompt(ua, list[i]); }

  std::string output = FormatMulticolumnPrompts(ua, 80, 1);

  EXPECT_STREQ(output.c_str(),
               "1:           "
               "2: Listsaved "
               "3: Cancel\n");
}

TEST_F(MulticolumPrompts, FormatPromptsContainingSpaces)
{
  const char* list[] = {_(""), _(" "), _("  "), nullptr};

  StartPrompt(ua, "start");
  for (int i = 0; list[i]; ++i) { AddPrompt(ua, list[i]); }

  std::string output = FormatMulticolumnPrompts(ua, 80, 20);

  EXPECT_STREQ(output.c_str(),
               "1: \n"
               "2:  \n"
               "3:   \n");
}
