/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2013 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2019 Bareos GmbH & Co. KG

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
 * Split from reserve.c October 2008
 */
/**
 * @file
 * Volume management functions for Storage Daemon
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/autochanger.h"
#include "include/jcr.h"
#include "lib/berrno.h"

namespace storagedaemon {

static const VolumeReservationItem emptyVol{};


const int debuglevel = 150;

static brwlock_t vol_list_lock;
static dlist* vol_list = NULL;
static dlist* read_vol_list = NULL;
static pthread_mutex_t read_vol_lock = PTHREAD_MUTEX_INITIALIZER;

/* Global static variables */
#ifdef SD_DEBUG_LOCK
int vol_list_lock_count = 0;
int read_vol_list_lock_count = 0;
#else
static int vol_list_lock_count = 0;
static int read_vol_list_lock_count = 0;
#endif

/* Forward referenced functions */
static void FreeVolItem(VolumeReservationItem* vol);
static void FreeReadVolItem(VolumeReservationItem* vol);
static VolumeReservationItem* new_vol_item(DeviceControlRecord* dcr,
                                           const char* VolumeName);
static void DebugListVolumes(const char* imsg);

/**
 * For append volumes the key is the VolumeName.
 */
static int CompareByVolumename(void* item1, void* item2)
{
  VolumeReservationItem* vol1 = (VolumeReservationItem*)item1;
  VolumeReservationItem* vol2 = (VolumeReservationItem*)item2;

  ASSERT(vol1->vol_name);
  ASSERT(vol2->vol_name);

  return strcmp(vol1->vol_name, vol2->vol_name);
}

/**
 * For read volumes the key is JobId, VolumeName.
 */
static int ReadCompare(void* item1, void* item2)
{
  VolumeReservationItem* vol1 = (VolumeReservationItem*)item1;
  VolumeReservationItem* vol2 = (VolumeReservationItem*)item2;

  ASSERT(vol1->vol_name);
  ASSERT(vol2->vol_name);

  if (vol1->GetJobid() == vol2->GetJobid()) {
    return strcmp(vol1->vol_name, vol2->vol_name);
  }

  if (vol1->GetJobid() < vol2->GetJobid()) { return -1; }

  return 1;
}

bool IsVolListEmpty() { return vol_list->empty(); }

/**
 *  Initialized the main volume list. Note, we are using a recursive lock.
 */
void InitVolListLock()
{
  int errstat;

  if ((errstat = RwlInit(&vol_list_lock, PRIO_SD_VOL_LIST)) != 0) {
    BErrNo be;
    Emsg1(M_ABORT, 0, _("Unable to initialize volume list lock. ERR=%s\n"),
          be.bstrerror(errstat));
  }
}

void TermVolListLock() { RwlDestroy(&vol_list_lock); }

/**
 * This allows a given thread to recursively call to LockVolumes()
 */
void _lockVolumes(const char* file, int line)
{
  int errstat;

  vol_list_lock_count++;
  if ((errstat = RwlWritelock_p(&vol_list_lock, file, line)) != 0) {
    BErrNo be;
    Emsg2(M_ABORT, 0, "RwlWritelock failure. stat=%d: ERR=%s\n", errstat,
          be.bstrerror(errstat));
  }
}

void _unLockVolumes()
{
  int errstat;

  vol_list_lock_count--;
  if ((errstat = RwlWriteunlock(&vol_list_lock)) != 0) {
    BErrNo be;
    Emsg2(M_ABORT, 0, "RwlWriteunlock failure. stat=%d: ERR=%s\n", errstat,
          be.bstrerror(errstat));
  }
}

void _lockReadVolumes(const char* file, int line)
{
  read_vol_list_lock_count++;
  pthread_mutex_lock(&read_vol_lock);
}

void _unLockReadVolumes()
{
  read_vol_list_lock_count--;
  pthread_mutex_unlock(&read_vol_lock);
}

