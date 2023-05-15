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

class BlockIdentity {
public:
  //TODO: replace with source_location (with C++20)
  constexpr explicit BlockIdentity(const char* name) : name(name)
  {}

  const char* c_str() const { return name; }
private:
  const char* name;
};

struct Duration
{
  bool ended = false;
  BlockIdentity const* block;
  using time_point = std::chrono::steady_clock::time_point;
  time_point start_point, end_point;

  Duration(const BlockIdentity& block);
  Duration(Duration&& d) = default;
  Duration& operator=(Duration&& d) = default;
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
  std::vector<Duration> times{};
  mutable std::mutex vec_mut{};
private:
  decltype(times)::iterator current;
};

class TimeKeeper
{
public:
  ThreadTimeKeeper& get_thread_local();
  std::string generate_report() const;
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
