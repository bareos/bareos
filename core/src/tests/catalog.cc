/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2026 Bareos GmbH & Co. KG

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
#include "cats/bvfs.h"
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
#include <string_view>
#include <vector>

using directordaemon::InitDirConfig;
using directordaemon::my_config;

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

    SetConfiguredWorkingDirectory(working_dir.c_str());
  }

  // parse config
  {
    std::string path_to_config_file = std::string("configs/catalog");
    my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);

    ASSERT_TRUE(my_config->ParseConfig());
  }

  // connect to database
  {
    jcr = directordaemon::NewDirectorJcr(my_config->GetCurrentConfiguration());
    jcr->dir_impl->res.catalog
        = (directordaemon::CatalogResource*)my_config->GetResWithName(
            directordaemon::R_CATALOG, catalog_backend_name.c_str());

    ASSERT_NE(jcr->dir_impl->res.catalog, nullptr);

    // Keep deliberately failing queries on the same transactional connection.
    jcr->dir_impl->res.catalog->try_reconnect = false;
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

  ASSERT_TRUE(db->SqlExec(client_query.c_str()));

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

  ASSERT_TRUE(db->SqlExec(job_query.c_str()));

  result = db->FindLastJobStartTimeForJobAndClient(jcr, "backup-bareos-fd",
                                                   "bareos-fd", stime);

  ASSERT_EQ(result, BareosDb::SqlFindResult::kSuccess)
      << "Preset database entries not found.";

  time_t time_converted = static_cast<time_t>(StrToUtime(stime.data()));

  EXPECT_EQ(time_converted, StrToUtime("2019-11-27 15:04:49"));
}

TEST_F(CatalogTest, EscapeStringPreservesExplicitViewLength)
{
  constexpr char input[] = "a'bcd";

  EXPECT_EQ(db->EscapeString(jcr, std::string_view{input, 3}), "a''b");
}

namespace {
struct BvfsRows {
  std::vector<std::string> names;
  size_t stop_after = 0;
  int stop_code = 1;
};

int CollectBvfsRows(void* ctx, int fields, char** row)
{
  auto& rows = *static_cast<BvfsRows*>(ctx);
  EXPECT_EQ(fields, 6);
  rows.names.emplace_back(row[BVFS_Name]);
  return rows.stop_after && rows.names.size() >= rows.stop_after
             ? rows.stop_code
             : 0;
}
}  // namespace

class BvfsCatalogTest : public CatalogTest {
 protected:
  void SetUp() override
  {
    CatalogTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_TRUE(db->SqlExec("BEGIN"));
    ASSERT_TRUE(db->SqlExec(
        "INSERT INTO Job (JobId,Job,Name,Type,Level,JobStatus,JobTDate) VALUES "
        "(91001,'bvfs-old','bvfs','B','F','T',1),"
        "(91002,'bvfs-new','bvfs','B','I','T',2)"));
    ASSERT_TRUE(db->SqlExec(
        "INSERT INTO Path (PathId,Path) VALUES (91000,'/bvfs/'),"
        "(91001,'/bvfs/a/'),(91002,'/bvfs/b/'),(91003,'/bvfs/c/')"));
    ASSERT_TRUE(
        db->SqlExec("INSERT INTO PathHierarchy (PathId,PPathId) VALUES "
                    "(91001,91000),(91002,91000),(91003,91000)"));
    ASSERT_TRUE(
        db->SqlExec("INSERT INTO PathVisibility (PathId,JobId) "
                    "SELECT PathId,JobId FROM Path CROSS JOIN Job "
                    "WHERE PathId BETWEEN 91000 AND 91003 "
                    "AND JobId IN (91001,91002)"));
    ASSERT_TRUE(
        db->SqlExec("INSERT INTO File (FileIndex,JobId,PathId,LStat,Md5,Name) "
                    "SELECT n,91001,91000,'stat','','file-' || n "
                    "FROM generate_series(1,5) n"));
    ASSERT_TRUE(
        db->SqlExec("INSERT INTO File (FileIndex,JobId,PathId,LStat,Md5,Name) "
                    "VALUES (6,91002,91000,'latest','','file-1')"));
    ASSERT_TRUE(db->SqlExec(
        "INSERT INTO File (FileIndex,JobId,PathId,LStat,Md5,Name) "
        "SELECT 1,JobId,PathId,'stat','','' FROM Path CROSS JOIN Job "
        "WHERE PathId BETWEEN 91001 AND 91003 "
        "AND JobId IN (91001,91002)"));
  }

