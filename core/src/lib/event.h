/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2023 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_EVENT_H_
#define BAREOS_LIB_EVENT_H_

#include <chrono>
#include <variant>
#include <thread>
#include <vector>
#include <mutex>
#include <cstdint>

class BlockIdentity {
 public:
  // TODO: replace with source_location (with C++20)
  constexpr explicit BlockIdentity(const char* name) : name{name} {}

  const char* c_str() const { return name; }

 private:
  const char* name;
};

namespace event {
using clock = std::chrono::steady_clock;
using time_point = clock::time_point;

struct CloseEvent {
  BlockIdentity const* source;
  time_point end;
  CloseEvent(BlockIdentity const* source) noexcept(true) : source{source}, end{clock::now()} {}
  CloseEvent(BlockIdentity const& source) noexcept(true) : source{&source}, end{clock::now()}
  {
  }
};

struct OpenEvent {
  BlockIdentity const* source;
  time_point start;

  OpenEvent(BlockIdentity const* source) noexcept(true) : source{source}, start{clock::now()}
  {
  }
  OpenEvent(BlockIdentity const& source) noexcept(true) : source{&source}, start{clock::now()}
  {
  }
  CloseEvent close() const { return CloseEvent{source}; }
};

struct StartRecording {
  time_point when;

  StartRecording() noexcept(true) : when{clock::now()}
  {
  }
};

struct StopRecording {
  time_point when;

  StopRecording() noexcept(true) : when{clock::now()}
  {
  }
};

using Event = std::variant<OpenEvent, CloseEvent, StartRecording, StopRecording>;


};  // namespace event


class EventBuffer {
 public:
  using thread_id = std::uint64_t;
  EventBuffer() {}
  EventBuffer(thread_id threadid,
              std::size_t initial_size)
      : threadid_{threadid}
  {
    events.reserve(initial_size);
  }
  EventBuffer(EventBuffer&&) = default;
  EventBuffer& operator=(EventBuffer&&) = default;
  thread_id threadid() const { return threadid_; }

  template <typename... Args>
  auto emplace_back(Args... args)
  {
    return events.emplace_back(std::forward<Args>(args)...);
  }

  auto size() const { return events.size(); }

  auto begin() const { return events.begin(); }
  auto end() const { return events.end(); }

 private:
  thread_id threadid_;
  std::vector<event::Event> events{};
};

#endif  // BAREOS_LIB_EVENT_H_
