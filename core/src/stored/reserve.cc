/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2023 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, MM
 * Split from job.c and acquire.c June 2005
 */
/**
 * @file
 * Drive reservation functions for Storage Daemon
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/device_control_record.h"
#include "stored/sd_device_control_record.h"
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/stored_jcr_impl.h"
#include "stored/wait.h"
#include "lib/berrno.h"
#include "lib/util.h"
#include "lib/bsock.h"
#include "include/jcr.h"
#include "lib/parse_conf.h"

namespace storagedaemon {

const int debuglevel = 150;

/* Global static variables */
static int reservations_lock_count = 0;

static brwlock_t reservation_lock;

/* Forward referenced functions */
static int CanReserveDrive(DeviceControlRecord* dcr, ReserveContext& rctx);
static int ReserveDevice(ReserveContext& rctx);
static bool ReserveDeviceForRead(DeviceControlRecord* dcr);
static bool ReserveDeviceForAppend(DeviceControlRecord* dcr,
                                   ReserveContext& rctx);
static bool UseDeviceCmd(JobControlRecord* jcr);
static void QueueReserveMessage(JobControlRecord* jcr);
static void PopReserveMessages(JobControlRecord* jcr);
// void SwitchDevice(DeviceControlRecord *dcr, Device *dev);

/* Requests from the Director daemon */
static char use_storage[]
    = "use storage=%127s media_type=%127s "
      "pool_name=%127s pool_type=%127s append=%d copy=%d stripe=%d\n";
static char use_device[] = "use device=%127s\n";

/* Responses sent to Director daemon */
static char OK_device[] = "3000 OK use device device=%s\n";
static char NO_device[]
    = "3924 Device \"%s\" not in SD Device"
      " resources or no matching Media Type.\n";
static char BAD_use[] = "3913 Bad use command: %s\n";

bool use_cmd(JobControlRecord* jcr)
{
  // Get the device, media, and pool information
  if (!UseDeviceCmd(jcr)) {
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
    memset(jcr->sd_auth_key, 0, strlen(jcr->sd_auth_key));
    return false;
  }
  return true;
}

/**
 * This allows a given thread to recursively call LockReservations.
 * It must, of course, call unlock_... the same number of times.
 */
void InitReservationsLock()
{
  int errstat;
  if ((errstat = RwlInit(&reservation_lock)) != 0) {
    BErrNo be;
    Emsg1(M_ABORT, 0, T_("Unable to initialize reservation lock. ERR=%s\n"),
          be.bstrerror(errstat));
  }

  InitVolListLock();
}

void TermReservationsLock()
{
  RwlDestroy(&reservation_lock);
  TermVolListLock();
}

// This applies to a drive and to Volumes
void LockReservations()
{
  int errstat;
  reservations_lock_count++;
  if ((errstat = RwlWritelock(&reservation_lock)) != 0) {
    BErrNo be;
    Emsg2(M_ABORT, 0, "RwlWritelock failure. stat=%d: ERR=%s\n", errstat,
          be.bstrerror(errstat));
  }
}

void UnlockReservations()
{
  int errstat;
  reservations_lock_count--;
  if ((errstat = RwlWriteunlock(&reservation_lock)) != 0) {
    BErrNo be;
    Emsg2(M_ABORT, 0, "RwlWriteunlock failure. stat=%d: ERR=%s\n", errstat,
          be.bstrerror(errstat));
  }
}

void DeviceControlRecord::SetReserved()
{
  reserved_ = true;
  Dmsg2(debuglevel, "Inc reserve=%d dev=%s\n", dev->NumReserved(),
        dev->print_name());
  dev->IncReserved();
}

void DeviceControlRecord::ClearReserved()
{
  if (reserved_) {
    reserved_ = false;
    dev->DecReserved();
    Dmsg2(debuglevel, "Dec reserve=%d dev=%s\n", dev->NumReserved(),
          dev->print_name());
  }
}

/**
 * Remove any reservation from a drive and tell the system
 * that the volume is unused at least by us.
 */
void DeviceControlRecord::UnreserveDevice()
{
  dev->Lock();
  if (IsReserved()) {
    ClearReserved();
    reserved_volume = false;

    // If we set read mode in reserving, remove it
    if (dev->CanRead()) { dev->ClearRead(); }

    if (dev->num_writers < 0) {
      Jmsg1(jcr, M_ERROR, 0, T_("Hey! num_writers=%d!!!!\n"), dev->num_writers);
      dev->num_writers = 0;
    }

    if (dev->NumReserved() == 0 && dev->num_writers == 0) {
      VolumeUnused(this);
    }
  }
  dev->Unlock();
}

/**
 * We get the following type of information:
 *
 * use storage=xxx media_type=yyy pool_name=xxx pool_type=yyy append=1 copy=0
 * strip=0 use device=zzz use device=aaa use device=bbb use storage=xxx
 * media_type=yyy pool_name=xxx pool_type=yyy append=0 copy=0 strip=0 use
 * device=bbb
 */
