/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.

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
/*
 * BAREOS thread watchdog routine. General routine that
 * allows setting a watchdog timer with a callback that is
 * called when the timer goes off.
 *
 * Kern Sibbald, January MMII
 */

#include "include/bareos.h"
#include "lib/timer_thread.h"
#include "include/jcr.h"
#include "lib/thread_specific_data.h"
#include "include/make_unique.h"

#include <atomic>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace TimerThread {

/* clang-format off */
static std::chrono::milliseconds idle_timeout_interval_milliseconds(60 * 1000);
static std::atomic<std::time_t> calendar_time_on_last_watchdog_run(time(nullptr));
/* clang-format on */

static std::mutex watchdog_sleep_mutex;
static std::condition_variable watchdog_sleep_condition;
static bool wakeup_event_occured = false;

static void WatchdogThread(void);

static std::atomic<bool> quit(false);
static std::atomic<bool> is_initialized(false);

static std::unique_ptr<std::thread> watchdog_thread;
static std::mutex controlled_items_list_mutex;

static std::vector<TimerThread::TimerControlledItem*> controlled_items_list;

bool CurrentThreadIsTimerThread()
{
  if (is_initialized.load() &&
      (std::this_thread::get_id() == watchdog_thread->get_id())) {
    return true;
  } else {
    return false;
  }
}

