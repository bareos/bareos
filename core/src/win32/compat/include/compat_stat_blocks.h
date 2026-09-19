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
#ifndef BAREOS_WIN32_COMPAT_INCLUDE_COMPAT_STAT_BLOCKS_H_
#define BAREOS_WIN32_COMPAT_INCLUDE_COMPAT_STAT_BLOCKS_H_

#include <cstdint>

/**
 * Windows has no native notion of "512-byte blocks allocated" like POSIX
 * stat(2)'s st_blocks, so bareos-fd's Windows compat layer derives a
 * synthetic value from the file size alone. POSIX defines st_blocks as
 * always counted in 512-byte units, independent of st_blksize (the
 * *preferred* I/O block size, which stays 4096 here) -- this helper
 * implements exactly that conversion so all call sites in compat.cc stay
 * consistent, and so the arithmetic can be unit/compile-time tested on
 * any platform without needing a live Windows filesystem.
 */
constexpr uint64_t WindowsSizeToBlocks(uint64_t st_size)
{
  return (st_size + 511) / 512;
}

// Compile-time tests: POSIX st_blocks is always counted in 512-byte
// units, rounded up to the next whole block, regardless of st_blksize.
static_assert(WindowsSizeToBlocks(0) == 0);
static_assert(WindowsSizeToBlocks(1) == 1);
static_assert(WindowsSizeToBlocks(512) == 1);
static_assert(WindowsSizeToBlocks(513) == 2);
static_assert(WindowsSizeToBlocks(1024) == 2);

#endif  // BAREOS_WIN32_COMPAT_INCLUDE_COMPAT_STAT_BLOCKS_H_