static bool UseDeviceCmd(JobControlRecord* jcr)
{
  PoolMem StoreName, dev_name, media_type, pool_name, pool_type;
  BareosSocket* dir = jcr->dir_bsock;
  int32_t append;
  bool ok;
  int32_t Copy, Stripe;
  DirectorStorage* store;
  ReserveContext rctx;

  memset(&rctx, 0, sizeof(ReserveContext));
  rctx.jcr = jcr;

  /* If there are multiple devices, the director sends us
   * use_device for each device that it wants to use. */
  jcr->sd_impl->reserve_msgs = new alist<const char*>(10, not_owned_by_alist);
  do {
    Dmsg1(debuglevel, "<dird: %s", dir->msg);
    ok = sscanf(dir->msg, use_storage, StoreName.c_str(), media_type.c_str(),
                pool_name.c_str(), pool_type.c_str(), &append, &Copy, &Stripe)
         == 7;
    if (!ok) { break; }
    alist<DirectorStorage*>* dirstore
        = new alist<DirectorStorage*>(10, not_owned_by_alist);
    if (append) {
      jcr->sd_impl->write_store = dirstore;
    } else {
      jcr->sd_impl->read_store = dirstore;
    }
    rctx.append = append;
    UnbashSpaces(StoreName);
    UnbashSpaces(media_type);
    UnbashSpaces(pool_name);
    UnbashSpaces(pool_type);
    store = new DirectorStorage;
    dirstore->append(store);
    memset(store, 0, sizeof(DirectorStorage));
    store->device = new alist<const char*>(10);
    bstrncpy(store->name, StoreName, sizeof(store->name));
    bstrncpy(store->media_type, media_type, sizeof(store->media_type));
    bstrncpy(store->pool_name, pool_name, sizeof(store->pool_name));
    bstrncpy(store->pool_type, pool_type, sizeof(store->pool_type));
    store->append = append;

    // Now get all devices
    while (dir->recv() >= 0) {
      Dmsg1(debuglevel, "<dird device: %s", dir->msg);
      ok = sscanf(dir->msg, use_device, dev_name.c_str()) == 1;
      if (!ok) { break; }
      UnbashSpaces(dev_name);
      store->device->append(strdup(dev_name.c_str()));
    }
  } while (ok && dir->recv() >= 0);

  InitJcrDeviceWaitTimers(jcr);
  jcr->sd_impl->dcr = new StorageDaemonDeviceControlRecord;
  SetupNewDcrDevice(jcr, jcr->sd_impl->dcr, NULL, NULL);
  if (rctx.append) { jcr->sd_impl->dcr->SetWillWrite(); }

  if (!jcr->sd_impl->dcr) {
    BareosSocket* dir = jcr->dir_bsock;
    dir->fsend(T_("3939 Could not get dcr\n"));
    Dmsg1(debuglevel, ">dird: %s", dir->msg);
    ok = false;
  }

  /* At this point, we have a list of all the Director's Storage resources
   * indicated for this Job, which include Pool, PoolType, storage name, and
   * Media type.
   *
   * Then for each of the Storage resources, we have a list of device names that
   * were given.
   *
   * Wiffle through them and find one that can do the backup. */
  if (ok) {
    int wait_for_device_retries = 0;
    int repeat = 0;
    bool fail = false;
    rctx.notify_dir = true;

    // Put new dcr in proper location
    if (rctx.append) {
      rctx.jcr->sd_impl->dcr = jcr->sd_impl->dcr;
    } else {
      rctx.jcr->sd_impl->read_dcr = jcr->sd_impl->dcr;
    }

    LockReservations();
    for (; !fail && !jcr->IsJobCanceled();) {
      PopReserveMessages(jcr);
      rctx.suitable_device = false;
      rctx.have_volume = false;
      rctx.VolumeName[0] = 0;
      rctx.any_drive = false;
      if (!jcr->sd_impl->PreferMountedVols) {
        /* Here we try to find a drive that is not used.
         * This will maximize the use of available drives. */
        rctx.num_writers = 20000000; /* start with impossible number */
        rctx.low_use_drive = NULL;
        rctx.PreferMountedVols = false;
        rctx.exact_match = false;
        rctx.autochanger_only = true;
        if ((ok = FindSuitableDeviceForJob(jcr, rctx))) { break; }

        // Look through all drives possibly for low_use drive
        if (rctx.low_use_drive) {
          rctx.try_low_use_drive = true;
          if ((ok = FindSuitableDeviceForJob(jcr, rctx))) { break; }
          rctx.try_low_use_drive = false;
        }
        rctx.autochanger_only = false;
        if ((ok = FindSuitableDeviceForJob(jcr, rctx))) { break; }
      }

      /* Now we look for a drive that may or may not be in use.
       * Look for an exact Volume match all drives */
      rctx.PreferMountedVols = true;
      rctx.exact_match = true;
      rctx.autochanger_only = false;
      if ((ok = FindSuitableDeviceForJob(jcr, rctx))) { break; }

      // Look for any mounted drive
      rctx.exact_match = false;
      if ((ok = FindSuitableDeviceForJob(jcr, rctx))) { break; }

      // Try any drive
      rctx.any_drive = true;
      if ((ok = FindSuitableDeviceForJob(jcr, rctx))) { break; }

      // during WaitForDevice()
      UnlockReservations();

      /* The idea of looping on repeat a few times it to ensure
       * that if there is some subtle timing problem between two
       * jobs, we will simply try again, and most likely succeed.
       * This can happen if one job reserves a drive or finishes using
       * a drive at the same time a second job wants it. */
      if (repeat++ > 1) {   /* try algorithm 3 times */
        Bmicrosleep(30, 0); /* wait a bit */
        Dmsg0(debuglevel, "repeat reserve algorithm\n");
      } else if (!rctx.suitable_device
                 || !WaitForDevice(jcr, wait_for_device_retries)) {
        Dmsg0(debuglevel, "Fail. !suitable_device || !WaitForDevice\n");
        fail = true;
      }
      LockReservations();
      dir->signal(BNET_HEARTBEAT); /* Inform Dir that we are alive */
    }
    UnlockReservations();

    if (!ok) {
      /* If we get here, there are no suitable devices available, which
       * means nothing configured.  If a device is suitable but busy
       * with another Volume, we will not come here. */
      UnbashSpaces(dir->msg);
      PmStrcpy(jcr->errmsg, dir->msg);
      Jmsg(jcr, M_FATAL, 0, T_("Device reservation failed for JobId=%d: %s\n"),
           jcr->JobId, jcr->errmsg);
      dir->fsend(NO_device, dev_name.c_str());

      Dmsg1(debuglevel, ">dird: %s", dir->msg);
    }
  } else {
    UnbashSpaces(dir->msg);
    PmStrcpy(jcr->errmsg, dir->msg);
    Jmsg(jcr, M_FATAL, 0, T_("Failed command: %s\n"), jcr->errmsg);
    dir->fsend(BAD_use, jcr->errmsg);
    Dmsg1(debuglevel, ">dird: %s", dir->msg);
  }

  ReleaseReserveMessages(jcr);
  return ok;
}

