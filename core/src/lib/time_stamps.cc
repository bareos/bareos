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

#include <lib/time_stamps.h>

#include <mutex>
#include <cassert>
#include <algorithm>
#include <atomic>

#include "lib/message.h"
#include "include/messages.h"

ThreadTimeKeeper::~ThreadTimeKeeper()
{
  buffer.emplace_back(event::StopRecording{});
  queue.lock()->put(std::move(buffer));
}


static void FlushEventsIfNecessary(synchronized<channel::in<EventBuffer>>& queue,
                                   ThreadTimeKeeper::thread_id this_id,
                                   EventBuffer& buffer)
{
  constexpr std::size_t buffer_filled_ok = 1000;
  if (buffer.size() >= buffer_filled_ok) {
    if (std::optional locked = queue.try_lock();
	locked.has_value()) {
      if ((*locked)->try_put(buffer)) {
	buffer = EventBuffer(this_id, ThreadTimeKeeper::event_buffer_init_capacity);
      }
    }
  }
}

void ThreadTimeKeeper::enter(const BlockIdentity& block)
{
  FlushEventsIfNecessary(queue, this_id, buffer);
  auto event = event::OpenEvent{block};
  buffer.emplace_back(std::move(event));
}

void ThreadTimeKeeper::exit(const BlockIdentity& block)
{
  FlushEventsIfNecessary(queue, this_id, buffer);
  auto event = event::CloseEvent{block};
  buffer.emplace_back(std::move(event));
}

static void write_reports(ReportGenerator* gen,
                          channel::out<EventBuffer> queue)
{
  for (;;) {
    if (std::optional opt = queue.get_all(); opt.has_value()) {
      for (EventBuffer& buf : opt.value()) {
	gen->add_events(buf);
      }
    } else {
      break;
    }
  }
}

TimeKeeper::TimeKeeper(
    std::pair<channel::in<EventBuffer>, channel::out<EventBuffer>> p)
    : queue{std::move(p.first)}
    , report_writer{&write_reports, &perf,
                    std::move(p.second)}
{
}

static TimeKeeper::thread_id get_my_id()
{
  static std::atomic<TimeKeeper::thread_id> counter{0};
  thread_local TimeKeeper::thread_id my_id = counter.fetch_add(1, std::memory_order_relaxed);
  return my_id;
}

ThreadTimeKeeper& TimeKeeper::get_thread_local()
{
  // this is most likely just a read from a thread local variable
  // anyways, so we do not need to store this inside a threadlocal ourselves

  // if (during the execution of a job) threads get destroyed and afterwards
  // new threads get created, then those threads may in fact have the same id
  // leading to get_thread_local to return the same reference.  Note that this
  // is still completely thread safe!
  // It will lead to weird results however so we use this alternative instead:
  thread_id my_id = get_my_id();
  {
    auto locked = keeper.rlock();
    if (auto found = locked->find(my_id); found != locked->end()) {
      return const_cast<ThreadTimeKeeper&>(found->second);
    }
  }
  {
    auto [iter, inserted] = keeper.wlock()->emplace(std::piecewise_construct,
						    std::forward_as_tuple(my_id),
						    std::forward_as_tuple(queue, my_id));
    ASSERT(inserted);
    return iter->second;
  }
}

void TimeKeeper::erase_thread_local()
{
  thread_id my_id = get_my_id();

  keeper.wlock()->erase(my_id);
}
