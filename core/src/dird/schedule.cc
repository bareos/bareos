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

Schedule::Schedule(const std::variant<std::vector<TimeOfDay>, Hourly>& _times)
    : times(_times)
{
}

bool Schedule::TriggersOnDay(DateTime date_time) const
{
  return TriggersOn(MonthOfYear{date_time.month})
         && TriggersOn(WeekOfYear(date_time.week_of_year))
         && (TriggersOn(WeekOfMonth{date_time.week_of_month})
             || (date_time.OnLast7DaysOfMonth()
                 && TriggersOn(*WeekOfMonth::FromName("last"))))
         && TriggersOn(DayOfMonth(date_time.day_of_month))
         && TriggersOn(DayOfWeek{date_time.day_of_week});
}
std::vector<time_t> Schedule::GetMatchingTimes(time_t from, time_t to) const
{
  std::vector<time_t> list;
  for (time_t day = from; day < to + kSecondsPerDay; day += kSecondsPerDay) {
    DateTime date_time(day);
    if (TriggersOnDay(date_time)) {
      if (auto* hourly = std::get_if<Hourly>(&times)) {
        for (int hour = 0; hour < 24; ++hour) {
          date_time.hour = hour;
          for (int minute : hourly->minutes) {
            date_time.minute = minute;
            time_t runtime = date_time.GetTime();
            if (from <= runtime && runtime <= to) { list.push_back(runtime); }
          }
        }
      } else {
        for (const auto& time : std::get<std::vector<TimeOfDay>>(times)) {
          date_time.hour = time.hour;
          date_time.minute = time.minute;
          time_t runtime = date_time.GetTime();
          if (from <= runtime && runtime <= to) { list.push_back(runtime); }
        }
      }
    }
  }
  return list;
}

}  // namespace directordaemon
