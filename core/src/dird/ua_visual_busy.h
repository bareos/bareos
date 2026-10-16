/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_DIRD_UA_VISUAL_BUSY_H_
#define BAREOS_DIRD_UA_VISUAL_BUSY_H_

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace directordaemon {

class UaContext;

namespace visual_busy_internal {

class BusySpinner {
 public:
  using Clock = std::chrono::steady_clock;

  char Start(Clock::time_point now);
  std::optional<char> Tick(Clock::time_point now);

 private:
  static constexpr auto kRefreshInterval = std::chrono::milliseconds(200);

  size_t frame_index_ = 0;
  Clock::time_point next_refresh_{};
};

std::string RenderBusyScreen(int width,
                             int height,
                             bool color,
                             std::string_view title,
                             std::string_view message,
                             std::string_view detail,
                             char spinner);

}  // namespace visual_busy_internal

class VisualBusyIndicator {
 public:
  using Renderer = std::function<std::string(char)>;

  VisualBusyIndicator(UaContext* ua, Renderer renderer);

  void Start();
  void Tick();

 private:
  void Redraw(char spinner);

  UaContext* ua_;
  Renderer renderer_;
  visual_busy_internal::BusySpinner spinner_;
};

}  // namespace directordaemon

#endif  // BAREOS_DIRD_UA_VISUAL_BUSY_H_
