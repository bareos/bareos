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

#ifndef BAREOS_DIRD_DATE_TIME_H_
#define BAREOS_DIRD_DATE_TIME_H_

#include "include/baconfig.h"

#include <array>
#include <string>
#include <ctime>
#include <stdexcept>
#include <chrono>

namespace directordaemon {

constexpr auto kSecondsPerMinute = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::minutes(1)).count();
constexpr auto kSecondsPerHour = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours(1)).count();
constexpr auto kSecondsPerDay = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::hours(24)).count();

struct MonthOfYear {
  static constexpr std::array<std::string_view, 12> kNames = {
      "January", "February", "March",     "April",   "Mai",      "June",
      "Juli",    "August",   "September", "October", "November", "December",
  };

  MonthOfYear(int index)
  {
    ASSERT(0 <= index && static_cast<size_t>(index) < kNames.size());
    name = kNames.at(index);
  }
  static std::optional<MonthOfYear> FromName(std::string_view name)
  {
    for (std::string_view other_name : kNames) {
      if (name.length() == other_name.length() || name.length() == 3) {
        if (bstrncasecmp(name.data(), other_name.data(), name.length())) {
          return MonthOfYear{other_name};
        }
      }
    }
    return std::nullopt;
  }

  size_t Index() const
  {
    for (size_t i = 0; i < kNames.size(); ++i) {
      if (kNames.at(i).data() == name.data()) { return i; }
    }
    throw std::logic_error{"Illegal MonthOfYear instance."};
  }
  operator int() const { return Index(); }

  std::string_view name;

 private:
  MonthOfYear(std::string_view _name) : name(_name) {}
};
class WeekOfYear {
 public:
  WeekOfYear() = default;
  WeekOfYear(int value) : _value(value) { ASSERT(0 <= value && value <= 53); }

  operator int() const { return _value; }

 private:
  int _value;
};
struct WeekOfMonth {
  static constexpr std::array<std::string_view, 6> kNames = {
      "first", "second", "third", "fourth", "last", "fifth",
  };
  static constexpr std::array<std::string_view, 6> kAlternativeNames = {
      "1st", "2nd", "3rd", "4th", "last", "5th",
  };
  static_assert(kNames.size() == kAlternativeNames.size());

  WeekOfMonth(int index)
  {
    ASSERT(0 <= index && static_cast<size_t>(index) < kNames.size());
    name = kNames.at(index);
  }
  static std::optional<WeekOfMonth> FromName(std::string_view name)
  {
    for (size_t i = 0; i < kNames.size(); ++i) {
      if (name.length() == kNames.at(i).length()) {
        if (bstrncasecmp(name.data(), kNames.at(i).data(), name.length())) {
          return WeekOfMonth{kNames.at(i)};
        }
      }
      if (name.length() == kAlternativeNames.at(i).length()) {
        if (bstrncasecmp(name.data(), kAlternativeNames.at(i).data(),
                         name.length())) {
          return WeekOfMonth{kNames.at(i)};
        }
      }
    }
    return std::nullopt;
  }

  size_t Index() const
  {
    for (size_t i = 0; i < kNames.size(); ++i) {
      if (kNames.at(i).data() == name.data()) { return i; }
    }
    throw std::logic_error{"Illegal WeekOfMonth instance."};
  }
  operator int() const { return Index(); }

  std::string_view name;

 private:
  WeekOfMonth(std::string_view _name) : name(_name) {}
};

class DayOfMonth {
 public:
  DayOfMonth() = default;
  DayOfMonth(int value) : _value(value) { ASSERT(0 <= value && value <= 30); }

  operator int() const { return _value; }

 private:
  int _value;
};
struct DayOfWeek {
  static constexpr std::array<std::string_view, 7> kNames = {
      "Sunday",   "Monday", "Tuesday",  "Wednesday",
      "Thursday", "Friday", "Saturday",
  };

  DayOfWeek(int index)
  {
    ASSERT(0 <= index && static_cast<size_t>(index) < kNames.size());
    name = kNames.at(index);
  }
  static std::optional<DayOfWeek> FromName(std::string_view name)
  {
    for (std::string_view other_name : kNames) {
      if (name.length() == other_name.length() || name.length() == 3) {
        if (bstrncasecmp(name.data(), other_name.data(), name.length())) {
          return DayOfWeek{other_name};
        }
      }
    }
    return std::nullopt;
  }

  size_t Index() const
  {
    for (size_t i = 0; i < kNames.size(); ++i) {
      if (kNames.at(i).data() == name.data()) { return i; }
    }
    throw std::logic_error{"Illegal DayOfWeek instance."};
  }
  operator int() const { return Index(); }

  std::string_view name;

 private:
  DayOfWeek(std::string_view _name) : name(_name) {}
};

struct TimeOfDay {
  TimeOfDay(int h, int min) : hour(h), minute(min) {}
  TimeOfDay(int h, int min, int sec) : hour(h), minute(min), second(sec) {}

  int hour, minute;
  int second = 0;
};

struct DateTime {
  DateTime(time_t time);

  bool OnLast7DaysOfMonth() const;
  void PrintDebugMessage(int debug_level) const;
  time_t GetTime() const;

  int year{0};
  int month{0};
  int week_of_year{0};
  int week_of_month{0};
  int day_of_year{0};
  int day_of_month{0};
  int day_of_week{0};
  int hour{0};
  int minute{0};
  int second{0};

  friend std::ostream& operator<<(std::ostream& stream,
                                  const DateTime& date_time);

 private:
  tm original_time_;
};

}  // namespace directordaemon

#endif  // BAREOS_DIRD_DATE_TIME_H_
