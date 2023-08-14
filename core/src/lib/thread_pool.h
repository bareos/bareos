/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

#include "lib/thread_util.h"

struct worker_pool {
  std::vector<std::thread> threads{};
  std::size_t num_borrowed{0};
  std::size_t min_workers{0};
  std::size_t dead_workers{0};

  std::size_t num_actual_workers() const
  {
    return threads.size() - num_borrowed - dead_workers;
  }
};

class thread_pool {
 public:
  // fixme(ssura): change this to std::move_only_function (c++23)
  using task = std::function<void()>;

  thread_pool(std::size_t num_threads = 0);

  void ensure_num_workers(std::size_t num_threads);
  void finish();   // wait until all submitted tasks are accomplished
  ~thread_pool();  // stops as fast as possible; dropping outstanding tasks

  void enqueue(task&& t);  // queue task to be worked on by worker threads;
                           // should not block
  void borrow_thread(task&& t);  // steal thread to work on task t; can block

 private:
  std::condition_variable worker_death;
  synchronized<worker_pool> workers{};

  std::condition_variable queue_or_death;
  synchronized<std::optional<std::deque<task>>> queue{std::in_place};

  std::size_t tasks_submitted{0};
  std::condition_variable on_task_completion;
  synchronized<std::size_t> tasks_completed{0u};

  static void pool_work(std::size_t id, thread_pool* pool);
  static void borrow_then_pool_work(task&& t,
                                    std::size_t id,
                                    thread_pool* pool);

  std::optional<task> dequeue();
  std::optional<task> finish_and_dequeue();
};

template <typename F>
auto enqueue(thread_pool& pool, F&& f) -> std::future<std::invoke_result_t<F>>
{
  using result_type = std::invoke_result_t<F>;
  // todo(ssura): in c++23 we can use std::move_only_function
  //              and pass the promise directly into the function.
  //              currently this approach does not work because
  //              std::function requires the function to be copyable,
  //              but std::promise obviously is not.
  std::shared_ptr p = std::make_shared<std::promise<result_type>>();
  std::future fut = p->get_future();
  pool.enqueue([f = std::move(f), mp = std::move(p)]() mutable {
    try {
      if constexpr (std::is_same_v<result_type, void>) {
        f();
        mp->set_value();
      } else {
        mp->set_value(f());
      }
    } catch (...) {
      mp->set_exception(std::current_exception());
    }
  });
  return fut;
}

template <typename F>
auto borrow_thread(thread_pool& pool, F&& f)
    -> std::future<std::invoke_result_t<F>>
{
  using result_type = std::invoke_result_t<F>;
  // todo(ssura): in c++23 we can use std::move_only_function
  //              and pass the promise directly into the function.
  //              currently this approach does not work because
  //              std::function requires the function to be copyable,
  //              but std::promise obviously is not.
  std::shared_ptr p = std::make_shared<std::promise<result_type>>();
  std::future fut = p->get_future();
  pool.borrow_thread([f = std::move(f), mp = std::move(p)]() mutable {
    try {
      if constexpr (std::is_same_v<result_type, void>) {
        f();
        mp->set_value();
      } else {
        mp->set_value(f());
      }
    } catch (...) {
      mp->set_exception(std::current_exception());
    }
  });
  return fut;
}


#endif  // BAREOS_LIB_THREAD_POOL_H_
