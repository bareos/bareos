/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2023 Bareos GmbH & Co. KG

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

/* Concerning times. There are a number of different time standards
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

#if defined(HAVE_WIN32)
#  define NO_UNDERSCORE_MACRO 1
#endif
#include "include/bareos.h"
#include "lib/btime.h"
#include <math.h>
#undef _
#include <date/date.h>
#include "include/timestamp_format.h"
#include <fmt/format.h>

#include <sstream>
#include <iomanip>
#include <iostream>
#include <ctime>



static date::sys_time<std::chrono::milliseconds> parse8601(std::istream&& is,
                                                           const char* pattern)
{
  std::string save(std::istreambuf_iterator<char>(is), {});
  std::istringstream in{save};
  date::sys_time<std::chrono::milliseconds> tp;
  in >> date::parse(pattern, tp);
  return tp;
}

#if HAVE_WIN32
#  define timegm _mkgmtime
#endif

// create string in ISO format, e.g. -0430
std::string GetCurrentTimezoneOffset(utime_t tim)
{
  const std::time_t t = static_cast<std::time_t>(tim);
  struct tm tm;
  Blocaltime(&t, &tm);
  const auto second_offset = timegm(&tm) - t;

  const auto hour_offset = second_offset / (60 * 60);
  const auto minute_offset = abs(second_offset % (60 * 60) / 60);

  return fmt::format(FMT_STRING("{:+03d}{:02d}"), hour_offset, minute_offset);
}

void Blocaltime(const time_t* time, struct tm* tm)
{
  (void)localtime_r(time, tm);
}

namespace {
static std::string bstrftime_notz(utime_t utime, const char* fmt)
{
  auto buf = std::string(kMaxTimeLength, '\0');
  struct tm tm;
  Blocaltime(&utime, &tm);
  strftime(buf.data(), kMaxTimeLength, fmt, &tm);
  buf.resize(strlen(buf.c_str()));
  return buf;
}

static std::string bstrftime_internal(utime_t utime, const char* fmt)
{
  return bstrftime_notz(utime, fmt) + GetCurrentTimezoneOffset(utime);
}

}  // namespace

// format for scheduler preview: Thu 05-Oct-2023 02:05+0200
std::string bstrftime_scheduler_preview(utime_t tim)
{
  return bstrftime_internal(tim, TimestampFormat::SchedPreview);
}

// format with standard format: 2023-10-01T17:16:53+0200
std::string bstrftime(utime_t tim)
{
  return bstrftime_internal(tim, TimestampFormat::Default);
}

// format with to use as filename : 2023-10-01T17.16.25+0200
// No characters unsuitable for filenames
std::string bstrftime_filename(utime_t tim)
{
  return bstrftime_internal(tim, TimestampFormat::Filename);
}

// format for debug output. This is the standard format plus microseconds
// example: 2023-10-01T17:16:53.123456+0200
std::string bstrftime_debug()
{
  struct timeval tv;
  if (gettimeofday(&tv, NULL) != 0) {
    tv.tv_sec = (long)time(NULL); /* fall back to old method */
    tv.tv_usec = 0;
  }
  return bstrftime_notz(tv.tv_sec, TimestampFormat::Default)
         + fmt::format(FMT_STRING(".{:06d}"), tv.tv_usec)
         + GetCurrentTimezoneOffset(tv.tv_sec);
}


utime_t StrToUtime(const char* str)
{
  date::sys_time<std::chrono::milliseconds> tp;
  tp = parse8601(std::istringstream{str}, "%FT%TZ");
  if (tp.time_since_epoch().count() == 0) {
    tp = parse8601(std::istringstream{str}, "%FT%T%z");
  }
  if (tp.time_since_epoch().count() == 0) {
    // if we only have a "bareos" timestring without timezone,
    // assume the current timezone is meant and add this to the string
    auto current_timezone_offset = GetCurrentTimezoneOffset(time(0));
    tp = parse8601(std::istringstream{str + current_timezone_offset},
                   "%F %T%z");
  }
  auto retval
      = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch())
            .count();
  return retval;
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
