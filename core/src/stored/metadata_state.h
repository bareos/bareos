/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_CORE_SRC_STORED_METADATA_STATE_H_
#define BAREOS_CORE_SRC_STORED_METADATA_STATE_H_

#include "include/bareos.h"
#include "stored/read_record.h"

namespace storagedaemon {

struct MetadataStateKey {
  uint32_t vol_session_time;
  uint32_t vol_session_id;
  uint32_t file_index;

  bool operator==(const MetadataStateKey& other) const
  {
    return vol_session_time == other.vol_session_time
           && vol_session_id == other.vol_session_id
           && file_index == other.file_index;
  }
};

struct MetadataStateKeyHash {
  // Keep the three fields well mixed without packing them into one integer.
  static constexpr size_t kHashCombineMultiplier = 1315423911u;

  size_t operator()(const MetadataStateKey& key) const
  {
    size_t h = std::hash<uint32_t>{}(key.vol_session_time);
    h = h * kHashCombineMultiplier ^ std::hash<uint32_t>{}(key.vol_session_id);
    h = h * kHashCombineMultiplier ^ std::hash<uint32_t>{}(key.file_index);
    return h;
  }
};

inline MetadataStateKey MetadataKey(const DeviceRecord* rec)
{
  return MetadataStateKey{static_cast<uint32_t>(rec->VolSessionTime),
                          static_cast<uint32_t>(rec->VolSessionId),
                          static_cast<uint32_t>(rec->FileIndex)};
}

}  // namespace storagedaemon

#endif  // BAREOS_CORE_SRC_STORED_METADATA_STATE_H_
