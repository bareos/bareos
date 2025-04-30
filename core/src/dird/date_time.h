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

#include <array>
#include <string_view>
#include <ctime>

namespace directordaemon {

// MonthOfYear
enum class MonthOfYear
{
  kJan,
  kFeb,
  kMar,
  kApr,
  kMai,
  kJun,
  kJul,
  kAug,
  kSep,
  kOct,
  kNov,
  kDec,
};
constexpr std::array<std::string_view, 12> kMonthOfYearLiterals = {
    "January", "February", "March", "April", "Mai", "June",
    "Juli", "August", "September", "October", "November", "December",
};
// WeekOfYear
enum class WeekOfYear
{
  kW00,
  kW01,
  kW02,
  kW03,
  kW04,
  kW05,
  kW06,
  kW07,
  kW08,
  kW09,
  kW10,
  kW11,
  kW12,
  kW13,
  kW14,
  kW15,
  kW16,
  kW17,
  kW18,
  kW19,
  kW20,
  kW21,
  kW22,
  kW23,
  kW24,
  kW25,
  kW26,
  kW27,
  kW28,
  kW29,
  kW30,
  kW31,
  kW32,
  kW33,
  kW34,
  kW35,
  kW36,
  kW37,
  kW38,
  kW39,
  kW40,
  kW41,
  kW42,
  kW43,
  kW44,
  kW45,
  kW46,
  kW47,
  kW48,
  kW49,
  kW50,
  kW51,
  kW52,
  kW53,
};
// WeekOfMonth
enum class WeekOfMonth
{
  kFirst,
  kSecond,
  kThird,
  kFourth,
  kFifth,
  kLast,
};
constexpr std::array<std::string_view, 6> kWeekOfMonthLiterals
    = {"first", "second", "third", "fourth", "fifth", "last"};
// DayOfMonth
enum class DayOfMonth
{
  k0,
  k1,
  k2,
  k3,
  k4,
  k5,
  k6,
  k7,
  k8,
  k9,
  k10,
  k11,
  k12,
  k13,
  k14,
  k15,
  k16,
  k17,
  k18,
  k19,
  k20,
  k21,
  k22,
  k23,
  k24,
  k25,
  k26,
  k27,
  k28,
  k29,
  k30,
};
// DayOfWeek
enum class DayOfWeek
{
  kSun,
  kMon,
  kTue,
  kWed,
  kThu,
  kFri,
  kSat,
};
constexpr std::array<std::string_view, 7> kDayOfWeekLiterals = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};

// TimeOfDay
struct TimeOfDay {
  TimeOfDay(int h, int min) : hour(h), minute(min) {}
  TimeOfDay(int h, int min, int sec) : hour(h), minute(min), second(sec) {}

  int hour, minute;
  int second = 0;
};

// DateTime
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

 private:
  int dst_ = 0;                 // daylight saving time
  int gmt_offset_ = 0;          // seconds east of UTC
  std::string_view time_zone_;  // abbreviated
};

}  // namespace directordaemon

#endif  // BAREOS_DIRD_DATE_TIME_H_
