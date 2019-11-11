/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_DIRD_SCHEDULER_SYSTEM_TIME_SOURCE_H_
#define BAREOS_SRC_DIRD_SCHEDULER_SYSTEM_TIME_SOURCE_H_

#include "dird/scheduler_time_adapter.h"

#include <chrono>
#include <atomic>
#include <thread>

namespace directordaemon {

class SystemTimeSource : public TimeSource {
 public:
  time_t SystemTime() override { return time(nullptr); }

  void SleepFor(std::chrono::seconds wait_interval) override
  {
    std::chrono::milliseconds wait_increment = std::chrono::milliseconds(100);
    std::size_t loop_count = wait_interval / wait_increment;

    // avoid loop counter wrap due to roundoff
    if (loop_count == 0) { loop_count = 1; }

    while (running_ && loop_count--) {
      // cannot use condition variable because notify_one
      // does not securely work in a signal handler
      std::this_thread::sleep_for(wait_increment);
    }
  }

  void Terminate() override { running_ = false; }

 private:
  std::atomic<bool> running_{true};
};

}  // namespace directordaemon

#endif  // BAREOS_SRC_DIRD_SCHEDULER_SYSTEM_TIME_SOURCE_H_
