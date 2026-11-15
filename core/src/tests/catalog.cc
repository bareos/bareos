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
#include "dird/catreq.h"
#include "dird/get_database_connection.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/job.h"
#include "dird/next_vol.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/bsock_tcp.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "dird/jcr_util.h"

#include <chrono>
#include <future>
#include <limits>
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

TEST_F(CatalogTest, StalePoolUpdateDoesNotChangeOtherChangerSlots)
{
  ASSERT_TRUE(db->SqlExec(
      "INSERT INTO Media "
      "(VolumeName,MediaType,VolStatus,PoolId,StorageId,Slot,InChanger) "
      "VALUES ('pool-guard-stale','File','Full',11,0,0,0),"
      "('pool-guard-other','File','Full',22,7,4,1)"));

  MediaDbRecord stale;
  bstrncpy(stale.VolumeName, "pool-guard-stale", sizeof(stale.VolumeName));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  stale.Slot = 4;
  stale.InChanger = 1;
  stale.StorageId = 7;
  ASSERT_TRUE(db->SqlExec(
      "UPDATE Media SET PoolId=22 WHERE VolumeName='pool-guard-stale'"));

  bool pool_changed = false;
  EXPECT_FALSE(db->UpdateMediaRecord(jcr, &stale, false, true, &pool_changed));
  EXPECT_TRUE(pool_changed);

  MediaDbRecord current;
  bstrncpy(current.VolumeName, "pool-guard-stale", sizeof(current.VolumeName));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &current));
  EXPECT_EQ(current.PoolId, 22);
  bstrncpy(current.VolumeName, "pool-guard-other", sizeof(current.VolumeName));
  current.MediaId = 0;
  ASSERT_TRUE(db->GetMediaRecord(jcr, &current));
  EXPECT_EQ(current.InChanger, 1);
  EXPECT_EQ(current.Slot, 4);
}

TEST_F(CatalogTest, StalePoolDefaultsDoNotOverwriteMovedVolume)
{
  ASSERT_TRUE(
      db->SqlExec("INSERT INTO Media "
                  "(VolumeName,MediaType,VolStatus,PoolId,VolRetention) "
                  "VALUES ('pool-guard-defaults','File','Full',11,123)"));
  MediaDbRecord stale;
  bstrncpy(stale.VolumeName, "pool-guard-defaults", sizeof(stale.VolumeName));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  stale.VolRetention = 1;
  ASSERT_TRUE(db->SqlExec(
      "UPDATE Media SET PoolId=22 WHERE VolumeName='pool-guard-defaults'"));
  EXPECT_FALSE(db->UpdateMediaDefaults(jcr, &stale));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  EXPECT_EQ(stale.PoolId, 22);
  EXPECT_EQ(stale.VolRetention, 123);
}

