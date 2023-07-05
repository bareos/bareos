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
  keeper.handle_event_buffer(std::move(buffer));
}

static constexpr std::size_t buffer_filled_ok = 1000;
static_assert(buffer_filled_ok < buffer_full);

static struct buffer_is_locked_t {
} event_buffer_locked;

static void FlushEventsIfNecessary(TimeKeeper& keeper,
                                   std::thread::id this_id,
                                   const std::vector<event::OpenEvent>& stack,
                                   EventBuffer& buffer,
                                   buffer_is_locked_t)
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
  std::unique_lock _{vec_mut};
  FlushEventsIfNecessary(keeper, this_id, stack, buffer, event_buffer_locked);
  auto& event = stack.emplace_back(block);
  buffer.events.emplace_back(event);
}

void ThreadTimeKeeper::switch_to(const BlockIdentity& block)
{
  std::unique_lock _{vec_mut};
  FlushEventsIfNecessary(keeper, this_id, stack, buffer, event_buffer_locked);
  ASSERT(stack.size() != 0);
  auto event = stack.back().close();
  buffer.events.push_back(event);
  stack.back() = event::OpenEvent(block);
  buffer.events.push_back(stack.back());
}

void ThreadTimeKeeper::exit()
{
  std::unique_lock _{vec_mut};
  FlushEventsIfNecessary(keeper, this_id, stack, buffer, event_buffer_locked);
  ASSERT(stack.size() != 0);
  auto event = stack.back().close();
  buffer.events.push_back(event);
  stack.pop_back();
}

static void write_reports(bool* end,
                          std::mutex* gen_mut,
                          std::optional<OverviewReport>* overview,
                          std::optional<CallstackReport>* callstack,
                          std::mutex* buf_mut,
                          std::condition_variable* buf_empty,
                          std::condition_variable* buf_not_empty,
                          std::deque<EventBuffer>* buf_queue)
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
    EventBuffer buf;
    bool now_empty = false;
    {
      std::unique_lock lock{*buf_mut};
      buf_not_empty->wait(
          lock, [end, buf_queue]() { return *end || buf_queue->size() > 0; });

      if (buf_queue->size() == 0) break;

      buf = std::move(buf_queue->front());
      buf_queue->pop_front();

      if (buf_queue->size() == 0) now_empty = true;
    }

    if (overview->has_value()) overview->value().add_events(buf);
    if (callstack->has_value()) callstack->value().add_events(buf);

    if (now_empty) buf_empty->notify_all();
  }
}

TimeKeeper::TimeKeeper()
    : report_writer{&write_reports, &end,           &gen_mut,
                    &overview,      &callstack,     &buf_mut,
                    &buf_empty,     &buf_not_empty, &buf_queue}
{
}

ThreadTimeKeeper& TimeKeeper::get_thread_local()
{
  // TODO: threadlocal here ?
  std::thread::id my_id = std::this_thread::get_id();
  {
    std::shared_lock read_lock(alloc_mut);
    if (auto found = keeper.find(my_id); found != keeper.end()) {
      return found->second;
    }
  }
  {
    std::unique_lock write_lock(alloc_mut);
    auto [iter, inserted]
        = keeper.emplace(std::piecewise_construct, std::forward_as_tuple(my_id),
                         std::forward_as_tuple(*this));
    ASSERT(inserted);
    return iter->second;
  }
}

void TimeKeeper::add_writer(std::shared_ptr<ReportGenerator> gen)
{
  auto now = event::clock::now();
  gen->begin_report(now);
  {
    std::unique_lock lock{gen_mut};
    gens.emplace_back(std::move(gen));
  }
}

void TimeKeeper::remove_writer(std::shared_ptr<ReportGenerator> gen)
{
  {
    std::unique_lock lock{gen_mut};
    gens.erase(std::remove(gens.begin(), gens.end(), gen), gens.end());
  }
  auto now = event::clock::now();
  gen->end_report(now);
}
