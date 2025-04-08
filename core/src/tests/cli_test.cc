/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2025 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#else
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#  include "include/bareos.h"
#endif

#include "lib/cli.h"

namespace {
std::string rtrim(std::string s, const char* whitespace = " \t\n\r\f\v")
{
  s.erase(s.find_last_not_of(whitespace) + 1);
  return s;
}
std::string trim(std::string s, const char* whitespace = " \t\n\r\f\v")
{
  return s.erase(s.find_last_not_of(whitespace) + 1)
      .erase(0, s.find_first_not_of(whitespace));
}
}  // namespace

TEST(CLI, HelpMessageDisplaysWithCorrectFormat)
{
  CLI::App app;
  InitCLIApp(app, "test app");
  auto default_help = app.help();

  // check that the help is included
  EXPECT_THAT(default_help, testing::HasSubstr("-h,-?,--help"));
  EXPECT_THAT(default_help,
              testing::HasSubstr("Print this help message and exit."));

  std::string random_default{"random default"};
  std::string random_option_text{"a random option."};
  bool random_flag;
  auto xarg = app.add_flag("!-x", random_flag, random_option_text);

  app.add_option("-y", random_default, random_option_text)
      ->required()
      ->capture_default_str()
      ->needs(xarg)
      ->excludes(xarg)
      ->expected(5);


  std::string expected_help{
      "    -x\n"
      "        Excludes: -y\n"
      "        "+random_option_text+" \n\n"
      "    -y TEXT x 5\n"
      "        Default: "+random_default+"\n"
      "        REQUIRED\n"
      "        Needs: -x\n"
      "        Excludes: -x\n"
      "        "+random_option_text
  };

  auto help = app.help();

  ASSERT_GE(help.size(), default_help.size());
  ASSERT_EQ(rtrim(help.substr(0, default_help.size())), rtrim(default_help));

  auto new_help = help.substr(default_help.size());

  EXPECT_STREQ(app.get_description().c_str(), "test app");


  // cli output is not very stable
  // we try to make this test more stable by not required an exact match
  // regarding trailing & preceeding whitespace
  EXPECT_EQ(trim(new_help), trim(expected_help)) << new_help << "\n---\n"
                                                 << help << "\n---\n"
                                                 << default_help << "\n---\n";
}
