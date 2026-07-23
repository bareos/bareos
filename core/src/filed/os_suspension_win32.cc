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

#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_WIN32)
#  include "compat.h"

namespace filedaemon {

void ActivateWindowsSleepPrevention(SleepPrevention& sleep_prevention)
{
  if (!sleep_prevention.windows_active) {
    PreventOsSuspensions();
    sleep_prevention.windows_active = true;
  }
}

void DeactivateWindowsSleepPrevention(SleepPrevention& sleep_prevention)
{
  if (sleep_prevention.windows_active) {
    AllowOsSuspensions();
    sleep_prevention.windows_active = false;
  }
}

}  // namespace filedaemon

#endif
