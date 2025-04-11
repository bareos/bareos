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

// List
template<class T>
struct List {
  using Type = T;
  
  std::vector<T> items;
};
// :: kIsList
template<class T>
static constexpr bool kIsList = false;
template<class T>
static constexpr bool kIsList<List<T>> = true;

// Mask
template<class T>
using Mask = List<Range<T>>;
// :: Contains
template<class T>
bool Contains(const Mask<T>& mask, T value) {
  for (const auto& item : mask.items) {
    if (item.Contains(value)) {
      return true;
    }
  }
  return false;
}
// :: all
template<class T>
Mask<T> All() {
  return { { { T(0), T(kMaxValue<T>) } } };
}

// Schedule
class Schedule {
public:
  Schedule();
  Schedule(Mask<MonthOfYear> month_of_year, Mask<WeekOfMonth> week_of_month, Mask<DayOfWeek> day_of_week, List<TimeOfDay> time);
  Schedule(Mask<MonthOfYear> month_of_year, Mask<DayOfWeek> day_of_week, List<TimeOfDay> time);
  Schedule(Mask<MonthOfYear> month_of_year, Mask<DayOfMonth> day_of_month, List<TimeOfDay> time);
  Schedule(Mask<WeekOfYear> week_of_year, Mask<DayOfWeek> day_of_week, List<TimeOfDay> time);
  Schedule(Mask<WeekOfMonth> week_of_month, Mask<DayOfWeek> day_of_week, List<TimeOfDay> time);
  Schedule(Mask<DayOfMonth> day_of_month, List<TimeOfDay> time);
  Schedule(Mask<DayOfWeek> day_of_week, List<TimeOfDay> time);
  Schedule(List<TimeOfDay> time);

  bool TriggersOnDay(DateTime date_time) const;
  std::vector<time_t> GetMatchingTimes(time_t from, time_t to) const;

  std::variant<
    std::tuple<Mask<MonthOfYear>, Mask<WeekOfMonth>, Mask<DayOfWeek>>,
    std::tuple<Mask<MonthOfYear>, Mask<DayOfMonth>>,
    std::tuple<Mask<WeekOfYear>, Mask<DayOfWeek>>
  > day_mask;
  List<TimeOfDay> times;
};
 
}  // namespace directordaemon

#endif  // BAREOS_DIRD_SCHEDULE_H_
