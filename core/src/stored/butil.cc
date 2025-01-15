/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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
// Kern Sibbald, MM
/**
 * @file
 * Utility routines for "tool" programs such as bscan, bls,
 * bextract, ...  Some routines also used by Bareos.
 *
 * Normally nothing in this file is called by the Storage
 * daemon because we interact more directly with the user
 * i.e. printf, ...
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/device.h"
#include "stored/device_control_record.h"
#include "stored/bsr.h"
#include "stored/stored_jcr_impl.h"
#include "lib/parse_bsr.h"
#include "lib/parse_conf.h"
#include "include/jcr.h"

namespace storagedaemon {

/* Forward referenced functions */
static bool setup_to_access_device(DeviceControlRecord* dcr,
                                   JobControlRecord* jcr,
                                   char* dev_name,
                                   const std::string& VolumeName,
                                   bool readonly);
static DeviceResource* find_device_res(char* archive_device_string,
                                       bool readonly);
static void MyFreeJcr(JobControlRecord* jcr);


JobControlRecord* SetupDummyJcr(const char* name,
                                BootStrapRecord* bsr,
                                DirectorResource* director)
{
  JobControlRecord* jcr = new_jcr(&MyFreeJcr);
  jcr->sd_impl = new StoredJcrImpl;
  register_jcr(jcr);

  jcr->sd_impl->read_session.bsr = bsr;
  jcr->sd_impl->director = director;
  jcr->VolSessionId = 1;
  jcr->VolSessionTime = (uint32_t)time(NULL);
  jcr->sd_impl->NumReadVolumes = 0;
  jcr->sd_impl->NumWriteVolumes = 0;
  jcr->JobId = 0;
  jcr->setJobType(JT_CONSOLE);
  jcr->setJobLevel(L_FULL);
  jcr->setJobStatus(JS_Terminated);
  jcr->where = strdup("");
  jcr->sd_impl->job_name = GetPoolMemory(PM_FNAME);
  PmStrcpy(jcr->sd_impl->job_name, "Dummy.Job.Name");
  jcr->client_name = GetPoolMemory(PM_FNAME);
  PmStrcpy(jcr->client_name, "Dummy.Client.Name");
  bstrncpy(jcr->Job, name, sizeof(jcr->Job));
  jcr->sd_impl->fileset_name = GetPoolMemory(PM_FNAME);
  PmStrcpy(jcr->sd_impl->fileset_name, "Dummy.fileset.name");
  jcr->sd_impl->fileset_md5 = GetPoolMemory(PM_FNAME);
  PmStrcpy(jcr->sd_impl->fileset_md5, "Dummy.fileset.md5");

  NewPlugins(jcr); /* instantiate plugins */

  return jcr;
}


/**
 * Setup a "daemon" JobControlRecord for the various standalone tools (e.g. bls,
 * bextract, bscan, ...)
 */
JobControlRecord* SetupJcr(const char* name,
                           char* dev_name,
                           BootStrapRecord* bsr,
                           DirectorResource* director,
                           DeviceControlRecord* dcr,
                           const std::string& VolumeName,
                           bool readonly)
{
  JobControlRecord* jcr = SetupDummyJcr(name, bsr, director);

  InitAutochangers();
  CreateVolumeLists();

  if (!setup_to_access_device(dcr, jcr, dev_name, VolumeName, readonly)) {
    return NULL;
  }

  if (!bsr && !VolumeName.empty()) {
    bstrncpy(dcr->VolumeName, VolumeName.c_str(), sizeof(dcr->VolumeName));
  }

  bstrncpy(dcr->pool_name, "Default", sizeof(dcr->pool_name));
  bstrncpy(dcr->pool_type, "Backup", sizeof(dcr->pool_type));

  return jcr;
}

