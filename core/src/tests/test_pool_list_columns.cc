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

#include "dird/ua_output_pool_internal.h"

#include <string>
#include <string_view>

#include "gtest/gtest.h"
#include "include/bareos.h"

namespace {

using directordaemon::pool_list_internal::kPoolListColumnCount;
using directordaemon::pool_list_internal::kPoolListColumns;
using directordaemon::pool_list_internal::kPoolListNameIndex;
using directordaemon::pool_list_internal::kPoolListPoolIdIndex;
using directordaemon::pool_list_internal::PoolListColumnIndex;
using directordaemon::pool_list_internal::PoolListColumnsAreUnique;

static_assert(PoolListColumnsAreUnique());
static_assert(kPoolListPoolIdIndex == 0);
static_assert(kPoolListNameIndex == 1);
static_assert(PoolListColumnIndex("unknown") == kPoolListColumnCount);

// The fields that `update volume=... frompool=yes` copies onto a volume must
// all be listed, otherwise a client cannot tell whether a volume still
// matches its pool. RecyclePoolId, Recycle, VolRetention, VolUseDuration,
// MaxVolJobs and MaxVolBytes have always been part of the listing.
static_assert(PoolListColumnIndex("maxvolfiles") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("actiononpurge") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("minblocksize") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("maxblocksize") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("recyclepoolid") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("recycle") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("volretention") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("voluseduration") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("maxvoljobs") < kPoolListColumnCount);
static_assert(PoolListColumnIndex("maxvolbytes") < kPoolListColumnCount);

TEST(PoolListColumns, KeysAreLowercaseColumnNames)
{
  for (const auto& column : kPoolListColumns) {
    std::string expected;
    for (const char c : column.column) {
      expected
          += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    EXPECT_EQ(std::string(column.key), expected);
  }
}

TEST(PoolListColumns, EveryKeyResolvesToItsOwnIndex)
{
  for (std::size_t i = 0; i < kPoolListColumns.size(); ++i) {
    EXPECT_EQ(PoolListColumnIndex(kPoolListColumns[i].key), i);
  }
}

TEST(PoolListColumns, SelectListMatchesTheColumnTable)
{
  std::string columns;
  for (const auto& column : kPoolListColumns) {
    if (!columns.empty()) { columns += ","; }
    columns.append(column.column);
  }

  EXPECT_EQ(columns,
            "PoolId,Name,NumVols,MaxVols,UseOnce,UseCatalog,AcceptAnyVolume,"
            "VolRetention,VolUseDuration,MaxVolJobs,MaxVolBytes,AutoPrune,"
            "Recycle,PoolType,LabelFormat,Enabled,ScratchPoolId,"
            "RecyclePoolId,LabelType,MaxVolFiles,ActionOnPurge,MinBlocksize,"
            "MaxBlocksize");
}

}  // namespace
