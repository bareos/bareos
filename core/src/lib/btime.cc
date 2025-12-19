/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2025 Bareos GmbH & Co. KG

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
/*
 * BAREOS floating point time and date routines -- John Walker
 *
 * Later double precision integer time/date routines -- Kern Sibbald
 *
 */

/* Concerning times. There are a number of differnt time standards
 * in BAREOS (fdate_t, ftime_t, time_t (Unix standard), btime_t, and
 * utime_t).  fdate_t and ftime_t are deprecated and should no longer
 * be used, and in general, Unix time time_t should no longer be used,
 * it is being phased out.
 *
 * Epoch is the base of Unix time in seconds (time_t, ...)
 *    and is 1 Jan 1970 at 0:0 UTC
 *
 * The major two times that should be left are:
 *    btime_t  (64 bit integer in microseconds base Epoch)
 *    utime_t  (64 bit integer in seconds base Epoch)
 */

#include "include/bareos.h"
#include "lib/btime.h"
#include <math.h>

#if defined(HAVE_MSVC)
void Blocaltime(const time_t* timep, struct tm* tm)
{
  auto error = localtime_s(tm, timep);
  if (error) { errno = error; }
}
#else
void Blocaltime(const time_t* time, struct tm* tm)
{
  (void)localtime_r(time, tm);
}
#endif /* HAVE_MSVC */

// Formatted time for user display: dd-Mon-yyyy hh:mm
char* bstrftime(char* dt, int maxlen, utime_t utime, const char* fmt)
{
  time_t time = (time_t)utime;
  struct tm tm;

  Blocaltime(&time, &tm);
  if (fmt) {
    strftime(dt, maxlen, fmt, &tm);
  } else {
    strftime(dt, maxlen, "%d-%b-%Y %H:%M", &tm);
  }

  return dt;
}

// Formatted time for user display: dd-Mon-yyyy hh:mm:ss
char* bstrftimes(char* dt, int maxlen, utime_t utime)
{
  return bstrftime(dt, maxlen, utime, "%d-%b-%Y %H:%M:%S");
}

// Formatted time for user display with weekday: weekday dd-Mon hh:mm
char* bstrftime_wd(char* dt, int maxlen, utime_t utime)
{
  return bstrftime(dt, maxlen, utime, "%a %d-%b-%Y %H:%M");
}

// Formatted time for user display: dd-Mon-yy hh:mm (no century)
char* bstrftime_nc(char* dt, int maxlen, utime_t utime)
{
  char *p, *q;

  // NOTE! since the compiler complains about %y, I use %Y and cut the century
  bstrftime(dt, maxlen, utime, "%d-%b-%Y %H:%M");

  // Overlay the century
  p = dt + 7;
  q = dt + 9;
  while (*q) { *p++ = *q++; }
  *p = 0;
  return dt;
}

// Unix time to standard time string yyyy-mm-dd hh:mm:ss
char* bstrutime(char* dt, int maxlen, utime_t utime)
{
  return bstrftime(dt, maxlen, utime, "%Y-%m-%d %H:%M:%S");
}

static bool is_leap(const tm& datetime)
{
  const int year = datetime.tm_year + 1900;
  return year % 4 == 0 && !(year % 100 == 0 && year % 400 != 0);
}

// Check if a given date is valid

static bool DateIsValid(const tm& datetime)
{
  if (datetime.tm_year <= 0
      || (datetime.tm_mon < month::january || datetime.tm_mon > month::december)
      || datetime.tm_mday <= 0
      || (datetime.tm_hour < 0 || datetime.tm_hour > 23)
      || (datetime.tm_min < 0 || datetime.tm_min > 59)
      || (datetime.tm_sec < 0 || datetime.tm_sec > 59)) {
    return false;
  }

  switch (datetime.tm_mon) {
    case month::february:
      if (is_leap(datetime)) {
        if (datetime.tm_mday > 29) { return false; }
      } else {
        if (datetime.tm_mday > 28) { return false; }
      }
      break;
    case month::april:
    case month::june:
    case month::september:
    case month::november:
      if (datetime.tm_mday > 30) { return false; }
      break;
    default:
      if (datetime.tm_mday > 31) { return false; }
      break;
  }
  return true;
}

// Convert standard time string yyyy-mm-dd hh:mm:ss to Unix time
utime_t StrToUtime(const char* str)
{
  tm datetime{};
  time_t time;

  char trailinggarbage[16]{""};

  // Check for bad argument
  if (!str || *str == 0) { return 0; }

  if ((sscanf(str, "%u-%u-%u %u:%u:%u%15s", &datetime.tm_year, &datetime.tm_mon,
              &datetime.tm_mday, &datetime.tm_hour, &datetime.tm_min,
              &datetime.tm_sec, trailinggarbage)
       != 7)
      || trailinggarbage[0] != '\0') {
    return 0;
  }

  // range for tm_mon is defined as 0-11
  --datetime.tm_mon;
  // tm_year is years since 1900
  datetime.tm_year -= 1900;

  // we don't know these, so we initialize to sane defaults
  datetime.tm_wday = datetime.tm_yday = 0;

  // set to -1 for "I don't know"
  datetime.tm_isdst = -1;

  if (!DateIsValid(datetime)) { return 0; }

  time = mktime(&datetime);
  if (time == -1) { time = 0; }
  return (utime_t)time;
}


/*
 * BAREOS's time (btime_t) is an unsigned 64 bit integer that contains
 * the number of microseconds since Epoch Time (1 Jan 1970) UTC.
 */

btime_t GetCurrentBtime()
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    tv.tv_sec = (long)time(NULL); /* fall back to old method */
    tv.tv_usec = 0;
  }
  return ((btime_t)tv.tv_sec) * 1000000 + (btime_t)tv.tv_usec;
}

// Convert btime to Unix time
time_t BtimeToUnix(btime_t bt) { return (time_t)(bt / 1000000); }

// Convert btime to utime
utime_t BtimeToUtime(btime_t bt) { return (utime_t)(bt / 1000000); }


/*
 * Given a Unix date return the week of the year.
 * The returned value can be 0-53.  Officially
 * the weeks are numbered from 1 to 53 where week1
 * is the week in which the first Thursday of the
 * year occurs (alternatively, the week which contains
 * the 4th of January).  We return 0, if the week of the
 * year does not fall in the current year.
 */
int TmWoy(time_t stime)
{
  int woy, fty, tm_yday;
  time_t time4;
  struct tm tm;

  memset(&tm, 0, sizeof(struct tm));
  Blocaltime(&stime, &tm);
  tm_yday = tm.tm_yday;
  tm.tm_mon = 0;
  tm.tm_mday = 4;
  tm.tm_isdst = 0; /* 4 Jan is not DST */
  time4 = mktime(&tm);
  Blocaltime(&time4, &tm);
  fty = 1 - tm.tm_wday;
  if (fty <= 0) { fty += 7; }
  woy = tm_yday - fty + 4;
  if (woy < 0) { return 0; }
  return 1 + woy / 7;
}
