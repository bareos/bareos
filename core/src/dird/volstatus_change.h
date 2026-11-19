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
 * Rules for changing a volume status with "update volume volstatus=".
 */
#ifndef BAREOS_DIRD_VOLSTATUS_CHANGE_H_
#define BAREOS_DIRD_VOLSTATUS_CHANGE_H_

#include <string_view>

namespace directordaemon {

enum class VolStatusChange
{
  kAllowed,
  kUnchanged,
  // A volume set to Recycle is relabelled on its next use, so any job
  // still stored on it would be lost.
  kRecycleRequiresPurged,
  // A data volume marked Cleaning is never used for backups again.
  kCleaningRequiresEmptyVolume,
};

constexpr bool VolStatusEquals(std::string_view lhs, std::string_view rhs)
{
  if (lhs.size() != rhs.size()) { return false; }
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    auto lower = [](char c) {
      return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    };
    if (lower(lhs[i]) != lower(rhs[i])) { return false; }
  }
  return true;
}

/**
 * Classifies a change of a volume status from @p current to @p requested.
 * @p volume_has_jobs tells whether the catalog still references jobs on
 * the volume; it is only consulted for Cleaning.
 */
constexpr VolStatusChange CheckVolStatusChange(std::string_view current,
                                               std::string_view requested,
                                               bool volume_has_jobs)
{
  if (VolStatusEquals(current, requested)) {
    return VolStatusChange::kUnchanged;
  }
  if (VolStatusEquals(requested, "Recycle")
      && !VolStatusEquals(current, "Purged")) {
    return VolStatusChange::kRecycleRequiresPurged;
  }
  if (VolStatusEquals(requested, "Cleaning") && volume_has_jobs) {
    return VolStatusChange::kCleaningRequiresEmptyVolume;
  }
  return VolStatusChange::kAllowed;
}

static_assert(VolStatusEquals("Read-Only", "read-only"));
static_assert(!VolStatusEquals("Used", "Full"));
static_assert(!VolStatusEquals("Use", "Used"));
static_assert(CheckVolStatusChange("Append", "Used", true)
              == VolStatusChange::kAllowed);
static_assert(CheckVolStatusChange("Used", "used", true)
              == VolStatusChange::kUnchanged);
static_assert(CheckVolStatusChange("Purged", "Recycle", false)
              == VolStatusChange::kAllowed);
static_assert(CheckVolStatusChange("Full", "Recycle", true)
              == VolStatusChange::kRecycleRequiresPurged);
static_assert(CheckVolStatusChange("Append", "Recycle", false)
              == VolStatusChange::kRecycleRequiresPurged);
static_assert(CheckVolStatusChange("Append", "Cleaning", true)
              == VolStatusChange::kCleaningRequiresEmptyVolume);
static_assert(CheckVolStatusChange("Append", "Cleaning", false)
              == VolStatusChange::kAllowed);
static_assert(CheckVolStatusChange("Error", "Append", true)
              == VolStatusChange::kAllowed);

}  // namespace directordaemon

#endif  // BAREOS_DIRD_VOLSTATUS_CHANGE_H_
