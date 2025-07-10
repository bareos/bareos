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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "remove_holes.h"

used_bitmap simple_bitmap(std::initializer_list<std::size_t> set_bits,
                          std::size_t max = 0)
{
  used_bitmap map = {};
  map.start = 0;
  map.unit_size = 1;

  for (auto idx : set_bits) {
    if (idx >= max) { max = idx + 1; }
  }

  map.bits.resize(max);
  for (auto idx : set_bits) { map.bits[idx] = true; }

  return map;
}

bool operator==(const used_interval& a, const used_interval& b)
{
  return a.start == b.start && a.length == b.length;
}

using used_intervals = std::vector<used_interval>;
// note: if a comment contains { X (Y) },
// then X is the actual output, where as Y is the corresponding "bit" in the map
// this is used when bitmap.start/bitmap.unit_size remove the one-to-one
// mapping between those two.

TEST(find_holes, simple_nooffset)
{
  auto bitmap = simple_bitmap({0, 1, 2, 5});
  used_intervals used;

  // we look at the range { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }
  find_used_data(used, bitmap, 0, 10, 1);

  EXPECT_THAT(used,
              testing::ElementsAre(used_interval{0, 3},  // { 0, 1, 2 }
                                   used_interval{5, 5}   // { 5, 6, 7, 8, 9 }
                                   ));
}

TEST(find_holes, simple_offset)
{
  auto bitmap = simple_bitmap({0, 1, 2, 5});
  bitmap.start = 3;
  used_intervals used;

  // we look at the range { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }
  find_used_data(used, bitmap, 0, 10, 1);

  EXPECT_THAT(used, testing::ElementsAre(
                        used_interval{0, 6},  // { 0, 1, 2, 3(0), 4(1), 5(2) }
                        used_interval{8, 2}   // { 8(5), 9 }
                        ));
}


TEST(find_holes, zero_start_nooffset)
{
  auto bitmap = simple_bitmap({1, 2, 5});
  used_intervals used;

  // we look at the range { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }
  find_used_data(used, bitmap, 0, 10, 1);

  EXPECT_THAT(used,
              testing::ElementsAre(used_interval{1, 2},  // { 1, 2 }
                                   used_interval{5, 5}   // { 5, 6, 7, 8, 9 }
                                   ));
}

TEST(find_holes, zero_start_offset)
{
  auto bitmap = simple_bitmap({1, 2, 5});
  bitmap.start = 3;
  used_intervals used;

  find_used_data(used, bitmap, 0, 10, 1);

  EXPECT_THAT(used, testing::ElementsAre(used_interval{0, 3},  // { 0, 1, 2 }
                                         used_interval{4, 2},  // { 4(1), 5(2) }
                                         used_interval{8, 2}   // { 8(), 9 }
                                         ));
}


TEST(find_holes, big_hole_middle)
{
  auto bitmap = simple_bitmap({0, 2, 4, 8, 9});
  used_intervals used;

  find_used_data(used, bitmap, 0, 10, 2);

  EXPECT_THAT(used,
              testing::ElementsAre(used_interval{0, 5},  // { 0, 1, 2, 3, 4 }
                                   used_interval{8, 2}   // { 8, 9 }
                                   ));
}

TEST(find_holes, big_hole_start)
{
  auto bitmap = simple_bitmap({0, 4, 6, 8, 9});
  used_intervals used;

  find_used_data(used, bitmap, 0, 10, 2);

  EXPECT_THAT(used,
              testing::ElementsAre(used_interval{0, 1},  // { 0 }
                                   used_interval{4, 6}   // { 4, 5, 6, 7, 8, 9 }
                                   ));
}

TEST(find_holes, small_hole_end)
{
  auto bitmap = simple_bitmap({8}, 10);
  used_intervals used;

  find_used_data(used, bitmap, 0, 10, 4);

  // holes at the end are always removed, regardless of their size
  EXPECT_THAT(used, testing::ElementsAre(used_interval{8, 1}  // { 8 }
                                         ));
}

TEST(find_holes, complex)
{
  auto bitmap = simple_bitmap({1, 2, 8}, 10);
  bitmap.start = 3;
  bitmap.unit_size = 2;

  used_intervals used;

  find_used_data(used, bitmap, 0, 20, 1);

  // holes at the end are always removed, regardless of their size
  EXPECT_THAT(used, testing::ElementsAre(
                        used_interval{0, 3},  // { 0, 1, 2 }
                        used_interval{5, 4},  // { 5 (1), 6 (1), 7 (2), 8 (2) }
                        used_interval{19, 1}  // { 19 (8) }
                        ));
}
