/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#include "dird/bsr.h"

#include <chrono>
#include <iostream>

using namespace std::chrono;
using namespace directordaemon;
using namespace std;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

enum
{
  kJobId_1 = 1,
  kJobId_2 = 2
};

TEST(fileindex_list, add_filendexes)
{
  RestoreBootstrapRecord bsr;
  AddFindex(&bsr, kJobId_1, 1);
  ASSERT_NE(bsr.fi, nullptr);
  EXPECT_EQ(bsr.fi->findex, 1);
  EXPECT_EQ(bsr.fi->findex2, 1);
  EXPECT_EQ(bsr.JobId, kJobId_1);

  AddFindex(&bsr, kJobId_2, 2);
  ASSERT_NE(bsr.next, nullptr);
  EXPECT_EQ(bsr.next->JobId, kJobId_2);
  ASSERT_NE(bsr.next->fi, nullptr);
  EXPECT_EQ(bsr.next->fi->findex, 2);
  EXPECT_EQ(bsr.next->fi->findex2, 2);

  AddFindex(&bsr, kJobId_1, 3);
  EXPECT_EQ(bsr.JobId, kJobId_1);
  ASSERT_NE(bsr.fi, nullptr);
  ASSERT_NE(bsr.fi->next, nullptr);
  EXPECT_EQ(bsr.fi->next->findex, 3);
  EXPECT_EQ(bsr.fi->next->findex2, 3);

  AddFindex(&bsr, kJobId_2, 4);
  ASSERT_NE(bsr.next, nullptr);
  EXPECT_EQ(bsr.next->JobId, kJobId_2);
  ASSERT_NE(bsr.next->fi, nullptr);
  EXPECT_EQ(bsr.next->fi->findex, 2);
  EXPECT_EQ(bsr.next->fi->findex2, 2);
  ASSERT_NE(bsr.next->fi->next, nullptr);
  EXPECT_EQ(bsr.next->fi->next->findex, 4);
  EXPECT_EQ(bsr.next->fi->next->findex2, 4);
}
