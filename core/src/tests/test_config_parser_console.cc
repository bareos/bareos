/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2025 Bareos GmbH & Co. KG

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

#include "lib/parse_conf.h"
#include "console/console_globals.h"
#include "console/console_conf.h"

namespace console {

static void InitConGlobals()
{
  OSDependentInit();
#if HAVE_WIN32
  WSA_Init();
#endif
  my_config = nullptr;
  director_resource = nullptr;
  console_resource = nullptr;
  me = nullptr;
}

TEST(ConfigParser, test_console_config)
{
  InitConGlobals();

  debug_level = 1000;

#if HAVE_WIN32
  WSA_Init();
#endif

  std::string path_to_config_file
      = std::string("configs/bareos-configparser-tests");

  my_config = InitConsConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  my_config->ParseConfig();

  my_config->DumpResources(PrintMessage, NULL);

  delete my_config;
}

}  // namespace console
