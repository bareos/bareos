/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, May MMIII
/**
 * @file
 * This file handles the status command
 */

#include "include/bareos.h"
#include "stored/device_status_information.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/spool.h"
#include "stored/status.h"
#include "lib/status_packet.h"
#include "lib/edit.h"
#include "include/jcr.h"
#include "lib/parse_conf.h"
#include "lib/bsock.h"
#include "lib/recent_job_results_list.h"
#include "lib/util.h"
#include "lib/version.h"

namespace storagedaemon {

/* Imported variables */
extern void* start_heap;

/* Static variables */
static char statuscmd[] = "status %s\n";
static char dotstatuscmd[] = ".status %127s\n";

static char OKdotstatus[] = "3000 OK .status\n";
static char DotStatusJob[] = "JobId=%d JobStatus=%c JobErrors=%d\n";

/* Forward referenced functions */
static void SendBlockedStatus(Device* dev, StatusPacket* sp);
static void SendDeviceStatus(Device* dev, StatusPacket* sp);
static void ListTerminatedJobs(StatusPacket* sp);
static void ListRunningJobs(StatusPacket* sp);
static void ListJobsWaitingOnReservation(StatusPacket* sp);
static void ListStatusHeader(StatusPacket* sp);
static void ListDevices(JobControlRecord* jcr,
                        StatusPacket* sp,
                        const char* devicenames);
static void ListVolumes(StatusPacket* sp, const char* devicenames);

static const char* JobLevelToString(int level);

// Status command from Director
static void OutputStatus(JobControlRecord* jcr,
                         StatusPacket* sp,
                         const char* devicenames)
{
  int len;
  PoolMem msg(PM_MESSAGE);

  ListStatusHeader(sp);
  ListRunningJobs(sp);
  ListJobsWaitingOnReservation(sp);
  ListTerminatedJobs(sp);
  ListDevices(jcr, sp, devicenames);

  if (!sp->api) {
    len = Mmsg(msg, T_("Used Volume status:\n"));
    sp->send(msg, len);
  }

  ListVolumes(sp, devicenames);
  if (!sp->api) {
    len = PmStrcpy(msg, "====\n\n");
    sp->send(msg, len);
  }

  ListSpoolStats(sp);
  if (!sp->api) {
    len = PmStrcpy(msg, "====\n\n");
    sp->send(msg, len);
  }
}

static bool NeedToListDevice(const char* devicenames, const char* devicename)
{
  char *cur, *bp;
  PoolMem namelist;

  Dmsg2(200, "NeedToListDevice devicenames %s, devicename %s\n", devicenames,
        devicename);

  // Make a local copy that we can split on ','
  PmStrcpy(namelist, devicenames);

  // See if devicename is in the list.
  cur = namelist.c_str();
  while (cur) {
    bp = strchr(cur, ',');
    if (bp) { *bp++ = '\0'; }

    if (Bstrcasecmp(cur, devicename)) { return true; }

    cur = bp;
  }

  Dmsg0(200, "NeedToListDevice no listing needed\n");

  return false;
}

static bool NeedToListDevice(const char* devicenames,
                             DeviceResource* device_resource)
{
  /* See if we are requested to list an explicit device name.
   * e.g. this happens when people address one particular device in
   * a autochanger via its own storage definition or an non autochanger device.
   */
  if (!NeedToListDevice(devicenames, device_resource->resource_name_)) {
    // See if this device is part of an autochanger.
    if (device_resource->changer_res) {
      /* See if we need to list this particular device part of the given
       * autochanger. */
      if (!NeedToListDevice(devicenames,
                            device_resource->changer_res->resource_name_)) {
        return false;
      }
    } else {
      return false;
    }
  }

  return true;
}

/*
 * Trigger the specific eventtype to get status information from any plugin that
 * registered the event to return specific device information.
 */
static void trigger_device_status_hook(JobControlRecord* jcr,
                                       DeviceResource* device_resource,
                                       StatusPacket* sp,
                                       bSdEventType eventType)
{
  DeviceStatusInformation dst;

  dst.device_resource = device_resource;
  dst.status = GetPoolMemory(PM_MESSAGE);
  dst.status_length = 0;

  if (GeneratePluginEvent(jcr, eventType, &dst) == bRC_OK) {
    if (dst.status_length > 0) { sp->send(dst.status, dst.status_length); }
  }
  FreePoolMemory(dst.status);
}

/*
 * Ask the device_resource if it want to log something specific in the status
 * overview.
 */
static void get_device_specific_status(DeviceResource* device_resource,
                                       StatusPacket* sp)
{
  DeviceStatusInformation dst;

  dst.device_resource = device_resource;
  dst.status = GetPoolMemory(PM_MESSAGE);
  dst.status_length = 0;

  if (device_resource && device_resource->dev
      && device_resource->dev->DeviceStatus(&dst)) {
    if (dst.status_length > 0) { sp->send(dst.status, dst.status_length); }
  }
  FreePoolMemory(dst.status);
}

static void ListDevices(JobControlRecord* jcr,
                        StatusPacket* sp,
                        const char* devicenames)
{
  int len;
  int bpb;
  Device* dev;
  AutochangerResource* changer;
  PoolMem msg(PM_MESSAGE);
  char b1[35], b2[35], b3[35];

  if (!sp->api) {
    len = Mmsg(msg, T_("\nDevice status:\n"));
    sp->send(msg, len);
  }

  foreach_res (changer, R_AUTOCHANGER) {
    // See if we need to list this autochanger.
    if (devicenames
        && !NeedToListDevice(devicenames, changer->resource_name_)) {
      continue;
    }

    len = Mmsg(msg, T_("Autochanger \"%s\" with devices:\n"),
               changer->resource_name_);
    sp->send(msg, len);

    if (changer->device_resources) {
      for (auto* device_resource : *changer->device_resources) {
        if (device_resource->dev) {
          len = Mmsg(msg, "   %s\n", device_resource->dev->print_name());
          sp->send(msg, len);
        } else {
          len = Mmsg(msg, "   %s\n", device_resource->resource_name_);
          sp->send(msg, len);
        }
      }
    }
  }

  DeviceResource* device_resource = nullptr;
  foreach_res (device_resource, R_DEVICE) {
    if (devicenames && !NeedToListDevice(devicenames, device_resource)) {
      continue;
    }

    dev = device_resource->dev;
    if (dev && dev->IsOpen()) {
      if (dev->IsLabeled()) {
        const char* state;

        switch (dev->blocked()) {
          case BST_NOT_BLOCKED:
          case BST_DESPOOLING:
          case BST_RELEASING:
            state = T_("mounted with");
            break;
          case BST_MOUNT:
            state = T_("waiting for");
            break;
          case BST_WRITING_LABEL:
            state = T_("being labeled with");
            break;
          case BST_DOING_ACQUIRE:
            state = T_("being acquired with");
            break;
          case BST_UNMOUNTED:
          case BST_WAITING_FOR_SYSOP:
          case BST_UNMOUNTED_WAITING_FOR_SYSOP:
            state = T_("waiting for sysop intervention");
            break;
          default:
            state = T_("unknown state");
            break;
        }

        len = Mmsg(msg,
                   T_("\nDevice %s is %s:\n"
                      "    Volume:      %s\n"
                      "    Pool:        %s\n"
                      "    Media type:  %s\n"),
                   dev->print_name(), state, dev->VolHdr.VolumeName,
                   dev->pool_name[0] ? dev->pool_name : "*unknown*",
                   dev->device_resource->media_type);
        sp->send(msg, len);
      } else {
        len = Mmsg(
            msg,
            T_("\nDevice %s open but no Bareos volume is currently mounted.\n"),
            dev->print_name());
        sp->send(msg, len);
      }

      get_device_specific_status(device_resource, sp);
      trigger_device_status_hook(jcr, device_resource, sp, bSdEventDriveStatus);

      SendBlockedStatus(dev, sp);

      if (dev->CanAppend()) {
        bpb = dev->VolCatInfo.VolCatBlocks;
        if (bpb <= 0) { bpb = 1; }
        bpb = dev->VolCatInfo.VolCatBytes / bpb;
        len = Mmsg(msg, T_("    Total Bytes=%s Blocks=%s Bytes/block=%s\n"),
                   edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, b1),
                   edit_uint64_with_commas(dev->VolCatInfo.VolCatBlocks, b2),
                   edit_uint64_with_commas(bpb, b3));
        sp->send(msg, len);
      } else { /* reading */
        bpb = dev->VolCatInfo.VolCatReads;
        if (bpb <= 0) { bpb = 1; }
        if (dev->VolCatInfo.VolCatRBytes > 0) {
          bpb = dev->VolCatInfo.VolCatRBytes / bpb;
        } else {
          bpb = 0;
        }
        len = Mmsg(
            msg, T_("    Total Bytes Read=%s Blocks Read=%s Bytes/block=%s\n"),
            edit_uint64_with_commas(dev->VolCatInfo.VolCatRBytes, b1),
            edit_uint64_with_commas(dev->VolCatInfo.VolCatReads, b2),
            edit_uint64_with_commas(bpb, b3));
        sp->send(msg, len);
      }

      len = Mmsg(msg, T_("    Positioned at File=%s Block=%s\n"),
                 edit_uint64_with_commas(dev->file, b1),
                 edit_uint64_with_commas(dev->block_num, b2));
      sp->send(msg, len);

      trigger_device_status_hook(jcr, device_resource, sp,
                                 bSdEventVolumeStatus);
    } else {
      if (dev) {
        len = Mmsg(msg, T_("\nDevice %s is not open.\n"), dev->print_name());
        sp->send(msg, len);
        SendBlockedStatus(dev, sp);
      } else {
        len = Mmsg(msg, T_("\nDevice \"%s\" is not open or does not exist.\n"),
                   device_resource->resource_name_);
        sp->send(msg, len);
      }

      get_device_specific_status(device_resource, sp);
    }

    if (!sp->api) {
      len = PmStrcpy(msg, "==\n");
      sp->send(msg, len);
    }
  }

