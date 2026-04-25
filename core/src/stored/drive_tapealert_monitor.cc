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

#include "include/bareos.h"

#include "stored/drive_tapealert_monitor.h"

#include <ctime>

#include "lib/scsi_tapealert.h"
#include "lib/scsi_tapealert_flags.h"
#include "stored/device_control_record.h"
#include "stored/device_resource.h"
#include "stored/sd_stats.h"
#include "stored/stored.h"

namespace storagedaemon {

namespace {

static pthread_mutex_t tapealert_operation_mutex = PTHREAD_MUTEX_INITIALIZER;

void DispatchDriveTapealertMessages(::JobControlRecord* jcr,
                                    const char* device,
                                    const char* volume,
                                    uint64_t flags)
{
  constexpr const char* job_msg
      = "TapeAlert on device \"%s\" with volume \"%s\": [%d] %s\n%s\n"
        "Possible cause: %s\n";
  constexpr const char* job_msg_novol
      = "TapeAlert on device \"%s\": [%d] %s\n%s\n"
        "Possible cause: %s\n";

  constexpr const char* print_msg
      = "TapeAlert on device \"%s\" with volume \"%s\" from jobid %lu: "
        "[%d] %s\n";
  constexpr const char* print_msg_novol
      = "TapeAlert on device \"%s\" from jobid %lu: [%d] %s\n";

  const uint64_t jobid{jcr ? jcr->JobId : 0};

  for (auto& flag : scsitapealert::flags) {
    if (!flag.present_in(flags)) { continue; }

    if (volume && volume[0] != '\0') {
      Jmsg(jcr, flag.type, 0, job_msg, device, volume, flag.no, flag.name,
           flag.message, flag.cause);
      Pmsg0(-1, print_msg, device, volume, jobid, flag.no, flag.name);
    } else {
      Jmsg(jcr, flag.type, 0, job_msg_novol, device, flag.no, flag.name,
           flag.message, flag.cause);
      Pmsg0(-1, print_msg_novol, device, jobid, flag.no, flag.name);
    }
  }
}

DeviceResource* GetTapealertDeviceResource(DeviceControlRecord* dcr)
{
  if (!dcr) { return nullptr; }
  if (dcr->device_resource) { return dcr->device_resource; }
  if (dcr->dev) { return dcr->dev->device_resource; }
  return nullptr;
}

}  // namespace

bool IsDriveTapealertPollingEvent(bSdEventType event_type)
{
  switch (event_type) {
    case bSdEventDeviceInit:
    case bSdEventVolumeLoad:
    case bSdEventLabelVerified:
    case bSdEventLabelWrite:
    case bSdEventReadError:
    case bSdEventWriteError:
    case bSdEventVolumeUnload:
    case bSdEventDeviceRelease:
    case bSdEventDeviceOpen:
    case bSdEventDeviceClose:
      return true;
    default:
      return false;
  }
}

void HandleDriveTapealertEvent(::JobControlRecord* jcr,
                               bSdEventType event_type,
                               void* value)
{
  if (!jcr || !value || !IsDriveTapealertPollingEvent(event_type)) { return; }

  auto* dcr = static_cast<DeviceControlRecord*>(value);
  if (!dcr || !dcr->dev) { return; }

  auto* device_resource = GetTapealertDeviceResource(dcr);
  if (!device_resource || !device_resource->drive_tapealert_enabled) { return; }

  const char* device_name = dcr->dev->archive_device_string;
  if (!device_name || device_name[0] == '\0') {
    device_name = device_resource->archive_device_string;
  }
  if (!device_name || device_name[0] == '\0') { return; }

  uint64_t flags = 0;

  Dmsg1(200, "sd: checking for tapealerts on device %s\n", device_name);
  lock_mutex(tapealert_operation_mutex);
  bool success = GetTapealertFlags(dcr->dev->fd, device_name, &flags);
  unlock_mutex(tapealert_operation_mutex);
  Dmsg1(200, "sd: tapealert check on device %s done\n", device_name);

  if (!success || flags == 0) { return; }

  UpdateDeviceTapealert(device_resource->resource_name_, flags,
                        static_cast<utime_t>(time(NULL)));
  DispatchDriveTapealertMessages(jcr, device_resource->resource_name_,
                                 dcr->dev->getVolCatName(), flags);
}

}  // namespace storagedaemon