/**
 * Add a volume to the read list.
 *
 * Note, we use VolumeReservationItem because it simplifies the code
 * even though, the only part of VolumeReservationItem that we need is
 * the volume name.  The same volume may be in the list
 * multiple times, but each one is distinguished by the
 * JobId.  We use JobId, VolumeName as the key.
 *
 * We can get called multiple times for the same volume because
 * when parsing the bsr, the volume name appears multiple times.
 */
void AddReadVolume(JobControlRecord* jcr, const char* VolumeName)
{
  VolumeReservationItem *nvol, *vol;

  nvol = new_vol_item(NULL, VolumeName);
  nvol->SetJobid(jcr->JobId);
  nvol->SetReading();
  LockReadVolumes();
  vol = (VolumeReservationItem*)read_vol_list->binary_insert(nvol, ReadCompare);
  if (vol != nvol) {
    FreeReadVolItem(nvol);
    Dmsg2(debuglevel, "read_vol=%s JobId=%d already in list.\n", VolumeName,
          jcr->JobId);
  } else {
    Dmsg2(debuglevel, "add_read_vol=%s JobId=%d\n", VolumeName, jcr->JobId);
  }
  UnlockReadVolumes();
}

/**
 * Remove a given volume name from the read list.
 */
void RemoveReadVolume(JobControlRecord* jcr, const char* VolumeName)
{
  VolumeReservationItem vol, *fvol;

  LockReadVolumes();
  vol = emptyVol;
  vol.vol_name = strdup(VolumeName);
  vol.SetJobid(jcr->JobId);

  fvol =
      (VolumeReservationItem*)read_vol_list->binary_search(&vol, ReadCompare);
  free(vol.vol_name);

  if (fvol) {
    Dmsg3(debuglevel, "remove_read_vol=%s JobId=%d found=%d\n", VolumeName,
          jcr->JobId, fvol != NULL);
  }
  if (fvol) {
    read_vol_list->remove(fvol);
    FreeReadVolItem(fvol);
  }
  UnlockReadVolumes();
  // pthread_cond_broadcast(&wait_next_vol);
}

/**
 * Search for a Volume name in the read Volume list.
 *
 * Returns: VolumeReservationItem entry on success
 *          NULL if the Volume is not in the list
 */
static VolumeReservationItem* find_read_volume(const char* VolumeName)
{
  VolumeReservationItem vol, *fvol;

  if (read_vol_list->empty()) {
    Dmsg0(debuglevel, "find_read_vol: read_vol_list empty.\n");
    return NULL;
  }

  /*
   * Do not lock reservations here
   */
  LockReadVolumes();
  vol = emptyVol;
  vol.vol_name = strdup(VolumeName);

  /*
   * Note, we do want a simple CompareByVolumename on volume name only here
   */
  fvol = (VolumeReservationItem*)read_vol_list->binary_search(
      &vol, CompareByVolumename);
  free(vol.vol_name);

  Dmsg2(debuglevel, "find_read_vol=%s found=%d\n", VolumeName, fvol != NULL);
  UnlockReadVolumes();

  return fvol;
}

/**
 * List Volumes -- this should be moved to status.c
 */
enum
{
  debug_lock = true,
  debug_nolock = false
};

static void DebugListVolumes(const char* imsg)
{
  VolumeReservationItem* vol;
  PoolMem msg(PM_MESSAGE);

  foreach_vol (vol) {
    if (vol->dev) {
      Mmsg(msg, "List %s: %s in_use=%d swap=%d on device %s\n", imsg,
           vol->vol_name, vol->IsInUse(), vol->IsSwapping(),
           vol->dev->print_name());
    } else {
      Mmsg(msg, "List %s: %s in_use=%d swap=%d no dev\n", imsg, vol->vol_name,
           vol->IsInUse(), vol->IsSwapping());
    }
    Dmsg1(debuglevel, "%s", msg.c_str());
  }
  endeach_vol(vol);
}

