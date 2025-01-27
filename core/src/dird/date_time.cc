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

#include "include/bareos.h"
#include "dird/date_time_mask.h"
#include "dird/date_time.h"

#include <array>
#include <iostream>

namespace directordaemon {

static constexpr int kMonthsPerYear{12};
static constexpr int kDaysPerWeek{7};

static bool IsLeapYear(int year) {
   if (year % 400 == 0) { return true; }
   if (year % 100 == 0) { return false; }
   if (year % 4 == 0) { return true; }
   return false;
}
static int LastDayOfMonth(int year, int month)
{
   if (IsLeapYear(year)) {
      static constexpr std::array<int, kMonthsPerYear> kLastDaysInMonth = {
         30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
      };
      return kLastDaysInMonth.at(month);
   }
   else {
      static constexpr std::array<int, kMonthsPerYear> kLastDaysInMonth = {
         30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364
      };
      return kLastDaysInMonth.at(month);
   }
}

bool DateTime::WeekOfMonth() const {
   return day_of_month / 7;
}
bool DateTime::OnLast7DaysOfMonth() const {
   auto last_day = LastDayOfMonth(year, month);
   ASSERT(last_day >= day_of_year);
   return last_day - kDaysPerWeek < day_of_year; 
}

DateTime::DateTime(time_t time_)
   : time(time_) {
   struct tm tm = {};
   Blocaltime(&time, &tm);
   day_of_month = tm.tm_mday - 1;
   day_of_week = tm.tm_wday;
   week_of_year = TmWoy(time);
   month = tm.tm_mon;
   day_of_year = tm.tm_yday;
   hour = tm.tm_hour;
}

void DateTime::PrintDebugMessage(int debug_level) const {
   Dmsg8(debug_level, "now = %x: h=%d m=%d md=%d wd=%d woy=%d yday=%d\n ",
   time, hour, month, day_of_month, day_of_week, week_of_year, day_of_year);
}

}  // namespace directordaemon