/**
 * Walk through the autochanger resources and check if the volume is in one of
 * them.
 *
 * Returns:  true  if volume is in device
 *           false otherwise
 */
static bool IsVolInAutochanger(ReserveContext& rctx, VolumeReservationItem* vol)
{
  AutochangerResource* changer = vol->dev->device_resource->changer_res;

  if (!changer) { return false; }

  // Find resource, and make sure we were able to open it
  if (bstrcmp(rctx.device_name, changer->resource_name_)) {
    Dmsg1(debuglevel, "Found changer device %s\n",
          vol->dev->device_resource->resource_name_);
    return true;
  }
  Dmsg1(debuglevel, "Incorrect changer device %s\n", changer->resource_name_);

  return false;
}

/**
 * Search for a device suitable for this job.
 *
 * Note, this routine sets sets rctx.suitable_device if any
 * device exists within the SD. The device may not be actually useable.
 * It also returns if it finds a useable device.
 */
bool FindSuitableDeviceForJob(JobControlRecord* jcr, ReserveContext& rctx)
{
  bool ok = false;
  DirectorStorage* store = nullptr;
  const char* device_name = nullptr;
  alist<DirectorStorage*>* dirstore;
  DeviceControlRecord* dcr = jcr->sd_impl->dcr;

  if (rctx.append) {
    dirstore = jcr->sd_impl->write_store;
  } else {
    dirstore = jcr->sd_impl->read_store;
  }
  Dmsg5(debuglevel,
        "Start find_suit_dev PrefMnt=%d exact=%d suitable=%d chgronly=%d "
        "any=%d\n",
        rctx.PreferMountedVols, rctx.exact_match, rctx.suitable_device,
        rctx.autochanger_only, rctx.any_drive);

  /* If the appropriate conditions of this if are met, namely that
   * we are appending and the user wants mounted drive (or we
   * force try a mounted drive because they are all busy), we
   * start by looking at all the Volumes in the volume list. */
  if (!IsVolListEmpty() && rctx.append && rctx.PreferMountedVols) {
    dlist<VolumeReservationItem>* temp_vol_list;
    VolumeReservationItem* vol = NULL;
    temp_vol_list = dup_vol_list(jcr);

    // Look through reserved volumes for one we can use
    Dmsg0(debuglevel, "look for vol in vol list\n");
    foreach_dlist (vol, temp_vol_list) {
      if (!vol->dev) {
        Dmsg1(debuglevel, "vol=%s no dev\n", vol->vol_name);
        continue;
      }

      // Check with Director if this Volume is OK
      bstrncpy(dcr->VolumeName, vol->vol_name, sizeof(dcr->VolumeName));
      if (!dcr->DirGetVolumeInfo(GET_VOL_INFO_FOR_WRITE)) { continue; }

      Dmsg1(debuglevel, "vol=%s OK for this job\n", vol->vol_name);
      foreach_alist (store, dirstore) {
        int status;
        rctx.store = store;
        foreach_alist (device_name, store->device) {
          // Found a device, try to use it
          rctx.device_name = device_name;
          rctx.device_resource = vol->dev->device_resource;

          if (vol->dev->AttachedToAutochanger()) {
            Dmsg1(debuglevel, "vol=%s is in changer\n", vol->vol_name);
            if (!IsVolInAutochanger(rctx, vol) || !vol->dev->autoselect) {
              continue;
            }
          } else if (!bstrcmp(device_name,
                              vol->dev->device_resource->resource_name_)) {
            Dmsg2(debuglevel, "device=%s not suitable want %s\n",
                  vol->dev->device_resource->resource_name_, device_name);
            continue;
          }

          bstrncpy(rctx.VolumeName, vol->vol_name, sizeof(rctx.VolumeName));
          rctx.have_volume = true;

          // Try reserving this device and volume
          Dmsg2(debuglevel, "try vol=%s on device=%s\n", rctx.VolumeName,
                device_name);
          status = ReserveDevice(rctx);
          if (status == 1) { /* found available device */
            Dmsg1(debuglevel, "Suitable device found=%s\n", device_name);
            ok = true;
            break;
          } else if (status == 0) { /* device busy */
            Dmsg1(debuglevel, "Suitable device=%s, busy: not use\n",
                  device_name);
          } else {
            Dmsg0(debuglevel, "No suitable device found.\n");
          }
          rctx.have_volume = false;
          rctx.VolumeName[0] = 0;
        }
        if (ok) { break; }
      }
      if (ok) { break; }
    } /* end for loop over reserved volumes */

    Dmsg0(debuglevel, "lock volumes\n");
    FreeTempVolList(temp_vol_list);
    temp_vol_list = NULL;
  }

  if (ok) {
    Dmsg1(debuglevel, "OK dev found. Vol=%s from in-use vols list\n",
          rctx.VolumeName);
    return true;
  } else {
    // no existing volume found; reseting volume name.
    dcr->VolumeName[0] = 0;
  }

  /* No reserved volume we can use, so now search for an available device.
   *
   * For each storage device that the user specified, we
   * search and see if there is a resource for that device. */
  foreach_alist (store, dirstore) {
    rctx.store = store;
    foreach_alist (device_name, store->device) {
      int status;
      rctx.device_name = device_name;
      status = SearchResForDevice(rctx);
      if (status == 1) { /* found available device */
        Dmsg1(debuglevel, "available device found=%s\n", device_name);
        ok = true;
        break;
      } else if (status == 0) { /* device busy */
        Dmsg1(debuglevel, "No usable device=%s, busy: not use\n", device_name);
      } else {
        Dmsg0(debuglevel, "No usable device found.\n");
      }
    }
    if (ok) { break; }
  }
  if (ok) {
    Dmsg1(debuglevel, "OK dev found. Vol=%s\n", rctx.VolumeName);
  } else {
    Dmsg0(debuglevel, "Leave find_suit_dev: no dev found.\n");
  }
  return ok;
}

