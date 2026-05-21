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

#include "include/bareos.h"
#include "gtest/gtest.h"

#include "findlib/fstype.h"

#if defined(HAVE_LINUX_OS)
namespace {

TEST(fstype, linux_magic_btrfs_is_known)
{
  EXPECT_STREQ(LinuxFstypeFromMagic(0x9123683E), "btrfs");
}

TEST(fstype, linux_magic_unknown_is_rejected)
{
  EXPECT_EQ(LinuxFstypeFromMagic(0x12345678), nullptr);
}

TEST(fstype, linux_procfs_uses_real_fstype_lookup)
{
  char fs[1000];

  ASSERT_TRUE(fstype("/proc", fs, sizeof(fs)));
  EXPECT_STREQ(fs, "proc");
}

}  // namespace
#endif
