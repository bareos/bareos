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
#include <shared_mutex>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstring>

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

struct Event
{
  bool ended = false;
  BlockIdentity const* block;
  using time_point = std::chrono::steady_clock::time_point;
  time_point start_point, end_point;

  Event(const BlockIdentity& block);
  Event(Event&& d) = default;
  Event& operator=(Event&& d) = default;
  time_point end_point_as_of(time_point current) const;
  void end();
};

class TimeKeeper;

class ThreadTimeKeeper
{
  friend class TimeKeeper;
public:
  ThreadTimeKeeper();
  void enter(const BlockIdentity& block);
  void switch_to(const BlockIdentity& block);
  void exit();
protected:
  std::vector<Event> times{};
  mutable std::mutex vec_mut{};
private:
  decltype(times)::iterator current;
};

class ReportGenerator {
public:
  virtual void begin_report(Event::time_point current [[maybe_unused]]) {};
  virtual void end_report() {};

  virtual void begin_thread(std::thread::id thread_id [[maybe_unused]]) {};
  virtual void end_thread() {};

  virtual void add_event(const Event& e [[maybe_unused]]) {};
};

class EchoReport : public ReportGenerator {
public:
  virtual void begin_report(Event::time_point current) override {
    report << "=== Start Performance Report (Echo) ===\n";
    now = current;
  }
  virtual void end_report() override {
    report << "=== End Performance Report ===\n";
  }

  virtual void begin_thread(std::thread::id thread_id) override {
    report << "== Thread: " << thread_id << " ==\n";
  }

  virtual void add_event(const Event& e) override {
    Event::time_point start = e.start_point;
    Event::time_point end   = e.end_point_as_of(now);
    std::uint64_t startns = std::chrono::duration_cast<std::chrono::nanoseconds>(start.time_since_epoch()).count();
    std::uint64_t endns = std::chrono::duration_cast<std::chrono::nanoseconds>(end.time_since_epoch()).count();

    report << e.block->c_str() << ": " << startns << " -- " << endns;
    if (!e.ended) {
      report << " (still active)";
    }
    report << "\n";
  }

  std::string str() const { return report.str(); }
private:
  Event::time_point now;
  std::ostringstream report;
};

class OverviewReport : public ReportGenerator {
public:
  virtual void begin_report(Event::time_point current) override {
    report << "=== Start Performance Report (Overview) ===\n";
    now = current;
  }
  virtual void end_report() override {
    report << "=== End Performance Report ===\n";
  }

  virtual void begin_thread(std::thread::id thread_id) override {
    report << "== Thread: " << thread_id << " ==\n";
  }

  virtual void end_thread() override {
    std::vector<std::pair<BlockIdentity const*, std::chrono::nanoseconds>> entries(cul_time.begin(), cul_time.end());
    std::sort(entries.begin(), entries.end(), [](auto& p1, auto& p2) {
      if (p1.second > p2.second) { return true; }
      if ((p1.second == p2.second) &&
	  (p1.first > p2.first)) { return true; }
      return false;
    });

    if (NumToShow != ShowAll) {
      entries.resize(NumToShow);
    }

    std::size_t maxwidth = 0;
    for (auto [id, _] : entries) {
      maxwidth = std::max(std::strlen(id->c_str()), maxwidth);
    }

    for (auto [id, time] : entries) {
      SplitDuration d(time);
      report << std::setw(maxwidth)
	     << id->c_str() << ": "
	     << std::setfill('0')
	     << std::setw(2) << d.hours() << ":" << std::setw(2) << d.minutes() << ":" << std::setw(2) << d.seconds() << "."
	     << std::setw(3) << d.millis() << "-" << std::setw(3) << d.micros()
	     << std::setfill(' ')
	     << "\n";

    }

    cul_time.clear();
  }

  virtual void add_event(const Event& e) override {
    using namespace std::chrono;
    auto start = e.start_point;
    auto end   = e.end_point_as_of(now);
    cul_time[e.block] += duration_cast<nanoseconds>(end - start);
  }

  OverviewReport(std::int32_t ShowTopN) : NumToShow{ShowTopN} {}
  static constexpr std::int32_t ShowAll = -1;
  std::string str() const { return report.str(); }
private:
  std::int32_t NumToShow;
  Event::time_point now;
  std::ostringstream report;
  std::unordered_map<BlockIdentity const*, std::chrono::nanoseconds> cul_time;
};

class TimeKeeper
{
public:
  ThreadTimeKeeper& get_thread_local();
  void generate_report(ReportGenerator* gen) const;
private:
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
