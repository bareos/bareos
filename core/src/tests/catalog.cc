/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2024 Bareos GmbH & Co. KG

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
#include "cats/sql_pooling.h"
#include "dird/get_database_connection.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/job.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "dird/jcr_util.h"

#include <string>
#include <vector>

using directordaemon::InitDirConfig;
using directordaemon::my_config;

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}

class CatalogTest : public ::testing::Test {
 protected:
  std::string catalog_backend_name;
  std::string config_dir;
  std::string working_dir;

  JobControlRecord* jcr{};
  BareosDb* db{};

  void SetUp() override;
  void TearDown() override;
};

void CatalogTest::SetUp()
{
  // get environment
  {
    catalog_backend_name = getenv_std_string("DBTYPE");
    config_dir = getenv_std_string("BAREOS_CONFIG_DIR");
    working_dir = getenv_std_string("BAREOS_WORKING_DIR");

    ASSERT_EQ(catalog_backend_name, "postgresql")
        << "Environment variable DBTYPE does not contain correct database "
           "backend: "
        << "<" << catalog_backend_name << ">";
    ASSERT_FALSE(config_dir.empty())
        << "Environment variable BAREOS_CONFIG_DIR not set.";
    ASSERT_FALSE(working_dir.empty())
        << "Environment variable BAREOS_WORKING_DIR not set.";

    SetWorkingDirectory(working_dir.c_str());
  }

  // parse config
  {
    std::string path_to_config_file = std::string("configs/catalog");
    my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);

    ASSERT_TRUE(my_config->ParseConfig());
  }

  // connect to database
  {
    jcr = directordaemon::NewDirectorJcr(directordaemon::DirdFreeJcr);
    jcr->dir_impl->res.catalog
        = (directordaemon::CatalogResource*)my_config->GetResWithName(
            directordaemon::R_CATALOG, catalog_backend_name.c_str());

    ASSERT_NE(jcr->dir_impl->res.catalog, nullptr);

    db = directordaemon::GetDatabaseConnection(jcr);

    ASSERT_NE(db, nullptr);
  }
}

void CatalogTest::TearDown()
{
  db->CloseDatabase(jcr);
  DbSqlPoolDestroy();

  if (jcr) {
    FreeJcr(jcr);
    jcr = nullptr;
  }

  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }
}

TEST_F(CatalogTest, database)
{
  std::vector<char> stime;
  auto result = db->FindLastJobStartTimeForJobAndClient(jcr, "backup-bareos-fd",
                                                        "bareos-fd", stime);

  EXPECT_EQ(result, BareosDb::SqlFindResult::kEmptyResultSet)
      << "Resultset should be empty.";

  std::string client_query{
      "INSERT INTO Client "
      " (Name, Uname)"
      " VALUES( "
      "  'bareos-fd',"
      "  '19.2.4~pre1035.d5f227724 (22Nov19) "
      "Linux-5.3.11-200.fc30.x86_64,redhat,Fedora release 30 (Thirty)')"};

  ASSERT_TRUE(db->SqlQuery(client_query.c_str(), 0));

  std::string job_query{
      "INSERT INTO Job "
      " (Job, Name, Type, Level, ClientId, JobStatus, StartTime, SchedTime)"
      " VALUES( "
      "  'backup-bareos-fd.2019-11-27_15.04.49_04', "
      "  'backup-bareos-fd', "
      "  'B', "
      "  'F', "
      "   1,  "
      "  'T', "
      "  '2019-11-27 15:04:49', "
      "  '2019-11-27 15:04:48') "};

  ASSERT_TRUE(db->SqlQuery(job_query.c_str(), 0));

  result = db->FindLastJobStartTimeForJobAndClient(jcr, "backup-bareos-fd",
                                                   "bareos-fd", stime);

  ASSERT_EQ(result, BareosDb::SqlFindResult::kSuccess)
      << "Preset database entries not found.";

  time_t time_converted = static_cast<time_t>(StrToUtime(stime.data()));

  EXPECT_EQ(time_converted, StrToUtime("2019-11-27 15:04:49"));
}
