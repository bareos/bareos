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

#include "schedule.h"

namespace directordaemon {

// (constructor)
Schedule::Schedule()
  : day_mask(std::make_tuple(All<WeekOfYear>(), All<DayOfWeek>())), times(List<TimeOfDay>({ { TimeOfDay(0, 0) } })) {}
Schedule::Schedule(Mask<MonthOfYear> month_of_year, Mask<WeekOfMonth> week_of_month, Mask<DayOfWeek> day_of_week, std::variant<List<TimeOfDay>, Hourly> time)
  : day_mask(std::make_tuple(month_of_year, week_of_month, day_of_week)), times(time) {}
Schedule::Schedule(Mask<MonthOfYear> month_of_year, Mask<DayOfWeek> day_of_week, std::variant<List<TimeOfDay>, Hourly> time)
  : day_mask(std::make_tuple(month_of_year, All<WeekOfMonth>(), day_of_week)), times(time) {}
Schedule::Schedule(Mask<MonthOfYear> month_of_year, Mask<DayOfMonth> day_of_month, std::variant<List<TimeOfDay>, Hourly> time)
  : day_mask(std::make_tuple(month_of_year, day_of_month)), times(time) {}
Schedule::Schedule(Mask<WeekOfYear> week_of_year, Mask<DayOfWeek> day_of_week, std::variant<List<TimeOfDay>, Hourly> time)
  : day_mask(std::make_tuple(week_of_year, day_of_week)), times(time) {}
Schedule::Schedule(Mask<WeekOfMonth> week_of_month, Mask<DayOfWeek> day_of_week, std::variant<List<TimeOfDay>, Hourly> time)
  : day_mask(std::make_tuple(All<MonthOfYear>(), week_of_month, day_of_week)), times(time) {}
Schedule::Schedule(Mask<DayOfMonth> day_of_month, std::variant<List<TimeOfDay>, Hourly> time)
  : day_mask(std::make_tuple(All<MonthOfYear>(), day_of_month)), times(time) {}
Schedule::Schedule(Mask<DayOfWeek> day_of_week, std::variant<List<TimeOfDay>, Hourly> time)
  : day_mask(std::make_tuple(All<WeekOfYear>(), day_of_week)), times(time) {}
Schedule::Schedule(std::variant<List<TimeOfDay>, Hourly> time)
  : day_mask(std::make_tuple(All<WeekOfYear>(), All<DayOfWeek>())), times(time) {}

// TriggersOnDay
bool Schedule::TriggersOnDay(DateTime date_time) const {
  if (day_mask.index() == 0) {
    const auto& value = std::get<0>(day_mask);
    bool matches_month = Contains(std::get<0>(value), MonthOfYear(date_time.month));
    bool matches_week = Contains(std::get<1>(value), WeekOfMonth(date_time.week_of_month));
    matches_week |= Contains(std::get<1>(value), WeekOfMonth::kLast) && date_time.OnLast7DaysOfMonth();
    bool matches_day = Contains(std::get<2>(value), DayOfWeek(date_time.day_of_week));
    return matches_month && matches_week && matches_day;
  }
  else if (day_mask.index() == 1) {
    const auto& value = std::get<1>(day_mask);
    bool matches_month = Contains(std::get<0>(value), MonthOfYear(date_time.month));
    bool matches_day = Contains(std::get<1>(value), DayOfMonth(date_time.day_of_month));
    return matches_month && matches_day;
  }
  else if (day_mask.index() == 2) {
    const auto& value = std::get<2>(day_mask);
    bool matches_week = Contains(std::get<0>(value), WeekOfYear(date_time.week_of_year));
    bool matches_day = Contains(std::get<1>(value), DayOfWeek(date_time.day_of_week));
    return matches_week && matches_day;
  }
  return false;
}
// GetMatchingTimes
std::vector<time_t> Schedule::GetMatchingTimes(time_t from, time_t to) const {
  std::vector<time_t> list;
  for (time_t day = from; day < to + 60 * 60 * 24; day += 60 * 60 * 24) {
    DateTime date_time(day);
    if (TriggersOnDay(date_time)) {
      if (std::holds_alternative<Hourly>(times)) {
        for (int hour = 0; hour < 24; ++hour) {
          date_time.hour = hour;
          date_time.minute = 0;
          time_t runtime = date_time.GetTime();
          if (from <= runtime && runtime <= to) {
            list.push_back(runtime);
          }
        }
      }
      else {
        for (const auto& time : std::get<List<TimeOfDay>>(times).items) {
          date_time.hour = time.hour;
          date_time.minute = time.minute;
          time_t runtime = date_time.GetTime();
          if (from <= runtime && runtime <= to) {
            list.push_back(runtime);
          }
        }
      }
    }
  }
  return list;
}
   
} // namespace directordaemon