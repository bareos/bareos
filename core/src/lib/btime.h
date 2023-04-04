/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_BTIME_H_
#define BAREOS_LIB_BTIME_H_

/* New btime definition -- use this */
btime_t GetCurrentBtime(void);
time_t BtimeToUnix(btime_t bt);   /* bareos time to epoch time */
utime_t BtimeToUtime(btime_t bt); /* bareos time to utime_t */

int TmWoy(time_t stime);

void Blocaltime(const time_t* time, struct tm* tm);
char* bstrutime(char* dt, int maxlen, utime_t tim);
char* bstrftime(char* dt, int maxlen, utime_t tim, const char* fmt = NULL);
char* bstrftimes(char* dt, int maxlen, utime_t tim);
char* bstrftime_nc(char* dt, int maxlen, utime_t tim);
char* bstrftime_wd(char* dt, int maxlen, utime_t tim);
utime_t StrToUtime(const char* str);

struct month {
  enum : int
  {
    january,
    february,
    march,
    april,
    may,
    june,
    july,
    august,
    september,
    october,
    november,
    december
  };
};

#endif  // BAREOS_LIB_BTIME_H_
