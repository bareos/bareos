/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#include "dird/ua_restore_point_display.h"

#include "lib/btime.h"
#include "lib/edit.h"

namespace directordaemon {

std::string FormatHumanReadableAge(utime_t seconds_ago)
{
  struct Unit {
    int64_t seconds;
    const char* singular;
    const char* plural;
  };
  static const Unit units[] = {
      {31536000, "year", "years"}, {2592000, "month", "months"},
      {604800, "week", "weeks"},   {86400, "day", "days"},
      {3600, "hour", "hours"},     {60, "minute", "minutes"},
  };
  if (seconds_ago < 0) { seconds_ago = 0; }
  for (const auto& unit : units) {
    if (seconds_ago >= unit.seconds) {
      int64_t count = seconds_ago / unit.seconds;
      PoolMem age(PM_NAME);
      Mmsg(age, "%" PRId64 " %s ago", count,
           count == 1 ? T_(unit.singular) : T_(unit.plural));
      return std::string(age.c_str());
    }
  }
  return T_("just now");
}

std::string FormatRestorePointCandidate(const RestorePointCandidate& candidate,
                                        utime_t now)
{
  char timestamp[MAX_TIME_LENGTH];
  bstrutime(timestamp, sizeof(timestamp), candidate.RestorePoint);
  std::string age = FormatHumanReadableAge(now - candidate.RestorePoint);

  char files_ec[50];
  PoolMem line(PM_MESSAGE);
  Mmsg(line, "%s (%s)  JobId=%" PRIu32 "  Files=%s  Size=%s  Jobs: %s",
       timestamp, age.c_str(), candidate.JobId,
       edit_uint64_with_commas(candidate.JobFiles, files_ec),
       SizeAsSiPrefixFormat(candidate.JobBytes).c_str(),
       candidate.JobNames.c_str());
  return std::string(line.c_str());
}

}  // namespace directordaemon
