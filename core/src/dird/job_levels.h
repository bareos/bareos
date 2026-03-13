/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_JOB_LEVELS_H_
#define BAREOS_DIRD_JOB_LEVELS_H_

/**
 * Keywords (RHS) permitted in Job Level records
 *
 * name level job_type
 */
#include "include/job_level.h"
#include "include/job_types.h"
#include <cstdint>

namespace directordaemon {

// Job Level keyword structure
struct s_jl {
  const char* name; /* level keyword */
  uint32_t level;   /* level */
  int32_t job_type; /* JobType permitting this level */
};

static constexpr inline struct s_jl joblevels[]
    = {{"Full", L_FULL, JT_BACKUP},
       {"Base", L_BASE, JT_BACKUP},
       {"Incremental", L_INCREMENTAL, JT_BACKUP},
       {"Differential", L_DIFFERENTIAL, JT_BACKUP},
       {"Since", L_SINCE, JT_BACKUP},
       {"VirtualFull", L_VIRTUAL_FULL, JT_BACKUP},
       {"Catalog", L_VERIFY_CATALOG, JT_VERIFY},
       {"InitCatalog", L_VERIFY_INIT, JT_VERIFY},
       {"VolumeToCatalog", L_VERIFY_VOLUME_TO_CATALOG, JT_VERIFY},
       {"DiskToCatalog", L_VERIFY_DISK_TO_CATALOG, JT_VERIFY},
       {"Data", L_VERIFY_DATA, JT_VERIFY},
       {"Full", L_FULL, JT_COPY},
       {"Incremental", L_INCREMENTAL, JT_COPY},
       {"Differential", L_DIFFERENTIAL, JT_COPY},
       {"Full", L_FULL, JT_MIGRATE},
       {"Incremental", L_INCREMENTAL, JT_MIGRATE},
       {"Differential", L_DIFFERENTIAL, JT_MIGRATE},
       {" ", L_NONE, JT_ADMIN},
       {" ", L_NONE, JT_ARCHIVE},
       {" ", L_NONE, JT_RESTORE},
       {" ", L_NONE, JT_CONSOLIDATE},
       {}};
};  // namespace directordaemon

#endif  // BAREOS_DIRD_JOB_LEVELS_H_