/**
 * Search for a particular storage device with particular storage
 * characteristics (MediaType).
 */
int SearchResForDevice(ReserveContext& rctx)
{
  int status;
  AutochangerResource* changer;

  // Look through Autochangers first
  foreach_res (changer, R_AUTOCHANGER) {
    Dmsg2(debuglevel, "Try match changer res=%s, wanted %s\n",
          changer->resource_name_, rctx.device_name);
    // Find resource, and make sure we were able to open it
    if (bstrcmp(rctx.device_name, changer->resource_name_)) {
      // Try each device_resource in this AutoChanger
      foreach_alist (rctx.device_resource, changer->device_resources) {
        Dmsg1(debuglevel, "Try changer device %s\n",
              rctx.device_resource->resource_name_);
        if (!rctx.device_resource->autoselect) {
          Dmsg1(100, "Device %s not autoselect skipped.\n",
                rctx.device_resource->resource_name_);
          continue; /* Device is not available */
        }
        status = ReserveDevice(rctx);
        if (status != 1) { /* Try another device */
          continue;
        }

        // Debug code
        if (rctx.store->append == SD_APPEND) {
          Dmsg2(debuglevel, "Device %s reserved=%d for append.\n",
                rctx.device_resource->resource_name_,
                rctx.jcr->sd_impl->dcr->dev->NumReserved());
        } else {
          Dmsg2(debuglevel, "Device %s reserved=%d for read.\n",
                rctx.device_resource->resource_name_,
                rctx.jcr->sd_impl->read_dcr->dev->NumReserved());
        }
        return status;
      }
    }
  }

  // Now if requested look through regular devices
  if (!rctx.autochanger_only) {
    foreach_res (rctx.device_resource, R_DEVICE) {
      Dmsg2(debuglevel, "Try match res=%s wanted %s\n",
            rctx.device_resource->resource_name_, rctx.device_name);

      // Find resource, and make sure we were able to open it
      if (bstrcmp(rctx.device_name, rctx.device_resource->resource_name_)) {
        status = ReserveDevice(rctx);
        if (status != 1) { /* Try another device_resource */
          continue;
        }
        // Debug code
        if (rctx.store->append == SD_APPEND) {
          Dmsg2(debuglevel, "Device %s reserved=%d for append.\n",
                rctx.device_resource->resource_name_,
                rctx.jcr->sd_impl->dcr->dev->NumReserved());
        } else {
          Dmsg2(debuglevel, "Device %s reserved=%d for read.\n",
                rctx.device_resource->resource_name_,
                rctx.jcr->sd_impl->read_dcr->dev->NumReserved());
        }
        return status;
      }
    }

    /* If we haven't found a available device_resource and the
     * devicereservebymediatype option is set we try one more time where we
     * allow any device_resource with a matching mediatype. */
    if (me->device_reserve_by_mediatype) {
      foreach_res (rctx.device_resource, R_DEVICE) {
        Dmsg3(debuglevel,
              "Try match res=%s, mediatype=%s wanted mediatype=%s\n",
              rctx.device_resource->resource_name_, rctx.store->media_type,
              rctx.store->media_type);

        if (bstrcmp(rctx.store->media_type, rctx.device_resource->media_type)) {
          status = ReserveDevice(rctx);
          if (status != 1) { /* Try another device_resource */
            continue;
          }

          // Debug code
          if (rctx.store->append == SD_APPEND) {
            Dmsg2(debuglevel, "Device %s reserved=%d for append.\n",
                  rctx.device_resource->resource_name_,
                  rctx.jcr->sd_impl->dcr->dev->NumReserved());
          } else {
            Dmsg2(debuglevel, "Device %s reserved=%d for read.\n",
                  rctx.device_resource->resource_name_,
                  rctx.jcr->sd_impl->read_dcr->dev->NumReserved());
          }
          return status;
        }
      }
    }
  }

  return -1; /* Nothing found */
}

/**
 * Try to reserve a specific device.
 *
 * Returns: 1 -- OK, have DeviceControlRecord
 *          0 -- must wait
 *         -1 -- fatal error
 */
