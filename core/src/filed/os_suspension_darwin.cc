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

#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_DARWIN_OS)
#  include <CoreFoundation/CoreFoundation.h>
#  include <IOKit/pwr_mgt/IOPMLib.h>

#  include "include/bareos.h"

namespace filedaemon {

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

void ActivateDarwinSleepPrevention(JobControlRecord* jcr,
                                   SleepPrevention& sleep_prevention)
{
  IOPMAssertionID assertion_id
      = static_cast<IOPMAssertionID>(sleep_prevention.darwin_assertion_id);
  if (assertion_id != kIOPMNullAssertionID) { return; }

  IOReturn status = IOPMAssertionCreateWithName(
      kIOPMAssertionTypePreventSystemSleep, kIOPMAssertionLevelOn,
      CFSTR("Bareos backup or restore running"), &assertion_id);
  if (status != kIOReturnSuccess) {
    sleep_prevention.darwin_assertion_id = 0;
    WarnDarwinSleepInhibitFailure(jcr, sleep_prevention.darwin_warning_logged,
                                  status);
    return;
  }

  sleep_prevention.darwin_assertion_id = assertion_id;
}

void DeactivateDarwinSleepPrevention(SleepPrevention& sleep_prevention)
{
  IOPMAssertionID assertion_id
      = static_cast<IOPMAssertionID>(sleep_prevention.darwin_assertion_id);
  if (assertion_id != kIOPMNullAssertionID) {
    IOPMAssertionRelease(assertion_id);
    sleep_prevention.darwin_assertion_id = 0;
  }
}

}  // namespace filedaemon

#endif
