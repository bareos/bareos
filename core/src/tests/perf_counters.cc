/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#else
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#  include "include/bareos.h"
#endif

#include <vector>
#include <unordered_set>

#define private public
#include "lib/thread_util.h"
#include "lib/perf_report.h"
#include "lib/time_stamps.h"
#undef private

static void RegisterThreadLocal(ThreadTimeKeeper** address,
				TimeKeeper* keeper)
{
  auto& timer = keeper->get_thread_local();
  *address = &timer;
}

TEST(time_keeper, CorrectThreadLocal)
{
  constexpr std::size_t num_threads = 50;

  std::vector<ThreadTimeKeeper*> addresses;
  addresses.resize(num_threads);
  TimeKeeper keeper;
  for (std::size_t i = 0; i < num_threads; ++i) {
    std::thread{RegisterThreadLocal, &addresses[i], &keeper}.join();
  }
  ASSERT_EQ(addresses.size(), num_threads);

  std::unordered_set<void*> unique{addresses.begin(), addresses.end()};
  EXPECT_EQ(addresses.size(), unique.size());
  std::unordered_set<EventBuffer::thread_id> unique_ids;
  for (std::size_t i = 0; i < num_threads; ++i) {
    unique_ids.insert(addresses[i]->this_id);
  }
  EXPECT_EQ(unique_ids.size(), addresses.size());
}
