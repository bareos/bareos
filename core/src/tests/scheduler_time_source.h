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

#ifndef BAREOS_SRC_TESTS_SCHEDULER_TIME_SOURCE_H_
#define BAREOS_SRC_TESTS_SCHEDULER_TIME_SOURCE_H_

#include "dird/scheduler.h"
#include "dird/scheduler_time_adapter.h"

#include <atomic>
#include <condition_variable>
#include <thread>
#include <iostream>

static bool debug{false};

class SimulatedTimeSource : public directordaemon::TimeSource {
 public:
  SimulatedTimeSource()
  {
    running_ = true;
    clock_value_ = 959817600;  // UTC: 01-06-2000 00:00:00

    if (debug) {
      time_t t{clock_value_};
      std::cout << std::put_time(
                       gmtime(&t),
                       "Start simulated Clock at time: %d-%m-%Y %H:%M:%S")
                << std::endl;
    }
  }

  ~SimulatedTimeSource() { Terminate(); }

  void SleepFor(std::chrono::seconds wait_interval_pseudo_seconds) override
  {
    time_t wait_until_ =
        clock_value_ +
        static_cast<time_t>(wait_interval_pseudo_seconds.count());
    while (running_ && clock_value_ < wait_until_) { clock_value_ += 1; }
  }

  void Terminate() override { running_ = false; }
  time_t SystemTime() override { return clock_value_; }
  static void ExecuteJob(JobControlRecord* jcr);

 private:
  std::atomic<bool> running_;
  std::atomic<time_t> clock_value_;
};

#endif  // BAREOS_SRC_TESTS_SCHEDULER_TIME_SOURCE_H_
