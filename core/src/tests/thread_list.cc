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
#include "lib/thread_list.h"
#include "include/make_unique.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>

class WaitCondition {
 public:
  enum class Status : int
  {
    kNotWaiting,
    kWaiting,
    kTimedOut,
    kSuccess
  };

 private:
  std::mutex mutex_;
  std::condition_variable cond_variable_;
  bool notified = false;
  std::atomic<Status> status_{Status::kNotWaiting};

 public:
  void WaitFor(std::chrono::milliseconds ms)
  {
    std::unique_lock<std::mutex> ul(mutex_);
    status_ = Status::kWaiting;
    bool success = cond_variable_.wait_for(ul, ms, [=]() { return notified; });
    status_ = success ? Status::kSuccess : Status::kTimedOut;
  }
  void NotifyOne()
  {
    std::lock_guard<std::mutex> l(mutex_);
    notified = true;
    cond_variable_.notify_one();
  }
  Status GetStatus() { return status_; }
};

static std::atomic<int> thread_counter(0);
static std::vector<std::unique_ptr<WaitCondition>> list_of_wait_conditions;

static void* ThreadHandler(ConfigurationParser*, void* data)
{
  WaitCondition* cond = reinterpret_cast<WaitCondition*>(data);
  cond->WaitFor(std::chrono::milliseconds(10000));
  thread_counter++;
  return nullptr;
}

static void* ShutdownCallback(void* data)
{
  WaitCondition* cond = reinterpret_cast<WaitCondition*>(data);
  cond->NotifyOne();
  return nullptr;
}

static constexpr int maximum_allowed_thread_count = 10;
static constexpr int try_to_start_thread_count = 11;

TEST(thread_list, thread_list_startup_and_shutdown)
{
  std::unique_ptr<ThreadList> t(std::make_unique<ThreadList>());

  t->Init(maximum_allowed_thread_count, ThreadHandler, ShutdownCallback);

  for (int i = 0; i < try_to_start_thread_count; i++) {
    auto wc(std::make_unique<WaitCondition>());
    if (t->CreateAndAddNewThread(nullptr, wc.get())) {
      list_of_wait_conditions.push_back(std::move(wc));
    }
  }

  t->ShutdownAndWaitForThreadsToFinish();

  EXPECT_EQ(t->Size(), 0);
  for (const auto& c : list_of_wait_conditions) {
    EXPECT_EQ(c.get()->GetStatus(), WaitCondition::Status::kSuccess);
  }

  EXPECT_EQ(thread_counter, maximum_allowed_thread_count);
}

static void* ThreadHandlerSleepRandomTime(ConfigurationParser*, void* data)
{
  std::mt19937_64 eng{std::random_device{}()};  // or seed however you want
  std::uniform_int_distribution<> dist{0, 10};
  std::chrono::milliseconds ms{dist(eng)};
  // std::cout << "sleep for: " << ms.count() << " ms" << std::endl;
  std::this_thread::sleep_for(ms);
  ++thread_counter;
  return nullptr;
}

TEST(thread_list, thread_random_shutdown)
{
  std::unique_ptr<ThreadList> t(std::make_unique<ThreadList>());

  t->Init(maximum_allowed_thread_count, ThreadHandlerSleepRandomTime, nullptr);

  thread_counter = 0;
  for (int i = 0; i < maximum_allowed_thread_count; i++) {
    t->CreateAndAddNewThread(nullptr, nullptr);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  t->ShutdownAndWaitForThreadsToFinish();

  EXPECT_EQ(t->Size(), 0);
  EXPECT_EQ(thread_counter, maximum_allowed_thread_count);
}