  if (!sp->api) {
    len = PmStrcpy(msg, "====\n\n");
    sp->send(msg, len);
  }
}

// List Volumes
static void ListVolumes(StatusPacket* sp, const char* devicenames)
{
  int len;
  VolumeReservationItem* vol;
  PoolMem msg(PM_MESSAGE);

  foreach_vol (vol) {
    Device* dev = vol->dev;

    if (dev) {
      if (devicenames && !NeedToListDevice(devicenames, dev->device_resource)) {
        continue;
      }

      len = Mmsg(msg, "%s on device %s\n", vol->vol_name, dev->print_name());
      sp->send(msg, len);
      len = Mmsg(msg, "    Reader=%d writers=%d reserves=%d volinuse=%d\n",
                 dev->CanRead() ? 1 : 0, dev->num_writers, dev->NumReserved(),
                 vol->IsInUse());
      sp->send(msg, len);
    } else {
      len = Mmsg(msg, "Volume %s no device. volinuse= %d\n", vol->vol_name,
                 vol->IsInUse());
      sp->send(msg, len);
    }
  }
  endeach_vol(vol);

  foreach_read_vol(vol)
  {
    Device* dev = vol->dev;

    if (dev) {
      if (devicenames && !NeedToListDevice(devicenames, dev->device_resource)) {
        continue;
      }

      len = Mmsg(msg, "Read volume: %s on device %s\n", vol->vol_name,
                 dev->print_name());
      sp->send(msg, len);
      len = Mmsg(msg,
                 "    Reader=%d writers=%d reserves=%d volinuse=%d JobId=%d\n",
                 dev->CanRead() ? 1 : 0, dev->num_writers, dev->NumReserved(),
                 vol->IsInUse(), vol->GetJobid());
      sp->send(msg, len);
    } else {
      len = Mmsg(msg, "Read Volume: %s no device. volinuse= %d\n",
                 vol->vol_name, vol->IsInUse());
      sp->send(msg, len);
    }
  }
  endeach_read_vol(vol);
}

