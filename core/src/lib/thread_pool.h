/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

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
/**
 * @file
 * simple thread pool
 */

#ifndef BAREOS_LIB_THREAD_POOL_H_
#define BAREOS_LIB_THREAD_POOL_H_

#include <vector>
#include <thread>
#include <optional>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <future>
#include <type_traits>
#include <exception>
#include <memory>

#include "lib/thread_util.h"
#include "lib/channel.h"
#include "include/baconfig.h"

/* this class is basically std::function<void(void)>; except that
 * it does not require the function to be copyable.
 * We can change this to be an alias for std::move_only_function<void(void)>
 * once it becomes available (C++23). */
class task {
 public:
  /* ensure that we cannot wrap a task with a task;
   * this is needed to disamiguate between creating a task
   * and move-constructing one. */
  template <typename F,
            typename = std::enable_if_t<!std::is_reference_v<F>>,
            typename = std::enable_if_t<!std::is_same_v<F, task>>>
  task(F&& f) : ptr{new impl<F>(std::move(f))}
  {
  }

  task(task&&) = default;
  task& operator=(task&&) = default;

  void operator()() { ptr->operator()(); }

 private:
  // impl_base + impl are used to type erase F
  struct impl_base {
    virtual void operator()() = 0;
    virtual ~impl_base() = default;
  };

  template <typename F>
  struct impl
      : impl_base
      , F {
    impl(F&& f) : F(std::move(f)) {}

    // using F::operator(); does not work :(
    void operator()() override { F::operator()(); }
  };

  std::unique_ptr<impl_base> ptr;
};

struct thread_pool {
  // f does not need to be copyable
  template <typename F> void borrow_thread(F&& f)
  {
    with_free_threads(1, [f = std::move(f)](work_unit& unit) mutable {
      unit.submit(std::move(f));
    });
  }

  // f needs to be copyable here
  template <typename F> void borrow_threads(std::size_t size, F&& f)
  {
    with_free_threads(
        size, [f = std::move(f)](work_unit& unit) mutable { unit.submit(f); });
  }

  ~thread_pool()
  {
    for (auto& sync : units) { sync->lock()->close(); }

    for (auto& thread : threads) { thread.join(); }
  }

 private:
  template <typename ThreadFn>
  void with_free_threads(std::size_t size, ThreadFn f)
  {
    std::size_t found = 0;
    for (std::size_t i = 0; i < threads.size() && found < size; ++i) {
      if (auto locked = units[i]->try_lock();
          locked && locked.value()->is_waiting()) {
        f(*locked.value());
        found += 1;
      }
    }

    // if there were not enough free threads, we need to create new ones
    for (std::size_t i = found; i < size; ++i) {
      auto idx = threads.size();
      add_thread();

      ASSERT(idx == threads.size() - 1);
      f(*units[idx]->lock());
    }
  }

  class work_unit {
   public:
    template <typename F> void submit(F f)
    {
      ASSERT(state == work_state::WAITING);
      fun = std::move(f);
      state = work_state::WORKING;
      state_changed.notify_all();
    }

    void close()
    {
      state = work_state::CLOSED;
      fun.reset();
      state_changed.notify_all();
    }

    bool is_waiting() const { return state == work_state::WAITING; }

    bool is_closed() const { return state == work_state::CLOSED; }

    void do_work()
    {
      ASSERT(state == work_state::WORKING);
      ASSERT(fun.has_value());
      fun.value()();
      fun.reset();
      state = work_state::WAITING;
      state_changed.notify_all();
    }

    std::condition_variable state_changed;

   private:
    enum class work_state
    {
      WORKING,
      WAITING,
      CLOSED
    };
    work_state state{work_state::WAITING};
    std::optional<task> fun;
  };

  std::vector<std::unique_ptr<synchronized<work_unit>>> units;
  std::vector<std::thread> threads{};

  void add_thread()
  {
    // sync will live at least as long as threads, so its ok to *NOT* use
    // a shared ptr here.
    auto& sync
        = units.emplace_back(std::make_unique<synchronized<work_unit>>());
    threads.emplace_back(
        [](thread_pool* pool, synchronized<work_unit>* unit) {
          pool->pool_work(*unit);
        },
        this, sync.get());
  }

  void pool_work(synchronized<work_unit>& unit)
  {
    auto locked = unit.lock();
    for (;;) {
      // the state is either WAITING/CLOSED or WORKING
      locked.wait(locked->state_changed,
                  [](const work_unit& w) { return !w.is_waiting(); });

      // state is either CLOSED or WORKING
      if (locked->is_closed()) { return; }

      // state is WORKING
      locked->do_work();
    }
  }
};

struct work_group {
  void work_until_completion()
  {
    std::optional<task> my_task;
    while (my_task = task_out.lock()->get(), my_task != std::nullopt) {
      my_task->operator()();
    }
  }

  template <typename F, typename T = std::invoke_result_t<F>>
  std::future<T> submit(F&& f)
  {
    std::promise<T> prom;
    std::future ret = prom.get_future();
    task t{[prom = std::move(prom), f = std::move(f)]() mutable {
      try {
        if constexpr (std::is_same_v<T, void>) {
          f();
          prom.set_value();
        } else {
          prom.set_value(f());
        }
      } catch (...) {
        prom.set_exception(std::current_exception());
      }
    }};

    task_in.emplace(std::move(t));
    return ret;
  }

  void shutdown() { task_in.close(); }

  channel::input<task> task_in;
  synchronized<channel::output<task>> task_out;

  work_group(channel::channel_pair<task> tasks)
      : task_in{std::move(tasks.first)}, task_out{std::move(tasks.second)}
  {
  }
  work_group(std::size_t cap)
      : work_group(channel::CreateBufferedChannel<task>(cap))
  {
  }

  // You need to shutdown the work group before destroying it.
  ~work_group() { ASSERT(task_in.closed()); }
};

#endif  // BAREOS_LIB_THREAD_POOL_H_
