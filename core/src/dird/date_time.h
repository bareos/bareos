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

namespace directordaemon {

struct DateTime {
   DateTime(time_t time);

   bool WeekOfMonth() const;
   bool OnLast7DaysOfMonth() const;
   void PrintDebugMessage(int debug_level) const;

   int year{0};
   int month{0};
   int week_of_year{0};
   int day_of_year{0};
   int day_of_month{0};
   int day_of_week{0};
   int hour{0};
   time_t time;
};

}  // namespace directordaemon

#endif  // BAREOS_DIRD_DATE_TIME_H_
