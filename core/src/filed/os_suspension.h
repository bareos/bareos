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

#ifndef BAREOS_FILED_OS_SUSPENSION_H_
#define BAREOS_FILED_OS_SUSPENSION_H_

#include <cstdint>

#include "include/bareos.h"

class JobControlRecord;

namespace filedaemon {

struct SleepPrevention {
#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_WIN32)
  bool windows_active = false;
#endif
#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_LINUX_OS) \
    && defined(HAVE_SYSTEMD)
  int linux_inhibitor_fd = -1;
  bool linux_warning_logged = false;
#endif
#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_DARWIN_OS)
  uintptr_t darwin_assertion_id = 0;
  bool darwin_warning_logged = false;
#endif
};

void ActivateSleepPrevention(JobControlRecord* jcr,
                             SleepPrevention& sleep_prevention);
void DeactivateSleepPrevention(SleepPrevention& sleep_prevention);

#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_WIN32)
void ActivateWindowsSleepPrevention(SleepPrevention& sleep_prevention);
void DeactivateWindowsSleepPrevention(SleepPrevention& sleep_prevention);
#endif

#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_LINUX_OS) \
    && defined(HAVE_SYSTEMD)
void ActivateLinuxSleepPrevention(JobControlRecord* jcr,
                                  SleepPrevention& sleep_prevention);
void DeactivateLinuxSleepPrevention(SleepPrevention& sleep_prevention);
#endif

#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_DARWIN_OS)
void ActivateDarwinSleepPrevention(JobControlRecord* jcr,
                                   SleepPrevention& sleep_prevention);
void DeactivateDarwinSleepPrevention(SleepPrevention& sleep_prevention);
#endif

}  // namespace filedaemon

#endif  // BAREOS_FILED_OS_SUSPENSION_H_