static void ListStatusHeader(StatusPacket* sp)
{
  int len;
  PoolMem msg(PM_MESSAGE);
  char dt[MAX_TIME_LENGTH];
  char b1[35];

  len = Mmsg(msg, T_("%s Version: %s (%s) %s \n"), my_name,
             kBareosVersionStrings.Full, kBareosVersionStrings.Date,
             kBareosVersionStrings.GetOsInfo());
  sp->send(msg, len);

  bstrftime_nc(dt, sizeof(dt), daemon_start_time);

  len = Mmsg(msg,
             T_("Daemon started %s. Jobs: run=%zu, running=%d, %s binary\n"),
             dt, NumJobsRun(), JobCount(), kBareosVersionStrings.BinaryInfo);
  sp->send(msg, len);

#if defined(HAVE_WIN32)
  if (debug_level > 0) {
    len = Mmsg(msg, "APIs=%sOPT,%sATP,%sLPV,%sCFA,%sCFW,\n",
               p_OpenProcessToken ? "" : "!",
               p_AdjustTokenPrivileges ? "" : "!",
               p_LookupPrivilegeValue ? "" : "!", p_CreateFileA ? "" : "!",
               p_CreateFileW ? "" : "!");
    sp->send(msg, len);
    len = Mmsg(msg,
               " %sWUL,%sWMKD,%sGFAA,%sGFAW,%sGFAEA,%sGFAEW,%sSFAA,%sSFAW,%sBR,"
               "%sBW,%sSPSP,\n",
               p_wunlink ? "" : "!", p_wmkdir ? "" : "!",
               p_GetFileAttributesA ? "" : "!", p_GetFileAttributesW ? "" : "!",
               p_GetFileAttributesExA ? "" : "!",
               p_GetFileAttributesExW ? "" : "!",
               p_SetFileAttributesA ? "" : "!", p_SetFileAttributesW ? "" : "!",
               p_BackupRead ? "" : "!", p_BackupWrite ? "" : "!",
               p_SetProcessShutdownParameters ? "" : "!");
    sp->send(msg, len);
    len = Mmsg(
        msg, " %sWC2MB,%sMB2WC,%sFFFA,%sFFFW,%sFNFA,%sFNFW,%sSCDA,%sSCDW,\n",
        p_WideCharToMultiByte ? "" : "!", p_MultiByteToWideChar ? "" : "!",
        p_FindFirstFileA ? "" : "!", p_FindFirstFileW ? "" : "!",
        p_FindNextFileA ? "" : "!", p_FindNextFileW ? "" : "!",
        p_SetCurrentDirectoryA ? "" : "!", p_SetCurrentDirectoryW ? "" : "!");
    sp->send(msg, len);
    len = Mmsg(msg, " %sGCDA,%sGCDW,%sGVPNW,%sGVNFVMPW\n",
               p_GetCurrentDirectoryA ? "" : "!",
               p_GetCurrentDirectoryW ? "" : "!",
               p_GetVolumePathNameW ? "" : "!",
               p_GetVolumeNameForVolumeMountPointW ? "" : "!");
    sp->send(msg, len);
  }
#endif

  len = Mmsg(msg,
             " Sizes: boffset_t=%d size_t=%d int32_t=%d int64_t=%d "
             "bwlimit=%skB/s\n",
             (int)sizeof(boffset_t), (int)sizeof(size_t), (int)sizeof(int32_t),
             (int)sizeof(int64_t),
             edit_uint64_with_commas(me->max_bandwidth_per_job / 1024, b1));
  sp->send(msg, len);


  if (me->secure_erase_cmdline) {
    len = Mmsg(msg, T_(" secure erase command='%s'\n"),
               me->secure_erase_cmdline);
    sp->send(msg, len);
  }

  len = ListSdPlugins(msg);
  if (len > 0) { sp->send(msg, len); }

  if (my_config->HasWarnings()) {
    sp->send(
        "\n"
        "There are WARNINGS for this storagedaemon's configuration!\n"
        "See output of 'bareos-sd -t' for details.\n");
  }
}