TEST_F(CatalogTest, AutomaticUpdatePreservesMovedVolumePoolAndDefaults)
{
  ASSERT_TRUE(
      db->SqlExec("INSERT INTO Media "
                  "(VolumeName,MediaType,VolStatus,PoolId,VolRetention) "
                  "VALUES ('pool-guard-automatic','File','Append',11,1)"));
  MediaDbRecord stale;
  bstrncpy(stale.VolumeName, "pool-guard-automatic", sizeof(stale.VolumeName));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  ASSERT_TRUE(db->SqlExec(
      "UPDATE Media SET PoolId=22,VolRetention=123,VolUseDuration=456,"
      "VolStatus='Cleaning',MaxVolBytes=100,MaxVolJobs=2,MaxVolFiles=3,Recycle="
      "1,"
      "RecyclePoolId=4,ActionOnPurge=1,MinBlocksize=5,MaxBlocksize=6 "
      "WHERE VolumeName='pool-guard-automatic'"));
  stale.VolBytes = 42;
  stale.FirstWritten = 1700000000;
  stale.LastWritten = 1700000010;
  stale.set_first_written = true;
  ASSERT_TRUE(db->UpdateMediaRecord(jcr, &stale, true));
  EXPECT_STREQ(stale.VolStatus, "Cleaning");
  EXPECT_EQ(stale.PoolId, 22);
  EXPECT_EQ(stale.VolRetention, 123);
  EXPECT_EQ(stale.VolUseDuration, 456);
  EXPECT_EQ(stale.MaxVolBytes, 100);
  EXPECT_EQ(stale.MaxVolJobs, 2);
  EXPECT_EQ(stale.MaxVolFiles, 3);
  EXPECT_EQ(stale.Recycle, 1);
  EXPECT_EQ(stale.RecyclePoolId, 4);
  EXPECT_EQ(stale.ActionOnPurge, 1);
  EXPECT_EQ(stale.MinBlocksize, 5);
  EXPECT_EQ(stale.MaxBlocksize, 6);
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  EXPECT_EQ(stale.PoolId, 22);
  EXPECT_EQ(stale.VolRetention, 123);
  EXPECT_EQ(stale.VolUseDuration, 456);
  EXPECT_EQ(stale.MaxVolBytes, 100);
  EXPECT_EQ(stale.MaxVolJobs, 2);
  EXPECT_EQ(stale.MaxVolFiles, 3);
  EXPECT_EQ(stale.Recycle, 1);
  EXPECT_EQ(stale.RecyclePoolId, 4);
  EXPECT_EQ(stale.ActionOnPurge, 1);
  EXPECT_EQ(stale.MinBlocksize, 5);
  EXPECT_EQ(stale.MaxBlocksize, 6);
  EXPECT_EQ(stale.VolBytes, 42);
  EXPECT_EQ(stale.FirstWritten, 1700000000);
  EXPECT_EQ(stale.LastWritten, 1700000010);
  stale.VolRetention = 789;
  ASSERT_TRUE(db->UpdateMediaRecord(jcr, &stale, true));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  EXPECT_EQ(stale.VolRetention, 789);
}

TEST_F(CatalogTest, ExpirationRechecksLimitsAfterPoolMove)
{
  ASSERT_TRUE(db->SqlExec(
      "INSERT INTO Media (VolumeName,MediaType,VolStatus,PoolId,VolJobs,"
      "MaxVolJobs) VALUES ('expired-pool-move','File','Append',11,1,1)"));
  MediaDbRecord stale;
  bstrncpy(stale.VolumeName, "expired-pool-move", sizeof(stale.VolumeName));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  ASSERT_TRUE(
      db->SqlExec("UPDATE Media SET PoolId=22,MaxVolJobs=100 "
                  "WHERE VolumeName='expired-pool-move'"));

  jcr->db = db;
  const bool expired = directordaemon::HasVolumeExpired(jcr, &stale);
  jcr->db = nullptr;
  EXPECT_FALSE(expired);
  EXPECT_EQ(stale.PoolId, 22);
  EXPECT_STREQ(stale.VolStatus, "Append");
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  EXPECT_STREQ(stale.VolStatus, "Append");
  EXPECT_EQ(stale.MaxVolJobs, 100);

  ASSERT_TRUE(
      db->SqlExec("UPDATE Media SET PoolId=11,MaxVolJobs=1 "
                  "WHERE VolumeName='expired-pool-move'"));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  ASSERT_TRUE(db->SqlExec(
      "UPDATE Media SET PoolId=22 WHERE VolumeName='expired-pool-move'"));
  jcr->db = db;
  const bool still_expired = directordaemon::HasVolumeExpired(jcr, &stale);
  jcr->db = nullptr;
  EXPECT_TRUE(still_expired);
  EXPECT_EQ(stale.PoolId, 22);
  EXPECT_STREQ(stale.VolStatus, "Used");
  ASSERT_TRUE(db->GetMediaRecord(jcr, &stale));
  EXPECT_STREQ(stale.VolStatus, "Used");
}