std::string AvailableDevicesListing() {
  std::vector<std::string> devices;
  for (BareosResource* resource = nullptr;
        (resource = my_config->GetNextRes(R_DEVICE, resource));) {
    DeviceResource* device = dynamic_cast<DeviceResource*>(resource);
    std::string device_str;
    device_str += " \"";
    device_str += device->resource_name_;
    device_str += "\" (";
    device_str += device->archive_device_string;
    device_str += ")\n";
    devices.emplace_back(std::move(device_str));
  }
  std::sort(devices.begin(), devices.end());
  std::string devices_str = "Available Devices:\n";
  for (const std::string& device : devices) {
    devices_str += device;
  }
  return devices_str;
}
/**
 * Setup device, jcr, and prepare to access device.
 *   If the caller wants read access, acquire the device, otherwise,
 *     the caller will do it.
 */
static bool setup_to_access_device(DeviceControlRecord* dcr,
                                   JobControlRecord* jcr,
                                   char* dev_name,
                                   const std::string& VolumeName,
                                   bool readonly)
{
  Device* dev;
  char* p;
  DeviceResource* device_resource;
  char VolName[MAX_NAME_LENGTH];

  InitReservationsLock();

  /* If no volume name already given and no bsr, and it is a file,
   * try getting name from Filename */
  if (!VolumeName.empty()) {
    bstrncpy(VolName, VolumeName.c_str(), sizeof(VolName));
    if (VolumeName.size() >= MAX_NAME_LENGTH) {
      /* We do not handle this case gracefully, so its best to just
       * abort the job here. */
      Jmsg0(jcr, M_FATAL, 0,
            T_("Volume name or names is too long. Please use a .bsr file.\n"));
      return false;
    }
  } else {
    VolName[0] = 0;
  }

  if (!jcr->sd_impl->read_session.bsr && VolName[0] == 0) {
    if (!bstrncmp(dev_name, "/dev/", 5)) {
      /* Try stripping file part */
      p = dev_name + strlen(dev_name);

      while (p > dev_name && !IsPathSeparator(*p)) p--;
      if (IsPathSeparator(*p)) {
        bstrncpy(VolName, p + 1, sizeof(VolName));
        *p = 0;
      }
    }
  }

  if ((device_resource = find_device_res(dev_name, readonly)) == NULL) {
    std::vector<std::string> devices;
    for (BareosResource* resource = nullptr;
         (resource = my_config->GetNextRes(R_DEVICE, resource));) {
      DeviceResource* device = dynamic_cast<DeviceResource*>(resource);
      std::string device_str;
      device_str += " \"";
      device_str += device->resource_name_;
      device_str += "\" (";
      device_str += device->archive_device_string;
      device_str += ")\n";
      devices.emplace_back(std::move(device_str));
    }
    std::sort(devices.begin(), devices.end());
    std::string devices_str;
    for (const std::string& device : devices) {
      devices_str += device;
    }
    Jmsg2(jcr, M_FATAL, 0,
          T_("Cannot find device \"%s\" in config file %s.\n%s"),
          dev_name, configfile, AvailableDevicesListing().c_str());
    return false;
  }

  dev = FactoryCreateDevice(jcr, device_resource);
  if (!dev) {
    Jmsg1(jcr, M_FATAL, 0, T_("Cannot init device %s\n"), dev_name);
    return false;
  }
  device_resource->dev = dev;
  jcr->sd_impl->dcr = dcr;
  SetupNewDcrDevice(jcr, dcr, dev, NULL);
  if (!readonly) { dcr->SetWillWrite(); }

  if (VolName[0]) {
    bstrncpy(dcr->VolumeName, VolName, sizeof(dcr->VolumeName));
  }
  bstrncpy(dcr->dev_name, device_resource->archive_device_string,
           sizeof(dcr->dev_name));

  CreateRestoreVolumeList(jcr);

  if (readonly) { /* read only access? */
    Dmsg0(100, "Acquire device for read\n");
    if (!AcquireDeviceForRead(dcr)) { return false; }
    jcr->sd_impl->read_dcr = dcr;
  } else {
    if (!FirstOpenDevice(dcr)) {
      Jmsg1(jcr, M_FATAL, 0, T_("Cannot open %s\n"), dev->print_name());
      return false;
    }
  }

  return true;
}

