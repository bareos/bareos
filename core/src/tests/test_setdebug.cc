/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/parse_conf.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "dird/ua_cmds.h"

using namespace std;
using namespace directordaemon;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

class SetdebugTest : public ::testing::Test {
 protected:
  static void SetUpTestCase();
};

static std::multiset<std::string> client_names;
static std::multiset<std::string> stored_names;

static void DoClientSetdebug(UaContext* ua,
                             ClientResource* client,
                             int level,
                             int trace_flag,
                             int hangup_flag,
                             int timestamp_flag)
{
  client_names.insert(client->resource_name_);
}

static void DoStorageSetdebug(UaContext* ua,
                              StorageResource* store,
                              int level,
                              int trace_flag,
                              int timestamp_flag)
{
  stored_names.insert(store->resource_name_);
}

void SetdebugTest::SetUpTestCase()
{
  std::string path_to_config_file = std::string(
      PROJECT_SOURCE_DIR "/src/tests/configs/bareos-configparser-tests");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);
  ASSERT_TRUE(my_config->ParseConfig());

  SetDoClientSetdebugFunction(DoClientSetdebug);
  SetDoStorageSetdebugFunction(DoStorageSetdebug);

  DoAllSetDebug(nullptr, 0, 0, 0, 0);

  if (my_config) { delete my_config; }
}

TEST_F(SetdebugTest, test_clients_list)
{
  ASSERT_EQ(client_names.size(), 3);
  EXPECT_EQ(client_names.count("bareos-fd"), 1);
  EXPECT_EQ(client_names.count("bareos-fd2"), 1);
  EXPECT_EQ(client_names.count("bareos-fd3"), 1);
  EXPECT_EQ(client_names.count("bareos-fd-duplicate-interface"), 0);
}

TEST_F(SetdebugTest, test_storages_list)
{
  ASSERT_EQ(stored_names.size(), 3);
  EXPECT_EQ(stored_names.count("File"), 1);
  EXPECT_EQ(stored_names.count("File2"), 1);
  EXPECT_EQ(stored_names.count("File3"), 1);
  EXPECT_EQ(stored_names.count("File-duplicate-interface"), 0);
}
