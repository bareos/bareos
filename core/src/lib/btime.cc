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
#include <date/date.h>
#include "include/timestamp_format.h"

#include <sstream>
#include <iomanip>
#include <iostream>

namespace TimestampFormat {
  // %z is missing because it is not correctly implemented in windows :(
  // As we optionally want to offer microseconds, we implement the %z ourselves on
  // all platforms
  const char* Default {"%Y-%m-%dT%H:%M:%S"};

  // for the scheduler preview
  const char* SchedPreview {"%a %d-%b-%Y %H:%M"};

  // for use in TO_CHAR database queries
  const char* DatabaseDefault {"YYYY-MM-DD\"T\"HH24:MI:SSTZH:TZM"};

  // to create filenames, so only what is allowed in filenames also on windows
  const char* Filename {"%Y-%m-%dT%H.%M.%S"};
}



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
std::string GetCurrentTimezoneOffset()
{
  std::time_t t = time(0);
  auto timeoffset = timegm(localtime(&t)) - t;

  // calculate hour and minutes
  auto hourdiff = timeoffset / (60 * 60);
  auto mindiff = abs(timeoffset % (60 * 60) / 60);
  std::stringstream output{};
  output << std::internal << std::showpos << std::setw(3) << std::setfill('0')
         << hourdiff << std::noshowpos << std::setw(2) << mindiff;
  return output.str();
}


void Blocaltime(const time_t* time, struct tm* tm)
{
  /* ***FIXME**** localtime_r() should be user configurable */
  (void)localtime_r(time, tm);
}


// Formatted time as iso8601 string
// only call from inside this file
static char* bstrftime_internal(char* dt,
                                int maxlen,
                                utime_t utime,
                                const char* fmt,
                                bool print_microseconds = false)
{
  time_t time = (time_t)utime;
  struct tm tm;
  std::vector<char> buf(MAX_NAME_LENGTH, '\0');
  std::stringstream microseconds{};
  if (print_microseconds) {
    uint32_t usecs{};
    usecs = GetCurrentBtime() % 1000000;
    microseconds << "." << std::internal << std::setw(6) << std::setfill('0')
                 << usecs;
  }
  // we have to add the content that usually is provided by %z and, if required
  // also the microseconds
  Blocaltime(&time, &tm);
  strftime(buf.data(), maxlen, fmt, &tm);
  std::string timeformat_without_timezone{buf.data()};
  std::string fullformat = timeformat_without_timezone + microseconds.str()
                           + GetCurrentTimezoneOffset();
  strncpy(dt, fullformat.data(), fullformat.size());
  dt[fullformat.size()] = '\0';
  return dt;
}

static char* bstrftime(char* dt, int maxlen, utime_t utime)
{
  return bstrftime_internal(dt, maxlen, utime, TimestampFormat::Default);
}

static char* bstrftime_scheduler_preview(char* dt, int maxlen, utime_t utime)
{
  return bstrftime_internal(dt, maxlen, utime, TimestampFormat::SchedPreview);
}

static char* bstrftime_filename(char* dt, int maxlen, utime_t utime)
{
  return bstrftime_internal(dt, maxlen, utime,  TimestampFormat::Filename);
}

// format for scheduler preview: Thu 05-Oct-2023 02:05+0200
std::string bstrftime_scheduler_preview(utime_t tim)
{
  std::vector<char> buf(MAX_TIME_LENGTH, '\0');
  bstrftime_scheduler_preview(buf.data(), MAX_TIME_LENGTH, tim);
  return std::string{buf.data()};
}

// format with standard format: 2023-10-01T17:16:53+0200
std::string bstrftime(utime_t tim)
{
  std::vector<char> buf(MAX_TIME_LENGTH, '\0');
  bstrftime(buf.data(), MAX_TIME_LENGTH, tim);
  return std::string{buf.data()};
}

// format with to use as filename : 2023-10-01T17.16.25+0200
// No characters unsuitable for filenames
std::string bstrftime_filename(utime_t tim)
{
  std::vector<char> buf(MAX_TIME_LENGTH, '\0');
  bstrftime_filename(buf.data(), MAX_TIME_LENGTH, tim);
  return std::string{buf.data()};
}

// format for debug output. This is the standard format plus microseconds
// example: 2023-10-01T17:16:53.123456+0200
std::string bstrftime_debug(utime_t tim)
{
  std::vector<char> buf(MAX_TIME_LENGTH, '\0');
  bstrftime_internal(buf.data(), MAX_TIME_LENGTH, tim,
                     TimestampFormat::Default, true);
  return std::string{buf.data()};
}


utime_t StrToUtime(const char* str)
{
  date::sys_time<std::chrono::milliseconds> tp;
  tp = parse8601(std::istringstream{str}, "%FT%TZ");
  if (tp.time_since_epoch().count() == 0) {
    tp = parse8601(std::istringstream{str}, "%FT%T%z");
  }
  auto current_timezone_offset = GetCurrentTimezoneOffset();
  if (tp.time_since_epoch().count() == 0) {
    // if we only have a "bareos" timestring without timezone,
    // assume the current timezone is meant and add this to the string
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