static void SendBlockedStatus(Device* dev, StatusPacket* sp)
{
  int len;
  PoolMem msg(PM_MESSAGE);

  if (!dev) {
    len = Mmsg(msg, T_("No Device structure.\n\n"));
    sp->send(msg, len);
    return;
  }
  switch (dev->blocked()) {
    case BST_UNMOUNTED:
      len = Mmsg(msg, T_("    Device is BLOCKED. User unmounted.\n"));
      sp->send(msg, len);
      break;
    case BST_UNMOUNTED_WAITING_FOR_SYSOP:
      len = Mmsg(msg,
                 T_("    Device is BLOCKED. User unmounted during wait for "
                    "media/mount.\n"));
      sp->send(msg, len);
      break;
    case BST_WAITING_FOR_SYSOP: {
      bool found_jcr = false;
      dev->Lock();
      for (auto dcr : dev->attached_dcrs) {
        if (dcr->jcr->getJobStatus() == JS_WaitMount) {
          len = Mmsg(
              msg,
              T_("    Device is BLOCKED waiting for mount of volume \"%s\",\n"
                 "       Pool:        %s\n"
                 "       Media type:  %s\n"),
              dcr->VolumeName, dcr->pool_name, dcr->media_type);
          sp->send(msg, len);
          found_jcr = true;
        } else if (dcr->jcr->getJobStatus() == JS_WaitMedia) {
          len = Mmsg(
              msg,
              T_("    Device is BLOCKED waiting to create a volume for:\n"
                 "       Pool:        %s\n"
                 "       Media type:  %s\n"),
              dcr->pool_name, dcr->media_type);
          sp->send(msg, len);
          found_jcr = true;
        }
      }
      dev->Unlock();
      if (!found_jcr) {
        len = Mmsg(msg, T_("    Device is BLOCKED waiting for media.\n"));
        sp->send(msg, len);
      }
    } break;
    case BST_DOING_ACQUIRE:
      len = Mmsg(msg, T_("    Device is being initialized.\n"));
      sp->send(msg, len);
      break;
    case BST_WRITING_LABEL:
      len = Mmsg(msg, T_("    Device is blocked labeling a Volume.\n"));
      sp->send(msg, len);
      break;
    default:
      break;
  }

  // Send autochanger slot status
  if (dev->AttachedToAutochanger()) {
    if (dev->GetSlot() > 0) {
      len = Mmsg(msg, T_("    Slot %hd %s loaded in drive %hd.\n"),
                 dev->GetSlot(), dev->IsOpen() ? "is" : "was last", dev->drive);
      sp->send(msg, len);
    } else if (dev->GetSlot() <= 0) {
      len = Mmsg(msg, T_("    Drive %hd is not loaded.\n"), dev->drive);
      sp->send(msg, len);
    }
  }
  if (debug_level > 1) { SendDeviceStatus(dev, sp); }
}

