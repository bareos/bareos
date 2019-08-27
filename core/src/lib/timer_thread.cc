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
static std::atomic<std::time_t> calendar_time_on_last_timer_run(time(nullptr));
/* clang-format on */

static std::mutex timer_sleep_mutex;
static std::condition_variable timer_sleep_condition;
static bool wakeup_event_occured = false;

static void TimerThread(void);

enum class TimerThreadState
{
  IS_NOT_INITIALZED,
  IS_STARTING,
  IS_RUNNING,
  IS_SHUTTING_DOWN,
  IS_SHUT_DOWN
};

static std::atomic<TimerThreadState> timer_thread_state(
    TimerThreadState::IS_NOT_INITIALZED);
static std::atomic<bool> quit_timer_thread(false);

static std::unique_ptr<std::thread> timer_thread;
static std::mutex controlled_items_list_mutex;

static std::vector<TimerThread::Timer*> controlled_items_list;

bool Start(void)
{
  if (timer_thread_state != TimerThreadState::IS_NOT_INITIALZED &&
      timer_thread_state != TimerThreadState::IS_SHUT_DOWN) {
    return false;
  }

  Dmsg0(800, "Starting timer thread\n");

  quit_timer_thread = false;
  timer_thread = std::make_unique<std::thread>(TimerThread);

  int timeout = 0;
  while (timer_thread_state.load() != TimerThreadState::IS_RUNNING &&
         ++timeout < 2000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  return true;
}

static void WakeTimer()
{
  std::lock_guard<std::mutex> l(timer_sleep_mutex);
  wakeup_event_occured = true;
  timer_sleep_condition.notify_one();
}

void Stop(void)
{
  if (timer_thread_state != TimerThreadState::IS_RUNNING) { return; }

  quit_timer_thread = true;
  WakeTimer();

  timer_thread->join();
}

TimerThread::Timer* NewTimer(void)
{
  TimerThread::Timer* t = new TimerThread::Timer;

  std::lock_guard<std::mutex> l(controlled_items_list_mutex);
  controlled_items_list.push_back(t);

  if (timer_thread_state != TimerThreadState::IS_RUNNING) { Start(); }

  return t;
}

bool RegisterTimer(TimerThread::Timer* t)
{
  assert(t->user_callback != nullptr);

  TimerThread::Timer wd_copy;

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

  Dmsg3(800, "Registered timer interval %d%s\n", wd_copy.interval,
        wd_copy.single_shot ? " one shot" : "");

  WakeTimer();

  return true;
}

bool UnregisterTimer(TimerThread::Timer* t)
{
  std::lock_guard<std::mutex> l(controlled_items_list_mutex);

  auto pos =
      std::find(controlled_items_list.begin(), controlled_items_list.end(), t);

  if (pos != controlled_items_list.end()) {
    if ((*pos)->user_destructor) { (*pos)->user_destructor((*pos)); }
    delete (*pos);
    controlled_items_list.erase(pos);
    Dmsg1(800, "Unregistered timer %p\n", t);
    return true;
  } else {
    Dmsg1(800, "Failed to unregister timer %p\n", t);
    return false;
  }
}

bool IsRegisteredTimer(const TimerThread::Timer* t)
{
  std::lock_guard<std::mutex> l(controlled_items_list_mutex);

  auto pos =
      std::find(controlled_items_list.begin(), controlled_items_list.end(), t);

  return pos != controlled_items_list.end();
}

static void Cleanup()
{
  std::lock_guard<std::mutex> l(controlled_items_list_mutex);

  for (auto p : controlled_items_list) {
    if (p->user_destructor) { p->user_destructor(p); }
    delete p;
  }
  controlled_items_list.clear();
}

static void SleepUntil(std::chrono::steady_clock::time_point next_timer_run)
{
  std::unique_lock<std::mutex> l(timer_sleep_mutex);
  timer_sleep_condition.wait_until(l, next_timer_run,
                                   []() { return wakeup_event_occured; });
  wakeup_event_occured = false;
}

static void LogMessage(TimerThread::Timer* p)
{
  Dmsg2(3400, "Timer callback p=0x%p scheduled_run_timepoint=%d\n", p,
        p->scheduled_run_timepoint);
}

static bool RunOneItem(TimerThread::Timer* p,
                       std::chrono::steady_clock::time_point& next_timer_run)
{
  std::chrono::time_point<std::chrono::steady_clock> last_timer_run_timepoint =
      std::chrono::steady_clock::now();

  bool remove_from_list = false;
  if (p->is_active && last_timer_run_timepoint > p->scheduled_run_timepoint) {
    LogMessage(p);
    p->user_callback(p);
    if (p->single_shot) {
      if (p->user_destructor) { p->user_destructor(p); }
      delete p;
      remove_from_list = true;
    } else {
      p->scheduled_run_timepoint = last_timer_run_timepoint + p->interval;
    }
  }
  next_timer_run = min(p->scheduled_run_timepoint, next_timer_run);
  return remove_from_list;
}

static void RunAllItemsAndRemoveOneShotItems(
    std::chrono::steady_clock::time_point& next_timer_run)
{
  std::unique_lock<std::mutex> l(controlled_items_list_mutex);

  auto new_end_of_vector = std::remove_if(  // one_shot items will be removed
      controlled_items_list.begin(), controlled_items_list.end(),
      [&next_timer_run](TimerThread::Timer* p) {
        calendar_time_on_last_timer_run = time(nullptr);

        return RunOneItem(p, next_timer_run);
      });

  controlled_items_list.erase(new_end_of_vector, controlled_items_list.end());
}

static void TimerThread(void)
{
  SetJcrInThreadSpecificData(INVALID_JCR);

  Dmsg0(800, "Timer thread started\n");
  timer_thread_state = TimerThreadState::IS_RUNNING;

  while (!quit_timer_thread) {
    std::chrono::steady_clock::time_point next_timer_run =
        std::chrono::steady_clock::now() + idle_timeout_interval_milliseconds;

    RunAllItemsAndRemoveOneShotItems(next_timer_run);

    SleepUntil(next_timer_run);
  }  // while (!quit_timer_thread)

  Cleanup();
  timer_thread_state = TimerThreadState::IS_SHUT_DOWN;
}

class TimerThreadGuard {
 public:
  TimerThreadGuard() = default;
  ~TimerThreadGuard()
  {
    if (timer_thread_state == TimerThreadState::IS_RUNNING) { Stop(); }
  }
};

static TimerThreadGuard timer_thread_guard;

void SetTimerIdleSleepTime(std::chrono::seconds time)
{
  std::lock_guard<std::mutex> lg(controlled_items_list_mutex);
  idle_timeout_interval_milliseconds = time;
}

bool CurrentThreadIsTimerThread()
{
  return (timer_thread_state == TimerThreadState::IS_RUNNING &&
          (std::this_thread::get_id() == timer_thread->get_id()));
}

std::time_t GetCalenderTimeOnLastTimerThreadRun()
{
  return calendar_time_on_last_timer_run;
}

}  // namespace TimerThread