  void TearDown() override
  {
    if (db) { db->SqlExec("ROLLBACK"); }
    CatalogTest::TearDown();
  }
};

TEST_F(BvfsCatalogTest, FileQueryCountsCallbacksAndPreservesEarlyStop)
{
  PoolMem query;
  query.strcpy("SELECT 'F','1',n::text,'1','','1' FROM generate_series(1,5) n");
  BvfsRows rows;
  EXPECT_EQ(db->BvfsBuildLsFileQuery(query, CollectBvfsRows, &rows), 5);
  EXPECT_EQ(rows.names, (std::vector<std::string>{"1", "2", "3", "4", "5"}));
  EXPECT_EQ(db->BvfsBuildLsFileQuery(query, nullptr, nullptr), 5);

  rows = {};
  rows.stop_after = 2;
  rows.stop_code = -7;
  EXPECT_EQ(db->BvfsBuildLsFileQuery(query, CollectBvfsRows, &rows), 2);
  EXPECT_EQ(rows.names, (std::vector<std::string>{"1", "2"}));

  rows = {};
  query.strcpy("SELECT 'F','1','empty','1','','1' WHERE false");
  EXPECT_EQ(db->BvfsBuildLsFileQuery(query, CollectBvfsRows, &rows), 0);
  EXPECT_TRUE(rows.names.empty());

  query.strcpy("SELECT bvfs_missing_column");
  EXPECT_EQ(db->BvfsBuildLsFileQuery(query, CollectBvfsRows, &rows), -1);
  EXPECT_TRUE(rows.names.empty());
}

TEST_F(BvfsCatalogTest, DirectoryQueryCountsRawRowsBeforeFiltering)
{
  Bvfs fs(jcr, db);
  BvfsRows rows;
  fs.SetHandler(CollectBvfsRows, &rows);
  PoolMem query;
  query.strcpy(
      "SELECT * FROM (VALUES "
      "('D','1','a','2','','1'),('D','1','a','1','','2'),"
      "('F','1','ignored','1','','3'),('D','2','b','1','','4')) AS t");
  EXPECT_EQ(db->BvfsLsDirs(query, &fs), 4);
  EXPECT_EQ(rows.names, (std::vector<std::string>{"a", "b"}));

  for (int page = 0; page < 3; ++page) {
    rows = {};
    Mmsg(query,
         "SELECT * FROM (VALUES "
         "('D','1','a','2','','1'),('D','1','a','1','','2'),"
         "('D','2','b','2','','3'),('D','2','b','1','','4')) AS t "
         "ORDER BY column3,column4 DESC LIMIT 2 OFFSET %d",
         page * 2);
    EXPECT_EQ(db->BvfsLsDirs(query, &fs), page < 2 ? 2 : 0);
    if (page < 2) {
      EXPECT_EQ(rows.names, (std::vector<std::string>{page == 0 ? "a" : "b"}));
    } else {
      EXPECT_TRUE(rows.names.empty());
    }
  }

  rows = {};
  rows.stop_after = 1;
  rows.stop_code = -7;
  query.strcpy(
      "SELECT * FROM (VALUES ('D','1','c','1','','1'),"
      "('D','2','d','1','','2')) AS t");
  EXPECT_EQ(db->BvfsLsDirs(query, &fs), 1);
  EXPECT_EQ(rows.names, (std::vector<std::string>{"c"}));

  query.strcpy("SELECT bvfs_missing_column");
  EXPECT_EQ(db->BvfsLsDirs(query, &fs), -1);
}

