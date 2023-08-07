/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2023 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_TIME_STAMPS_H_
#define BAREOS_LIB_TIME_STAMPS_H_

#include <chrono>
#include <thread>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <variant>
#include <memory>  // shared_ptr
#include <deque>
#include <optional>

#include "event.h"
#include "perf_report.h"
#include "lib/thread_util.h"
#include "lib/channel.h"


class TimeKeeper;

class ThreadTimeKeeper {
  friend class TimeKeeper;

 public:
  static constexpr std::size_t event_buffer_init_capacity = 2000;
  ThreadTimeKeeper(TimeKeeper& keeper)
      : this_id{std::this_thread::get_id()}, keeper{keeper}
  {
  }
  ~ThreadTimeKeeper();
  void enter(const BlockIdentity& block);
  void switch_to(const BlockIdentity& block);
  void exit(const BlockIdentity& block);

  std::thread::id threadid() const { return this_id; }

  std::vector<event::OpenEvent> stk() const { return stack; }

 private:
  std::thread::id this_id;
  TimeKeeper& keeper;
  std::vector<event::OpenEvent> stack{};
  EventBuffer buffer{this_id, event_buffer_init_capacity, stack};
};

class TimeKeeper {
 public:
  ThreadTimeKeeper& get_thread_local();
  TimeKeeper() : TimeKeeper{channel::CreateBufferedChannel<EventBuffer>(1000)}
  {
  }

  ~TimeKeeper()
  {
    keeper.wlock()->clear(); // this flushes left over events
    queue.lock()->close();
    report_writer.join();
  }

  void handle_event_buffer(EventBuffer buf)
  {
    queue.lock()->put(std::move(buf));
  }

  bool try_handle_event_buffer(EventBuffer& buf)
  {
    if (std::optional locked = queue.try_lock();
	locked.has_value()) {
      return (*locked)->try_put(buf);
    } else {
      return false;
    }
  }

  const CallstackReport& callstack_report() const {
    return callstack;
  }

  const OverviewReport& overview_report() const {
    return overview;
  }

 private:
  TimeKeeper(std::pair<channel::in<EventBuffer>, channel::out<EventBuffer>> p);
  std::condition_variable buf_empty{};
  std::condition_variable buf_not_empty{};
  synchronized<channel::in<EventBuffer>> queue;
  OverviewReport overview{};
  CallstackReport callstack{};
  std::vector<ReportGenerator*> gens;
  rw_synchronized<std::unordered_map<std::thread::id, ThreadTimeKeeper>>
      keeper{};
  std::thread report_writer;
};

class TimedBlock {
 public:
  TimedBlock(ThreadTimeKeeper& timer, const BlockIdentity& block) : timer{timer}
								  , source{&block}
  {
    timer.enter(block);
  }
  ~TimedBlock() {
    timer.exit(*source);
  }
  void switch_to(const BlockIdentity& block) {
    source = &block;
    timer.switch_to(block);
  }

 private:
  ThreadTimeKeeper& timer;
  BlockIdentity const* source;
};

#endif  // BAREOS_LIB_TIME_STAMPS_H_
