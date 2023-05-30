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


class ThreadTimeKeeper {
 public:
  static constexpr std::size_t event_buffer_init_capacity = 2000;
  ThreadTimeKeeper(synchronized<channel::in<EventBuffer>>& queue)
      : this_id{std::this_thread::get_id()}, queue{queue}
  {
  }
  ~ThreadTimeKeeper();
  void enter(const BlockIdentity& block);
  void switch_to(const BlockIdentity& block);
  void exit(const BlockIdentity& block);

 private:
  std::thread::id this_id;
  synchronized<channel::in<EventBuffer>>& queue;
  std::vector<event::OpenEvent> stack{};
  EventBuffer buffer{this_id, event_buffer_init_capacity, stack};
};

struct ThreadHandle {
  ThreadTimeKeeper* timer{nullptr};

  void enter(const BlockIdentity& block) {
    if (timer) { timer->enter(block); }
  }
  void switch_to(const BlockIdentity& block) {
    if (timer) { timer->switch_to(block); }
  }
  void exit(const BlockIdentity& block) {
    if (timer) { timer->exit(block); }
  }
};

class TimeKeeper {
 public:
  ThreadHandle get_thread_local();
  TimeKeeper(bool enabled = true) : TimeKeeper{enabled, channel::CreateBufferedChannel<EventBuffer>(1000)}
  {
  }

  ~TimeKeeper()
  {
    // any other thread that was holding a thread time keeper reference
    // has to be dead by now for two reasons
    // 1) it cannot safely dereference the referenc, and
    // 2) the join()ing of the thread acts as a memory barrier
    //    which guarantees us that we are reading correct data
    //    when flushing left over events
    keeper.wlock()->clear(); // this flushes left over events
    queue.lock()->close();
    report_writer.join();
  }

  const CallstackReport& callstack_report() const {
    return callstack;
  }

 private:
  const bool enabled;
  TimeKeeper(bool enabled, std::pair<channel::in<EventBuffer>, channel::out<EventBuffer>> p);
  synchronized<channel::in<EventBuffer>> queue;
  CallstackReport callstack{};
  rw_synchronized<std::unordered_map<std::thread::id, ThreadTimeKeeper>>
      keeper{};
  std::thread report_writer;
};

class TimedBlock {
 public:
  TimedBlock(ThreadHandle timer, const BlockIdentity& block) : timer{timer}
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
  ThreadHandle timer;
  BlockIdentity const* source;
};

#endif  // BAREOS_LIB_TIME_STAMPS_H_
