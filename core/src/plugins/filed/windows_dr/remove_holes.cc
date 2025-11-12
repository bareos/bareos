/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include "remove_holes.h"
#include <optional>
#include <cassert>

void find_used_data(std::vector<used_interval>& result,
                    const used_bitmap& map,
                    std::size_t start,
                    std::size_t end,
                    std::size_t min_hole_size)
{
  // basic idea:
  // Let 'current' be the first byte in the extent that we have not
  // handled yet (starts at the beginning).
  // Then we search in the bitmap for at least min_hole_clusters in a row.
  // Once such a hole has been found, we add to the list of extents
  // [ current, start_of_the_hole ), and move current to end_of_the_hole.

  // Once we have reached the end of the bitmap we also add
  // [current, end) to the list of extents, if current != end,
  // as we can not know if these bytes are used or not!

  std::size_t current = start;
  std::optional<std::size_t> hole_start;

  using bit_idx = std::size_t;

  auto& bits = map.bits;
  for (bit_idx idx = 0; idx < bits.size(); ++idx) {
    auto offset_for_idx = idx * map.unit_size + map.start;

    if (offset_for_idx >= end) { break; }

    if (bits[idx]) {
      if (hole_start) {
        bit_idx offset_for_hole_start = hole_start.value();
        auto unused_count = offset_for_idx - offset_for_hole_start;

        if (unused_count >= min_hole_size) {
          // we have found a hole that starts at hstart and ends at hend.
          // before we add [current, hstart) to the list of extents
          // we need to make sure that this is actually not empty.

          if (offset_for_hole_start > current) {
            result.push_back({current, offset_for_hole_start - current});
          }

          // in any case, lets update current to the newest non-hole byte offset
          current = offset_for_idx;
        }

        hole_start.reset();
      }

    } else {
      if (!hole_start) { hole_start = offset_for_idx; }
    }

    // no need to look farther
    if (current >= end) { break; }
  }

  // holes at the end are always removed, regardless of their size
  if (hole_start) {
    // we are ending with an open hole, so remove it completely:
    // This is simply done by moving end back to the start of the hole.
    // Note: by construction we know that hole_start < end
    assert(hole_start.value() < end);
    end = hole_start.value();
  }

  if (end > current) { result.push_back({current, end - current}); }
}
