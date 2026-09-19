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
#include "include/filetypes.h"

#include <limits>

#include "dird/dird.h"
#include "ndmp/ndmagents.h"
#include "dird/ndmp_dma_priv.h"
#include "lib/attribs.h"
#include "lib/mem_pool.h"

namespace {
struct stat DecodeConvertedFstat(ndmp9_file_stat& fstat)
{
  int8_t FileType = FT_REG;
  PoolMem attribs(PM_FNAME);

  directordaemon::NdmpConvertFstat(&fstat, 1, &FileType, attribs);

  struct stat statp{};
  int32_t LinkFI;
  DecodeStat(attribs.c_str(), &statp, sizeof(statp), &LinkFI);
  return statp;
}

ndmp9_file_stat MakeRegularFileStat()
{
  ndmp9_file_stat fstat{};
  fstat.ftype = NDMP9_FILE_REG;
  fstat.mode.valid = NDMP9_VALIDITY_VALID;
  fstat.mode.value = 0644;
  return fstat;
}
} /* namespace */

TEST(ndmp_fhdb_helpers, size_is_converted_to_st_size_and_st_blocks)
{
  ndmp9_file_stat fstat = MakeRegularFileStat();
  fstat.size.valid = NDMP9_VALIDITY_VALID;
  fstat.size.value = 513;

  struct stat statp = DecodeConvertedFstat(fstat);

  EXPECT_EQ(statp.st_size, 513);
  EXPECT_EQ(statp.st_blksize, 512);
  EXPECT_EQ(statp.st_blocks, 2); /* ceil(513 / 512) */
}

TEST(ndmp_fhdb_helpers, exact_block_multiple_size_does_not_round_up)
{
  ndmp9_file_stat fstat = MakeRegularFileStat();
  fstat.size.valid = NDMP9_VALIDITY_VALID;
  fstat.size.value = 1024;

  struct stat statp = DecodeConvertedFstat(fstat);

  EXPECT_EQ(statp.st_size, 1024);
  EXPECT_EQ(statp.st_blocks, 2);
}

TEST(ndmp_fhdb_helpers, invalid_size_is_not_guessed)
{
  ndmp9_file_stat fstat = MakeRegularFileStat();
  fstat.size.valid = NDMP9_VALIDITY_INVALID;
  fstat.size.value = 999999; /* must be ignored, .valid is false */

  struct stat statp = DecodeConvertedFstat(fstat);

  EXPECT_EQ(statp.st_size, 0);
  EXPECT_EQ(statp.st_blocks, 0);
}

TEST(ndmp_fhdb_helpers, out_of_range_size_is_rejected_not_truncated)
{
  // An untrusted/misbehaving NDMP data server could report a size that
  // does not fit into st_size (a signed field) or that would overflow
  // the ceil(size/512) block-rounding arithmetic. Such a value must be
  // rejected outright (left at 0/"unknown"), not sign-flipped or wrapped
  // into a bogus small/negative value.
  ndmp9_file_stat fstat = MakeRegularFileStat();
  fstat.size.valid = NDMP9_VALIDITY_VALID;
  fstat.size.value = std::numeric_limits<uint64_t>::max();

  struct stat statp = DecodeConvertedFstat(fstat);

  EXPECT_EQ(statp.st_size, 0);
  EXPECT_EQ(statp.st_blocks, 0);
}
