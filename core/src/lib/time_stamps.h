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
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <variant>
#include <memory> // shared_ptr
#include <deque>

struct SplitDuration {
  std::chrono::hours h;
  std::chrono::minutes m;
  std::chrono::seconds s;
  std::chrono::milliseconds ms;
  std::chrono::microseconds us;
  std::chrono::nanoseconds ns;

  template <typename Duration> SplitDuration(Duration d)
  {
    h = std::chrono::duration_cast<std::chrono::hours>(d);
    d -= h;
    m = std::chrono::duration_cast<std::chrono::minutes>(d);
    d -= m;
    s = std::chrono::duration_cast<std::chrono::seconds>(d);
    d -= s;
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(d);
    d -= ms;
    us = std::chrono::duration_cast<std::chrono::microseconds>(d);
    d -= us;
    ns = std::chrono::duration_cast<std::chrono::nanoseconds>(d);
  }

  int64_t hours() { return h.count(); }
  int64_t minutes() { return m.count(); }
  int64_t seconds() { return s.count(); }
  int64_t millis() { return ms.count(); }
  int64_t micros() { return us.count(); }
  int64_t nanos() { return ns.count(); }
};

class BlockIdentity {
public:
  //TODO: replace with source_location (with C++20)
  constexpr explicit BlockIdentity(const char* name) : name(name)
  {}

  const char* c_str() const { return name; }
private:
  const char* name;
};

namespace event {
  using clock = std::chrono::steady_clock;
  using time_point = clock::time_point;

  struct CloseEvent {
    BlockIdentity const* source;
    time_point end;
    CloseEvent(BlockIdentity const* source) : source{source}
					    , end{clock::now()}
    {}
    CloseEvent(BlockIdentity const& source) : source{&source}
					    , end{clock::now()}
    {}

    CloseEvent(const CloseEvent&) = default;
    CloseEvent& operator=(const CloseEvent&) = default;
  };

  struct OpenEvent {
    BlockIdentity const* source;
    time_point start;

    OpenEvent(BlockIdentity const* source) : source{source}
					   , start{clock::now()}
    {}
    OpenEvent(BlockIdentity const& source) : source{&source}
					   , start{clock::now()}
    {}

    OpenEvent(const OpenEvent&) = default;
    OpenEvent& operator=(const OpenEvent&) = default;
    CloseEvent close() const {
      return CloseEvent{source};
    }

  };

  using Event = std::variant<OpenEvent, CloseEvent>;
};


class EventBuffer {
public:
  EventBuffer() {}
  EventBuffer(std::thread::id thread_id,
	      std::size_t initial_size,
	      const std::vector<event::OpenEvent>& current_stack) : thread_id{thread_id}
								  , initial_stack{current_stack}
  {
    events.reserve(initial_size);
  }
  EventBuffer(EventBuffer&&) = default;
  EventBuffer& operator=(EventBuffer&&) = default;
  std::vector<event::Event> events{};
  const std::vector<event::OpenEvent>& stack() const { return initial_stack; }
  const std::thread::id threadid() const { return thread_id; }
private:
  std::thread::id thread_id;
  std::vector<event::OpenEvent> initial_stack{};
};

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

class ReportGenerator {
public:
  virtual void begin_report(event::time_point start [[maybe_unused]]) {};
  virtual void end_report(event::time_point end [[maybe_unused]]) {};

  virtual void add_events(const EventBuffer& buf [[maybe_unused]]) {}
};

static void write_reports(bool* end,
			  std::mutex* gen_mut,
			  std::vector<std::shared_ptr<ReportGenerator>>* gens,
			  std::mutex* buf_mut,
			  std::condition_variable* buf_empty,
			  std::condition_variable* buf_not_empty,
			  std::deque<EventBuffer>* buf_queue)
{
  for (;;) {
    EventBuffer buf;
    bool now_empty = false;
    {
      std::unique_lock lock{*buf_mut};
      buf_not_empty->wait(lock, [end, buf_queue]() { return *end || buf_queue->size() > 0; });

      if (buf_queue->size() == 0) break;

      buf = std::move(buf_queue->front());
      buf_queue->pop_front();

      if (buf_queue->size() == 0) now_empty = true;
    }

    // cannot notify while holding the lock!
    if (now_empty) buf_empty->notify_all();

    std::vector<std::shared_ptr<ReportGenerator>> local_copy;
    {
      std::unique_lock lock{*gen_mut};
      local_copy = *gens;
    }

    for (auto& gen : local_copy) {
      gen->add_events(buf);
    }
  }
}

class TimeKeeper
{
public:
  ThreadTimeKeeper& get_thread_local();
  TimeKeeper() : report_writer{&write_reports, &end, &gen_mut, &gens,
			       &buf_mut, &buf_empty, &buf_not_empty, &buf_queue} {}
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
private:
  mutable std::mutex gen_mut{};
  bool end{false};
  std::vector<std::shared_ptr<ReportGenerator>> gens;
  std::mutex buf_mut{};
  std::condition_variable buf_empty{};
  std::condition_variable buf_not_empty{};
  std::deque<EventBuffer> buf_queue;
  std::thread report_writer;
  mutable std::shared_mutex alloc_mut{};
  std::unordered_map<std::thread::id, ThreadTimeKeeper> keeper{};
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
