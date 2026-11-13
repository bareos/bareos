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

#ifndef BAREOS_DIRD_NDMP_FILESET_VALIDATION_H_
#define BAREOS_DIRD_NDMP_FILESET_VALIDATION_H_

namespace directordaemon {

// Outcome of checking the fileset of an NDMP backup job.
enum class NdmpFilesetValidation
{
  kOk,
  kNoIncludeSet,
  kMultipleIncludeSets,
  kNoFileEntry,
  kMultipleFileEntries,
};

/* An NDMP backup job saves exactly one filesystem, so its fileset has to
 * consist of exactly one Include block holding exactly one File entry.
 *
 * Backing up several filesystems in a single job would require several NDMP
 * sessions sharing one device reservation, which cannot be tracked reliably.
 * Use one job per filesystem instead. */
constexpr NdmpFilesetValidation ValidateNdmpFileset(int include_sets,
                                                    int file_entries)
{
  if (include_sets < 1) { return NdmpFilesetValidation::kNoIncludeSet; }
  if (include_sets > 1) { return NdmpFilesetValidation::kMultipleIncludeSets; }
  if (file_entries < 1) { return NdmpFilesetValidation::kNoFileEntry; }
  if (file_entries > 1) { return NdmpFilesetValidation::kMultipleFileEntries; }

  return NdmpFilesetValidation::kOk;
}

// Explanation shown to the user for a rejected fileset.
constexpr const char* NdmpFilesetValidationMessage(NdmpFilesetValidation result)
{
  switch (result) {
    case NdmpFilesetValidation::kOk:
      return "";
    case NdmpFilesetValidation::kNoIncludeSet:
      return "FileSet contains no Include block. An NDMP backup job needs"
             " exactly one Include block with exactly one File entry.\n";
    case NdmpFilesetValidation::kMultipleIncludeSets:
      return "FileSet contains more than one Include block. An NDMP backup job"
             " saves exactly one filesystem, so please use one job per"
             " filesystem.\n";
    case NdmpFilesetValidation::kNoFileEntry:
      return "Include block of the FileSet contains no File entry. An NDMP"
             " backup job needs exactly one File entry naming the filesystem"
             " to save.\n";
    case NdmpFilesetValidation::kMultipleFileEntries:
      return "Include block of the FileSet contains more than one File entry."
             " An NDMP backup job saves exactly one filesystem, so please use"
             " one job per filesystem.\n";
  }

  return "";
}

} /* namespace directordaemon */

#endif  // BAREOS_DIRD_NDMP_FILESET_VALIDATION_H_
