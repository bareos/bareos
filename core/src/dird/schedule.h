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

namespace directordaemon {

// kMaxValue
template<class T>
constexpr int kMaxValue = 0;
template<>
constexpr int kMaxValue<MonthOfYear> = 11;
template<>
constexpr int kMaxValue<WeekOfYear> = 53;
template<>
constexpr int kMaxValue<WeekOfMonth> = 4;
template<>
constexpr int kMaxValue<DayOfMonth> = 30;
template<>
constexpr int kMaxValue<DayOfWeek> = 6;

// kFullRangeLiteral
template<class T>
constexpr std::string_view kFullRangeLiteral = "";
template<>
constexpr std::string_view kFullRangeLiteral<MonthOfYear> = "monthly";
template<>
constexpr std::string_view kFullRangeLiteral<WeekOfYear> = "weekly";
template<>
constexpr std::string_view kFullRangeLiteral<WeekOfMonth> = "weekly";
template<>
constexpr std::string_view kFullRangeLiteral<DayOfMonth> = "daily";
template<>
constexpr std::string_view kFullRangeLiteral<DayOfWeek> = "daily";

// Range
template<class T>
struct Range {
  using Type = T;

  bool Contains(T value) const {
    if (from <= to) {
      return from <= value && value <= to;
    }
    else {
      return from <= value || value <= to;
    }
  }

  T from, to;
};
// :: kIsRange
template<class T>
constexpr bool kIsRange = false;
template<class T>
constexpr bool kIsRange<Range<T>> = true;

// Modulo
template<class T>
struct Modulo {
  using Type = T;

  bool Contains(T value) const {
    return (int(value) % int(right)) == int(left);
  }

  T left, right;
};
// :: kIsModulo
template<class T>
constexpr bool kIsModulo = false;
template<class T>
constexpr bool kIsModulo<Modulo<T>> = true;

// Mask
template<class T>
using Mask = std::variant<Range<T>, Modulo<T>, T>;
// :: Contains
template<class T>
bool Contains(const Mask<T>& mask, T value) {
  if (std::holds_alternative<Range<T>>(mask)) {
    return std::get<Range<T>>(mask).Contains(value);
  }
  else if (std::holds_alternative<Range<T>>(mask)) {
    return std::get<Range<T>>(mask).Contains(value);
  }
  else {
    return std::get<T>(mask) == value;
  }
  return false;
}
// :: all
template<class T>
Mask<T> All() {
  return { { Range<T>({ T(0), T(kMaxValue<T>) }) } };
}

// Hourly
class Hourly {};

// Schedule
class Schedule {
public:
  Schedule() = default;

  template<class T>
  bool IsRestricted() const {
    for (const auto& mask : day_masks) {
      if (std::holds_alternative<Mask<T>>(mask)) {
        return true;
      }
    }
    return false;
  }

  template<class T>
  bool TriggersOn(T value) const {
    if (!IsRestricted<T>()) {
      return true;
    }
    for (const auto& mask : day_masks) {
      if (std::holds_alternative<Mask<T>>(mask)) {
        if (Contains(std::get<Mask<T>>(mask), value)) {
          return true;
        }
      }
    }
    return false;
  }

  bool TriggersOnDay(DateTime date_time) const;
  std::vector<time_t> GetMatchingTimes(time_t from, time_t to) const;

  std::vector<std::variant<
    Mask<MonthOfYear>,
    Mask<WeekOfYear>,
    Mask<WeekOfMonth>,
    Mask<DayOfMonth>,
    Mask<DayOfWeek>
  >> day_masks;
  std::variant<std::vector<TimeOfDay>, Hourly> times = Hourly();
};
 
}  // namespace directordaemon

#endif  // BAREOS_DIRD_SCHEDULE_H_
