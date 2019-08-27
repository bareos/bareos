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
#include "lib/thread_list.h"
#include "include/jcr.h"
#include "lib/berrno.h"
#include "lib/bsock.h"

#include <algorithm>
#include <thread>

struct ThreadListItem {
  std::thread WorkerThread_;
  void* data_ = nullptr;
  ConfigurationParser* config_ = nullptr;
  std::function<void*(ConfigurationParser* config_, void* data)>
      ThreadInvokedHandler_ = nullptr;
};

struct ThreadListPrivate {
  std::size_t maximum_thread_count_ = 0;

  std::set<ThreadListItem*> thread_list_;
  std::mutex thread_list_mutex_;

  std::condition_variable wait_shutdown_condition;

  std::function<void*(ConfigurationParser* config, void* data)>
      ThreadInvokedHandler_ = nullptr;

  std::function<void*(void* data)> ShutdownCallback_ = nullptr;

  void CallRegisteredShutdownCallbackForAllThreads();
  bool WaitForThreadsToShutdown();
};

ThreadList::ThreadList() { impl_ = new ThreadListPrivate; }
ThreadList::~ThreadList() { delete impl_; }

void ThreadList::Init(int maximum_thread_count,
                      std::function<void*(ConfigurationParser* config,
                                          void* data)> ThreadInvokedHandler,
                      std::function<void*(void* data)> ShutdownCallback)
{
  if (impl_->thread_list_.size()) { return; }
  impl_->maximum_thread_count_ = maximum_thread_count;
  impl_->ThreadInvokedHandler_ = ThreadInvokedHandler;
  impl_->ShutdownCallback_ = ShutdownCallback;
}

void ThreadListPrivate::CallRegisteredShutdownCallbackForAllThreads()
{
  std::lock_guard<std::mutex> l(thread_list_mutex_);

  for (auto item : thread_list_) {
    if (ShutdownCallback_) ShutdownCallback_(item->data_);
  }
}

bool ThreadListPrivate::WaitForThreadsToShutdown()
{
  bool list_is_empty = false;

  std::unique_lock<std::mutex> l(thread_list_mutex_);

  int timeout = 0;
  do {
    list_is_empty = wait_shutdown_condition.wait_for(
        l, std::chrono::seconds(10), [&]() { return thread_list_.empty(); });
  } while (!list_is_empty && ++timeout < 3);

  return list_is_empty;
}

bool ThreadList::WaitUntilThreadListIsEmpty()
{
  impl_->CallRegisteredShutdownCallbackForAllThreads();

  bool shutdown_successful = impl_->WaitForThreadsToShutdown();

  return shutdown_successful;
}

struct CleanupOwnThreadAndNotify {
  ThreadListItem* item_;
  ThreadList* t_;
  CleanupOwnThreadAndNotify(ThreadListItem* item, ThreadList* t)
      : item_(item), t_(t)
  {
  }
  ~CleanupOwnThreadAndNotify()
  {
    std::lock_guard<std::mutex> l(t_->impl_->thread_list_mutex_);
    t_->impl_->thread_list_.erase(item_);
    t_->impl_->wait_shutdown_condition.notify_one();
    delete item_;
  }
};

static void WorkerThread(ThreadListItem* item, ThreadList* t)
{
  SetJcrInTsd(INVALID_JCR);

  item->ThreadInvokedHandler_(item->config_, item->data_);

  CleanupOwnThreadAndNotify cleanup(item, t);
}

bool ThreadList::CreateAndAddNewThread(ConfigurationParser* config, void* data)
{
  if (!impl_) { return false; }

  std::lock_guard<std::mutex> l(impl_->thread_list_mutex_);

  int success = false;
  if (impl_->thread_list_.size() < impl_->maximum_thread_count_) {
    ThreadListItem* item = new ThreadListItem;
    item->data_ = data;
    item->config_ = config;
    item->ThreadInvokedHandler_ = impl_->ThreadInvokedHandler_;

    item->WorkerThread_ = std::thread(WorkerThread, item, this);
    item->WorkerThread_.detach();

    impl_->thread_list_.insert(item);

    success = true;
  }
  return success;
}

std::size_t ThreadList::GetSize() { return impl_->thread_list_.size(); }