static int ReserveDevice(ReserveContext& rctx)
{
  bool ok;
  DeviceControlRecord* dcr;
  const int name_len = MAX_NAME_LENGTH;

  // Make sure MediaType is OK
  Dmsg2(debuglevel, "chk MediaType device=%s request=%s\n",
        rctx.device_resource->media_type, rctx.store->media_type);
  if (!bstrcmp(rctx.device_resource->media_type, rctx.store->media_type)) {
    return -1;
  }

  // Make sure device access mode matches
  Dmsg3(debuglevel, "chk AccessMode append=%d access_mode=%d\n", rctx.append,
        rctx.device_resource->access_mode);
  if (rctx.append && rctx.device_resource->access_mode == IODirection::READ) {
    // Trying to write but access mode is readonly
    return -1;
  } else if (!rctx.append
             && rctx.device_resource->access_mode == IODirection::WRITE) {
    // Trying to read but access mode is writeonly
    return -1;
  }

  // Make sure device_resource exists -- i.e. we can stat() it
  if (!rctx.device_resource->dev) {
    rctx.device_resource->dev
        = FactoryCreateDevice(rctx.jcr, rctx.device_resource);
  }
  if (!rctx.device_resource->dev) {
    if (rctx.device_resource->changer_res) {
      Jmsg(rctx.jcr, M_WARNING, 0,
           T_("\n"
              "     Device \"%s\" in changer \"%s\" requested by DIR could not "
              "be opened or does not exist.\n"),
           rctx.device_resource->resource_name_, rctx.device_name);
    } else {
      Jmsg(rctx.jcr, M_WARNING, 0,
           T_("\n"
              "     Device \"%s\" requested by DIR could not be opened or does "
              "not exist.\n"),
           rctx.device_name);
    }
    return -1; /* no use waiting */
  }

  rctx.suitable_device = true;
  Dmsg1(debuglevel, "try reserve %s\n", rctx.device_resource->resource_name_);

  if (rctx.store->append) {
    SetupNewDcrDevice(rctx.jcr, rctx.jcr->sd_impl->dcr,
                      rctx.device_resource->dev, NULL);
    dcr = rctx.jcr->sd_impl->dcr;
  } else {
    SetupNewDcrDevice(rctx.jcr, rctx.jcr->sd_impl->read_dcr,
                      rctx.device_resource->dev, NULL);
    dcr = rctx.jcr->sd_impl->read_dcr;
  }

  if (!dcr) {
    BareosSocket* dir = rctx.jcr->dir_bsock;

    dir->fsend(T_("3926 Could not get dcr for device: %s\n"), rctx.device_name);
    Dmsg1(debuglevel, ">dird: %s", dir->msg);
    return -1;
  }

  if (rctx.store->append) { dcr->SetWillWrite(); }

  bstrncpy(dcr->pool_name, rctx.store->pool_name, name_len);
  bstrncpy(dcr->pool_type, rctx.store->pool_type, name_len);
  bstrncpy(dcr->media_type, rctx.store->media_type, name_len);
  bstrncpy(dcr->dev_name, rctx.device_name, name_len);
  if (rctx.store->append == SD_APPEND) {
    Dmsg2(debuglevel, "call reserve for append: have_vol=%d vol=%s\n",
          rctx.have_volume, rctx.VolumeName);
    ok = ReserveDeviceForAppend(dcr, rctx);
    if (!ok) { goto bail_out; }

    rctx.jcr->sd_impl->dcr = dcr;
    Dmsg5(debuglevel, "Reserved=%d dev_name=%s mediatype=%s pool=%s ok=%d\n",
          dcr->dev->NumReserved(), dcr->dev_name, dcr->media_type,
          dcr->pool_name, ok);
    Dmsg3(debuglevel, "Vol=%s num_writers=%d, have_vol=%d\n", rctx.VolumeName,
          dcr->dev->num_writers, rctx.have_volume);
    if (rctx.have_volume) {
      Dmsg0(debuglevel, "Call reserve_volume for append.\n");
      if (reserve_volume(dcr, rctx.VolumeName)) {
        Dmsg1(debuglevel, "Reserved vol=%s\n", rctx.VolumeName);
      } else {
        Dmsg1(debuglevel, "Could not reserve vol=%s\n", rctx.VolumeName);
        goto bail_out;
      }
    } else {
      dcr->any_volume = true;
      Dmsg0(debuglevel, "no vol, call find_next_appendable_vol.\n");
      if (dcr->DirFindNextAppendableVolume()) {
        bstrncpy(rctx.VolumeName, dcr->VolumeName, sizeof(rctx.VolumeName));
        rctx.have_volume = true;
        Dmsg1(debuglevel, "looking for Volume=%s\n", rctx.VolumeName);
      } else {
        Dmsg0(debuglevel, "No next volume found\n");
        rctx.have_volume = false;
        rctx.VolumeName[0] = 0;

        /* If there is at least one volume that is valid and in use,
         * but we get here, check if we are running with prefers
         * non-mounted drives.  In that case, we have selected a
         * non-used drive and our one and only volume is mounted
         * elsewhere, so we bail out and retry using that drive. */
        if (dcr->FoundInUse() && !rctx.PreferMountedVols) {
          rctx.PreferMountedVols = true;
          if (dcr->VolumeName[0]) { dcr->UnreserveDevice(); }
          goto bail_out;
        }

        /* Note. Under some circumstances, the Director can hand us
         * a Volume name that is not the same as the one on the current
         * drive, and in that case, the call above to find the next
         * volume will fail because in attempting to reserve the Volume
         * the code will realize that we already have a tape mounted,
         * and it will fail.  This *should* only happen if there are
         * writers, thus the following test.  In that case, we simply
         * bail out, and continue waiting, rather than plunging on
         * and hoping that the operator can resolve the problem. */
        if (dcr->dev->num_writers != 0) {
          if (dcr->VolumeName[0]) { dcr->UnreserveDevice(); }
          goto bail_out;
        }
      }
    }
  } else {
    ok = ReserveDeviceForRead(dcr);
    if (ok) {
      rctx.jcr->sd_impl->read_dcr = dcr;
      Dmsg5(debuglevel,
            "Read reserved=%d dev_name=%s mediatype=%s pool=%s ok=%d\n",
            dcr->dev->NumReserved(), dcr->dev_name, dcr->media_type,
            dcr->pool_name, ok);
    }
  }
  if (!ok) { goto bail_out; }

  if (rctx.notify_dir) {
    PoolMem dev_name;
    BareosSocket* dir = rctx.jcr->dir_bsock;
    PmStrcpy(dev_name, rctx.device_resource->resource_name_);
    BashSpaces(dev_name);
    ok = dir->fsend(OK_device, dev_name.c_str()); /* Return real device name */
    Dmsg1(debuglevel, ">dird: %s", dir->msg);
  } else {
    ok = true;
  }
  return ok ? 1 : -1;

bail_out:
  rctx.have_volume = false;
  rctx.VolumeName[0] = 0;
  Dmsg0(debuglevel, "Not OK.\n");
  return 0;
}