TEST_F(CatalogTest, VolumeSelectionKeepsRequestedPoolAfterConcurrentMove)
{
  directordaemon::StorageResource storage;
  directordaemon::PoolResource pool;
  char media_type[] = "selection-pool-test";
  storage.media_type = media_type;
  jcr->dir_impl->res.write_storage = &storage;
  jcr->dir_impl->res.pool = &pool;
  BareosDb* reader = db->CloneDatabaseConnection(jcr, true, false, true);
  ASSERT_NE(reader, nullptr);
  jcr->db = reader;

  for (int new_limit : {1, 100}) {
    ASSERT_TRUE(db->SqlExec(
        "DELETE FROM Media WHERE MediaType='selection-pool-test';"
        "INSERT INTO Media "
        "(VolumeName,MediaType,VolStatus,PoolId,VolJobs,MaxVolJobs,"
        "LastWritten) VALUES "
        "('selection-moving','selection-pool-test','Append',111,1,1,"
        "'2025-01-03'),"
        "('selection-original','selection-pool-test','Append',111,0,0,"
        "'2025-01-02'),"
        "('selection-other','selection-pool-test','Append',222,0,0,"
        "'2025-01-01')"));
    std::string move = "BEGIN; UPDATE Media SET PoolId=222,MaxVolJobs="
                       + std::to_string(new_limit)
                       + " WHERE VolumeName='selection-moving'";
    ASSERT_TRUE(db->SqlExec(move.c_str()));

    MediaDbRecord selected;
    selected.PoolId = 111;
    auto pending = std::async(std::launch::async, [&] {
      return directordaemon::FindNextVolumeForAppend(jcr, &selected, 1, nullptr,
                                                     false, false);
    });
    int waiting = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!waiting && std::chrono::steady_clock::now() < deadline) {
      EXPECT_TRUE(db->SqlQuery(
          "SELECT pg_stat_clear_snapshot(); "
          "SELECT 1 FROM pg_stat_activity WHERE wait_event_type='Lock' "
          "AND query LIKE 'UPDATE Media SET VolJobs=%'",
          +[](void* context, int, char**) {
            ++*static_cast<int*>(context);
            return 0;
          },
          &waiting));
      if (!waiting) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }
    EXPECT_EQ(waiting, 1);
    EXPECT_TRUE(db->SqlExec("COMMIT"));
    EXPECT_TRUE(pending.get());
    EXPECT_EQ(selected.PoolId, 111);
    EXPECT_STREQ(selected.VolumeName, "selection-original");
  }
  jcr->db = nullptr;
  jcr->dir_impl->res.write_storage = nullptr;
  jcr->dir_impl->res.pool = nullptr;
  reader->CloseDatabase(jcr);
}

