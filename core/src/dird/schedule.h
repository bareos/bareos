/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_SCHEDULE_H
#define BAREOS_DIRD_SCHEDULE_H

#include "date_time.h"

#include <iostream>
#include <sstream>
#include <optional>
#include <string_view>
#include <vector>
#include <array>
#include <variant>
#include <set>

namespace directordaemon {

// kMaxValue
template <class T> constexpr int kMaxValue = 0;
template <> constexpr int kMaxValue<MonthOfYear> = 11;
template <> constexpr int kMaxValue<WeekOfYear> = 53;
template <> constexpr int kMaxValue<WeekOfMonth> = 4;
template <> constexpr int kMaxValue<DayOfMonth> = 30;
template <> constexpr int kMaxValue<DayOfWeek> = 6;

// Interval
template <class T> struct Interval {
  using Type = T;

  bool Contains(T value) const
  {
    if (first <= last) {
      return first <= value && value <= last;
    } else {
      return first <= value || value <= last;
    }
  }

  T first, last;
};
// :: kIsRange
template <class T> constexpr bool kIsRange = false;
template <class T> constexpr bool kIsRange<Interval<T>> = true;

// Modulo
template <class T> struct Modulo {
  using Type = T;

  bool Contains(T value) const
  {
    return (int(value) % int(right)) == int(left);
  }

  T left, right;
};
// :: kIsModulo
template <class T> constexpr bool kIsModulo = false;
template <class T> constexpr bool kIsModulo<Modulo<T>> = true;

// Mask
template <class T> using Mask = std::variant<Interval<T>, Modulo<T>, T>;
// :: Contains
template <class T> bool Contains(const Mask<T>& mask, T value)
{
  if (auto* range = std::get_if<Interval<T>>(&mask)) {
    return range->Contains(value);
  } else if (auto* modulo = std::get_if<Modulo<T>>(&mask)) {
    return modulo->Contains(value);
  } else {
    return std::get<T>(mask) == value;
  }
  return false;
}
// :: all
template <class T> Mask<T> All()
{
  return {{Interval<T>({T(0), T(kMaxValue<T>)})}};
}

// Hourly
struct Hourly {
  std::set<int> minutes;
};

// Schedule
class Schedule {
 public:
  Schedule() = default;
  Schedule(const std::variant<std::vector<TimeOfDay>, Hourly>& times);

  bool TriggersOnDay(DateTime date_time) const;
  std::vector<time_t> GetMatchingTimes(time_t from, time_t to) const;

  std::vector<std::variant<Mask<MonthOfYear>,
                           Mask<WeekOfYear>,
                           Mask<WeekOfMonth>,
                           Mask<DayOfMonth>,
                           Mask<DayOfWeek>>>
      day_masks;
  std::variant<std::vector<TimeOfDay>, Hourly> times = std::vector<TimeOfDay>{};

 private:
  template <class T> bool IsRestricted() const
  {
    for (const auto& mask : day_masks) {
      if (std::holds_alternative<Mask<T>>(mask)) { return true; }
    }
    return false;
  }

  template <class T> bool TriggersOn(T value) const
  {
    if (!IsRestricted<T>()) { return true; }
    for (const auto& mask : day_masks) {
      if (std::holds_alternative<Mask<T>>(mask)) {
        if (Contains(std::get<Mask<T>>(mask), value)) { return true; }
      }
    }
    return false;
  }
};

}  // namespace directordaemon

#endif  // BAREOS_DIRD_SCHEDULE_H_
