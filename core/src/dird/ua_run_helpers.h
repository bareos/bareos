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
 * Pure helpers of the interactive run/restore screens (ua_run.cc):
 * text fitting, relative times and destination browse path handling.
 * Kept free of UaContext so they can be unit tested.
 */

#ifndef BAREOS_DIRD_UA_RUN_HELPERS_H_
#define BAREOS_DIRD_UA_RUN_HELPERS_H_

#include <ctime>
#include <string>
#include <string_view>

namespace directordaemon {

/** Truncate text to width columns, ending in "..." if it was cut. */
std::string FitRestoreOptionText(std::string_view text, size_t width);

/** Show a width column window of text starting at offset, with < > markers. */
std::string ScrollRestoreOptionText(std::string_view text,
                                    size_t width,
                                    size_t offset);

/** Describe when relative to now, e.g. "in 2 hours" or "3 days ago". */
std::string FormatRunScheduleRelativeTime(time_t when, time_t now);

/** True if text has a "YYYY-MM-DD HH:MM:SS" timestamp at pos. */
bool IsListingTimestamp(std::string_view text, size_t pos);

/**
 * Parse one line of an estimate listing into the file name and whether it
 * is a directory. A symlink target (" -> ...") is dropped.
 */
bool ParseEstimateListingLine(std::string_view line,
                              std::string* path,
                              bool* directory);

/** Strip trailing separators; an empty path becomes "/". */
std::string NormalizeBrowsePath(std::string_view path);
std::string ParentBrowsePath(std::string_view path);
std::string BrowsePathBasename(std::string_view path);

/** True if child is directly below parent (not deeper, not equal). */
bool IsImmediateBrowseChild(std::string_view parent, std::string_view child);

/** Remove the last UTF-8 character of text. */
void RemoveLastBrowseSearchCharacter(std::string* text);

/** An empty search matches everything. */
bool ContainsBrowseSearchMatch(std::string_view text, std::string_view search);

}  // namespace directordaemon

#endif  // BAREOS_DIRD_UA_RUN_HELPERS_H_
