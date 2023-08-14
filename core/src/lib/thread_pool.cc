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
#include "lib/thread_pool.h"

thread_pool::thread_pool(std::size_t num_threads)
{
  if (num_threads) ensure_num_workers(num_threads);
}

void thread_pool::enqueue(task&& t)
{
  bool task_submitted = false;

  if (auto wlocked = workers.lock(); wlocked->num_actual_workers() > 0) {
    auto locked = queue.lock();
    if (locked->has_value()) {
      locked->value().emplace_back(std::move(t));
      tasks_submitted += 1;
      queue_or_death.notify_one();
      task_submitted = true;
    }
  }

  if (!task_submitted) { t(); }
}

void thread_pool::ensure_num_workers(std::size_t num_threads)
{
  auto locked = workers.lock();
  locked->min_workers = std::max(locked->min_workers, num_threads);
  for (std::size_t i = locked->threads.size(); i < num_threads; ++i) {
    locked->threads.emplace_back(&pool_work, i, this);
  }
}

void thread_pool::finish()
{
  auto locked = tasks_completed.lock();

  locked.wait(on_task_completion,
              [this](auto completed) { return completed == tasks_submitted; });
}

thread_pool::~thread_pool()
{
  queue.lock()->reset();
  queue_or_death.notify_all();

  auto locked = workers.lock();
  locked.wait(worker_death, [](const auto& pool) {
    return pool.dead_workers == pool.threads.size();
  });

  for (auto& thread : locked->threads) { thread.join(); }
}

void thread_pool::borrow_then_pool_work(task&& t, size_t id, thread_pool* pool)
{
  {
    auto fun = std::move(t);
    fun();
  }

  {
    auto locked = pool->workers.lock();
    locked->num_borrowed -= 1;
  }

  pool_work(id, pool);
}

void thread_pool::borrow_thread(task&& t)
{
  enqueue([this, t = std::move(t)]() {
    {
      auto locked = workers.lock();
      auto min_left_over_workers
          = std::max(std::size_t{1}, locked->min_workers);
      if (locked->num_actual_workers() <= min_left_over_workers) {
        locked->threads.emplace_back(borrow_then_pool_work, std::move(t),
                                     locked->threads.size(), this);
        locked->num_borrowed += 1;
        return;
      }
      locked->num_borrowed += 1;
    }

    {
      auto fun = std::move(t);
      fun();
    }

    {
      auto locked = workers.lock();
      locked->num_borrowed -= 1;
    }
  });
}

void thread_pool::pool_work(std::size_t id, thread_pool* pool)
{
  // id is kept here for debugging purposes.
  (void)id;
  try {
    for (std::optional my_task = pool->dequeue(); my_task.has_value();
         my_task = pool->finish_and_dequeue()) {
      std::optional t = std::move(my_task);
      my_task.reset();
      (*t)();
    }

    pool->workers.lock()->dead_workers += 1;
    pool->worker_death.notify_one();
  } catch (...) {
    pool->workers.lock()->dead_workers += 1;
    pool->worker_death.notify_one();
    throw;
  }
}

auto thread_pool::dequeue() -> std::optional<task>
{
  auto locked = queue.lock();
  locked.wait(queue_or_death, [](auto& queue) {
    return !queue.has_value() || queue->size() > 0;
  });
  if (!locked->has_value()) { return std::nullopt; }
  task t = std::move(locked->value().front());
  locked->value().pop_front();
  return t;
}

auto thread_pool::finish_and_dequeue() -> std::optional<task>
{
  *tasks_completed.lock() += 1;
  on_task_completion.notify_all();

  return dequeue();
}