/**
 * Create a Volume item to put in the Volume list
 * Ensure that the device points to it.
 */
static VolumeReservationItem* new_vol_item(DeviceControlRecord* dcr,
                                           const char* VolumeName)
{
  VolumeReservationItem* vol;

  vol = (VolumeReservationItem*)malloc(sizeof(VolumeReservationItem));
  *vol = emptyVol;
  vol->vol_name = strdup(VolumeName);
  if (dcr) {
    vol->dev = dcr->dev;
    Dmsg3(debuglevel, "new Vol=%s at %p dev=%s\n", VolumeName, vol->vol_name,
          vol->dev->print_name());
  }
  vol->InitMutex();
  vol->IncUseCount();

  return vol;
}

static void FreeVolItem(VolumeReservationItem* vol)
{
  Device* dev = NULL;

  vol->DecUseCount();
  vol->Lock();
  if (vol->UseCount() > 0) {
    vol->Unlock();
    return;
  }
  vol->Unlock();
  free(vol->vol_name);
  if (vol->dev) { dev = vol->dev; }
  vol->DestroyMutex();
  free(vol);
  if (dev) { dev->vol = NULL; }
}

static void FreeReadVolItem(VolumeReservationItem* vol)
{
  Device* dev = NULL;

  vol->DecUseCount();
  vol->Lock();
  if (vol->UseCount() > 0) {
    vol->Unlock();
    return;
  }
  vol->Unlock();
  free(vol->vol_name);
  if (vol->dev) { dev = vol->dev; }
  vol->DestroyMutex();
  free(vol);
  if (dev) { dev->vol = NULL; }
}

/**
 * Put a new Volume entry in the Volume list. This
 * effectively reserves the volume so that it will
 * not be mounted again.
 *
 * If the device has any current volume associated with it,
 * and it is a different Volume, and the device is not busy,
 * we release the old Volume item and insert the new one.
 *
 * It is assumed that the device is free and locked so that
 * we can change the device structure.
 *
 * Some details of the Volume list handling:
 *
 *  1. The Volume list entry is attached to the drive (rather than
 *     attached to a job as it was previously. I.e. the drive that "owns"
 *     the volume (in use, mounted)
 *     must point to the volume (still to be maintained in a list).
 *
 *  2. The Volume is entered in the list when a drive is reserved.
 *
 *  3. When a drive is in use, the device code must appropriately update the
 *     volume name as it changes.
 *
 *     This code keeps the same list entry as long as the drive
 *     has any volume associated with it but the volume name in the list
 *     must be updated when the drive has a different volume mounted.
 *
 *  4. A job that has reserved a volume, can un-reserve the volume, and if the
 *     volume is not mounted, and not reserved, and not in use, it will be
 *     removed from the list.
 *
 *  5. If a job wants to reserve a drive with a different Volume from the one on
 *     the drive, it can re-use the drive for the new Volume.
 *
 *  6. If a job wants a Volume that is in a different drive, it can either use
 * the other drive or take the volume, only if the other drive is not in use or
 *     not reserved.
 *
 *  One nice aspect of this is that the reserve use count and the writer use
 * count already exist and are correctly programmed and will need no changes --
 * use counts are always very tricky.
 *
 *  The old code had a concept of "reserving" a Volume, but was changed
 *  to reserving and using a drive.  A volume is must be attached to (owned by)
 * a drive and can move from drive to drive or be unused given certain specific
 *  conditions of the drive.  The key is that the drive must "own" the Volume.
 *
 *  Return: VolumeReservationItem entry on success
 *          NULL volume busy on another drive
 */
