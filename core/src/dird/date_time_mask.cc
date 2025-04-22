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
#include "dird/date_time.h"

#include <array>
#include <iostream>

namespace directordaemon {

bool DateTimeMask::TriggersOnDay(time_t time) const
{
  DateTime date_time(time);
  return BitIsSet(date_time.day_of_month, mday)
         && BitIsSet(date_time.day_of_week, wday)
         && BitIsSet(date_time.month, month)
         && (BitIsSet(date_time.week_of_month, wom)
             || (date_time.OnLast7DaysOfMonth() && last_7days_of_month))
         && BitIsSet(date_time.week_of_year, woy);
}
bool DateTimeMask::TriggersOnDayAndHour(time_t time) const
{
  DateTime date_time(time);
  return BitIsSet(date_time.day_of_month, mday)
         && BitIsSet(date_time.day_of_week, wday)
         && BitIsSet(date_time.month, month)
         && (BitIsSet(date_time.week_of_month, wom)
             || (date_time.OnLast7DaysOfMonth() && last_7days_of_month))
         && BitIsSet(date_time.week_of_year, woy)
         && BitIsSet(date_time.hour, hour);
}

}  // namespace directordaemon
