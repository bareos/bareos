/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, June 2002
/**
 * @file
 * BootStrap record definition -- for restoring files.
 */
#ifndef BAREOS_STORED_BSR_H_
#define BAREOS_STORED_BSR_H_

#include <memory>
#ifndef HAVE_REGEX_H
#  include "lib/bregex.h"
#else
#  include <regex.h>
#endif
#include "lib/attr.h"

namespace storagedaemon {

/**
 * List of Volume names to be read by Storage daemon.
 *  Formed by Storage daemon from BootStrapRecord
 */
struct VolumeList {
  VolumeList* next;
  char VolumeName[MAX_NAME_LENGTH];
  char MediaType[MAX_NAME_LENGTH];
  char device[MAX_NAME_LENGTH];
  int Slot;
  uint32_t start_file;
};

struct BootStrapRecord;

namespace bsr {
struct interval {
  std::uint64_t start, end;

  interval() = default;
  interval(std::uint64_t start_, std::uint64_t end_) noexcept
      : start{start_}, end{end_}
  {
  }

  bool contains(std::uint64_t value) const noexcept
  {
    return start <= value && value <= end;
  }
};

struct volume {
  BootStrapRecord* root{nullptr};

  std::string volume_name{};
  std::uint64_t count{}; /* count of files to restore this bsr */
  std::uint64_t found{}; /* count of restored files this bsr */
  bool done{};           /* set when everything found for this volume */

  std::optional<std::string> media_type{};
  std::optional<std::string> device{};
  std::optional<std::int32_t> slot{};

  std::vector<interval> files;
  std::vector<interval> blocks;

  std::vector<interval> addresses;
  std::vector<interval> file_indices;

  std::vector<std::uint32_t> session_times;
  std::vector<interval> session_ids;

  std::vector<interval> job_ids;

  std::vector<std::string> clients;

  std::vector<std::string> jobs;
  std::vector<std::uint32_t> job_types;
  std::vector<std::uint32_t> job_levels;

  std::vector<std::int32_t> streams;

  std::string fileregex{}; /* set if restore is filtered on filename */
  std::unique_ptr<regex_t> fileregex_re{nullptr};
};
};  // namespace bsr

struct BootStrapRecord {
  bool Reposition{};         /* set when any bsr is marked done */
  bool mount_next_volume{};  /* set when next volume should be mounted */
  bool use_fast_rejection{}; /* set if fast rejection can be used */
  bool use_positioning{};    /* set if we can position the archive */
  bool skip_file{};          /* skip all records for current file */

  std::size_t current_volume = std::numeric_limits<std::size_t>::max();
  std::vector<bsr::volume> volumes;

  Attributes* attr{nullptr}; /* scratch space for unpacking */

  bsr::volume* current()
  {
    if (current_volume >= volumes.size()) { return nullptr; }
    return &volumes[current_volume];
  }

  void advance() { current_volume += 1; }
};


void CreateRestoreVolumeList(JobControlRecord* jcr);
void FreeRestoreVolumeList(JobControlRecord* jcr);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BSR_H_