VolumeReservationItem* reserve_volume(DeviceControlRecord* dcr,
                                      const char* VolumeName)
{
  VolumeReservationItem *vol, *nvol;
  Device* volatile dev = dcr->dev;

  if (JobCanceled(dcr->jcr)) { return NULL; }
  ASSERT(dev != NULL);

  Dmsg2(debuglevel, "enter reserve_volume=%s drive=%s\n", VolumeName,
        dcr->dev->print_name());

  /*
   * If aquiring a volume for writing it may not be on the read volume list.
   */
  if (me->filedevice_concurrent_read && dcr->IsWriting() &&
      find_read_volume(VolumeName)) {
    Mmsg(dcr->jcr->errmsg,
         _("Could not reserve volume \"%s\" for append, because it is read by "
           "another Job.\n"),
         dev->VolHdr.VolumeName);
    return NULL;
  }

  /*
   * We lock the reservations system here to ensure when adding a new volume
   * that no newly scheduled job can reserve it.
   */
  LockVolumes();
  if (debug_level >= debuglevel) { DebugListVolumes("begin reserve_volume"); }

  /*
   * First, remove any old volume attached to this device as it is no longer
   * used.
   */
  if (dev->vol) {
    vol = dev->vol;
    Dmsg4(debuglevel, "Vol attached=%s, newvol=%s volinuse=%d on %s\n",
          vol->vol_name, VolumeName, vol->IsInUse(), dev->print_name());
    /*
     * Make sure we don't remove the current volume we are inserting
     * because it was probably inserted by another job, or it
     * is not being used and is marked as not reserved.
     */
    if (bstrcmp(vol->vol_name, VolumeName)) {
      Dmsg2(debuglevel, "=== set reserved vol=%s dev=%s\n", VolumeName,
            vol->dev->print_name());
      goto get_out; /* Volume already on this device */
    } else {
      /*
       * Don't release a volume if it was reserved by someone other than us
       */
      if (vol->IsInUse() && !dcr->reserved_volume) {
        Dmsg1(debuglevel, "Cannot free vol=%s. It is reserved.\n",
              vol->vol_name);
        vol = NULL; /* vol in use */
        goto get_out;
      }
      Dmsg2(debuglevel, "reserve_vol free vol=%s at %p\n", vol->vol_name,
            vol->vol_name);

      /*
       * If old Volume is still mounted, must unload it
       */
      if (bstrcmp(vol->vol_name, dev->VolHdr.VolumeName)) {
        Dmsg0(50, "SetUnload\n");
        dev->SetUnload(); /* have to unload current volume */
      }
      FreeVolume(dev); /* Release old volume entry */

      if (debug_level >= debuglevel) { DebugListVolumes("reserve_vol free"); }
    }
  }

  /*
   * Create a new Volume entry
   */
  nvol = new_vol_item(dcr, VolumeName);

  /*
   * See if this is a request for reading a file type device which can be
   * accesses by multiple readers at once without disturbing each other.
   */
  if (me->filedevice_concurrent_read && !dcr->IsWriting() && dev->IsFile()) {
    nvol->SetJobid(dcr->jcr->JobId);
    nvol->SetReading();
    vol = nvol;
    dev->vol = vol;

    /*
     * Read volumes on file based devices are not inserted into the write volume
     * list.
     */
    goto get_out;
  } else {
    /*
     * Now try to insert the new Volume
     */
    vol = (VolumeReservationItem*)vol_list->binary_insert(nvol,
                                                          CompareByVolumename);
  }

  if (vol != nvol) {
    Dmsg2(debuglevel, "Found vol=%s dev-same=%d\n", vol->vol_name,
          dev == vol->dev);

    /*
     * At this point, a Volume with this name already is in the list,
     * so we simply release our new Volume entry. Note, this should
     * only happen if we are moving the volume from one drive to another.
     */
    Dmsg2(debuglevel, "reserve_vol free-tmp vol=%s at %p\n", vol->vol_name,
          vol->vol_name);

    /*
     * Clear dev pointer so that FreeVolItem() doesn't take away our volume.
     */
    nvol->dev = NULL; /* don't zap dev entry */
    FreeVolItem(nvol);

    if (vol->dev) {
      Dmsg2(debuglevel, "dev=%s vol->dev=%s\n", dev->print_name(),
            vol->dev->print_name());
    }

    /*
     * Check if we are trying to use the Volume on a different drive dev is our
     * device vol->dev is where the Volume we want is
     */
    if (dev != vol->dev) {
      /*
       * Caller wants to switch Volume to another device
       */
      if (!vol->dev->IsBusy() && !vol->IsSwapping()) {
        slot_number_t slot;

        Dmsg3(debuglevel, "==== Swap vol=%s from dev=%s to %s\n", VolumeName,
              vol->dev->print_name(), dev->print_name());
        FreeVolume(dev); /* free any volume attached to our drive */
        Dmsg1(50, "SetUnload dev=%s\n", dev->print_name());
        dev->SetUnload();      /* Unload any volume that is on our drive */
        dcr->SetDev(vol->dev); /* temp point to other dev */
        slot = GetAutochangerLoadedSlot(dcr); /* get slot on other drive */
        dcr->SetDev(dev);                     /* restore dev */
        vol->SetSlotNumber(slot);             /* save slot */
        vol->dev->SetUnload();                /* unload the other drive */
        vol->SetSwapping();                   /* swap from other drive */
        dev->swap_dev = vol->dev;             /* remember to get this vol */
        dev->SetLoad();                       /* then reload on our drive */
        vol->dev->vol = NULL; /* remove volume from other drive */
        vol->dev = dev;       /* point the Volume at our drive */
        dev->vol = vol;       /* point our drive at the Volume */
      } else {
        Jmsg7(dcr->jcr, M_WARNING, 0,
              "Need volume from other drive, but swap not possible. "
              "Status: read=%d num_writers=%d num_reserve=%d swap=%d "
              "vol=%s from dev=%s to %s\n",
              vol->dev->CanRead(), vol->dev->num_writers,
              vol->dev->NumReserved(), vol->IsSwapping(), VolumeName,
              vol->dev->print_name(), dev->print_name());
        if (vol->IsSwapping() && dev->swap_dev) {
          Dmsg3(debuglevel, "Swap failed vol=%s from=%s to dev=%s\n",
                vol->vol_name, dev->swap_dev->print_name(), dev->print_name());
        } else {
          Dmsg3(debuglevel, "Swap failed vol=%s from=%p to dev=%s\n",
                vol->vol_name, dev->swap_dev, dev->print_name());
        }

        if (debug_level >= debuglevel) { DebugListVolumes("failed swap"); }

        vol = NULL; /* device busy */
        goto get_out;
      }
    } else {
      dev->vol = vol;
    }
  } else {
    dev->vol = vol; /* point to newly inserted volume */
  }

get_out:
  if (vol) {
    Dmsg2(debuglevel, "=== set in_use. vol=%s dev=%s\n", vol->vol_name,
          vol->dev->print_name());
    vol->SetInUse();
    dcr->reserved_volume = true;
    bstrncpy(dcr->VolumeName, vol->vol_name, sizeof(dcr->VolumeName));
  }

  if (debug_level >= debuglevel) { DebugListVolumes("end new volume"); }

  UnlockVolumes();
  return vol;
}

