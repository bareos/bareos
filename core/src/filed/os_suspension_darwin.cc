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

#include "filed/os_suspension.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

#include "include/bareos.h"

namespace filedaemon {

struct SleepPrevention {
  IOPMAssertionID assertion_id = kIOPMNullAssertionID;
  bool warning_logged = false;
};

static void WarnDarwinSleepInhibitFailure(JobControlRecord* jcr,
                                          bool& warning_logged,
                                          IOReturn status)
{
  if (warning_logged) { return; }

  Qmsg(jcr, M_INFO, 0,
       T_("Failed to inhibit system sleep on macOS (IOKit status %d). "
          "Continuing without sleep inhibition.\n"),
       status);
  warning_logged = true;
}

SleepPrevention* ActivateSleepPrevention(JobControlRecord* jcr)
{
  auto* sleep_prevention = new SleepPrevention;
  IOPMAssertionID assertion_id = sleep_prevention->assertion_id;

  IOReturn status = IOPMAssertionCreateWithName(
      kIOPMAssertionTypePreventSystemSleep, kIOPMAssertionLevelOn,
      CFSTR("Bareos backup or restore running"), &assertion_id);
  if (status != kIOReturnSuccess) {
    WarnDarwinSleepInhibitFailure(jcr, sleep_prevention->warning_logged,
                                  status);
    return sleep_prevention;
  }

  sleep_prevention->assertion_id = assertion_id;
  return sleep_prevention;
}

void DeactivateSleepPrevention(SleepPrevention* sleep_prevention)
{
  if (!sleep_prevention) { return; }

  if (sleep_prevention->assertion_id != kIOPMNullAssertionID) {
    IOPMAssertionRelease(sleep_prevention->assertion_id);
  }
  delete sleep_prevention;
}

}  // namespace filedaemon
