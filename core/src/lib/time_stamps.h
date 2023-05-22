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
#include <memory> // shared_ptr
#include <deque>
#include <optional>

#include "event.h"
#include "perf_report.h"


class TimeKeeper;

class ThreadTimeKeeper
{
  friend class TimeKeeper;
public:
  ThreadTimeKeeper(TimeKeeper& keeper) : buffer{std::this_thread::get_id(), 20000, {}}
				       , this_id{std::this_thread::get_id()}
				       , keeper{keeper} {}
  ~ThreadTimeKeeper();
  void enter(const BlockIdentity& block);
  void switch_to(const BlockIdentity& block);
  void exit();
protected:
  EventBuffer flush() {
    std::unique_lock _{vec_mut};
    EventBuffer new_buffer(this_id, 20000, stack);
    std::swap(new_buffer, buffer);
    return new_buffer;
  }
  EventBuffer buffer;
  mutable std::mutex vec_mut{};
private:
  std::thread::id this_id;
  TimeKeeper& keeper;
  std::vector<event::OpenEvent> stack{};
};

class TimeKeeper
{
public:
  ThreadTimeKeeper& get_thread_local();
  TimeKeeper();
  ~TimeKeeper() {
    auto now = event::clock::now();
    flush();
    {
      std::unique_lock lock{gen_mut};
      for (auto& gen : gens) {
	gen->end_report(now);
      }
      gens.clear();
    }
    {
      std::unique_lock lock{buf_mut};
      end = true;
    }
    buf_not_empty.notify_one();
    report_writer.join();
  }
  void add_writer(std::shared_ptr<ReportGenerator> gen);
  void remove_writer(std::shared_ptr<ReportGenerator> gen);
  void handle_event_buffer(EventBuffer buf) {
    {
      std::unique_lock{buf_mut};
      buf_queue.emplace_back(std::move(buf));
    }
    buf_not_empty.notify_one();
  }
  bool try_handle_event_buffer(EventBuffer& buf) {
    if (std::unique_lock lock{buf_mut, std::try_to_lock};
	lock.owns_lock()) {
      buf_queue.emplace_back(std::move(buf));
      return true;
    }
    return false;
  }
  void flush() {
    {
      std::unique_lock lock{alloc_mut};
      for (auto& [_, thread] : keeper) {
	auto buf = thread.flush();
	handle_event_buffer(std::move(buf));
      }
    }
    {
      // TODO: we should somehow only wait until the above buffers were handled
      std::unique_lock lock{buf_mut};
      buf_empty.wait(lock, [this]() { return this->buf_queue.size() == 0; });
    }
  }
  std::string str() {
    flush();
    std::unique_lock lock(gen_mut);
    if (overview.has_value()) {
      return overview.value().str();
    } else {
      return "";
    }
  }
private:
  mutable std::mutex gen_mut{};
  bool end{false};
  std::vector<std::shared_ptr<ReportGenerator>> gens;
  std::mutex buf_mut{};
  std::condition_variable buf_empty{};
  std::condition_variable buf_not_empty{};
  std::deque<EventBuffer> buf_queue;
  mutable std::shared_mutex alloc_mut{};
  std::unordered_map<std::thread::id, ThreadTimeKeeper> keeper{};
  std::optional<OverviewReport> overview{std::nullopt};
  std::thread report_writer;
};

class TimedBlock
{
public:
  TimedBlock(ThreadTimeKeeper& timer, const BlockIdentity& block) : timer{timer}
  {
    timer.enter(block);
  }
  ~TimedBlock() { timer.exit(); }
  void switch_to(const BlockIdentity& block) {
    timer.switch_to(block);
  }

 private:
  ThreadTimeKeeper& timer;
};

#endif  // BAREOS_LIB_TIME_STAMPS_H_