/**
 * Start walk of vol chain
 * The proper way to walk the vol chain is:
 *
 * VolumeReservationItem *vol;
 * foreach_vol(vol) {
 *    ...
 * }
 * endeach_vol(vol);
 *
 * It is possible to leave out the endeach_vol(vol), but in that case,
 * the last vol referenced must be explicitly released with:
 *
 * FreeVolItem(vol);
 */
VolumeReservationItem* vol_walk_start()
{
  VolumeReservationItem* vol;
  LockVolumes();
  vol = (VolumeReservationItem*)vol_list->first();
  if (vol) {
    vol->IncUseCount();
    Dmsg2(debuglevel, "Inc walk_start UseCount=%d volname=%s\n",
          vol->UseCount(), vol->vol_name);
  }
  UnlockVolumes();

  return vol;
}

/**
 * Get next vol from chain, and release current one
 */
VolumeReservationItem* VolWalkNext(VolumeReservationItem* prev_vol)
{
  VolumeReservationItem* vol;

  LockVolumes();
  vol = (VolumeReservationItem*)vol_list->next(prev_vol);
  if (vol) {
    vol->IncUseCount();
    Dmsg2(debuglevel, "Inc walk_next UseCount=%d volname=%s\n", vol->UseCount(),
          vol->vol_name);
  }
  if (prev_vol) { FreeVolItem(prev_vol); }
  UnlockVolumes();

  return vol;
}