TEST_F(CatalogTest, CatalogWriteRequestRejectsVolumeMovedDuringExpiration)
{
  ASSERT_TRUE(db->SqlExec(
      "INSERT INTO Media "
      "(VolumeName,MediaType,VolStatus,PoolId,VolJobs,MaxVolJobs) VALUES "
      "('request-pool-move','File','Append',111,1,1)"));
  BareosDb* reader = db->CloneDatabaseConnection(jcr, true, false, true);
  ASSERT_NE(reader, nullptr);
  directordaemon::StorageResource storage;
  char media_type[] = "File";
  storage.media_type = media_type;
  jcr->dir_impl->res.write_storage = &storage;
  jcr->dir_impl->jr.PoolId = 111;
  class CaptureSocket : public BareosSocketTCP {
   public:
    bool send() override { return true; }
  } socket;
  for (const char* moved_status : {"Append", "Purged"}) {
    SCOPED_TRACE(moved_status);
    if (!db->SqlExec(
            "UPDATE Media SET PoolId=111,MaxVolJobs=1,VolStatus='Append' "
            "WHERE VolumeName='request-pool-move'")) {
      ADD_FAILURE() << "Could not reset media record";
      break;
    }
    PmStrcpy(socket.msg,
             "CatReq Job=test GetVolInfo VolName=request-pool-move write=1\n");
    socket.message_length = strlen(socket.msg);
    const std::string move
        = "BEGIN; UPDATE Media SET PoolId=222,MaxVolJobs=100,VolStatus='"
          + std::string{moved_status}
          + "' WHERE VolumeName='request-pool-move'";
    if (!db->SqlExec(move.c_str())) {
      ADD_FAILURE() << "Could not lock media record";
      break;
    }
    jcr->db = reader;

    auto pending = std::async(std::launch::async, [&] {
      directordaemon::CatalogRequest(jcr, &socket);
    });
    int waiting = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!waiting && std::chrono::steady_clock::now() < deadline) {
      EXPECT_TRUE(db->SqlQuery(
          "SELECT pg_stat_clear_snapshot(); "
          "SELECT 1 FROM pg_stat_activity WHERE wait_event_type='Lock' "
          "AND query LIKE 'UPDATE Media SET VolJobs=%'",
          +[](void* context, int, char**) {
            ++*static_cast<int*>(context);
            return 0;
          },
          &waiting));
      if (!waiting) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }
    EXPECT_EQ(waiting, 1);
    if (!db->SqlExec("COMMIT")) {
      ADD_FAILURE() << "Could not commit pool move";
      EXPECT_TRUE(db->SqlExec("ROLLBACK"));
    }
    pending.get();
    const std::string response{socket.msg};
    EXPECT_EQ(response.rfind("1998 ", 0), 0) << response;
    EXPECT_NE(response.find("not in Pool"), std::string::npos) << response;
    EXPECT_NE(response.find(std::string{"catalog status is "} + moved_status),
              std::string::npos)
        << response;

    PmStrcpy(socket.msg,
             "CatReq Job=test GetVolInfo VolName=request-pool-move write=0\n");
    socket.message_length = strlen(socket.msg);
    directordaemon::CatalogRequest(jcr, &socket);
    EXPECT_EQ(std::string{socket.msg}.rfind("1000 OK ", 0), 0) << socket.msg;

    EXPECT_TRUE(
        db->SqlExec("UPDATE Media SET PoolId=111,VolStatus='Append' "
                    "WHERE VolumeName='request-pool-move'"));
    PmStrcpy(socket.msg,
             "CatReq Job=test GetVolInfo VolName=request-pool-move write=1\n");
    socket.message_length = strlen(socket.msg);
    directordaemon::CatalogRequest(jcr, &socket);
    EXPECT_EQ(std::string{socket.msg}.rfind("1000 OK ", 0), 0) << socket.msg;
  }

  jcr->db = nullptr;
  jcr->dir_impl->res.write_storage = nullptr;
  reader->CloseDatabase(jcr);
}

TEST_F(CatalogTest, JobMediaOnCleaningVolumePreservesRecordAndMarksError)
{
  ASSERT_TRUE(
      db->SqlExec("INSERT INTO Media (VolumeName,MediaType,VolStatus) "
                  "VALUES ('jobmedia-cleaning-test','File','Cleaning')"));

  MediaDbRecord media;
  bstrncpy(media.VolumeName, "jobmedia-cleaning-test",
           sizeof(media.VolumeName));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &media));

  bstrncpy(media.VolStatus, "Append", sizeof(media.VolStatus));
  ASSERT_TRUE(db->UpdateMediaRecord(jcr, &media, true));
  EXPECT_STREQ(media.VolStatus, "Cleaning");

  JobMediaDbRecord jobmedia;
  jobmedia.JobId = 1234;
  jobmedia.MediaId = media.MediaId;
  jobmedia.EndFile = 12;
  jobmedia.EndBlock = 34;
  ASSERT_TRUE(db->CreateJobmediaRecord(jcr, &jobmedia));

  ASSERT_TRUE(db->GetMediaRecord(jcr, &media));
  EXPECT_STREQ(media.VolStatus, "Error");
  EXPECT_EQ(media.EndFile, 12);
  EXPECT_EQ(media.EndBlock, 34);
  int records = 0;
  ASSERT_TRUE(db->SqlQuery(
      "SELECT JobId FROM JobMedia WHERE JobId=1234",
      +[](void* context, int, char**) {
        ++*static_cast<int*>(context);
        return 0;
      },
      &records));
  EXPECT_EQ(records, 1);

  bstrncpy(media.VolStatus, "Append", sizeof(media.VolStatus));
  ASSERT_TRUE(db->UpdateMediaRecord(jcr, &media, true));
  EXPECT_STREQ(media.VolStatus, "Error");
  ASSERT_TRUE(db->GetMediaRecord(jcr, &media));
  EXPECT_STREQ(media.VolStatus, "Error");

  bstrncpy(media.VolStatus, "Full", sizeof(media.VolStatus));
  ASSERT_TRUE(db->UpdateMediaRecord(jcr, &media));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &media));
  EXPECT_STREQ(media.VolStatus, "Full");
  bstrncpy(media.VolStatus, "Append", sizeof(media.VolStatus));
  ASSERT_TRUE(db->UpdateMediaRecord(jcr, &media, true));
  EXPECT_STREQ(media.VolStatus, "Append");
}

