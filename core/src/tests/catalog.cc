/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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

#include "include/bareos.h"
#include "cats/cats.h"
#include "cats/cats_backends.h"
#include "cats/sql_pooling.h"
#include "dird/get_database_connection.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/run_on_incoming_connect_interval.h"
#include "dird/scheduler.h"
#include "dird/scheduler_time_adapter.h"
#include "gmock/gmock.h"
#include "include/bareos.h"
#include "include/jcr.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

using directordaemon::InitDirConfig;
using directordaemon::my_config;

static std::string catalog_backend_name;
static std::string backend_dirs;

static std::string getenv_std_string(std::string env_var)
{
  auto v{std::getenv(env_var.c_str())};
  return v ? std::string(v) : std::string();
}

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);

  catalog_backend_name = getenv_std_string("CATALOG_BACKEND");
  backend_dirs = getenv_std_string("BACKEND_DIRS");

  return RUN_ALL_TESTS();
}

class CatalogTest : public ::testing::Test {
  void SetUp() override;
  void TearDown() override;
};

void CatalogTest::SetUp()
{
  InitMsg(nullptr, nullptr);

  std::string path_to_config_file =
      std::string(PROJECT_SOURCE_DIR "/src/tests/configs/catalog");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);

  ASSERT_TRUE(my_config->ParseConfig());
  ASSERT_FALSE(backend_dirs.empty())
      << "Environment variable BACKEND_DIRS not set.";

  directordaemon::me = (directordaemon::DirectorResource*)my_config->GetNextRes(
      directordaemon::R_DIRECTOR, NULL);
}

void CatalogTest::TearDown()
{
  DbSqlPoolDestroy();
  DbFlushBackends();

  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }
}

TEST_F(CatalogTest, database)
{
  std::set<std::string> testable_catalog_backends{"postgresql", "sqlite3",
                                                  "mysql"};

  ASSERT_NE(testable_catalog_backends.find(catalog_backend_name),
            testable_catalog_backends.end())
      << "Environment variable CATALOG_BACKEND does not contain a name for a "
         "testable catalog backend.";

  JobControlRecord* jcr = directordaemon::NewDirectorJcr();
  jcr->impl->res.catalog =
      (directordaemon::CatalogResource*)my_config->GetResWithName(
          directordaemon::R_CATALOG, catalog_backend_name.c_str());

  ASSERT_NE(jcr->impl->res.catalog, nullptr);

  DbSetBackendDirs(std::vector<std::string>{backend_dirs});
  BareosDb* db = directordaemon::GetDatabaseConnection(jcr);

  ASSERT_NE(db, nullptr);
}
