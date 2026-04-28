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

#if HAVE_POSTGRESQL
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
#else
TEST(SqlQueryTest, DISABLED_VolumeQueriesAlwaysExposeHasEncryptionKey) {}
#endif

}  // namespace
