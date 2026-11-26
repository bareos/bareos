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
#include "lib/output_formatter.h"
#include "lib/util.h"
#include "dird/jcr_util.h"

#include <cstdarg>
#include <jansson.h>
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

TEST_F(CatalogTest, big_query_stops_when_handler_requests_it)
{
  int rows = 0;
  auto stop_after_first = [](void* context, int, char**) {
    ++*static_cast<int*>(context);
    return 1;
  };
  ASSERT_TRUE(db->BigSqlQuery("SELECT n FROM generate_series(1, 350) AS n",
                              stop_after_first, &rows));
  EXPECT_EQ(rows, 1);
}

static bool CaptureCatalogOutput(void* context, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  PoolMem output;
  output.Bvsprintf(format, args);
  va_end(args);
  *static_cast<std::string*>(context) += output.c_str();
  return true;
}

TEST_F(CatalogTest, filtered_job_pages_cross_cursor_batches)
{
  struct Cleanup {
    BareosDb* db;
    ~Cleanup() { db->SqlExec("DELETE FROM Job WHERE Name LIKE 'cr-page-%'"); }
  } cleanup{db};
  ASSERT_TRUE(db->SqlExec(
      "INSERT INTO Job (Job, Name, Type, Level, JobStatus) "
      "SELECT 'cr-page-' || n, CASE WHEN n <= 120 THEN 'cr-page-allowed' "
      "ELSE 'cr-page-denied' END, 'B', 'F', 'T' FROM generate_series(1, 340) "
      "n"));

  auto filter = [](void* context, void* data, of_filter_tuple* tuple) {
    ++*static_cast<int*>(context);
    const auto row = static_cast<char**>(data);
    return strcmp(row[tuple->u.acl_filter.column], "cr-page-allowed") == 0
               ? OF_FILTER_STATE_SHOW
               : OF_FILTER_STATE_SUPPRESS;
  };
  for (const auto type : {HORZ_LIST, VERT_LIST}) {
    for (const bool descending : {false, true}) {
      for (const bool last : {false, true}) {
        std::vector<std::string> expected;
        auto list = [&](uint64_t limit, uint64_t offset, bool paginated) {
          std::string output;
          int filter_calls = 0;
          OutputFormatter formatter(CaptureCatalogOutput, &output, filter,
                                    &filter_calls, API_MODE_JSON);
          formatter.AddAclFilterTuple(type == VERT_LIST ? 2 : 1, 0);
          JobDbRecord jr{};
          jr.limit = limit;
          jr.offset = offset;
          const std::string range
              = paginated ? " LIMIT " + std::to_string(limit) + " OFFSET "
                                + std::to_string(offset)
                          : "";
          db->ListJobRecords(jcr, &jr, range.c_str(), nullptr, {}, {}, {},
                             nullptr, nullptr, 0, last, false, &formatter, type,
                             descending, "jobid", "cr-page-");
          if (paginated && limit == 2 && offset == 0 && !last) {
            EXPECT_LE(filter_calls, descending ? 224 : 4);
          }
          formatter.FinalizeResult(true);
          json_error_t error{};
          json_t* root = json_loads(output.c_str(), 0, &error);
          EXPECT_NE(root, nullptr) << output << " " << error.text;
          std::vector<std::string> ids;
          if (root) {
            json_t* jobs
                = json_object_get(json_object_get(root, "result"), "jobs");
            EXPECT_TRUE(json_is_array(jobs)) << output;
            size_t i;
            json_t* job;
            json_array_foreach(jobs, i, job)
            {
              const char* id = json_string_value(json_object_get(job, "jobid"));
              EXPECT_NE(id, nullptr);
              if (id) { ids.emplace_back(id); }
            }
            json_decref(root);
          }
          return ids;
        };
        expected = list(0, 0, false);
        ASSERT_EQ(expected.size(), last ? 1 : 120);
        for (const uint64_t offset : {0, 1, 99, 100, 119, 120}) {
          const auto start = std::min<size_t>(offset, expected.size());
          const auto end = std::min(start + 2, expected.size());
          EXPECT_EQ(list(2, offset, true),
                    (std::vector<std::string>(expected.begin() + start,
                                              expected.begin() + end)));
        }
        EXPECT_TRUE(list(0, 0, true).empty());
      }
    }
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
