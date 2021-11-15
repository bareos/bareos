/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_WATCHDOG_TIMER_H_
#define BAREOS_LIB_WATCHDOG_TIMER_H_

#include "include/bareos.h"
#include <chrono>

namespace TimerThread {
struct Timer;
}

class JobControlRecord;
class BareosSocket;

class WatchdogTimer {
 public:
  WatchdogTimer(JobControlRecord* jcr = nullptr);
  ~WatchdogTimer();

  void Start(std::chrono::milliseconds interval);
  void Stop();

  JobControlRecord* jcr_ = nullptr;
  const TimerThread::Timer* GetTimerControlledItem() const
  {
    return timer_item;
  }

 protected:
  TimerThread::Timer* timer_item = nullptr;

  // deleted:
  WatchdogTimer() = delete;
  WatchdogTimer(const WatchdogTimer& other) = delete;
  WatchdogTimer(const WatchdogTimer&& other) = delete;
  WatchdogTimer& operator=(const WatchdogTimer& rhs) = delete;
  WatchdogTimer& operator=(const WatchdogTimer&& rhs) = delete;
};

class BThreadWatchdog : public WatchdogTimer {
 public:
  BThreadWatchdog(JobControlRecord* jcr);
  BThreadWatchdog(std::chrono::milliseconds waittime, JobControlRecord* jcr);
  ~BThreadWatchdog() = default;

  pthread_t thread_id_;
  static void Callback(TimerThread::Timer* timer);

 private:
  void Init();
};

class BProcessWatchdog : public WatchdogTimer {
 public:
  pid_t pid_;
  bool killed_ = false;
};

class BSockWatchdog : public WatchdogTimer {
 public:
  BareosSocket* bsock_;
};

#endif  // BAREOS_LIB_WATCHDOG_TIMER_H_