/**
 * Release last vol referenced
 */
void VolWalkEnd(VolumeReservationItem* vol)
{
  if (vol) {
    LockVolumes();
    Dmsg2(debuglevel, "Free walk_end UseCount=%d volname=%s\n", vol->UseCount(),
          vol->vol_name);
    FreeVolItem(vol);
    UnlockVolumes();
  }
}

/*
 * Start walk of vol chain
 * The proper way to walk the vol chain is:
 *
 * VolumeReservationItem *vol;
 * foreach_read_vol(vol) {
 *    ...
 * }
 * endeach_read_vol(vol);
 *
 * It is possible to leave out the endeach_read_vol(vol), but in that case,
 * the last vol referenced must be explicitly released with:
 *
 * FreeReadVolItem(vol);
 */
VolumeReservationItem* read_vol_walk_start()
{
  VolumeReservationItem* vol;
  LockReadVolumes();
  vol = (VolumeReservationItem*)read_vol_list->first();
  if (vol) {
    vol->IncUseCount();
    Dmsg2(debuglevel, "Inc walk_start UseCount=%d volname=%s\n",
          vol->UseCount(), vol->vol_name);
  }
  UnlockReadVolumes();

  return vol;
}

/*
 * Get next vol from chain, and release current one
 */
VolumeReservationItem* ReadVolWalkNext(VolumeReservationItem* prev_vol)
{
  VolumeReservationItem* vol;

  LockReadVolumes();
  vol = (VolumeReservationItem*)read_vol_list->next(prev_vol);
  if (vol) {
    vol->IncUseCount();
    Dmsg2(debuglevel, "Inc walk_next UseCount=%d volname=%s\n", vol->UseCount(),
          vol->vol_name);
  }
  if (prev_vol) { FreeReadVolItem(prev_vol); }
  UnlockReadVolumes();

  return vol;
}

/*
 * Release last vol referenced
 */
void ReadVolWalkEnd(VolumeReservationItem* vol)
{
  if (vol) {
    LockReadVolumes();
    Dmsg2(debuglevel, "Free walk_end UseCount=%d volname=%s\n", vol->UseCount(),
          vol->vol_name);
    FreeReadVolItem(vol);
    UnlockReadVolumes();
  }
}

/**
 * Search for a Volume name in the Volume list.
 *
 * Returns: VolumeReservationItem entry on success
 *          NULL if the Volume is not in the list
 */
static VolumeReservationItem* find_volume(const char* VolumeName)
{
  VolumeReservationItem vol, *fvol;

  if (vol_list->empty()) { return NULL; }
  /* Do not lock reservations here */
  LockVolumes();
  vol.vol_name = strdup(VolumeName);
  fvol = (VolumeReservationItem*)vol_list->binary_search(&vol,
                                                         CompareByVolumename);
  free(vol.vol_name);
  Dmsg2(debuglevel, "find_vol=%s found=%d\n", VolumeName, fvol != NULL);

  if (debug_level >= debuglevel) { DebugListVolumes("find_volume"); }

  UnlockVolumes();
  return fvol;
}

/**
 * Free a Volume from the Volume list if it is no longer used
 * Note, for tape drives we want to remember where the Volume
 * was when last used, so rather than free the volume entry,
 * we simply mark it "not reserved" so when the drive is really
 * needed for another volume, we can reuse it.
 *
 * Returns: true if the Volume found and "removed" from the list
 *          false if the Volume is not in the list or is in use
 */