TEST_F(CatalogTest, FailedJobMediaInsertDoesNotChangeCleaningVolume)
{
  ASSERT_TRUE(
      db->SqlExec("INSERT INTO Media (VolumeName,MediaType,VolStatus) "
                  "VALUES ('jobmedia-failure-test','File','Cleaning')"));
  MediaDbRecord media;
  bstrncpy(media.VolumeName, "jobmedia-failure-test", sizeof(media.VolumeName));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &media));

  JobMediaDbRecord jobmedia;
  jobmedia.JobId = 1235;
  jobmedia.MediaId = media.MediaId;
  jobmedia.FirstIndex = std::numeric_limits<uint32_t>::max();
  EXPECT_FALSE(db->CreateJobmediaRecord(jcr, &jobmedia));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &media));
  EXPECT_STREQ(media.VolStatus, "Cleaning");
}

TEST_F(CatalogTest, JobMediaWaitsForCleaningTransition)
{
  ASSERT_TRUE(
      db->SqlExec("INSERT INTO Media (VolumeName,MediaType,VolStatus) "
                  "VALUES ('jobmedia-wait-test','File','Append')"));
  MediaDbRecord media;
  bstrncpy(media.VolumeName, "jobmedia-wait-test", sizeof(media.VolumeName));
  ASSERT_TRUE(db->GetMediaRecord(jcr, &media));
  BareosDb* writer = db->CloneDatabaseConnection(jcr, true, false, true);
  ASSERT_NE(writer, nullptr);
  ASSERT_TRUE(
      db->SqlExec("BEGIN; UPDATE Media SET VolStatus='Cleaning' "
                  "WHERE VolumeName='jobmedia-wait-test'"));

  JobMediaDbRecord jobmedia;
  jobmedia.JobId = 1236;
  jobmedia.MediaId = media.MediaId;
  auto pending = std::async(std::launch::async, [&] {
    return writer->CreateJobmediaRecord(jcr, &jobmedia);
  });
  int waiting = 0;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!waiting && std::chrono::steady_clock::now() < deadline) {
    EXPECT_TRUE(db->SqlQuery(
        "SELECT pg_stat_clear_snapshot(); "
        "SELECT 1 FROM pg_stat_activity WHERE wait_event_type='Lock' "
        "AND query LIKE 'WITH updated_media AS%'",
        +[](void* context, int, char**) {
          ++*static_cast<int*>(context);
          return 0;
        },
        &waiting));
    if (!waiting) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }
  EXPECT_EQ(waiting, 1);
  EXPECT_TRUE(db->SqlExec("COMMIT"));
  EXPECT_TRUE(pending.get());
  writer->CloseDatabase(jcr);

  ASSERT_TRUE(db->GetMediaRecord(jcr, &media));
  EXPECT_STREQ(media.VolStatus, "Error");
  int records = 0;
  ASSERT_TRUE(db->SqlQuery(
      "SELECT JobId FROM JobMedia WHERE JobId=1236",
      +[](void* context, int, char**) {
        ++*static_cast<int*>(context);
        return 0;
      },
      &records));
  EXPECT_EQ(records, 1);
}
