/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include <gtest/gtest.h>

#include <memory>
#include <string_view>

#include "cats/postgresql.h"

namespace {

constexpr std::string_view kHasEncryptionKeyColumn = "END AS HasEncryptionKey";

using DbHandle = std::unique_ptr<BareosDb, void (*)(BareosDb*)>;

DbHandle CreateDb()
{
  return DbHandle(
      new BareosDbPostgresql(nullptr, "", "", "", nullptr, nullptr, 0, nullptr,
                             false, false, false, false, false),
      [](BareosDb* db) { db->CloseDatabase(nullptr); });
}

TEST(SqlQueryTest, VolumeQueriesAlwaysExposeHasEncryptionKey)
{
  auto db = CreateDb();

  const auto short_query = std::string_view(
      db->get_predefined_query(BareosDb::SQL_QUERY::list_volumes_select_0));
  const auto long_query = std::string_view(db->get_predefined_query(
      BareosDb::SQL_QUERY::list_volumes_select_long_0));

  EXPECT_NE(short_query.find(kHasEncryptionKeyColumn), std::string_view::npos);
  EXPECT_NE(long_query.find(kHasEncryptionKeyColumn), std::string_view::npos);
}

// `llist jobs last` / `llist jobs long last` must honor the same whitelisted
// `sortby=<column>` mechanism as the plain (non-`last`) job list queries --
// see GetJobsSortColumn() in sql_list.cc -- instead of always sorting by
// StartTime, otherwise a client-requested sort order (e.g. for the WebUI's
// paginated "Recent Jobs" widget) is silently ignored server-side.
TEST(SqlQueryTest, LastJobQueriesAcceptOrderByColumnPlaceholder)
{
  auto db = CreateDb();

  const auto short_query = std::string_view(
      db->get_predefined_query(BareosDb::SQL_QUERY::list_jobs_last));
  const auto long_query = std::string_view(
      db->get_predefined_query(BareosDb::SQL_QUERY::list_jobs_long_last));

  EXPECT_NE(short_query.find("ORDER BY %s%s;"), std::string_view::npos);
  EXPECT_NE(long_query.find("ORDER BY %s%s;"), std::string_view::npos);
  EXPECT_EQ(short_query.find("ORDER BY StartTime"), std::string_view::npos);
  EXPECT_EQ(long_query.find("ORDER BY StartTime"), std::string_view::npos);
}

// `list jobs count last` must count the same "one row per job name" dataset
// `llist jobs last` returns, instead of counting every historical Job row
// (which would report a count larger than the range query's result set and
// break pagination for anything built on top of it, e.g. the WebUI's
// "Recent Jobs" widget).
TEST(SqlQueryTest, JobsCountLastDedupsToOneRowPerJobName)
{
  auto db = CreateDb();

  const auto query = std::string_view(
      db->get_predefined_query(BareosDb::SQL_QUERY::list_jobs_count_last));

  EXPECT_NE(query.find("COUNT(DISTINCT Job.JobId)"), std::string_view::npos);
  EXPECT_NE(query.find("GROUP BY Job.Name"), std::string_view::npos);
  EXPECT_NE(query.find("MAX(Job.JobId)"), std::string_view::npos);
}

}  // namespace