bool VolumeUnused(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;

  if (!dev->vol) {
    Dmsg1(debuglevel, "vol_unused: no vol on %s\n", dev->print_name());
    if (debug_level >= debuglevel) {
      DebugListVolumes("null vol cannot unreserve_volume");
    }

    return false;
  }

  Dmsg1(debuglevel, "=== clear in_use vol=%s\n", dev->vol->vol_name);
  dev->vol->ClearInUse();

  if (dev->vol->IsSwapping()) {
    Dmsg1(debuglevel, "vol_unused: vol being swapped on %s\n",
          dev->print_name());

    if (debug_level >= debuglevel) {
      DebugListVolumes("swapping vol cannot FreeVolume");
    }
    return false;
  }

  /*
   * If this is a tape, we do not free the volume, rather we wait
   * until the autoloader unloads it, or until another tape is
   * explicitly read in this drive. This allows the SD to remember
   * where the tapes are or last were.
   */
  Dmsg4(debuglevel,
        "=== set not reserved vol=%s num_writers=%d dev_reserved=%d dev=%s\n",
        dev->vol->vol_name, dev->num_writers, dev->NumReserved(),
        dev->print_name());
  if (dev->IsTape() || dev->IsAutochanger()) {
    return true;
  } else {
    /*
     * Note, this frees the volume reservation entry, but the file descriptor
     * remains open with the OS.
     */
    return FreeVolume(dev);
  }
}

/**
 * Unconditionally release the volume entry
 */
bool FreeVolume(Device* dev)
{
  VolumeReservationItem* vol;

  LockVolumes();
  vol = dev->vol;
  if (vol == NULL) {
    Dmsg1(debuglevel, "No vol on dev %s\n", dev->print_name());
    UnlockVolumes();
    return false;
  }

  /*
   * Don't free a volume while it is being swapped
   */
  if (!vol->IsSwapping()) {
    Dmsg1(debuglevel, "=== clear in_use vol=%s\n", vol->vol_name);
    dev->vol = NULL;

    /*
     * Volume is on write volume list if one of the folling is applicable:
     *  - The volume is written to.
     *  - Config option filedevice_concurrent_read is not on.
     *  - The device is not of type File.
     */
    if (vol->IsWriting() || !me->filedevice_concurrent_read || !dev->IsFile()) {
      vol_list->remove(vol);
    }
    Dmsg2(debuglevel, "=== remove volume %s dev=%s\n", vol->vol_name,
          dev->print_name());
    FreeVolItem(vol);

    if (debug_level >= debuglevel) { DebugListVolumes("FreeVolume"); }
  } else {
    Dmsg1(debuglevel, "=== cannot clear swapping vol=%s\n", vol->vol_name);
  }
  UnlockVolumes();
  // pthread_cond_broadcast(&wait_next_vol);

  return true;
}

/**
 * Create the Volume list
 */
void CreateVolumeLists()
{
  VolumeReservationItem* vol = NULL;
  if (vol_list == NULL) { vol_list = new dlist(vol, &vol->link); }
  if (read_vol_list == NULL) { read_vol_list = new dlist(vol, &vol->link); }
}

/**
 * Free normal append volumes list
 */
static inline void FreeVolumeList(const char* what, dlist* vollist)
{
  VolumeReservationItem* vol;

  foreach_dlist (vol, vollist) {
    if (vol->dev) {
      Dmsg3(debuglevel, "free %s Volume=%s dev=%s\n", what, vol->vol_name,
            vol->dev->print_name());
    } else {
      Dmsg2(debuglevel, "free %s Volume=%s No dev\n", what, vol->vol_name);
    }
    free(vol->vol_name);
    vol->vol_name = NULL;
    vol->DestroyMutex();
  }
}

/**
 * Release all Volumes from the list
 */
