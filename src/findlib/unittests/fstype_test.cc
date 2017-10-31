/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2015      Bareos GmbH & Co. KG

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
/* originally was Written by Preben 'Peppe' Guldberg, December MMIV */
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for unittest framework cmocka
 *
 * Philipp Storz, April 2015
 */

#include "gtest/gtest.h"
#include <stdio.h>
#include "bareos.h"
#include "findlib/find.h"

TEST(FstypeTest, RootFs) {
  char fs[1000];
  bool retval;
  retval = fstype("/", fs, sizeof(fs));
  EXPECT_TRUE(retval);
  EXPECT_STREQ("xfs", (const char *) fs) << "checking root fs to be xfs";
}

TEST(FstypeTest, Proc) {
  char fs[1000];
  bool retval;

  retval = fstype("/proc", fs, sizeof(fs));
  EXPECT_TRUE(retval);
  EXPECT_STREQ("proc", (const char *) fs) << "checking proc to be proc";
}

TEST(FstypeTest, Run) {
  char fs[1000];
  bool retval;

  retval = fstype("/run", fs, sizeof(fs));
  EXPECT_TRUE(retval);
  EXPECT_STREQ("tmpfs", (const char *) fs) << "checking run to be tmpfs";
}
