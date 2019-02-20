/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2015 Bareos GmbH & Co. KG

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
#include <math.h>

void Blocaltime(const time_t* time, struct tm* tm)
{
  /* ***FIXME**** localtime_r() should be user configurable */
  (void)localtime_r(time, tm);
}

/*
 * Formatted time for user display: dd-Mon-yyyy hh:mm
 */
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

/*
 * Formatted time for user display: dd-Mon-yyyy hh:mm:ss
 */
char* bstrftimes(char* dt, int maxlen, utime_t utime)
{
  return bstrftime(dt, maxlen, utime, "%d-%b-%Y %H:%M:%S");
}

/*
 * Formatted time for user display: dd-Mon hh:mm
 */
char* bstrftime_ny(char* dt, int maxlen, utime_t utime)
{
  return bstrftime(dt, maxlen, utime, "%d-%b %H:%M");
}

/*
 * Formatted time for user display with weekday: weekday dd-Mon hh:mm
 */
char* bstrftime_wd(char* dt, int maxlen, utime_t utime)
{
  return bstrftime(dt, maxlen, utime, "%a %d-%b-%Y %H:%M");
}

/*
 * Formatted time for user display: dd-Mon-yy hh:mm (no century)
 */
char* bstrftime_nc(char* dt, int maxlen, utime_t utime)
{
  char *p, *q;

  /*
   * NOTE! since the compiler complains about %y, I use %Y and cut the century
   */
  bstrftime(dt, maxlen, utime, "%d-%b-%Y %H:%M");

  /*
   * Overlay the century
   */
  p = dt + 7;
  q = dt + 9;
  while (*q) { *p++ = *q++; }
  *p = 0;
  return dt;
}

/*
 * Unix time to standard time string yyyy-mm-dd hh:mm:ss
 */
char* bstrutime(char* dt, int maxlen, utime_t utime)
{
  return bstrftime(dt, maxlen, utime, "%Y-%m-%d %H:%M:%S");
}

/*
 * Convert standard time string yyyy-mm-dd hh:mm:ss to Unix time
 */
