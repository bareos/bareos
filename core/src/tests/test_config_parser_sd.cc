/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2024 Bareos GmbH & Co. KG

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
#include "stored/stored_globals.h"
#include "stored/stored_conf.h"

namespace storagedaemon {

TEST(ConfigParser_SD, test_stored_config)
{
  OSDependentInit();

  std::string path_to_config_file
      = std::string("configs/bareos-configparser-tests");
  my_config = InitSdConfig(path_to_config_file.c_str(), M_CONFIG_ERROR);
  ParseSdConfig(configfile, M_CONFIG_ERROR);

  my_config->DumpResources(PrintMessage, NULL);

  delete my_config;
}

void test_CFG_TYPE_STR_VECTOR_OF_DIRS(StorageResource* me)
{
  EXPECT_EQ(me->backend_directories.size(), 9);
  /*  WIN32:
   *  cmake uses some value for PATH_BAREOS_BACKENDDIR,
   *  which ends up in the configuration files
   *  but this is later overwritten in the Director Daemon with ".".
   *  Therefore we skip this test. */
#if !defined(HAVE_WIN32)
  EXPECT_EQ(me->backend_directories.at(0), PATH_BAREOS_BACKENDDIR);
#endif
}

#if HAVE_DYNAMIC_SD_BACKENDS
TEST(ConfigParser_SD, CFG_TYPE_STR_VECTOR_OF_DIRS)
{
  std::string test_name = std::string(
      ::testing::UnitTest::GetInstance()->current_test_info()->name());

  OSDependentInit();

  InitMsg(nullptr, nullptr);

  std::string path_to_config_file
      = std::string("configs/bareos-configparser-tests/bareos-sd-") + test_name
        + std::string(".conf");
  my_config = InitSdConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config->ParseConfig());

  my_config->DumpResources(PrintMessage, NULL);

  std::string sd_resource_name = "bareos-sd";

  StorageResource* me
      = static_cast<StorageResource*>(my_config->GetNextRes(R_STORAGE, NULL));
  EXPECT_EQ(sd_resource_name, me->resource_name_);

  test_CFG_TYPE_STR_VECTOR_OF_DIRS(me);

  TermMsg();

  delete my_config;
}
#endif

}  // namespace storagedaemon