/**
 * We "reserve" the drive by setting the ST_READREADY bit.
 * No one else should touch the drive until that is cleared.
 * This allows the DIR to "reserve" the device before actually starting the job.
 */
static bool ReserveDeviceForRead(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  JobControlRecord* jcr = dcr->jcr;
  bool ok = false;

  ASSERT(dcr);
  if (jcr->IsJobCanceled()) { return false; }

  dev->Lock();

  if (dev->IsDeviceUnmounted()) {
    Dmsg1(debuglevel, "Device %s is BLOCKED due to user unmount.\n",
          dev->print_name());
    Mmsg(jcr->errmsg,
         T_("3601 JobId=%u device %s is BLOCKED due to user unmount.\n"),
         jcr->JobId, dev->print_name());
    QueueReserveMessage(jcr);
    goto bail_out;
  }

  if (dev->IsBusy()) {
    Dmsg4(debuglevel,
          "Device %s is busy ST_READREADY=%d num_writers=%d reserved=%d.\n",
          dev->print_name(), BitIsSet(ST_READREADY, dev->state) ? 1 : 0,
          dev->num_writers, dev->NumReserved());
    Mmsg(jcr->errmsg,
         T_("3602 JobId=%u device %s is busy (already reading/writing).\n"),
         jcr->JobId, dev->print_name());
    QueueReserveMessage(jcr);
    goto bail_out;
  }

  // Note: on failure this returns jcr->errmsg properly edited
  if (GeneratePluginEvent(jcr, bSdEventDeviceReserve, dcr) != bRC_OK) {
    QueueReserveMessage(jcr);
    goto bail_out;
  }
  dev->ClearAppend();
  dev->SetRead();
  dcr->SetReserved();
  ok = true;

bail_out:
  dev->Unlock();
  return ok;
}

/**
 * We reserve the device for appending by incrementing
 * NumReserved(). We do virtually all the same work that
 * is done in AcquireDeviceForAppend(), but we do
 * not attempt to mount the device. This routine allows
 * the DIR to reserve multiple devices before *really*
 * starting the job. It also permits the SD to refuse
 * certain devices (not up, ...).
 *
 * Note, in reserving a device, if the device is for the
 * same pool and the same pool type, then it is acceptable.
 * The Media Type has already been checked. If we are
 * the first to reserve the device, we put the pool
 * name and pool type in the device record.
 */
static bool ReserveDeviceForAppend(DeviceControlRecord* dcr,
                                   ReserveContext& rctx)
{
  JobControlRecord* jcr = dcr->jcr;
  Device* dev = dcr->dev;
  bool ok = false;

  ASSERT(dcr);
  if (jcr->IsJobCanceled()) { return false; }

  dev->Lock();

  // If device is being read, we cannot write it
  if (dev->CanRead()) {
    Mmsg(jcr->errmsg, T_("3603 JobId=%u device %s is busy reading.\n"),
         jcr->JobId, dev->print_name());
    Dmsg1(debuglevel, "Failed: %s", jcr->errmsg);
    QueueReserveMessage(jcr);
    goto bail_out;
  }

  // If device is unmounted, we are out of luck
  if (dev->IsDeviceUnmounted()) {
    Mmsg(jcr->errmsg,
         T_("3604 JobId=%u device %s is BLOCKED due to user unmount.\n"),
         jcr->JobId, dev->print_name());
    Dmsg1(debuglevel, "Failed: %s", jcr->errmsg);
    QueueReserveMessage(jcr);
    goto bail_out;
  }

  Dmsg1(debuglevel, "reserve_append device is %s\n", dev->print_name());

  // Now do detailed tests ...
  if (CanReserveDrive(dcr, rctx) != 1) {
    Dmsg0(debuglevel, "CanReserveDrive!=1\n");
    goto bail_out;
  }

  // Note: on failure this returns jcr->errmsg properly edited
  if (GeneratePluginEvent(jcr, bSdEventDeviceReserve, dcr) != bRC_OK) {
    QueueReserveMessage(jcr);
    goto bail_out;
  }
  dcr->SetReserved();
  ok = true;

bail_out:
  dev->Unlock();
  return ok;
}

