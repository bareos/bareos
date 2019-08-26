/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
#include "lib/timer_thread.h"

#include <thread>

class TimerThreadTest : public ::testing::Test {
 public:
  static void TimerCallback(TimerThread::TimerControlledItem* t);
  static void TimerUserDestructorCallback(TimerThread::TimerControlledItem* t);

  static bool timer_callback_was_called;
  static bool user_destructor_was_called;
  static bool timer_callback_thread_is_timer;

  bool stop_timer_thread_on_tear_down = true;
  TimerThread::TimerControlledItem* timer_item = nullptr;

  void SetUp(bool one_shot = false);

 private:
  void TearDown() override;
};

bool TimerThreadTest::timer_callback_was_called = false;
bool TimerThreadTest::user_destructor_was_called = false;
bool TimerThreadTest::timer_callback_thread_is_timer = false;

void TimerThreadTest::SetUp(bool one_shot)
{
  timer_callback_was_called = false;
  user_destructor_was_called = false;

  timer_item = TimerThread::NewControlledItem();

  timer_item->single_shot = one_shot;
  timer_item->interval = std::chrono::milliseconds(100);

  timer_item->user_callback = TimerCallback;
  timer_item->user_destructor = TimerUserDestructorCallback;
}

void TimerThreadTest::TearDown()
{
  if (stop_timer_thread_on_tear_down) { TimerThread::Stop(); }
}

void TimerThreadTest::TimerCallback(TimerThread::TimerControlledItem* t)
{
  timer_callback_was_called = true;
  timer_callback_thread_is_timer = TimerThread::CurrentThreadIsTimerThread();
}

void TimerThreadTest::TimerUserDestructorCallback(
    TimerThread::TimerControlledItem* t)
{
  user_destructor_was_called = true;
}

TEST_F(TimerThreadTest, timeout)
{
  SetUp();

  EXPECT_TRUE(RegisterTimer(timer_item));
  EXPECT_FALSE(TimerThread::CurrentThreadIsTimerThread());

  timer_callback_was_called = false;
  timer_callback_thread_is_timer = false;

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(timer_callback_was_called);
  EXPECT_FALSE(timer_callback_thread_is_timer);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  EXPECT_TRUE(timer_callback_was_called);
  EXPECT_TRUE(timer_callback_thread_is_timer);
}

TEST_F(TimerThreadTest, timeout_one_shot)
{
  SetUp(true);

  EXPECT_TRUE(RegisterTimer(timer_item));
  EXPECT_FALSE(TimerThread::CurrentThreadIsTimerThread());

  timer_callback_was_called = false;
  timer_callback_thread_is_timer = false;

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_FALSE(timer_callback_was_called);
  EXPECT_FALSE(timer_callback_thread_is_timer);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  EXPECT_TRUE(timer_callback_was_called);
  EXPECT_TRUE(timer_callback_thread_is_timer);

  EXPECT_FALSE(TimerThread::UnregisterTimer(timer_item));
}

TEST_F(TimerThreadTest, user_destructor_stop)
{
  SetUp();
  EXPECT_TRUE(TimerThread::RegisterTimer(timer_item));

  TimerThread::Stop();

  EXPECT_TRUE(user_destructor_was_called);
}

TEST_F(TimerThreadTest, user_destructor_unregister)
{
  SetUp();
  EXPECT_TRUE(TimerThread::RegisterTimer(timer_item));

  EXPECT_FALSE(user_destructor_was_called);
  UnregisterTimer(timer_item);
  EXPECT_TRUE(user_destructor_was_called);
}

TEST_F(TimerThreadTest, register_twice_fails)
{
  SetUp();
  EXPECT_TRUE(TimerThread::RegisterTimer(timer_item));
  EXPECT_TRUE(TimerThread::RegisterTimer(timer_item));
  TimerThread::UnregisterTimer(timer_item);
}

TEST_F(TimerThreadTest, should_shutdown_with_timer_thread_guard)
{
  SetUp();

  /* This test does not shutdown the timer thread during tear down,
   * as a matter of fact the timer thread keeps running and will be
   * stopped by the timer thread guard. */

  stop_timer_thread_on_tear_down = false;
  // use debugger to check c++ library shut-down
}

// TEST_F(TimerThreadTest, just_run_timer_thread)
//{
//  while (1) continue;
//  return;
//}
