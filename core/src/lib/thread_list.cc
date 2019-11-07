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

#include "include/bareos.h"
#include "include/jcr.h"
#include "include/make_unique.h"
#include "lib/berrno.h"
#include "lib/bsock.h"
#include "lib/thread_list.h"
#include "lib/thread_specific_data.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

static constexpr int debuglevel{800};

struct ThreadListItem {
  void* data_{};
};

struct ThreadListContainer {
  std::set<ThreadListItem*> thread_list_;
  std::mutex thread_list_mutex_;
  std::condition_variable wait_shutdown_condition;
};

class ThreadListPrivate {
  friend class ThreadList;
  friend class ThreadGuard;

 private:
  std::size_t maximum_thread_count_{};

  std::shared_ptr<ThreadListContainer> l{
      std::make_shared<ThreadListContainer>()};


  ThreadList::ThreadHandler ThreadInvokedHandler_{};
  ThreadList::ShutdownCallback ShutdownCallback_{};

  void CallRegisteredShutdownCallbackForAllThreads();
  bool WaitForThreadsToShutdown();
};

ThreadList::ThreadList() : impl_(std::make_unique<ThreadListPrivate>()) {}
ThreadList::~ThreadList() = default;

void ThreadList::Init(int maximum_thread_count,
                      ThreadHandler ThreadInvokedHandler,
                      ShutdownCallback ShutdownCallback)
{
  if (!impl_->l->thread_list_.empty()) { return; }
  impl_->maximum_thread_count_ = maximum_thread_count;
  impl_->ThreadInvokedHandler_ = std::move(ThreadInvokedHandler);
  impl_->ShutdownCallback_ = std::move(ShutdownCallback);
}

void ThreadListPrivate::CallRegisteredShutdownCallbackForAllThreads()
{
  std::lock_guard<std::mutex> lg(l->thread_list_mutex_);

  for (auto item : l->thread_list_) {
    if (ShutdownCallback_) { ShutdownCallback_(item->data_); };
  }
}

bool ThreadListPrivate::WaitForThreadsToShutdown()
{
  bool list_is_empty = false;

  int tries = 0;
  do {
    std::unique_lock<std::mutex> ul(l->thread_list_mutex_);
    static constexpr auto timeout = std::chrono::seconds(10);

    list_is_empty = l->wait_shutdown_condition.wait_for(
        ul, timeout, [&]() { return l->thread_list_.empty(); });

  } while (!list_is_empty && ++tries < 3);

  return list_is_empty;
}

bool ThreadList::ShutdownAndWaitForThreadsToFinish()
{
  impl_->CallRegisteredShutdownCallbackForAllThreads();

  bool shutdown_successful = impl_->WaitForThreadsToShutdown();

  return shutdown_successful;
}

class ThreadGuard {
 public:
  ThreadGuard(std::shared_ptr<ThreadListContainer> l,
              std::unique_ptr<ThreadListItem>&& item)
      : l_(l), item_(std::move(item))
  {
    // thread_list_mutex_ locked by CreateAndAddNewThread
    l_->thread_list_.insert(item_.get());
  }
  ~ThreadGuard()
  {
    std::lock_guard<std::mutex> lg(l_->thread_list_mutex_);
    l_->thread_list_.erase(item_.get());
    l_->wait_shutdown_condition.notify_one();
  }

 private:
  std::shared_ptr<ThreadListContainer> l_;
  std::unique_ptr<ThreadListItem> item_;  // finally destroys the item
};

class RunningCondition {
 public:
  void Running()
  {
    std::lock_guard<std::mutex> lg(mutex_);
    is_running_ = true;
    var_.notify_one();
  }
  enum class Result
  {
    kRunning,
    kTimedout
  };
  Result WaitUntilThreadIsRunning()
  {
    static constexpr auto waittime = std::chrono::seconds(10);
    std::unique_lock<std::mutex> ul(mutex_);

    return var_.wait_for(ul, waittime, [&]() { return is_running_; })
               ? Result::kRunning
               : Result::kTimedout;
  }

 private:
  std::mutex mutex_;
  std::condition_variable var_;
  bool is_running_{false};
};

static void WorkerThread(
    std::shared_ptr<ThreadListContainer> l,
    const ThreadList::ThreadHandler& ThreadInvokedHandler,
    ConfigurationParser* config,
    void* data,
    std::shared_ptr<RunningCondition> run_condition)  // copy, not reference
{
  std::unique_ptr<ThreadListItem> item{std::make_unique<ThreadListItem>()};
  item->data_ = data;

  ThreadGuard guard(l, std::move(item));

  run_condition->Running();
  SetJcrInThreadSpecificData(nullptr);

  ThreadInvokedHandler(config, data);

  Dmsg0(debuglevel, "Finished WorkerThread.\n");
}

bool ThreadList::CreateAndAddNewThread(ConfigurationParser* config, void* data)
{
  std::lock_guard<std::mutex> lg(impl_->l->thread_list_mutex_);

  if (impl_->l->thread_list_.size() >= impl_->maximum_thread_count_) {
    Dmsg1(debuglevel, "Number of maximum threads exceeded: %d\n",
          impl_->maximum_thread_count_);
    return false;
  }

  auto run_condition = std::make_shared<RunningCondition>();

  try {
    std::thread(WorkerThread, impl_->l, impl_->ThreadInvokedHandler_, config,
                data, run_condition)
        .detach();
  } catch (const std::system_error& e) {
    Dmsg1(debuglevel, "Could not start and detach thread: %s\n", e.what());
    return false;
  }

  if (run_condition->WaitUntilThreadIsRunning() ==
      RunningCondition::Result::kRunning) {
    return true;
  }
  Dmsg0(debuglevel, "Could not run WorkerThread.\n");
  return false;
}

std::size_t ThreadList::Size() const
{
  std::lock_guard<std::mutex> l(impl_->l->thread_list_mutex_);
  return impl_->l->thread_list_.size();
}
