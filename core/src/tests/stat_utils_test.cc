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

#include "lib/stat_utils.h"

static_assert(IsUnknownStatField(int64_t{-1}));
static_assert(!IsUnknownStatField(int64_t{0}));
static_assert(IsUnknownStatField(std::numeric_limits<uint64_t>::max()));
static_assert(!IsUnknownStatField(uint64_t{0}));

static_assert(BlocksFromSize(0) == 0);
static_assert(BlocksFromSize(1) == 1);
static_assert(BlocksFromSize(512) == 1);
static_assert(BlocksFromSize(513) == 2);
static_assert(BlocksFromSize(1024) == 2);

TEST(stat_utils, compile_time_checks) { SUCCEED(); }
