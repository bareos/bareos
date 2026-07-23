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

namespace filedaemon {

void ActivateSleepPrevention(JobControlRecord* jcr,
                             SleepPrevention& sleep_prevention)
{
  (void)jcr;
  (void)sleep_prevention;

#if defined(FILED_CLIENT_SLEEP_INHIBITION)
#  if defined(HAVE_WIN32)
  ActivateWindowsSleepPrevention(sleep_prevention);
#  endif
#  if defined(HAVE_LINUX_OS) && defined(HAVE_SYSTEMD)
  ActivateLinuxSleepPrevention(jcr, sleep_prevention);
#  endif
#  if defined(HAVE_DARWIN_OS)
  ActivateDarwinSleepPrevention(jcr, sleep_prevention);
#  endif
#endif
}

void DeactivateSleepPrevention(SleepPrevention& sleep_prevention)
{
  (void)sleep_prevention;

#if defined(FILED_CLIENT_SLEEP_INHIBITION)
#  if defined(HAVE_WIN32)
  DeactivateWindowsSleepPrevention(sleep_prevention);
#  endif
#  if defined(HAVE_LINUX_OS) && defined(HAVE_SYSTEMD)
  DeactivateLinuxSleepPrevention(sleep_prevention);
#  endif
#  if defined(HAVE_DARWIN_OS)
  DeactivateDarwinSleepPrevention(sleep_prevention);
#  endif
#endif
}

}  // namespace filedaemon
