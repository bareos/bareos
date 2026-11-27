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

#ifndef BAREOS_DIRD_UA_OUTPUT_POOL_INTERNAL_H_
#define BAREOS_DIRD_UA_OUTPUT_POOL_INTERNAL_H_

#include <array>
#include <string_view>

namespace directordaemon {
namespace pool_list_internal {

/* One column of the `llist pools` result: the name used in the SQL SELECT
 * and the key it is emitted under. Both the query and the output are built
 * from this table, so they cannot drift apart. */
struct PoolListColumn {
  std::string_view column;
  std::string_view key;
};

inline constexpr std::array kPoolListColumns{
    PoolListColumn{"PoolId", "poolid"},
    PoolListColumn{"Name", "name"},
    PoolListColumn{"NumVols", "numvols"},
    PoolListColumn{"MaxVols", "maxvols"},
    PoolListColumn{"UseOnce", "useonce"},
    PoolListColumn{"UseCatalog", "usecatalog"},
    PoolListColumn{"AcceptAnyVolume", "acceptanyvolume"},
    PoolListColumn{"VolRetention", "volretention"},
    PoolListColumn{"VolUseDuration", "voluseduration"},
    PoolListColumn{"MaxVolJobs", "maxvoljobs"},
    PoolListColumn{"MaxVolBytes", "maxvolbytes"},
    PoolListColumn{"AutoPrune", "autoprune"},
    PoolListColumn{"Recycle", "recycle"},
    PoolListColumn{"PoolType", "pooltype"},
    PoolListColumn{"LabelFormat", "labelformat"},
    PoolListColumn{"Enabled", "enabled"},
    PoolListColumn{"ScratchPoolId", "scratchpoolid"},
    PoolListColumn{"RecyclePoolId", "recyclepoolid"},
    PoolListColumn{"LabelType", "labeltype"},
    /* The volume defaults below complete the set of fields that
     * `update volume=... frompool=yes` copies onto a volume, so clients can
     * tell whether a volume still matches its pool. */
    PoolListColumn{"MaxVolFiles", "maxvolfiles"},
    PoolListColumn{"ActionOnPurge", "actiononpurge"},
    PoolListColumn{"MinBlocksize", "minblocksize"},
    PoolListColumn{"MaxBlocksize", "maxblocksize"},
};

inline constexpr std::size_t kPoolListColumnCount = kPoolListColumns.size();

/* Index of a column by its emitted key, or kPoolListColumnCount when the key
 * is unknown. */
constexpr std::size_t PoolListColumnIndex(std::string_view key)
{
  for (std::size_t i = 0; i < kPoolListColumns.size(); ++i) {
    if (kPoolListColumns[i].key == key) { return i; }
  }
  return kPoolListColumnCount;
}

constexpr bool PoolListColumnsAreUnique()
{
  for (std::size_t i = 0; i < kPoolListColumns.size(); ++i) {
    for (std::size_t j = i + 1; j < kPoolListColumns.size(); ++j) {
      if (kPoolListColumns[i].key == kPoolListColumns[j].key
          || kPoolListColumns[i].column == kPoolListColumns[j].column) {
        return false;
      }
    }
  }
  return true;
}

inline constexpr std::size_t kPoolListPoolIdIndex
    = PoolListColumnIndex("poolid");
inline constexpr std::size_t kPoolListNameIndex = PoolListColumnIndex("name");

}  // namespace pool_list_internal
}  // namespace directordaemon

#endif  // BAREOS_DIRD_UA_OUTPUT_POOL_INTERNAL_H_