static void SendDeviceStatus(Device* dev, StatusPacket* sp)
{
  int len;
  bool found = false;
  PoolMem msg(PM_MESSAGE);

  if (debug_level > 5) {
    len = Mmsg(msg, T_("Configured device capabilities:\n"));
    sp->send(msg, len);

    len = Mmsg(
        msg,
        "  %sEOF %sBSR %sBSF %sFSR %sFSF %sEOM %sREM %sRACCESS %sAUTOMOUNT "
        "%sLABEL %sANONVOLS %sALWAYSOPEN\n",
        dev->HasCap(CAP_EOF) ? "" : "!", dev->HasCap(CAP_BSR) ? "" : "!",
        dev->HasCap(CAP_BSF) ? "" : "!", dev->HasCap(CAP_FSR) ? "" : "!",
        dev->HasCap(CAP_FSF) ? "" : "!", dev->HasCap(CAP_EOM) ? "" : "!",
        dev->HasCap(CAP_REM) ? "" : "!", dev->HasCap(CAP_RACCESS) ? "" : "!",
        dev->HasCap(CAP_AUTOMOUNT) ? "" : "!",
        dev->HasCap(CAP_LABEL) ? "" : "!", dev->HasCap(CAP_ANONVOLS) ? "" : "!",
        dev->HasCap(CAP_ALWAYSOPEN) ? "" : "!");
    sp->send(msg, len);
  }

  len = Mmsg(msg, T_("Device state:\n"));
  sp->send(msg, len);

  len = Mmsg(msg,
             "  %sOPENED %sLABEL %sMALLOC %sAPPEND %sREAD %sEOT %sWEOT %sEOF "
             "%sNEXTVOL %sSHORT %sMOUNTED\n",
             dev->IsOpen() ? "" : "!", dev->IsLabeled() ? "" : "!",
             BitIsSet(ST_ALLOCATED, dev->state) ? "" : "!",
             dev->CanAppend() ? "" : "!", dev->CanRead() ? "" : "!",
             dev->AtEot() ? "" : "!", BitIsSet(ST_WEOT, dev->state) ? "" : "!",
             dev->AtEof() ? "" : "!",
             BitIsSet(ST_NEXTVOL, dev->state) ? "" : "!",
             BitIsSet(ST_SHORT, dev->state) ? "" : "!",
             BitIsSet(ST_MOUNTED, dev->state) ? "" : "!");
  sp->send(msg, len);

  len = Mmsg(msg, T_("  num_writers=%d reserves=%d block=%d\n"),
             dev->num_writers, dev->NumReserved(), dev->blocked());
  sp->send(msg, len);

  len = Mmsg(msg, T_("Attached Jobs: "));
  sp->send(msg, len);
  dev->Lock();
  for (auto dcr : dev->attached_dcrs) {
    if (dcr->jcr) {
      if (found) {
        len = Mmsg(msg, ",%d", (int)dcr->jcr->JobId);
      } else {
        len = Mmsg(msg, "%d", (int)dcr->jcr->JobId);
      }
      sp->send(msg, len);
      found = true;
    }
  }
  dev->Unlock();
  sp->send("\n");

  len = Mmsg(msg, T_("Device parameters:\n"));
  sp->send(msg, len);
  len = Mmsg(msg, T_("  Archive name: %s\nDevice name: %s\nDevice Type: %s\n"),
             dev->archive_name(), dev->name(), dev->type().c_str());
  sp->send(msg, len);
  len = Mmsg(msg, T_("  File=%u block=%u\n"), dev->file, dev->block_num);
  sp->send(msg, len);
  len = Mmsg(msg, T_("  Min block=%u Max block=%u\n"), dev->min_block_size,
             dev->max_block_size);
  sp->send(msg, len);
  len = Mmsg(msg, T_("  Auto Select=%s\n"), dev->autoselect ? "yes" : "no");
  sp->send(msg, len);
}