static int IsPoolOk(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  JobControlRecord* jcr = dcr->jcr;

  // Now check if we want the same Pool and pool type
  if (bstrcmp(dev->pool_name, dcr->pool_name)
      && bstrcmp(dev->pool_type, dcr->pool_type)) {
    // OK, compatible device
    Dmsg1(debuglevel, "OK dev: %s num_writers=0, reserved, pool matches\n",
          dev->print_name());
    return 1;
  } else {
    /* Drive Pool not suitable for us */
    Mmsg(jcr->errmsg,
         T_("3608 JobId=%u wants Pool=\"%s\" but have Pool=\"%s\" nreserve=%d "
            "on drive %s.\n"),
         (uint32_t)jcr->JobId, dcr->pool_name, dev->pool_name,
         dev->NumReserved(), dev->print_name());
    Dmsg1(debuglevel, "Failed: %s", jcr->errmsg);
    QueueReserveMessage(jcr);
  }
  return 0;
}

static bool IsMaxJobsOk(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  JobControlRecord* jcr = dcr->jcr;

  Dmsg5(debuglevel, "MaxJobs=%d Jobs=%d reserves=%d Status=%s Vol=%s\n",
        dcr->VolCatInfo.VolCatMaxJobs, dcr->VolCatInfo.VolCatJobs,
        dev->NumReserved(), dcr->VolCatInfo.VolCatStatus, dcr->VolumeName);

  // Limit max concurrent jobs on this drive
  if (dev->max_concurrent_jobs > 0
      && dev->max_concurrent_jobs
             <= (uint32_t)(dev->num_writers + dev->NumReserved())) {
    // Max Concurrent Jobs depassed or already reserved
    Mmsg(jcr->errmsg,
         T_("3609 JobId=%u Max concurrent jobs exceeded on drive %s.\n"),
         (uint32_t)jcr->JobId, dev->print_name());
    Dmsg1(debuglevel, "Failed: %s", jcr->errmsg);
    QueueReserveMessage(jcr);
    return false;
  }
  if (bstrcmp(dcr->VolCatInfo.VolCatStatus, "Recycle")) { return true; }
  if (dcr->VolCatInfo.VolCatMaxJobs > 0
      && dcr->VolCatInfo.VolCatMaxJobs
             <= (dcr->VolCatInfo.VolCatJobs + dev->NumReserved())) {
    // Max Job Vols depassed or already reserved
    Mmsg(jcr->errmsg,
         T_("3610 JobId=%u Volume max jobs exceeded on drive %s.\n"),
         (uint32_t)jcr->JobId, dev->print_name());
    Dmsg1(debuglevel, "reserve dev failed: %s", jcr->errmsg);
    QueueReserveMessage(jcr);
    return false; /* wait */
  }
  return true;
}

/**
 * Returns: 1 if drive can be reserved
 *          0 if we should wait
 *         -1 on error or impossibility
 */
