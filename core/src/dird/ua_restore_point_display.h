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
/**
 * @file
 * Pure, testable helpers for formatting restore-point candidates in the
 * guided FileSet@Client restore picker (ua_restore.cc). Kept separate
 * from ua_restore.cc (which owns the SQL query + prompt wiring) so this
 * logic can be unit tested without a database connection.
 */

#ifndef BAREOS_DIRD_UA_RESTORE_POINT_DISPLAY_H_
#define BAREOS_DIRD_UA_RESTORE_POINT_DISPLAY_H_

#include "include/bc_types.h"

#include <cstdint>
#include <string>

namespace directordaemon {

/**
 * One selectable restore point in the guided FileSet@Client restore
 * picker: the anchor Full backup job plus the resolved chain's
 * aggregate info (job count, distinct job names) and the anchor Full
 * job's own file/byte counts.
 */
struct RestorePointCandidate {
  JobId_t JobId = 0;
  utime_t RestorePoint = 0;
  uint32_t JobCount = 0;
  std::string JobNames;   /**< Distinct job names, comma separated. */
  uint64_t JobFiles = 0;  /**< Anchor Full job's own JobFiles. */
  uint64_t JobBytes = 0;  /**< Anchor Full job's own JobBytes. */
};

/**
 * Format a duration (in seconds, always >= 0) as a human readable age,
 * e.g. "3 days ago", "1 hour ago", "just now".
 */
std::string FormatHumanReadableAge(utime_t seconds_ago);

/**
 * Format one restore-point candidate into a single, human readable
 * prompt line: age/timestamp, JobId, job name(s), file count and byte
 * size, e.g.
 *   "2026-09-12 14:33:21 (3 days ago)  JobId=42  Files=12,345  \
 *    Size=1.234 GB  Jobs: FullBackup, IncrBackup"
 *
 * \param now current time (seconds since epoch), injected so the
 *   resulting "X ago" text is deterministically testable.
 */
std::string FormatRestorePointCandidate(const RestorePointCandidate& candidate,
                                        utime_t now);

}  // namespace directordaemon

#endif  // BAREOS_DIRD_UA_RESTORE_POINT_DISPLAY_H_