static void ListRunningJobs(StatusPacket* sp)
{
  JobControlRecord* jcr;
  DeviceControlRecord *dcr, *rdcr;
  bool found = false;
  PoolMem msg(PM_MESSAGE);
  int len;
  char JobName[MAX_NAME_LENGTH];
  char b1[50], b2[50], b3[50], b4[50];

  if (!sp->api) {
    len = Mmsg(msg, T_("\nJob inventory:\n\n"));
    sp->send(msg, len);
  }

  foreach_jcr (jcr) {
    if (jcr->getJobStatus() == JS_WaitFD) {
      len = Mmsg(msg, T_("%s Job %s waiting for Client connection.\n"),
                 job_type_to_str(jcr->getJobType()), jcr->Job);
      sp->send(msg, len);
    }
    dcr = jcr->sd_impl->dcr;
    rdcr = jcr->sd_impl->read_dcr;
    if ((dcr && dcr->device_resource) || (rdcr && rdcr->device_resource)) {
      bstrncpy(JobName, jcr->Job, sizeof(JobName));
      /* There are three periods after the Job name */
      char* p;
      for (int i = 0; i < 3; i++) {
        if ((p = strrchr(JobName, '.')) != NULL) { *p = 0; }
      }
      len = Mmsg(msg, T_("JobId=%d Level=%s Type=%s Name=%s Status=%s\n"),
                 jcr->JobId, job_level_to_str(jcr->getJobLevel()),
                 job_type_to_str(jcr->getJobType()), JobName,
                 JobstatusToAscii(jcr->getJobStatus()).c_str());
      sp->send(msg, len);

      if (rdcr && rdcr->device_resource) {
        len = Mmsg(msg,
                   T_("Reading: Volume=\"%s\"\n"
                      "    pool=\"%s\" device=%s\n"),
                   rdcr->VolumeName, rdcr->pool_name,
                   rdcr->dev ? rdcr->dev->print_name()
                             : rdcr->device_resource->archive_device_string);
        sp->send(msg, len);
      }
      if (dcr && dcr->device_resource) {
        len = Mmsg(msg,
                   T_("Writing: Volume=\"%s\"\n"
                      "    pool=\"%s\" device=%s\n"),
                   dcr->VolumeName, dcr->pool_name,
                   dcr->dev ? dcr->dev->print_name()
                            : dcr->device_resource->archive_device_string);
        sp->send(msg, len);
        len = Mmsg(msg, T_("    spooling=%d despooling=%d despool_wait=%d\n"),
                   dcr->spooling, dcr->despooling, dcr->despool_wait);
        sp->send(msg, len);
      }

      jcr->UpdateJobStats();

      len = Mmsg(msg,
                 T_("    Files=%s Bytes=%s AveBytes/sec=%s LastBytes/sec=%s\n"),
                 edit_uint64_with_commas(jcr->JobFiles, b1),
                 edit_uint64_with_commas(jcr->JobBytes, b2),
                 edit_uint64_with_commas(jcr->AverageRate, b3),
                 edit_uint64_with_commas(jcr->LastRate, b4));
      sp->send(msg, len);

      found = true;
      if (jcr->file_bsock) {
        len = Mmsg(msg, T_("    FDReadSeqNo=%s in_msg=%u out_msg=%d fd=%d\n\n"),
                   edit_uint64_with_commas(jcr->file_bsock->read_seqno, b1),
                   jcr->file_bsock->in_msg_no, jcr->file_bsock->out_msg_no,
                   jcr->file_bsock->fd_);
        sp->send(msg, len);
      } else {
        len = Mmsg(msg, T_("    FDSocket closed\n\n"));
        sp->send(msg, len);
      }
    }
  }
  endeach_jcr(jcr);

  if (!found) {
    if (!sp->api) {
      len = Mmsg(msg, T_("No Jobs running.\n"));
      sp->send(msg, len);
    }
  }

  if (!sp->api) {
    len = PmStrcpy(msg, "====\n");
    sp->send(msg, len);
  }
}