static int CanReserveDrive(DeviceControlRecord* dcr, ReserveContext& rctx)
{
  Device* dev = dcr->dev;
  JobControlRecord* jcr = dcr->jcr;

  Dmsg5(debuglevel, "PrefMnt=%d exact=%d suitable=%d chgronly=%d any=%d\n",
        rctx.PreferMountedVols, rctx.exact_match, rctx.suitable_device,
        rctx.autochanger_only, rctx.any_drive);

  // Check for max jobs on this Volume
  if (!IsMaxJobsOk(dcr)) { return 0; }

  // Setting any_drive overrides PreferMountedVols flag
  if (!rctx.any_drive) {
    /* When PreferMountedVols is set, we keep track of the
     * drive in use that has the least number of writers, then if
     * no unmounted drive is found, we try that drive. This
     * helps spread the load to the least used drives. */
    if (rctx.try_low_use_drive && dev == rctx.low_use_drive) {
      Dmsg2(debuglevel, "OK dev=%s == low_drive=%s.\n", dev->print_name(),
            rctx.low_use_drive->print_name());
      return 1;
    }

    // If he wants a free drive, but this one is busy, no go
    if (!rctx.PreferMountedVols && dev->IsBusy()) {
      // Save least used drive
      if ((dev->num_writers + dev->NumReserved()) < rctx.num_writers) {
        rctx.num_writers = dev->num_writers + dev->NumReserved();
        rctx.low_use_drive = dev;
        Dmsg2(debuglevel, "set low use drive=%s num_writers=%d\n",
              dev->print_name(), rctx.num_writers);
      } else {
        Dmsg1(debuglevel, "not low use num_writers=%d\n",
              dev->num_writers + dev->NumReserved());
      }
      Mmsg(jcr->errmsg,
           T_("3605 JobId=%u wants free drive but device %s is busy.\n"),
           jcr->JobId, dev->print_name());
      Dmsg1(debuglevel, "Failed: %s", jcr->errmsg);
      QueueReserveMessage(jcr);
      return 0;
    }

    // Check for prefer mounted volumes
    if (rctx.PreferMountedVols && !dev->vol && dev->IsTape()) {
      Mmsg(jcr->errmsg,
           T_("3606 JobId=%u prefers mounted drives, but drive %s has no "
              "Volume.\n"),
           jcr->JobId, dev->print_name());
      Dmsg1(debuglevel, "Failed: %s", jcr->errmsg);
      QueueReserveMessage(jcr);
      return 0; /* No volume mounted */
    }

    /* Check for exact Volume name match
     * ***FIXME*** for Disk, we can accept any volume that goes with this drive.
     */
    if (rctx.exact_match && rctx.have_volume) {
      bool ok;

      Dmsg5(debuglevel, "PrefMnt=%d exact=%d suitable=%d chgronly=%d any=%d\n",
            rctx.PreferMountedVols, rctx.exact_match, rctx.suitable_device,
            rctx.autochanger_only, rctx.any_drive);
      Dmsg4(debuglevel, "have_vol=%d have=%s resvol=%s want=%s\n",
            rctx.have_volume, dev->VolHdr.VolumeName,
            dev->vol ? dev->vol->vol_name : "*None*", rctx.VolumeName);
      ok = bstrcmp(dev->VolHdr.VolumeName, rctx.VolumeName)
           || (dev->vol && bstrcmp(dev->vol->vol_name, rctx.VolumeName));
      if (!ok) {
        Mmsg(jcr->errmsg,
             T_("3607 JobId=%u wants Vol=\"%s\" drive has Vol=\"%s\" on drive "
                "%s.\n"),
             jcr->JobId, rctx.VolumeName, dev->VolHdr.VolumeName,
             dev->print_name());
        QueueReserveMessage(jcr);
        Dmsg3(debuglevel, "not OK: dev have=%s resvol=%s want=%s\n",
              dev->VolHdr.VolumeName, dev->vol ? dev->vol->vol_name : "*None*",
              rctx.VolumeName);
        return 0;
      }
      if (!dcr->Can_i_use_volume()) {
        return 0; /* fail if volume on another drive */
      }
    }
  }

  // Check for unused autochanger drive
  if (rctx.autochanger_only && !dev->IsBusy()
      && dev->VolHdr.VolumeName[0] == 0) {
    // Device is available but not yet reserved, reserve it for us
    Dmsg1(debuglevel, "OK Res Unused autochanger %s.\n", dev->print_name());
    bstrncpy(dev->pool_name, dcr->pool_name, sizeof(dev->pool_name));
    bstrncpy(dev->pool_type, dcr->pool_type, sizeof(dev->pool_type));
    return 1; /* reserve drive */
  }

  // Handle the case that there are no writers
  if (dev->num_writers == 0) {
    // Now check if there are any reservations on the drive
    if (dev->NumReserved()) {
      return IsPoolOk(dcr);
    } else if (dev->CanAppend()) {
      if (IsPoolOk(dcr)) {
        return 1;
      } else {
        // Changing pool, unload old tape if any in drive
        Dmsg0(debuglevel,
              "OK dev: num_writers=0, not reserved, pool change, unload "
              "changer\n");
        // use SetUnload()
        UnloadAutochanger(dcr, -1);
      }
    }

    // Device is available but not yet reserved, reserve it for us
    Dmsg1(debuglevel, "OK Dev avail reserved %s\n", dev->print_name());
    bstrncpy(dev->pool_name, dcr->pool_name, sizeof(dev->pool_name));
    bstrncpy(dev->pool_type, dcr->pool_type, sizeof(dev->pool_type));
    return 1; /* reserve drive */
  }

  /* Check if the device is in append mode with writers (i.e. available if pool
   * is the same). */
  if (dev->CanAppend() || dev->num_writers > 0) {
    return IsPoolOk(dcr);
  } else {
    Pmsg1(000, T_("Logic error!!!! JobId=%u Should not get here.\n"),
          (int)jcr->JobId);
    Mmsg(jcr->errmsg,
         T_("3910 JobId=%u Logic error!!!! drive %s Should not get here.\n"),
         jcr->JobId, dev->print_name());
    QueueReserveMessage(jcr);
    Jmsg0(jcr, M_FATAL, 0, T_("Logic error!!!! Should not get here.\n"));

    return -1; /* error, should not get here */
  }
}

// Queue a reservation error or failure message for this jcr
static void QueueReserveMessage(JobControlRecord* jcr)
{
  int i;
  const char* msg;

  jcr->lock();

  auto msgs = jcr->sd_impl->reserve_msgs;
  if (!msgs) { goto bail_out; }
  // Look for duplicate message.  If found, do not insert
  for (i = msgs->size() - 1; i >= 0; i--) {
    msg = msgs->get(i);
    if (!msg) { goto bail_out; }

    // Comparison based on 4 digit message number
    if (bstrncmp(msg, jcr->errmsg, 4)) { goto bail_out; }
  }

  // Message unique, so insert it.
  jcr->sd_impl->reserve_msgs->push(strdup(jcr->errmsg));

bail_out:
  jcr->unlock();
}

// Pop and release any reservations messages
static void PopReserveMessages(JobControlRecord* jcr)
{
  char* msg;

  jcr->lock();
  auto msgs = jcr->sd_impl->reserve_msgs;
  if (!msgs) { goto bail_out; }
  while ((msg = (char*)msgs->pop())) { free(msg); }
bail_out:
  jcr->unlock();
}

// Also called from acquire.c
void ReleaseReserveMessages(JobControlRecord* jcr)
{
  PopReserveMessages(jcr);
  jcr->lock();
  if (!jcr->sd_impl->reserve_msgs) { goto bail_out; }
  delete jcr->sd_impl->reserve_msgs;
  jcr->sd_impl->reserve_msgs = NULL;

bail_out:
  jcr->unlock();
}

} /* namespace storagedaemon */
