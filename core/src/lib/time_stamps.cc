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

#include "lib/message.h"
#include "include/messages.h"

ThreadTimeKeeper::~ThreadTimeKeeper()
{
  keeper.handle_event_buffer(std::move(buffer.lock().get()));
}

static constexpr std::size_t buffer_filled_ok = 1000;
static_assert(buffer_filled_ok < buffer_full);

static void FlushEventsIfNecessary(TimeKeeper& keeper,
                                   std::thread::id this_id,
                                   const std::vector<event::OpenEvent>& stack,
                                   EventBuffer& buffer)
{
  if (buffer.events.size() >= buffer_full) {
    keeper.handle_event_buffer(std::move(buffer));
    buffer = EventBuffer(this_id, buffer_full, stack);
  } else if (buffer.events.size() >= buffer_filled_ok) {
    if (keeper.try_handle_event_buffer(buffer)) {
      buffer = EventBuffer(this_id, buffer_full, stack);
    }
  }
}

void ThreadTimeKeeper::enter(const BlockIdentity& block)
{
  auto locked = buffer.lock();
  FlushEventsIfNecessary(keeper, this_id, stack, *locked);
  auto& event = stack.emplace_back(block);
  locked->events.emplace_back(event);
}

void ThreadTimeKeeper::switch_to(const BlockIdentity& block)
{
  auto locked = buffer.lock();
  FlushEventsIfNecessary(keeper, this_id, stack, *locked);
  ASSERT(stack.size() != 0);
  auto event = stack.back().close();
  locked->events.push_back(event);
  stack.back() = event::OpenEvent(block);
  locked->events.push_back(stack.back());
}

void ThreadTimeKeeper::exit()
{
  auto locked = buffer.lock();
  FlushEventsIfNecessary(keeper, this_id, stack, *locked);
  ASSERT(stack.size() != 0);
  auto event = stack.back().close();
  locked->events.push_back(event);
  stack.pop_back();
}

static void write_reports(std::mutex* gen_mut,
                          std::optional<OverviewReport>* overview,
                          std::optional<CallstackReport>* callstack,
                          channel::out<EventBuffer> queue)
{
  for (;;) {
    {
      int perf = GetPerf();
      if (!(perf & static_cast<std::int32_t>(PerfReport::Overview))) {
        if (overview->has_value()) {
          std::unique_lock lock{*gen_mut};
          overview->reset();
        }
      } else {
        if (!overview->has_value()) {
          std::unique_lock lock{*gen_mut};
          overview->emplace(OverviewReport::ShowAll)
              .begin_report(event::clock::now());
        }
      }
      if (!(perf & static_cast<std::int32_t>(PerfReport::Stack))) {
        if (callstack->has_value()) {
          std::unique_lock lock{*gen_mut};
          callstack->reset();
        }
      } else {
        if (!callstack->has_value()) {
          std::unique_lock lock{*gen_mut};
          callstack->emplace(CallstackReport::ShowAll)
              .begin_report(event::clock::now());
        }
      }
    }

    if (std::optional opt = queue.get(); opt.has_value()) {
      EventBuffer& buf = opt.value();
      if (overview->has_value()) overview->value().add_events(buf);
      if (callstack->has_value()) callstack->value().add_events(buf);
    } else {
      break;
    }
  }
}

TimeKeeper::TimeKeeper(
    std::pair<channel::in<EventBuffer>, channel::out<EventBuffer>> p)
    : queue{std::move(p.first)}
    , report_writer{&write_reports, &gen_mut, &overview, &callstack,
                    std::move(p.second)}
{
}

ThreadTimeKeeper& TimeKeeper::get_thread_local()
{
  // this is most likely just a read from a thread local variable
  // anyways, so we do not need to store this inside a threadlocal ourselves
  std::thread::id my_id = std::this_thread::get_id();
  {
    auto locked = keeper.rlock();
    if (auto found = locked->find(my_id); found != locked->end()) {
      return const_cast<ThreadTimeKeeper&>(found->second);
    }
  }
  {
    auto [iter, inserted] = keeper.wlock()->emplace(
        std::piecewise_construct, std::forward_as_tuple(my_id),
        std::forward_as_tuple(*this));
    ASSERT(inserted);
    return iter->second;
  }
}
