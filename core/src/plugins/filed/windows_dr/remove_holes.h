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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_REMOVE_HOLES_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_REMOVE_HOLES_H_

#include <cstdlib>
#include <vector>

struct used_bitmap {
  std::size_t start;
  std::size_t unit_size;  // each bit represents this amount of memory

  // bit i corresponds to the memory range [ start + i * unit_size, start +
  // (i+1) * unit_size ) a set bit means that the range is used, otherwise the
  // range is unused.
  std::vector<bool> bits;
};

struct used_interval {
  std::size_t start;
  std::size_t length;
};


/**
 * find_used_data - remove unused holes from a memory range
 * @result - storage for the found used intervals
 * @map - bitmap indicating which parts are used
 * @start - start of the memory range to examine
 * @end - end of the memory range to examine
 * @min_hole_size - minimum length of consecutive unused bytes that should be
 * removed
 *
 * This function takes in a range of memory, part of which may be unused,
 * and returns (through result) a list of memory ranges that are in use.
 *
 * In use means that there are at most min_hole_size - 1 consecutive bytes
 * in that range which are unused (according to the map).
 *
 * All bytes that are outside of the memory region described by the map are
 * treated as used.
 * If the memory range ends in consecutive unused bytes, then these are removed
 * regardless of their count.
 */
void find_used_data(std::vector<used_interval>& result,
                    const used_bitmap& map,
                    std::size_t start,
                    std::size_t end,
                    std::size_t min_hole_size);

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_REMOVE_HOLES_H_
