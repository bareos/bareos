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

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "include/streams.h"

#include "lib/attribs.h"
#include "lib/mem_pool.h"

namespace {
struct stat RoundTrip(struct stat statp)
{
  PoolMem attribs(PM_FNAME);
  EncodeStat(attribs.c_str(), &statp, sizeof(statp), 0, STREAM_UNIX_ATTRIBUTES);

  struct stat decoded{};
  int32_t LinkFI;
  DecodeStat(attribs.c_str(), &decoded, sizeof(decoded), &LinkFI);
  return decoded;
}
} /* namespace */

TEST(attribs, round_trips_zero_size)
{
  struct stat statp{};
  statp.st_size = 0;
  statp.st_blocks = 0;

  struct stat decoded = RoundTrip(statp);

  EXPECT_EQ(decoded.st_size, 0);
  EXPECT_EQ(decoded.st_blocks, 0);
}

TEST(attribs, round_trips_small_size)
{
  struct stat statp{};
  statp.st_size = 513;
  statp.st_blocks = 2; /* ceil(513 / 512) */

  struct stat decoded = RoundTrip(statp);

  EXPECT_EQ(decoded.st_size, 513);
  EXPECT_EQ(decoded.st_blocks, 2);
}

TEST(attribs, round_trips_large_size_above_4gb)
{
  struct stat statp{};
  const int64_t size = 5LL * 1024 * 1024 * 1024 + 42; /* > 4 GiB */
  statp.st_size = size;
  statp.st_blocks = (size + 511) / 512;

  struct stat decoded = RoundTrip(statp);

  EXPECT_EQ(decoded.st_size, size);
  EXPECT_EQ(decoded.st_blocks, (size + 511) / 512);
}

TEST(attribs, round_trips_non_block_aligned_size)
{
  struct stat statp{};
  statp.st_size = 1000;
  statp.st_blocks = (1000 + 511) / 512; /* 2 */

  struct stat decoded = RoundTrip(statp);

  EXPECT_EQ(decoded.st_size, 1000);
  EXPECT_EQ(decoded.st_blocks, 2);
}
