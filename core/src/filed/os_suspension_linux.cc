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

#include <fcntl.h>
#include <memory>
#include <systemd/sd-bus.h>
#include <unistd.h>

#include "include/bareos.h"
#include "lib/berrno.h"

namespace filedaemon {

struct SleepPrevention {
  int inhibitor_fd = -1;
  bool warning_logged = false;
};

struct SdBusError {
  sd_bus_error error = SD_BUS_ERROR_NULL;

  ~SdBusError() { sd_bus_error_free(&error); }
};

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

SleepPrevention* ActivateSleepPrevention(JobControlRecord* jcr)
{
  auto sleep_prevention = std::make_unique<SleepPrevention>();

  std::unique_ptr<sd_bus, decltype(&sd_bus_unref)> bus{nullptr, sd_bus_unref};
  std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)> reply{
      nullptr, sd_bus_message_unref};
  SdBusError error;

  auto warn_and_return = [&](const char* reason) {
    WarnLinuxSleepInhibitFailure(jcr, sleep_prevention->warning_logged, reason);
  };

  sd_bus* raw_bus = nullptr;
  int status = sd_bus_open_system(&raw_bus);
  bus.reset(raw_bus);
  if (status < 0) {
    BErrNo be;
    warn_and_return(be.bstrerror(-status));
    return sleep_prevention.release();
  }

  sd_bus_message* raw_reply = nullptr;
  status = sd_bus_call_method(
      bus.get(), "org.freedesktop.login1", "/org/freedesktop/login1",
      "org.freedesktop.login1.Manager", "Inhibit", &error.error, &raw_reply,
      "ssss", "sleep", "bareos-fd", "Backup or restore running", "block");
  reply.reset(raw_reply);
  if (status < 0) {
    if (error.error.message != nullptr) {
      warn_and_return(error.error.message);
    } else {
      BErrNo be;
      warn_and_return(be.bstrerror(-status));
    }
    return sleep_prevention.release();
  }

  int fd = -1;
  status = sd_bus_message_read(reply.get(), "h", &fd);
  if (status < 0 || fd < 0) {
    BErrNo be;
    warn_and_return(be.bstrerror((status < 0) ? -status : EINVAL));
    return sleep_prevention.release();
  }

  // The message owns fd; duplicate it before the reply is released.
  int dupfd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
  if (dupfd < 0) {
    BErrNo be;
    warn_and_return(be.bstrerror());
    return sleep_prevention.release();
  }

  sleep_prevention->inhibitor_fd = dupfd;
  return sleep_prevention.release();
}

void DeactivateSleepPrevention(SleepPrevention* sleep_prevention)
{
  if (!sleep_prevention) { return; }

  if (sleep_prevention->inhibitor_fd >= 0) {
    close(sleep_prevention->inhibitor_fd);
  }
  delete sleep_prevention;
}

}  // namespace filedaemon