// Send any reservation messages queued for this jcr
static inline void SendDriveReserveMessages(JobControlRecord* jcr,
                                            StatusPacket* sp)
{
  int i;
  alist<const char*>* msgs;
  char* msg;

  std::unique_lock l(jcr->mutex_guard());

  msgs = jcr->sd_impl->reserve_msgs;
  if (!msgs || msgs->size() == 0) { return; }
  for (i = msgs->size() - 1; i >= 0; i--) {
    msg = (char*)msgs->get(i);
    if (msg) {
      sp->send("   ");
      sp->send(msg, strlen(msg));
    } else {
      break;
    }
  }
}

static void ListJobsWaitingOnReservation(StatusPacket* sp)
{
  int len;
  JobControlRecord* jcr;
  PoolMem msg(PM_MESSAGE);

  if (!sp->api) {
    len = Mmsg(msg, T_("\nJobs waiting to reserve a drive:\n"));
    sp->send(msg, len);
  }

  foreach_jcr (jcr) {
    if (!jcr->sd_impl->reserve_msgs) { continue; }
    SendDriveReserveMessages(jcr, sp);
  }
  endeach_jcr(jcr);

  if (!sp->api) {
    len = PmStrcpy(msg, "====\n");
    sp->send(msg, len);
  }
}

static void ListTerminatedJobs(StatusPacket* sp)
{
  int len;
  char level[10];
  PoolMem msg(PM_MESSAGE);
  char dt[MAX_TIME_LENGTH], b1[30], b2[30];

  if (!sp->api) {
    len = PmStrcpy(msg, T_("\nTerminated Jobs:\n"));
    sp->send(msg, len);
  }

  if (RecentJobResultsList::IsEmpty()) {
    if (!sp->api) {
      len = PmStrcpy(msg, "====\n");
      sp->send(msg, len);
    }
    return;
  }

  if (!sp->api) {
    len = PmStrcpy(msg, T_(" JobId  Level    Files      Bytes   Status   "
                           "Finished        Name \n"));
    sp->send(msg, len);
    len = PmStrcpy(msg,
                   T_("===================================================="
                      "===============\n"));
    sp->send(msg, len);
  }

  for (const RecentJobResultsList::JobResult& je :
       RecentJobResultsList::Get()) {
    char JobName[MAX_NAME_LENGTH];
    const char* termstat;

    bstrftime_nc(dt, sizeof(dt), je.end_time);
    switch (je.JobType) {
      case JT_ADMIN:
      case JT_RESTORE:
        bstrncpy(level, "    ", sizeof(level));
        break;
      default:
        bstrncpy(level, JobLevelToString(je.JobLevel), sizeof(level));
        level[4] = 0;
        break;
    }
    switch (je.JobStatus) {
      case JS_Created:
        termstat = T_("Created");
        break;
      case JS_FatalError:
      case JS_ErrorTerminated:
        termstat = T_("Error");
        break;
      case JS_Differences:
        termstat = T_("Diffs");
        break;
      case JS_Canceled:
        termstat = T_("Cancel");
        break;
      case JS_Terminated:
        termstat = T_("OK");
        break;
      case JS_Warnings:
        termstat = T_("OK -- with warnings");
        break;
      default:
        termstat = T_("Other");
        break;
    }
    bstrncpy(JobName, je.Job, sizeof(JobName));
    /* There are three periods after the Job name */
    char* p;
    for (int i = 0; i < 3; i++) {
      if ((p = strrchr(JobName, '.')) != NULL) { *p = 0; }
    }
    if (sp->api) {
      len = Mmsg(msg, T_("%6d\t%-6s\t%8s\t%10s\t%-7s\t%-8s\t%s\n"), je.JobId,
                 level, edit_uint64_with_commas(je.JobFiles, b1),
                 edit_uint64_with_suffix(je.JobBytes, b2), termstat, dt,
                 JobName);
    } else {
      len = Mmsg(msg, T_("%6d  %-6s %8s %10s  %-7s  %-8s %s\n"), je.JobId,
                 level, edit_uint64_with_commas(je.JobFiles, b1),
                 edit_uint64_with_suffix(je.JobBytes, b2), termstat, dt,
                 JobName);
    }
    sp->send(msg, len);
  }

  if (!sp->api) {
    len = PmStrcpy(msg, "====\n");
    sp->send(msg, len);
  }
}