utime_t StrToUtime(const char* str)
{
  struct tm tm;
  time_t time;

  /*
   * Check for bad argument
   */
  if (!str || *str == 0) { return 0; }

  if (sscanf(str, "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
             &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6) {
    return 0;
  }
  if (tm.tm_mon > 0) {
    tm.tm_mon--;
  } else {
    return 0;
  }
  if (tm.tm_year >= 1900) {
    tm.tm_year -= 1900;
  } else {
    return 0;
  }
  tm.tm_wday = tm.tm_yday = 0;
  tm.tm_isdst = -1;
  time = mktime(&tm);
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

/*
 * Convert btime to Unix time
 */
time_t BtimeToUnix(btime_t bt) { return (time_t)(bt / 1000000); }

/*
 * Convert btime to utime
 */
utime_t BtimeToUtime(btime_t bt) { return (utime_t)(bt / 1000000); }

/*
 * Return the week of the month, base 0 (wom)
 * given tm_mday and tm_wday. Value returned
 * can be from 0 to 4 => week1, ... week5
 */
int tm_wom(int mday, int wday)
{
  int fs; /* first sunday */
  int wom;

  fs = (mday % 7) - wday;
  if (fs <= 0) { fs += 7; }
  if (mday <= fs) { return 0; }
  wom = 1 + (mday - fs - 1) / 7;

  return wom;
}

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

/*
 * Deprecated. Do not use.
 */
void get_current_time(struct date_time* dt)
{
  struct tm tm;
  time_t now;

  now = time(NULL);
  (void)gmtime_r(&now, &tm);
  Dmsg6(200, "m=%d d=%d y=%d h=%d m=%d s=%d\n", tm.tm_mon + 1, tm.tm_mday,
        tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
  TmEncode(dt, &tm);
  Dmsg2(200, "jday=%f jmin=%f\n", dt->julian_day_number,
        dt->julian_day_fraction);
  TmDecode(dt, &tm);
  Dmsg6(200, "m=%d d=%d y=%d h=%d m=%d s=%d\n", tm.tm_mon + 1, tm.tm_mday,
        tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
}


/*  DateEncode  --  Encode civil date as a Julian day number.  */

/* Deprecated. Do not use. */
fdate_t DateEncode(uint32_t year, uint8_t month, uint8_t day)
{
  /* Algorithm as given in Meeus, Astronomical Algorithms, Chapter 7, page 61 */

  int32_t a, b, m;
  uint32_t y;

  ASSERT(month < 13);
  ASSERT(day > 0 && day < 32);

  m = month;
  y = year;

  if (m <= 2) {
    y--;
    m += 12;
  }

  /* Determine whether date is in Julian or Gregorian calendar based on
     canonical date of calendar reform. */

  if ((year < 1582) ||
      ((year == 1582) && ((month < 9) || (month == 9 && day < 5)))) {
    b = 0;
  } else {
    a = ((int)(y / 100));
    b = 2 - a + (a / 4);
  }

  return (((int32_t)(365.25 * (y + 4716))) + ((int)(30.6001 * (m + 1))) + day +
          b - 1524.5);
}

/*  TimeEncode  --  Encode time from hours, minutes, and seconds
                     into a fraction of a day.  */

/* Deprecated. Do not use. */
ftime_t TimeEncode(uint8_t hour,
                   uint8_t minute,
                   uint8_t second,
                   float32_t second_fraction)
{
  ASSERT((second_fraction >= 0.0) || (second_fraction < 1.0));
  return (ftime_t)(((second + 60L * (minute + 60L * hour)) / 86400.0)) +
         second_fraction;
}

/*  date_time_encode  --  Set day number and fraction from date
                          and time.  */

/* Deprecated. Do not use. */
void date_time_encode(struct date_time* dt,
                      uint32_t year,
                      uint8_t month,
                      uint8_t day,
                      uint8_t hour,
                      uint8_t minute,
                      uint8_t second,
                      float32_t second_fraction)
{
  dt->julian_day_number = DateEncode(year, month, day);
  dt->julian_day_fraction = TimeEncode(hour, minute, second, second_fraction);
}

/*  DateDecode  --  Decode a Julian day number into civil date.  */

/* Deprecated. Do not use. */
void DateDecode(fdate_t date, uint32_t* year, uint8_t* month, uint8_t* day)
{
  fdate_t z, f, a, alpha, b, c, d, e;

  date += 0.5;
  z = floor(date);
  f = date - z;

  if (z < 2299161.0) {
    a = z;
  } else {
    alpha = floor((z - 1867216.25) / 36524.25);
    a = z + 1 + alpha - floor(alpha / 4);
  }

  b = a + 1524;
  c = floor((b - 122.1) / 365.25);
  d = floor(365.25 * c);
  e = floor((b - d) / 30.6001);

  *day = (uint8_t)(b - d - floor(30.6001 * e) + f);
  *month = (uint8_t)((e < 14) ? (e - 1) : (e - 13));
  *year = (uint32_t)((*month > 2) ? (c - 4716) : (c - 4715));
}

/*  TimeDecode  --  Decode a day fraction into civil time.  */

/* Deprecated. Do not use. */
void TimeDecode(ftime_t time,
                uint8_t* hour,
                uint8_t* minute,
                uint8_t* second,
                float32_t* second_fraction)
{
  uint32_t ij;

  ij = (uint32_t)((time - floor(time)) * 86400.0);
  *hour = (uint8_t)(ij / 3600L);
  *minute = (uint8_t)((ij / 60L) % 60L);
  *second = (uint8_t)(ij % 60L);
  if (second_fraction != NULL) {
    *second_fraction = (float32_t)(time - floor(time));
  }
}

/*  date_time_decode  --  Decode a Julian day and day fraction
                          into civil date and time.  */

/* Deprecated. Do not use. */
void date_time_decode(struct date_time* dt,
                      uint32_t* year,
                      uint8_t* month,
                      uint8_t* day,
                      uint8_t* hour,
                      uint8_t* minute,
                      uint8_t* second,
                      float32_t* second_fraction)
{
  DateDecode(dt->julian_day_number, year, month, day);
  TimeDecode(dt->julian_day_fraction, hour, minute, second, second_fraction);
}

/*  TmEncode  --  Encode a civil date and time from a tm structure
 *                 to a Julian day and day fraction.
 */

/* Deprecated. Do not use. */
void TmEncode(struct date_time* dt, struct tm* tm)
{
  uint32_t year;
  uint8_t month, day, hour, minute, second;

  year = tm->tm_year + 1900;
  month = tm->tm_mon + 1;
  day = tm->tm_mday;
  hour = tm->tm_hour;
  minute = tm->tm_min;
  second = tm->tm_sec;
  dt->julian_day_number = DateEncode(year, month, day);
  dt->julian_day_fraction = TimeEncode(hour, minute, second, 0.0);
}


/*  TmDecode  --  Decode a Julian day and day fraction
                   into civil date and time in tm structure */

/* Deprecated. Do not use. */
void TmDecode(struct date_time* dt, struct tm* tm)
{
  uint32_t year;
  uint8_t month, day, hour, minute, second;

  DateDecode(dt->julian_day_number, &year, &month, &day);
  TimeDecode(dt->julian_day_fraction, &hour, &minute, &second, NULL);
  tm->tm_year = year - 1900;
  tm->tm_mon = month - 1;
  tm->tm_mday = day;
  tm->tm_hour = hour;
  tm->tm_min = minute;
  tm->tm_sec = second;
}


/*  DateTimeCompare  --  Compare two dates and times and return
                           the relationship as follows:

                                    -1    dt1 < dt2
                                     0    dt1 = dt2
                                     1    dt1 > dt2
*/

/* Deprecated. Do not use. */
int DateTimeCompare(struct date_time* dt1, struct date_time* dt2)
{
  if (dt1->julian_day_number == dt2->julian_day_number) {
    if (dt1->julian_day_fraction == dt2->julian_day_fraction) { return 0; }
    return (dt1->julian_day_fraction < dt2->julian_day_fraction) ? -1 : 1;
  }
  return (dt1->julian_day_number - dt2->julian_day_number) ? -1 : 1;
}