/**
 * Called here when freeing JobControlRecord so that we can get rid
 *  of "daemon" specific memory allocated.
 */
static void MyFreeJcr(JobControlRecord* jcr)
{
  if (jcr->sd_impl->job_name) {
    FreePoolMemory(jcr->sd_impl->job_name);
    jcr->sd_impl->job_name = NULL;
  }

  if (jcr->client_name) {
    FreePoolMemory(jcr->client_name);
    jcr->client_name = NULL;
  }

  if (jcr->sd_impl->fileset_name) {
    FreePoolMemory(jcr->sd_impl->fileset_name);
    jcr->sd_impl->fileset_name = NULL;
  }

  if (jcr->sd_impl->fileset_md5) {
    FreePoolMemory(jcr->sd_impl->fileset_md5);
    jcr->sd_impl->fileset_md5 = NULL;
  }

  if (jcr->comment) {
    FreePoolMemory(jcr->comment);
    jcr->comment = NULL;
  }

  if (jcr->sd_impl->VolList) { FreeRestoreVolumeList(jcr); }

  if (jcr->sd_impl->dcr) {
    FreeDeviceControlRecord(jcr->sd_impl->dcr);
    jcr->sd_impl->dcr = NULL;
  }

  if (jcr->sd_impl) {
    delete jcr->sd_impl;
    jcr->sd_impl = nullptr;
  }

  return;
}

/**
 * Search for device resource that corresponds to
 * device name on command line (or default).
 *
 * Returns: NULL on failure
 *          Device resource pointer on success
 */
static DeviceResource* find_device_res(char* archive_device_string,
                                       bool readonly)
{
  bool found = false;
  DeviceResource* device_resource;

  Dmsg0(900, "Enter find_device_res\n");
  foreach_res (device_resource, R_DEVICE) {
    Dmsg2(900, "Compare %s and %s\n", device_resource->archive_device_string,
          archive_device_string);
    if (bstrcmp(device_resource->archive_device_string,
                archive_device_string)) {
      found = true;
      break;
    }
  }

  if (!found) {
    /* Search for name of Device resource rather than archive name */
    if (archive_device_string[0] == '"') {
      int len = strlen(archive_device_string);
      bstrncpy(archive_device_string, archive_device_string + 1, len + 1);
      len--;
      if (len > 0) { archive_device_string[len - 1] = 0; /* zap trailing " */ }
    }
    foreach_res (device_resource, R_DEVICE) {
      Dmsg2(900, "Compare %s and %s\n", device_resource->resource_name_,
            archive_device_string);
      if (bstrcmp(device_resource->resource_name_, archive_device_string)) {
        found = true;
        break;
      }
    }
  }

  if (!found) {
    Pmsg2(0, T_("Could not find device \"%s\" in config file %s.\n"),
          archive_device_string, configfile);
    return NULL;
  }

  if (readonly) {
    Pmsg1(0, T_("Using device: \"%s\" for reading.\n"), archive_device_string);
  } else {
    Pmsg1(0, T_("Using device: \"%s\" for writing.\n"), archive_device_string);
  }

  return device_resource;
}

// Device got an error, attempt to analyse it
void DisplayTapeErrorStatus(JobControlRecord* jcr, Device* dev)
{
  char* status;

  status = dev->StatusDev();

  if (BitIsSet(BMT_EOD, status))
    Jmsg(jcr, M_ERROR, 0, T_("Unexpected End of Data\n"));
  else if (BitIsSet(BMT_EOT, status))
    Jmsg(jcr, M_ERROR, 0, T_("Unexpected End of Tape\n"));
  else if (BitIsSet(BMT_EOF, status))
    Jmsg(jcr, M_ERROR, 0, T_("Unexpected End of File\n"));
  else if (BitIsSet(BMT_DR_OPEN, status))
    Jmsg(jcr, M_ERROR, 0, T_("Tape Door is Open\n"));
  else if (!BitIsSet(BMT_ONLINE, status))
    Jmsg(jcr, M_ERROR, 0, T_("Unexpected Tape is Off-line\n"));

  free(status);
}

} /* namespace storagedaemon */
