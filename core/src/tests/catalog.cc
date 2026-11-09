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
#include "lib/edit.h"
#include "lib/util.h"
#include "dird/jcr_util.h"
#include "dird/ua_acct.h"
#include "dird/ua_acct_internal.h"

#include <chrono>
#include <string>
#include <string_view>
#include <thread>
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
  BareosDb* secondary_db{};
  bool secondary_catalog_created{false};

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
    jcr = directordaemon::NewDirectorJcr(my_config->GetCurrentConfiguration());
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
  directordaemon::StopSubscriptionAccountingThread();
  if (secondary_db) { secondary_db->CloseDatabase(jcr); }
  if (secondary_catalog_created) {
    EXPECT_TRUE(db->SqlExec("DROP DATABASE regress_catalog_secondary"));
  }
  if (db) { db->CloseDatabase(jcr); }
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

TEST_F(CatalogTest, AccountingWorkerUsesPrivateConnection)
{
  jcr->dir_impl->res.catalog->mult_db_connections = false;
  const auto count_connections = [&] {
    int connections = 0;
    EXPECT_TRUE(db->SqlQuery(
        "SELECT COUNT(*) FROM pg_stat_activity "
        "WHERE datname=current_database() AND backend_type='client backend'",
        [](void* ctx, int, char** row) {
          *static_cast<int*>(ctx) = str_to_int64(row[0]);
          return 0;
        },
        &connections));
    return connections;
  };
  const int connections_before = count_connections();
  ASSERT_TRUE(directordaemon::StartSubscriptionAccountingThread());
  for (int attempt = 0;
       attempt < 500
       && !directordaemon::GetSubscriptionAccountingThreadStatus().available;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_TRUE(
      directordaemon::GetSubscriptionAccountingThreadStatus().available);
  EXPECT_EQ(count_connections(), connections_before + 1);

  auto* shared = directordaemon::GetDatabaseConnection(jcr);
  EXPECT_EQ(shared, db);
  if (shared) { DbSqlClosePooledConnection(jcr, shared); }

  directordaemon::StopSubscriptionAccountingThread();
  EXPECT_EQ(count_connections(), connections_before);
}

TEST_F(CatalogTest, AccountingDeltaUsesBoundedCursorFetches)
{
  // Keep intentional SQL failures from resetting the session-local fixtures.
  jcr->dir_impl->res.catalog->try_reconnect = false;
  db->CloseDatabase(jcr);
  db = directordaemon::GetDatabaseConnection(jcr);
  ASSERT_NE(db, nullptr);
  ASSERT_TRUE(db->SqlExec(R"SQL(
    CREATE TEMP TABLE Client (LIKE public.Client INCLUDING DEFAULTS);
    CREATE TEMP TABLE FileSet (LIKE public.FileSet INCLUDING DEFAULTS);
    CREATE TEMP TABLE Job (LIKE public.Job INCLUDING DEFAULTS);
    CREATE TEMP TABLE Path (LIKE public.Path INCLUDING DEFAULTS);
    CREATE TEMP TABLE acct_files (LIKE public.File INCLUDING DEFAULTS);
    INSERT INTO Client (ClientId, Name, Uname) VALUES (1, 'acct-cursor', 'Linux');
    INSERT INTO FileSet (FileSetId, FileSet, Md5, CreateTime)
      VALUES (1, 'AcctCursor', 'cursor', CURRENT_TIMESTAMP);
    INSERT INTO Path (PathId, Path) VALUES (1, '/accounting-cursor/');
    INSERT INTO Job (JobId, Job, Name, Type, Level, ClientId, FileSetId,
                     JobStatus, JobTDate)
      VALUES (1, 'cursor-full', 'cursor', 'B', 'F', 1, 1, 'T', 2000000000),
             (2, 'cursor-inc', 'cursor', 'B', 'I', 1, 1, 'T', 2000000001);
    UPDATE Job SET AccountingValid=TRUE, AccountingExcluded=FALSE,
      AccountingFiles=0, AccountingBytes=0, AccountingLogicalBytes=0,
      AccountingOpaqueFiles=0, AccountingNdmpArchives=0,
      AccountingRule='st_blocks*512', AccountingJobsInChain=1 WHERE JobId=1;
    INSERT INTO acct_files (FileId, FileIndex, JobId, PathId, Name, LStat, Md5)
      SELECT n, n, 2, 1, 'file-' || n,
             'A A IGk B A A A B BAA B A A A A A A', ''
      FROM generate_series(1, 101) n;
    CREATE TEMP TABLE acct_fetch_probe (query TEXT NOT NULL);
    CREATE FUNCTION pg_temp.acct_probe_lstat(value TEXT) RETURNS TEXT
      LANGUAGE plpgsql VOLATILE AS $$
    BEGIN
      IF NOT EXISTS (SELECT 1 FROM pg_cursors WHERE name='_bar_cursor')
         OR current_query() <> 'FETCH 100 FROM _bar_cursor' THEN
        RAISE EXCEPTION 'changed-file scan did not use a bounded cursor fetch';
      END IF;
      INSERT INTO acct_fetch_probe VALUES (current_query());
      RETURN value;
    END;
    $$;
    CREATE TEMP VIEW File AS
      SELECT FileId, FileIndex, JobId, PathId, DeltaSeq, MarkId, Fhinfo, Fhnode,
             pg_temp.acct_probe_lstat(LStat) AS LStat, Md5, Name FROM acct_files;
  )SQL"))
      << db->strerror();

  directordaemon::UaContext ua(jcr);
  ua.db = db;
  ua.SetConsoleConnected(false);
  directordaemon::AccountingRow delta;
  std::string error;
  ASSERT_TRUE(directordaemon::CalculateSubscriptionAccountingForJob(
      &ua, 2, &delta, &error))
      << error << ": " << db->strerror();
  EXPECT_EQ(delta.files, 101);
  EXPECT_EQ(delta.bytes, 101 * 512);
  EXPECT_EQ(delta.logical_bytes, 101);
  EXPECT_EQ(delta.jobs_in_chain, 2);

  int observed_rows = 0;
  ASSERT_TRUE(db->SqlQuery(
      "SELECT COUNT(*) FROM acct_fetch_probe",
      [](void* ctx, int, char** row) {
        *static_cast<int*>(ctx) = str_to_int64(row[0]);
        return 0;
      },
      &observed_rows));
  EXPECT_EQ(observed_rows, 101);

  ASSERT_TRUE(
      db->SqlExec("UPDATE Job SET AccountingValid=FALSE WHERE JobId=1"));
  directordaemon::AccountingRow full;
  ASSERT_TRUE(directordaemon::CalculateSubscriptionAccountingForJob(
      &ua, 2, &full, &error))
      << error;
  EXPECT_EQ(full.files, delta.files);
  EXPECT_EQ(full.bytes, delta.bytes);
  EXPECT_EQ(full.logical_bytes, delta.logical_bytes);

  std::atomic_bool cancelled{true};
  EXPECT_FALSE(directordaemon::CalculateSubscriptionAccountingForJob(
      &ua, 2, &full, &error, &cancelled));
  EXPECT_EQ(error, "accounting calculation cancelled");

  ASSERT_TRUE(db->SqlExec(R"SQL(
    UPDATE Job SET AccountingValid=TRUE WHERE JobId=1;
    CREATE OR REPLACE FUNCTION pg_temp.acct_probe_lstat(value TEXT) RETURNS TEXT
      LANGUAGE plpgsql VOLATILE AS $$
    BEGIN
      RAISE EXCEPTION 'injected accounting scan failure';
    END;
    $$;
  )SQL"));
  EXPECT_FALSE(directordaemon::CalculateSubscriptionAccountingForJob(
      &ua, 2, &delta, &error));
  EXPECT_EQ(error, "failed to scan changed file attributes");
  EXPECT_TRUE(db->SqlExec("SELECT 1"));
}

TEST_F(CatalogTest, AccountingScansPreferLatestFileId)
{
  ASSERT_TRUE(db->SqlExec(R"SQL(
    CREATE TEMP TABLE Client (LIKE public.Client INCLUDING DEFAULTS);
    CREATE TEMP TABLE FileSet (LIKE public.FileSet INCLUDING DEFAULTS);
    CREATE TEMP TABLE Job (LIKE public.Job INCLUDING DEFAULTS);
    CREATE TEMP TABLE Path (LIKE public.Path INCLUDING DEFAULTS);
    CREATE TEMP TABLE File (LIKE public.File INCLUDING DEFAULTS);
    INSERT INTO Client (ClientId, Name, Uname) VALUES (1, 'acct-tie', 'Linux');
    INSERT INTO FileSet (FileSetId, FileSet, Md5, CreateTime)
      VALUES (1, 'AcctTie', 'tie', CURRENT_TIMESTAMP);
    INSERT INTO Path (PathId, Path) VALUES (1, '/accounting-tie/');
    INSERT INTO Job (JobId, Job, Name, Type, Level, ClientId, FileSetId,
                     JobStatus, JobTDate)
      VALUES (1, 'tie-full', 'tie', 'B', 'F', 1, 1, 'T', 2000000000),
             (2, 'tie-inc', 'tie', 'B', 'I', 1, 1, 'E', 2000000001);
    UPDATE Job SET AccountingValid=TRUE, AccountingExcluded=FALSE,
      AccountingFiles=1, AccountingBytes=1024, AccountingLogicalBytes=2,
      AccountingOpaqueFiles=0, AccountingNdmpArchives=0,
      AccountingRule='st_blocks*512', AccountingJobsInChain=1 WHERE JobId=1;
    INSERT INTO File (FileId, FileIndex, JobId, PathId, Name, LStat, Md5)
      VALUES (2, 2, 1, 1, 'same', 'A A IGk B A A A C BAA C A A A A A A', ''),
             (3, 1, 2, 1, 'same', 'A A IGk B A A A D BAA D A A A A A A', ''),
             (4, 2, 2, 1, 'same', 'A A IGk B A A A E BAA E A A A A A A', '');
  )SQL"))
      << db->strerror();

  directordaemon::UaContext ua(jcr);
  ua.db = db;
  ua.SetConsoleConnected(false);
  directordaemon::AccountingRow full;
  std::string error;
  ASSERT_TRUE(directordaemon::CalculateSubscriptionAccountingForJob(
      &ua, 1, &full, &error))
      << error;
  EXPECT_EQ(full.files, 1);
  EXPECT_EQ(full.bytes, 1024);
  EXPECT_EQ(full.logical_bytes, 2);

  ASSERT_TRUE(db->SqlExec("UPDATE Job SET JobStatus='T' WHERE JobId=2"));
  directordaemon::AccountingRow delta;
  ASSERT_TRUE(directordaemon::CalculateSubscriptionAccountingForJob(
      &ua, 2, &delta, &error))
      << error;
  EXPECT_EQ(delta.files, 1);
  EXPECT_EQ(delta.bytes, 2048);
  EXPECT_EQ(delta.logical_bytes, 4);

  std::vector<directordaemon::AccountingRow> recalculated;
  ASSERT_TRUE(directordaemon::CalculateSubscriptionAccounting(
      &ua, &recalculated, &error))
      << error;
  ASSERT_EQ(recalculated.size(), 1);
  EXPECT_EQ(recalculated[0].files, delta.files);
  EXPECT_EQ(recalculated[0].bytes, delta.bytes);
  EXPECT_EQ(recalculated[0].logical_bytes, delta.logical_bytes);
}

TEST_F(CatalogTest, SecondaryCatalogCompletionDoesNotRefreshPrimary)
{
  ASSERT_TRUE(db->SqlExec(R"SQL(
    INSERT INTO FileSet (FileSetId, FileSet, Md5, CreateTime)
      VALUES (1, 'AcctCatalogGate', 'gate', CURRENT_TIMESTAMP);
    INSERT INTO Path (PathId, Path) VALUES (1, '/accounting-catalog/');
    INSERT INTO File (FileId, FileIndex, JobId, PathId, Name, LStat, Md5)
      VALUES (1, 1, 1, 1, 'regular', 'A A IGk B A A A C BAA C A A A A A A', '');
    UPDATE Job SET FileSetId=1, AccountingValid=TRUE, AccountingExcluded=FALSE,
      AccountingFiles=1, AccountingBytes=512, AccountingLogicalBytes=1,
      AccountingRule='st_blocks*512', AccountingJobsInChain=1 WHERE JobId=1;
    UPDATE SubscriptionAccountingSnapshot
      SET LastSuccess=CURRENT_TIMESTAMP, LastError=NULL WHERE SnapshotId=1;
  )SQL"))
      << db->strerror();
  ASSERT_TRUE(db->SqlExec(
      "CREATE DATABASE regress_catalog_secondary TEMPLATE regress_catalog"));
  secondary_catalog_created = true;

  auto* primary = jcr->dir_impl->res.catalog;
  directordaemon::CatalogResource secondary;
  char secondary_name[] = "secondary";
  char secondary_database[] = "regress_catalog_secondary";
  secondary.resource_name_ = secondary_name;
  secondary.db_name = secondary_database;
  secondary.db_user = primary->db_user;
  secondary.db_password = primary->db_password;
  secondary.db_address = primary->db_address;
  secondary.db_port = primary->db_port;
  secondary.db_socket = primary->db_socket;
  jcr->dir_impl->res.catalog = &secondary;
  secondary_db = directordaemon::GetDatabaseConnection(jcr, true);
  jcr->dir_impl->res.catalog = primary;
  ASSERT_NE(secondary_db, nullptr);

  const auto wait_for_idle = [] {
    for (int attempt = 0; attempt < 500; ++attempt) {
      const auto status
          = directordaemon::GetSubscriptionAccountingThreadStatus();
      if (status.available && !status.running && !status.queued) { return; }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ADD_FAILURE() << "Accounting worker did not become idle";
  };
  const auto complete
      = [&](BareosDb* connection, directordaemon::CatalogResource* catalog) {
          jcr->db = connection;
          jcr->dir_impl->res.catalog = catalog;
          jcr->JobId = 1;
          jcr->setJobType(JT_BACKUP);
          jcr->setJobStatus(JS_Terminated);
          jcr->dir_impl->jr.JobLevel = L_FULL;
          jcr->dir_impl->jr.ClientId = 1;
          jcr->dir_impl->jr.FileSetId = 1;
          directordaemon::UpdateJobEndRecord(jcr);
          jcr->JobId = 0;
          jcr->db = nullptr;
          jcr->dir_impl->res.catalog = primary;
        };
  const auto read_value = [&](const char* query) {
    std::string value;
    EXPECT_TRUE(db->SqlQuery(
        query,
        [](void* ctx, int, char** row) {
          *static_cast<std::string*>(ctx) = row[0];
          return 0;
        },
        &value));
    return value;
  };

  ASSERT_TRUE(directordaemon::StartSubscriptionAccountingThread());
  wait_for_idle();
  complete(secondary_db, &secondary);
  wait_for_idle();
  EXPECT_EQ(read_value("SELECT AccountingBytes FROM Job WHERE JobId=1"), "512");

  ASSERT_TRUE(db->SqlExec("UPDATE File SET LStat='' WHERE JobId=1"));
  complete(secondary_db, &secondary);
  wait_for_idle();
  EXPECT_EQ(read_value("SELECT COALESCE(LastError, '') FROM "
                       "SubscriptionAccountingSnapshot WHERE SnapshotId=1"),
            "");

  ASSERT_TRUE(
      db->SqlExec("UPDATE File SET LStat='A A IGk B A A A C BAA C A A A A A A' "
                  "WHERE JobId=1"));
  complete(db, primary);
  wait_for_idle();
  EXPECT_EQ(read_value("SELECT AccountingBytes FROM Job WHERE JobId=1"),
            "1024");
}
