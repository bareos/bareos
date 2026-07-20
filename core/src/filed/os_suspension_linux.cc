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

#if defined(FILED_CLIENT_SLEEP_INHIBITION) && defined(HAVE_LINUX_OS) \
    && defined(HAVE_SYSTEMD)
#  include <fcntl.h>
#  include <systemd/sd-bus.h>
#  include <unistd.h>

#  include "include/bareos.h"
#  include "lib/berrno.h"

namespace filedaemon {

static void WarnLinuxSleepInhibitFailure(JobControlRecord* jcr,
                                         bool& warning_logged,
                                         const char* reason)
{
  if (warning_logged) { return; }

  Qmsg(jcr, M_INFO, 0,
       T_("Failed to inhibit system sleep on Linux: %s. "
          "Continuing without sleep inhibition.\n"),
       reason);
  warning_logged = true;
}

void ActivateLinuxSleepPrevention(JobControlRecord* jcr,
                                  SleepPrevention& sleep_prevention)
{
  if (sleep_prevention.linux_inhibitor_fd >= 0) { return; }

  sd_bus* bus = nullptr;
  sd_bus_message* reply = nullptr;
  sd_bus_error error = SD_BUS_ERROR_NULL;

  auto cleanup = [&]() {
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
  };

  auto warn_and_return = [&](const char* reason) {
    WarnLinuxSleepInhibitFailure(jcr, sleep_prevention.linux_warning_logged,
                                 reason);
    cleanup();
  };

  int status = sd_bus_open_system(&bus);
  if (status < 0) {
    BErrNo be;
    warn_and_return(be.bstrerror(-status));
    return;
  }

  status = sd_bus_call_method(bus, "org.freedesktop.login1",
                              "/org/freedesktop/login1",
                              "org.freedesktop.login1.Manager", "Inhibit",
                              &error, &reply, "ssss", "sleep", "bareos-fd",
                              "Backup or restore running", "block");
  if (status < 0) {
    if (error.message != nullptr) {
      warn_and_return(error.message);
    } else {
      BErrNo be;
      warn_and_return(be.bstrerror(-status));
    }
    return;
  }

  int fd = -1;
  status = sd_bus_message_read(reply, "h", &fd);
  if (status < 0 || fd < 0) {
    BErrNo be;
    warn_and_return(be.bstrerror((status < 0) ? -status : EINVAL));
    return;
  }

  int dupfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
  if (dupfd < 0) {
    BErrNo be;
    warn_and_return(be.bstrerror());
    return;
  }

  sleep_prevention.linux_inhibitor_fd = dupfd;
  cleanup();
}

void DeactivateLinuxSleepPrevention(SleepPrevention& sleep_prevention)
{
  if (sleep_prevention.linux_inhibitor_fd >= 0) {
    close(sleep_prevention.linux_inhibitor_fd);
    sleep_prevention.linux_inhibitor_fd = -1;
  }
}

}  // namespace filedaemon

#endif
