/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_TIMER_THREAD_H_
#define BAREOS_LIB_TIMER_THREAD_H_

#include "lib/dlist.h"

#include <chrono>

namespace TimerThread {

struct Timer {
  bool single_shot = true;
  bool is_active = false;
  std::chrono::milliseconds interval;
  void (*user_callback)(Timer* t) = nullptr;
  void (*user_destructor)(Timer* t) = nullptr;
  void* user_data = nullptr;

  std::chrono::steady_clock::time_point scheduled_run_timepoint;
};

bool Start(void);
void Stop(void);

Timer* NewTimer(void);

bool RegisterTimer(Timer* t);
bool UnregisterTimer(Timer* t);
bool IsRegisteredTimer(const Timer* t);

bool CurrentThreadIsTimerThread();
void SetTimerIdleSleepTime(std::chrono::seconds time);

std::time_t GetCalenderTimeOnLastTimerThreadRun();

}  // namespace TimerThread

#endif /* BAREOS_LIB_TIMER_THREAD_H_ */