// Convert Job Level into a string
static const char* JobLevelToString(int level)
{
  const char* str;

  switch (level) {
    case L_BASE:
      str = T_("Base");
      break;
    case L_FULL:
      str = T_("Full");
      break;
    case L_INCREMENTAL:
      str = T_("Incremental");
      break;
    case L_DIFFERENTIAL:
      str = T_("Differential");
      break;
    case L_SINCE:
      str = T_("Since");
      break;
    case L_VERIFY_CATALOG:
      str = T_("Verify Catalog");
      break;
    case L_VERIFY_INIT:
      str = T_("Init Catalog");
      break;
    case L_VERIFY_VOLUME_TO_CATALOG:
      str = T_("Volume to Catalog");
      break;
    case L_VERIFY_DISK_TO_CATALOG:
      str = T_("Disk to Catalog");
      break;
    case L_VERIFY_DATA:
      str = T_("Data");
      break;
    case L_NONE:
      str = " ";
      break;
    default:
      str = T_("Unknown Job Level");
      break;
  }
  return str;
}

// Status command from Director
bool StatusCmd(JobControlRecord* jcr)
{
  PoolMem devicenames;
  StatusPacket sp;
  BareosSocket* dir = jcr->dir_bsock;

  sp.bs = dir;
  devicenames.check_size(dir->message_length);
  if (sscanf(dir->msg, statuscmd, devicenames.c_str()) != 1) {
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(T_("3900 No arg in status command: %s\n"), jcr->errmsg);
    dir->signal(BNET_EOD);

    return false;
  }
  UnbashSpaces(devicenames);

  dir->fsend("\n");
  OutputStatus(jcr, &sp, devicenames.c_str());
  dir->signal(BNET_EOD);

  return true;
}

// .status command from Director
bool DotstatusCmd(JobControlRecord* jcr)
{
  JobControlRecord* njcr;
  PoolMem cmd;
  StatusPacket sp;
  BareosSocket* dir = jcr->dir_bsock;

  sp.bs = dir;
  cmd.check_size(dir->message_length);
  if (sscanf(dir->msg, dotstatuscmd, cmd.c_str()) != 1) {
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(T_("3900 No arg in .status command: %s\n"), jcr->errmsg);
    dir->signal(BNET_EOD);

    return false;
  }
  UnbashSpaces(cmd);

  Dmsg1(200, "cmd=%s\n", cmd.c_str());

  if (Bstrcasecmp(cmd.c_str(), "current")) {
    dir->fsend(OKdotstatus, cmd.c_str());
    foreach_jcr (njcr) {
      if (njcr->JobId != 0) {
        dir->fsend(DotStatusJob, njcr->JobId, njcr->getJobStatus(),
                   njcr->JobErrors);
      }
    }
    endeach_jcr(njcr);
  } else if (Bstrcasecmp(cmd.c_str(), "last")) {
    dir->fsend(OKdotstatus, cmd.c_str());
    if (RecentJobResultsList::Count() > 0) {
      RecentJobResultsList::JobResult job
          = RecentJobResultsList::GetMostRecentJobResult();
      dir->fsend(DotStatusJob, job.JobId, job.JobStatus, job.Errors);
    }
  } else if (Bstrcasecmp(cmd.c_str(), "header")) {
    sp.api = true;
    ListStatusHeader(&sp);
  } else if (Bstrcasecmp(cmd.c_str(), "running")) {
    sp.api = true;
    ListRunningJobs(&sp);
  } else if (Bstrcasecmp(cmd.c_str(), "waitreservation")) {
    sp.api = true;
    ListJobsWaitingOnReservation(&sp);
  } else if (Bstrcasecmp(cmd.c_str(), "devices")) {
    sp.api = true;
    ListDevices(jcr, &sp, NULL);
  } else if (Bstrcasecmp(cmd.c_str(), "volumes")) {
    sp.api = true;
    ListVolumes(&sp, NULL);
  } else if (Bstrcasecmp(cmd.c_str(), "spooling")) {
    sp.api = true;
    ListSpoolStats(&sp);
  } else if (Bstrcasecmp(cmd.c_str(), "terminated")) {
    sp.api = true;
    ListTerminatedJobs(&sp);
  } else {
    PmStrcpy(jcr->errmsg, dir->msg);
    dir->fsend(T_("3900 Unknown arg in .status command: %s\n"), jcr->errmsg);
    dir->signal(BNET_EOD);
    return false;
  }
  dir->signal(BNET_EOD);

  return true;
}

} /* namespace storagedaemon */