TEST_F(BvfsCatalogTest, ListingsReportFullPagesNotGuaranteedContinuation)
{
  Bvfs fs(jcr, db);
  char jobids[] = "91001,91002";
  fs.SetJobids(jobids);
  fs.ChDir(PathId_t{91000});
  BvfsRows rows;
  fs.SetHandler(CollectBvfsRows, &rows);
  fs.SetLimit(2);
  EXPECT_TRUE(fs.ls_files());
  EXPECT_EQ(rows.names, (std::vector<std::string>{"file-1", "file-2"}));
  rows = {};
  fs.SetOffset(2);
  EXPECT_TRUE(fs.ls_files());
  EXPECT_EQ(rows.names, (std::vector<std::string>{"file-3", "file-4"}));
  rows = {};
  fs.SetOffset(4);
  EXPECT_FALSE(fs.ls_files());
  EXPECT_EQ(rows.names, (std::vector<std::string>{"file-5"}));
  rows = {};
  fs.SetOffset(5);
  EXPECT_FALSE(fs.ls_files());
  EXPECT_TRUE(rows.names.empty());
  fs.SetOffset(0);
  fs.SetLimit(5);
  EXPECT_TRUE(fs.ls_files());
  EXPECT_EQ(rows.names.size(), 5);

  rows = {};
  fs.SetLimit(2);
  EXPECT_TRUE(fs.ls_dirs());
  EXPECT_EQ(rows.names, (std::vector<std::string>{".", "/bvfs/a/"}));
  rows = {};
  fs.SetOffset(2);
  EXPECT_TRUE(fs.ls_dirs());
  EXPECT_EQ(rows.names, (std::vector<std::string>{"/bvfs/b/", "/bvfs/c/"}));
  rows = {};
  fs.SetOffset(4);
  EXPECT_FALSE(fs.ls_dirs());
  EXPECT_TRUE(rows.names.empty());
  fs.SetOffset(0);
  fs.SetLimit(5);
  EXPECT_FALSE(fs.ls_dirs());
  EXPECT_EQ(rows.names.size(), 4);

  rows = {};
  fs.SetOffset(0);
  fs.SetLimit(0);
  EXPECT_FALSE(fs.ls_files());
  EXPECT_FALSE(fs.ls_dirs());
  EXPECT_TRUE(rows.names.empty());
  fs.SetLimit(2);
  jobids[0] = 0;
  fs.SetJobids(jobids);
  EXPECT_FALSE(fs.ls_files());
  EXPECT_FALSE(fs.ls_dirs());
  EXPECT_TRUE(rows.names.empty());
}

TEST_F(BvfsCatalogTest, ListingsHonorCallbackStopAndReportQueryFailure)
{
  Bvfs fs(jcr, db);
  char jobids[] = "91001,91002";
  fs.SetJobids(jobids);
  fs.ChDir(PathId_t{91000});
  fs.SetLimit(2);
  BvfsRows rows;
  rows.stop_after = 1;
  rows.stop_code = -7;
  fs.SetHandler(CollectBvfsRows, &rows);
  EXPECT_FALSE(fs.ls_files());
  EXPECT_EQ(rows.names.size(), 1);
  rows.names.clear();
  EXPECT_FALSE(fs.ls_dirs());
  EXPECT_EQ(rows.names.size(), 1);

  char invalid_jobids[] = "bvfs_missing_column";
  fs.SetJobids(invalid_jobids);
  rows = {};
  const auto errors = jcr->JobErrors;
  ASSERT_TRUE(db->SqlExec("SAVEPOINT bvfs_failure"));
  EXPECT_FALSE(fs.ls_files());
  EXPECT_EQ(jcr->JobErrors, errors + 1);
  ASSERT_TRUE(db->SqlExec("ROLLBACK TO SAVEPOINT bvfs_failure"));
  EXPECT_FALSE(fs.ls_dirs());
  EXPECT_EQ(jcr->JobErrors, errors + 2);
  EXPECT_TRUE(rows.names.empty());
}
