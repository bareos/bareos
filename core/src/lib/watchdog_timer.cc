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

#include "watchdog_timer.h"
#include "include/jcr.h"
#include "lib/timer_thread.h"
#include "lib/watchdog.h"

#include <cassert>

static const int debuglevel = 900;

WatchdogTimer::WatchdogTimer(JobControlRecord* jcr)
{
  timer_item = TimerThread::NewControlledItem();
  timer_item->user_data = this;
  timer_item->single_shot = true;
  jcr_ = jcr;
}

WatchdogTimer::~WatchdogTimer()
{
  if (timer_item) { UnregisterTimer(timer_item); }
}

void WatchdogTimer::Start(std::chrono::seconds interval)
{
  timer_item->interval = interval;
  RegisterTimer(timer_item);
}

void WatchdogTimer::Stop()
{
  if (timer_item) {
    UnregisterTimer(timer_item);
    timer_item = nullptr;
  }
}

void BThreadWatchdog::Callback(TimerThread::TimerControlledItem* item)
{
  BThreadWatchdog* timer = reinterpret_cast<BThreadWatchdog*>(item->user_data);
  if (!timer) { return; }

  if (timer->jcr_) {
    Dmsg2(debuglevel, "killed JobId=%u Job=%s\n", timer->jcr_->JobId,
          timer->jcr_->Job);
  }

  pthread_kill(timer->thread_id_, TIMEOUT_SIGNAL);
}

void BThreadWatchdog::Init()
{
  thread_id_ = pthread_self();
  timer_item->user_callback = Callback;
}

BThreadWatchdog::BThreadWatchdog(JobControlRecord* jcr) : WatchdogTimer(jcr)
{
  // has to be started by Start(interval)
  Init();
}

BThreadWatchdog::BThreadWatchdog(std::chrono::seconds interval,
                                 JobControlRecord* jcr)
    : WatchdogTimer(jcr)
{
  assert(interval != std::chrono::seconds::zero());

  Init();

  Start(interval);
}
