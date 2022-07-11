/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "lib/cli.h"

TEST(CLI, HelpMessageDisplaysWithCorrectFormat)
{
  CLI::App app;
  InitCLIApp(app, "test app");
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


  /* clang-format off */
  std::string expected_help{
      "test app\n"
      "Usage: [OPTIONS]\n\n"
      "Options:\n"
      "    -h,-?,--help\n"
      "        Print this help message and exit. \n\n"
      "    --version\n"
      "        Display program version information and exit \n\n"
      "    -x\n"
      "        Excludes: -y\n"
      "        "+random_option_text+" \n\n"
      "    -y TEXT x 5\n"
      "        Default: "+random_default+"\n"
      "        REQUIRED\n"
      "        Needs: -x\n"
      "        Excludes: -x\n"
      "        "+random_option_text+" \n\n\n"};
  /* clang-format on */

  EXPECT_STREQ(app.get_description().c_str(), "test app");
  EXPECT_STREQ(app.help().c_str(), expected_help.c_str());
}