void FreeVolumeLists()
{
  if (vol_list) {
    LockVolumes();
    FreeVolumeList("vol_list", vol_list);
    delete vol_list;
    vol_list = NULL;
    UnlockVolumes();
  }

  if (read_vol_list) {
    LockReadVolumes();
    FreeVolumeList("read_vol_list", read_vol_list);
    delete read_vol_list;
    read_vol_list = NULL;
    UnlockReadVolumes();
  }
}

/**
 * Determine if caller can write on volume
 */
bool DeviceControlRecord::Can_i_write_volume()
{
  VolumeReservationItem* vol;

  vol = find_read_volume(VolumeName);
  if (vol) {
    Dmsg1(100, "Found in read list; cannot write vol=%s\n", VolumeName);
    return false;
  }

  return Can_i_use_volume();
}

/**
 * Determine if caller can read or write volume
 */
bool DeviceControlRecord::Can_i_use_volume()
{
  bool rtn = true;
  VolumeReservationItem* vol;

  if (JobCanceled(jcr)) { return false; }
  LockVolumes();
  vol = find_volume(VolumeName);
  if (!vol) {
    Dmsg1(debuglevel, "Vol=%s not in use.\n", VolumeName);
    goto get_out; /* vol not in list */
  }
  ASSERT(vol->dev != NULL);

  if (dev == vol->dev) { /* same device OK */
    Dmsg1(debuglevel, "Vol=%s on same dev.\n", VolumeName);
    goto get_out;
  } else {
    Dmsg3(debuglevel, "Vol=%s on %s we have %s\n", VolumeName,
          vol->dev->print_name(), dev->print_name());
  }
  /* ***FIXME*** check this ... */
  if (!vol->dev->IsBusy()) {
    Dmsg2(debuglevel, "Vol=%s dev=%s not busy.\n", VolumeName,
          vol->dev->print_name());
    goto get_out;
  } else {
    Dmsg2(debuglevel, "Vol=%s dev=%s busy.\n", VolumeName,
          vol->dev->print_name());
  }
  Dmsg2(debuglevel, "Vol=%s in use by %s.\n", VolumeName,
        vol->dev->print_name());
  rtn = false;

get_out:
  UnlockVolumes();
  return rtn;
}

/**
 * Create a temporary copy of the volume list.  We do this,
 * to avoid having the volume list locked during the
 * call to ReserveDevice(), which would cause a deadlock.
 *
 * Note, we may want to add an update counter on the vol_list
 * so that if it is modified while we are traversing the copy
 * we can take note and act accordingly (probably redo the
 * search at least a few times).
 */
dlist* dup_vol_list(JobControlRecord* jcr)
{
  dlist* temp_vol_list;
  VolumeReservationItem* vol = NULL;

  Dmsg0(debuglevel, "lock volumes\n");

  Dmsg0(debuglevel, "duplicate vol list\n");
  temp_vol_list = new dlist(vol, &vol->link);
  foreach_vol (vol) {
    VolumeReservationItem *nvol, *tvol;

    tvol = new_vol_item(NULL, vol->vol_name);
    tvol->dev = vol->dev;
    nvol = (VolumeReservationItem*)temp_vol_list->binary_insert(
        tvol, CompareByVolumename);
    if (tvol != nvol) {
      tvol->dev = NULL; /* don't zap dev entry */
      FreeVolItem(tvol);
      Pmsg0(000, "Logic error. Duplicating vol list hit duplicate.\n");
      Jmsg(jcr, M_WARNING, 0,
           "Logic error. Duplicating vol list hit duplicate.\n");
    }
  }
  endeach_vol(vol);
  Dmsg0(debuglevel, "unlock volumes\n");

  return temp_vol_list;
}

/**
 * Free the specified temp list.
 */
void FreeTempVolList(dlist* temp_vol_list)
{
  FreeVolumeList("temp_vol_list", temp_vol_list);
  delete temp_vol_list;
}

} /* namespace storagedaemon */