int StartTimerThread(void)
{
  if (is_initialized.load()) { return 0; }

  Dmsg0(800, "Starting watchdog thread\n");

  quit.store(false);
  watchdog_thread = std::make_unique<std::thread>(WatchdogThread);

  int timeout = 0;
  while (!is_initialized.load() && ++timeout < 2000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return 0;
}

static void WakeWatchdog()
{
  std::lock_guard<std::mutex> l(watchdog_sleep_mutex);
  wakeup_event_occured = true;
  watchdog_sleep_condition.notify_one();
}

void StopTimerThread(void)
{
  if (!is_initialized.load()) { return; }

  quit.store(true);
  WakeWatchdog();

  watchdog_thread->join();
}

TimerThread::TimerControlledItem* NewTimerControlledItem(void)
{
  TimerThread::TimerControlledItem* t = new TimerThread::TimerControlledItem;

  std::lock_guard<std::mutex> l(controlled_items_list_mutex);
  controlled_items_list.push_back(t);

  if (!is_initialized.load()) { StartTimerThread(); }

  return t;
}

bool RegisterTimer(TimerThread::TimerControlledItem* t)
{
  assert(t->callback != nullptr);

  TimerThread::TimerControlledItem wd_copy;

  {
    std::lock_guard<std::mutex> l(controlled_items_list_mutex);

    if (std::find(controlled_items_list.begin(), controlled_items_list.end(),
                  t) == controlled_items_list.end()) {
      return false;
    }

    t->scheduled_run_timepoint = std::chrono::steady_clock::now() + t->interval;
    t->is_active = true;

    wd_copy = *t;
  }

  Dmsg3(800, "Registered watchdog interval %d%s\n", wd_copy.interval,
        wd_copy.one_shot ? " one shot" : "");

  WakeWatchdog();

  return true;
}

bool UnregisterTimer(TimerThread::TimerControlledItem* t)
{
  if (!is_initialized.load()) {
    Jmsg0(nullptr, M_ABORT, 0,
          _("UnregisterWatchdog_unlocked called before StartTimerThread\n"));
  }

  std::lock_guard<std::mutex> l(controlled_items_list_mutex);

  auto pos =
      std::find(controlled_items_list.begin(), controlled_items_list.end(), t);

  if (pos != controlled_items_list.end()) {
    if ((*pos)->destructor) { (*pos)->destructor((*pos)); }
    delete (*pos);
    controlled_items_list.erase(pos);
    Dmsg1(800, "Unregistered watchdog %p\n", t);
    return true;
  } else {
    Dmsg1(800, "Failed to unregister watchdog %p\n", t);
    return false;
  }
}

bool IsRegisteredTimer(const TimerThread::TimerControlledItem* t)
{
  if (!is_initialized.load()) {
    Jmsg0(nullptr, M_ABORT, 0,
          _("UnregisterWatchdog_unlocked called before StartTimerThread\n"));
  }

  std::lock_guard<std::mutex> l(controlled_items_list_mutex);

  auto pos =
      std::find(controlled_items_list.begin(), controlled_items_list.end(), t);

  return pos != controlled_items_list.end();
}

static void Cleanup()
{
  std::lock_guard<std::mutex> l(controlled_items_list_mutex);

  for (auto p : controlled_items_list) {
    if (p->destructor) { p->destructor(p); }
    delete p;
  }
  controlled_items_list.clear();
}

static void SleepUntil(std::chrono::steady_clock::time_point next_watchdog_run)
{
  std::unique_lock<std::mutex> l(watchdog_sleep_mutex);
  watchdog_sleep_condition.wait_until(l, next_watchdog_run,
                                      []() { return wakeup_event_occured; });
  wakeup_event_occured = false;
}

static bool LogMessage(TimerThread::TimerControlledItem* p)
{
  Dmsg2(3400, "Watchdog callback p=0x%p scheduled_run_timepoint=%d\n", p,
        p->scheduled_run_timepoint);
}

static bool RunOneItem(TimerThread::TimerControlledItem* p,
                       std::chrono::steady_clock::time_point& next_watchdog_run)
{
  std::chrono::time_point<std::chrono::steady_clock>
      last_watchdog_run_timepoint = std::chrono::steady_clock::now();

  bool remove_from_list = false;
  if (p->is_active &&
      last_watchdog_run_timepoint > p->scheduled_run_timepoint) {
    LogMessage(p);
    p->callback(p);
    if (p->one_shot) {
      if (p->destructor) { p->destructor(p); }
      delete p;
      remove_from_list = true;
    } else {
      p->scheduled_run_timepoint = last_watchdog_run_timepoint + p->interval;
    }
  }
  next_watchdog_run = min(p->scheduled_run_timepoint, next_watchdog_run);
  return remove_from_list;
}

static void RunAllItemsAndRemoveOneShotItems(
    std::chrono::steady_clock::time_point& next_watchdog_run)
{
  std::unique_lock<std::mutex> l(controlled_items_list_mutex);

  auto new_end_of_vector = std::remove_if(  // one_shot items will be removed
      controlled_items_list.begin(), controlled_items_list.end(),
      [&next_watchdog_run](TimerThread::TimerControlledItem* p) {
        calendar_time_on_last_watchdog_run.store(time(nullptr));

        return RunOneItem(p, next_watchdog_run);
      });

  controlled_items_list.erase(new_end_of_vector, controlled_items_list.end());
}

static void WatchdogThread(void)
{
  SetJcrInThreadSpecificData(INVALID_JCR);

  Dmsg0(800, "Watchdog thread started\n");
  is_initialized.store(true);

  while (!quit.load()) {
    std::chrono::steady_clock::time_point next_watchdog_run =
        std::chrono::steady_clock::now() + idle_timeout_interval_milliseconds;

    RunAllItemsAndRemoveOneShotItems(next_watchdog_run);

    SleepUntil(next_watchdog_run);
  }  // while (!quit)

  Cleanup();
  is_initialized.store(false);
}

class StopWatchdogThreadGuard {
 public:
  StopWatchdogThreadGuard() = default;
  ~StopWatchdogThreadGuard()
  {
    if (is_initialized.load()) { StopTimerThread(); }
  }
};

static StopWatchdogThreadGuard stop_watchdog_thread_guard;

void SetTimerIdleSleepTime(std::chrono::seconds time)
{
  std::lock_guard<std::mutex> lg(controlled_items_list_mutex);
  idle_timeout_interval_milliseconds = time;
}

std::time_t GetCalenderTimeOnLastTimerThreadRun()
{
  return calendar_time_on_last_watchdog_run.load();
}

}  // namespace TimerThread
