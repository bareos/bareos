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

template <class T> struct Interval {
  T first, last;
};
template <class T> struct Modulo {
  int remainder, divisor;
};
template <class T> using Mask = std::variant<Interval<T>, Modulo<T>, T>;

template <class T> bool Contains(const Interval<T>& interval, T value)
{
  if (int(interval.first) <= int(interval.last)) {
    return int(interval.first) <= int(value)
           && int(value) <= int(interval.last);
  } else {
    return int(interval.first) <= int(value)
           || int(value) <= int(interval.last);
  }
}
template <class T> bool Contains(const Modulo<T>& modulo, T value)
{
  return (int(value) % int(modulo.divisor)) == int(modulo.remainder);
}
template <class T> bool Contains(const T& a, T b) { return int(a) == int(b); }
template <class T> bool Contains(const Mask<T>& mask, T value)
{
  return std::visit([&value](const auto& x) { return Contains(x, value); },
                    mask);
}

struct Hourly {
  std::set<int> minutes;
};

class Schedule {
 public:
  Schedule() = default;
  Schedule(const std::variant<std::vector<TimeOfDay>, Hourly>& times);

  bool TriggersOnDay(DateTime date_time) const;
  std::vector<time_t> GetMatchingTimes(time_t from, time_t to) const;

  std::tuple<std::vector<Mask<MonthOfYear>>,
             std::vector<Mask<WeekOfYear>>,
             std::vector<Mask<WeekOfMonth>>,
             std::vector<Mask<DayOfMonth>>,
             std::vector<Mask<DayOfWeek>>>
      day_masks;
  std::variant<std::vector<TimeOfDay>, Hourly> times = std::vector<TimeOfDay>{};

 private:
  template <class T> bool IsRestricted() const
  {
    return !std::get<std::vector<Mask<T>>>(day_masks).empty();
  }

  template <class T> bool TriggersOn(T value) const
  {
    if (!IsRestricted<T>()) { return true; }
    for (const auto& mask : std::get<std::vector<Mask<T>>>(day_masks)) {
      if (Contains(mask, value)) { return true; }
    }
    return false;
  }
};

template <class T> struct Parser;
// expose for unit testing
template <> struct Parser<Schedule> {
  struct Error {
    std::string message;
  };
  struct Warnings {
    std::vector<std::string> messages;
  };
  static std::pair<std::variant<Schedule, Error>, Warnings> Parse(
      const std::vector<std::string>& tokens);
  static std::pair<std::variant<Schedule, Error>, Warnings> Parse(
      std::string_view str);
};
std::string ToString(const Schedule& schedule);  // defined in dird/dird_conf.cc

}  // namespace directordaemon

#endif  // BAREOS_DIRD_SCHEDULE_H_
