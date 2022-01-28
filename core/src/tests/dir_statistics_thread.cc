/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2021 Bareos GmbH & Co. KG

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
#endif


#include "lib/parse_conf.h"
#include "dird/socket_server.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"

#include "lib/address_conf.h"
#include "lib/watchdog.h"
#include "lib/berrno.h"
#include "dird/stats.h"


namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

static void InitGlobals()
{
  OSDependentInit();
#if HAVE_WIN32
  WSA_Init();
#endif
  directordaemon::my_config = nullptr;
  directordaemon::me = nullptr;
  InitMsg(NULL, NULL);
}

typedef std::unique_ptr<ConfigurationParser> PConfigParser;

static PConfigParser DirectorPrepareResources(const std::string& path_to_config)
{
  PConfigParser director_config(
      directordaemon::InitDirConfig(path_to_config.c_str(), M_INFO));
  directordaemon::my_config
      = director_config.get(); /* set the director global variable */

  EXPECT_NE(director_config.get(), nullptr);
  if (!director_config) { return nullptr; }

  bool parse_director_config_ok = director_config->ParseConfig();
  EXPECT_TRUE(parse_director_config_ok) << "Could not parse director config";
  if (!parse_director_config_ok) { return nullptr; }

  Dmsg0(200, "Start UA server\n");
  directordaemon::me
      = (directordaemon::DirectorResource*)director_config->GetNextRes(
          directordaemon::R_DIRECTOR, nullptr);
  directordaemon::my_config->own_resource_ = directordaemon::me;

  return director_config;
}

static void test_starting_statistics_thread(std::string path_to_config)
{
  debug_level = 10;  // set debug level high enough so we can see error messages
  InitGlobals();

  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return; }

  EXPECT_FALSE(directordaemon::StartStatisticsThread());
}


TEST(dir_statistics_thread, default_collect_statistics)
{
  InitGlobals();
  std::string path_to_config = std::string(
      RELATIVE_PROJECT_SOURCE_DIR "/configs/statistics_thread/default_config");

  test_starting_statistics_thread(path_to_config);
}

TEST(dir_statistics_thread, only_interval_set)
{
  InitGlobals();
  std::string path_to_config
      = std::string(RELATIVE_PROJECT_SOURCE_DIR
                    "/configs/statistics_thread/only_interval_set");

  test_starting_statistics_thread(path_to_config);
}

TEST(dir_statistics_thread, only_collect_set)
{
  InitGlobals();
  std::string path_to_config
      = std::string(RELATIVE_PROJECT_SOURCE_DIR
                    "/configs/statistics_thread/only_interval_set");

  test_starting_statistics_thread(path_to_config);
}
