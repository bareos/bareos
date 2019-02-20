/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2013 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, August MMII
 */
/**
 * @file
 * Routines to acquire and release a device for read/write
 */

#include "include/bareos.h" /* pull in global headers */
#include "stored/stored.h"  /* pull in Storage Daemon headers */
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/bsr.h"
#include "stored/job.h"
#include "stored/label.h"
#include "stored/sd_plugins.h"
#include "stored/sd_stats.h"
#include "stored/wait.h"
#include "lib/edit.h"
#include "include/jcr.h"
#include "stored/block.h"

namespace storagedaemon {

DeviceControlRecord::DeviceControlRecord()
{
  PoolMem errmsg(PM_MESSAGE);
  int errstat;

  tid = pthread_self();
  spool_fd = -1;
  if ((errstat = pthread_mutex_init(&mutex_, NULL)) != 0) {
    BErrNo be;

    Mmsg(errmsg, _("Unable to init mutex: ERR=%s\n"), be.bstrerror(errstat));
    Jmsg0(NULL, M_ERROR_TERM, 0, errmsg.c_str());
  }

  if ((errstat = pthread_mutex_init(&r_mutex, NULL)) != 0) {
    BErrNo be;

    Mmsg(errmsg, _("Unable to init r_mutex: ERR=%s\n"), be.bstrerror(errstat));
    Jmsg0(NULL, M_ERROR_TERM, 0, errmsg.c_str());
  }
}

static int const rdebuglevel = 100;

/* Forward referenced functions */
static void AttachDcrToDev(DeviceControlRecord* dcr);
static void DetachDcrFromDev(DeviceControlRecord* dcr);
static void SetDcrFromVol(DeviceControlRecord* dcr, VolumeList* vol);

/**
 * Acquire device for reading.
 *  The drive should have previously been reserved by calling
 *  ReserveDeviceForRead(). We read the Volume label from the block and
 *  leave the block pointers just after the label.
 *
 *  Returns: NULL if failed for any reason
 *           dcr  if successful
 */
bool AcquireDeviceForRead(DeviceControlRecord* dcr)
{
  Device* dev;
  JobControlRecord* jcr = dcr->jcr;
  bool retval = false;
  bool tape_previously_mounted;
  VolumeList* vol;
  bool try_autochanger = true;
  int i;
  int vol_label_status;
  int retry = 0;

  Enter(rdebuglevel);
  dev = dcr->dev;
  dev->Lock_read_acquire();
  Dmsg2(rdebuglevel, "dcr=%p dev=%p\n", dcr, dcr->dev);
  Dmsg2(rdebuglevel, "MediaType dcr=%s dev=%s\n", dcr->media_type,
        dev->device->media_type);
  dev->dblock(BST_DOING_ACQUIRE);

  if (dev->num_writers > 0) {
    Jmsg2(jcr, M_FATAL, 0,
          _("Acquire read: num_writers=%d not zero. Job %d canceled.\n"),
          dev->num_writers, jcr->JobId);
    goto get_out;
  }

  /* Find next Volume, if any */
  vol = jcr->VolList;
  if (!vol) {
    char ed1[50];
    Jmsg(jcr, M_FATAL, 0,
         _("No volumes specified for reading. Job %s canceled.\n"),
         edit_int64(jcr->JobId, ed1));
    goto get_out;
  }
  jcr->CurReadVolume++;
  for (i = 1; i < jcr->CurReadVolume; i++) { vol = vol->next; }
  if (!vol) {
    Jmsg(jcr, M_FATAL, 0,
         _("Logic error: no next volume to read. Numvol=%d Curvol=%d\n"),
         jcr->NumReadVolumes, jcr->CurReadVolume);
    goto get_out; /* should not happen */
  }
  SetDcrFromVol(dcr, vol);

  Dmsg2(rdebuglevel, "Want Vol=%s Slot=%d\n", vol->VolumeName, vol->Slot);

  /*
   * If the MediaType requested for this volume is not the
   * same as the current drive, we attempt to find the same
   * device that was used to write the orginal volume.  If
   * found, we switch to using that device.
   *
   * N.B. A lot of routines rely on the dcr pointer not changing
   * ReadRecords.c even has multiple dcrs cached, so we take care
   * here to release all important parts of the dcr and re-acquire
   * them such as the block pointer (size may change), but we do
   * not release the dcr.
   */
  Dmsg2(rdebuglevel, "MediaType dcr=%s dev=%s\n", dcr->media_type,
        dev->device->media_type);
  if (dcr->media_type[0] &&
      !bstrcmp(dcr->media_type, dev->device->media_type)) {
    ReserveContext rctx;
    DirectorStorage* store;
    int status;

    Jmsg3(jcr, M_INFO, 0,
          _("Changing read device. Want Media Type=\"%s\" have=\"%s\"\n"
            "  device=%s\n"),
          dcr->media_type, dev->device->media_type, dev->print_name());
    Dmsg3(rdebuglevel,
          "Changing read device. Want Media Type=\"%s\" have=\"%s\"\n"
          "  device=%s\n",
          dcr->media_type, dev->device->media_type, dev->print_name());

    dev->dunblock(DEV_UNLOCKED);

    LockReservations();
    memset(&rctx, 0, sizeof(ReserveContext));
    rctx.jcr = jcr;
    jcr->read_dcr = dcr;
    jcr->reserve_msgs = New(alist(10, not_owned_by_alist));
    rctx.any_drive = true;
    rctx.device_name = vol->device;
    store = new DirectorStorage;
    memset(store, 0, sizeof(DirectorStorage));
    store->name[0] = 0; /* No dir name */
    bstrncpy(store->media_type, vol->MediaType, sizeof(store->media_type));
    bstrncpy(store->pool_name, dcr->pool_name, sizeof(store->pool_name));
    bstrncpy(store->pool_type, dcr->pool_type, sizeof(store->pool_type));
    store->append = false;
    rctx.store = store;
    CleanDevice(dcr); /* clean up the dcr */

    /*
     * Search for a new device
     */
    status = SearchResForDevice(rctx);
    ReleaseReserveMessages(jcr); /* release queued messages */
    UnlockReservations();

    if (status == 1) { /* found new device to use */
      /*
       * Switching devices, so acquire lock on new device, then release the old
       * one.
       */
      dcr->dev->Lock_read_acquire(); /* lock new one */
      dev->Unlock_read_acquire();    /* release old one */
      dev = dcr->dev;                /* get new device pointer */
      dev->dblock(BST_DOING_ACQUIRE);

      dcr->VolumeName[0] = 0;
      Jmsg(jcr, M_INFO, 0,
           _("Media Type change.  New read device %s chosen.\n"),
           dev->print_name());
      Dmsg1(50, "Media Type change.  New read device %s chosen.\n",
            dev->print_name());

      bstrncpy(dcr->VolumeName, vol->VolumeName, sizeof(dcr->VolumeName));
      dcr->setVolCatName(vol->VolumeName);
      bstrncpy(dcr->media_type, vol->MediaType, sizeof(dcr->media_type));
      dcr->VolCatInfo.Slot = vol->Slot;
      dcr->VolCatInfo.InChanger = vol->Slot > 0;
      bstrncpy(dcr->pool_name, store->pool_name, sizeof(dcr->pool_name));
      bstrncpy(dcr->pool_type, store->pool_type, sizeof(dcr->pool_type));
    } else {
      /* error */
      Jmsg1(jcr, M_FATAL, 0,
            _("No suitable device found to read Volume \"%s\"\n"),
            vol->VolumeName);
      Dmsg1(rdebuglevel, "No suitable device found to read Volume \"%s\"\n",
            vol->VolumeName);
      goto get_out;
    }
  }
  Dmsg2(rdebuglevel, "MediaType dcr=%s dev=%s\n", dcr->media_type,
        dev->device->media_type);

  dev->ClearUnload();

  if (dev->vol && dev->vol->IsSwapping()) {
    dev->vol->SetSlot(vol->Slot);
    Dmsg3(rdebuglevel, "swapping: slot=%d Vol=%s dev=%s\n", dev->vol->GetSlot(),
          dev->vol->vol_name, dev->print_name());
  }

  InitDeviceWaitTimers(dcr);

  tape_previously_mounted =
      dev->CanRead() || dev->CanAppend() || dev->IsLabeled();
  // tape_initially_mounted = tape_previously_mounted;

  /*
   * Volume info is always needed because of VolParts
   */
  Dmsg1(rdebuglevel, "DirGetVolumeInfo vol=%s\n", dcr->VolumeName);
  if (!dcr->DirGetVolumeInfo(GET_VOL_INFO_FOR_READ)) {
    Dmsg2(rdebuglevel, "dir_get_vol_info failed for vol=%s: %s\n",
          dcr->VolumeName, jcr->errmsg);
    Jmsg1(jcr, M_WARNING, 0, "Read acquire: %s", jcr->errmsg);
  }
  dev->SetLoad(); /* set to load volume */

  while (1) {
    /*
     * If not polling limit retries
     */
    if (!dev->poll && retry++ > 10) { break; }

    /*
     * See if we are changing the volume in the device.
     * If so we need to force a reread of the tape label.
     */
    if (dev->device->drive_crypto_enabled ||
        (dev->HasCap(CAP_ALWAYSOPEN) &&
         !bstrcmp(dev->VolHdr.VolumeName, dcr->VolumeName))) {
      dev->ClearLabeled();
    }

    if (JobCanceled(jcr)) {
      char ed1[50];
      Mmsg1(dev->errmsg, _("Job %s canceled.\n"), edit_int64(jcr->JobId, ed1));
      Jmsg(jcr, M_INFO, 0, dev->errmsg);
      goto get_out; /* error return */
    }

    dcr->DoUnload();
    dcr->DoSwapping(false /*!IsWriting*/);
    dcr->DoLoad(false /*!IsWriting*/);
    SetDcrFromVol(dcr, vol); /* refresh dcr with desired volume info */

    /*
     * This code ensures that the device is ready for reading. If it is a file,
     * it opens it. If it is a tape, it checks the volume name
     */
    Dmsg1(rdebuglevel, "stored: open vol=%s\n", dcr->VolumeName);
    if (!dev->open(dcr, OPEN_READ_ONLY)) {
      if (!dev->poll) {
        Jmsg3(jcr, M_WARNING, 0,
              _("Read open device %s Volume \"%s\" failed: ERR=%s\n"),
              dev->print_name(), dcr->VolumeName, dev->bstrerror());
      }
      goto default_path;
    }
    Dmsg1(rdebuglevel, "opened dev %s OK\n", dev->print_name());

    /*
     * See if we are changing the volume in the device.
     * If so we need to force a reread of the tape label.
     */
    if (!dev->device->drive_crypto_enabled && dev->HasCap(CAP_ALWAYSOPEN) &&
        bstrcmp(dev->VolHdr.VolumeName, dcr->VolumeName)) {
      vol_label_status = VOL_OK;
    } else {
      /*
       * Read Volume Label
       */
      Dmsg0(rdebuglevel, "calling read-vol-label\n");
      vol_label_status = ReadDevVolumeLabel(dcr);
    }

    switch (vol_label_status) {
      case VOL_OK:
        Dmsg0(rdebuglevel, "Got correct volume.\n");
        retval = true;
        dev->VolCatInfo = dcr->VolCatInfo; /* structure assignment */
        break;                             /* got it */
      case VOL_IO_ERROR:
        Dmsg0(rdebuglevel, "IO Error\n");
        /*
         * Send error message generated by ReadDevVolumeLabel() only when we
         * really had a tape mounted. This supresses superfluous error messages
         * when nothing is mounted.
         */
        if (tape_previously_mounted) {
          Jmsg(jcr, M_WARNING, 0, "Read acquire: %s", jcr->errmsg);
        }
        goto default_path;
      case VOL_NAME_ERROR:
        Dmsg3(rdebuglevel, "Vol name=%s want=%s drv=%s.\n",
              dev->VolHdr.VolumeName, dcr->VolumeName, dev->print_name());
        if (dev->IsVolumeToUnload()) { goto default_path; }
        dev->SetUnload(); /* force unload of unwanted tape */
        if (!UnloadAutochanger(dcr, -1)) {
          /*
           * At least free the device so we can re-open with correct volume
           */
          dev->close(dcr);
          FreeVolume(dev);
        }
        dev->SetLoad();
        /* Fall through */
      default:
        Jmsg1(jcr, M_WARNING, 0, "Read acquire: %s", jcr->errmsg);
      default_path:
        Dmsg0(rdebuglevel, "default path\n");
        tape_previously_mounted = true;

        /*
         * If the device requires mount, close it, so the device can be ejected.
         */
        if (dev->RequiresMount()) {
          dev->close(dcr);
          FreeVolume(dev);
        }

        /*
         * Call autochanger only once unless ask_sysop called
         */
        if (try_autochanger) {
          int status;
          Dmsg2(rdebuglevel, "calling autoload Vol=%s Slot=%d\n",
                dcr->VolumeName, dcr->VolCatInfo.Slot);
          status = AutoloadDevice(dcr, 0, NULL);
          if (status > 0) {
            try_autochanger = false;
            continue; /* try reading volume mounted */
          }
        }

        /*
         * Mount a specific volume and no other
         */
        Dmsg0(rdebuglevel, "calling dir_ask_sysop\n");
        if (!dcr->DirAskSysopToMountVolume(ST_READREADY)) {
          goto get_out; /* error return */
        }

        /*
         * Volume info is always needed because of VolParts
         */
        Dmsg1(150, "DirGetVolumeInfo vol=%s\n", dcr->VolumeName);
        if (!dcr->DirGetVolumeInfo(GET_VOL_INFO_FOR_READ)) {
          Dmsg2(150, "dir_get_vol_info failed for vol=%s: %s\n",
                dcr->VolumeName, jcr->errmsg);
          Jmsg1(jcr, M_WARNING, 0, "Read acquire: %s", jcr->errmsg);
        }
        dev->SetLoad(); /* set to load volume */

        try_autochanger = true; /* permit trying the autochanger again */

        continue; /* try reading again */
    }             /* end switch */
    break;
  } /* end while loop */

  if (!retval) {
    Jmsg1(jcr, M_FATAL, 0,
          _("Too many errors trying to mount device %s for reading.\n"),
          dev->print_name());
    goto get_out;
  }

  dev->ClearAppend();
  dev->SetRead();
  jcr->sendJobStatus(JS_Running);
  Jmsg(jcr, M_INFO, 0, _("Ready to read from volume \"%s\" on device %s.\n"),
       dcr->VolumeName, dev->print_name());

get_out:
  dev->Lock();
  dcr->ClearReserved();

  /*
   * Normally we are blocked, but in at least one error case above we are not
   * blocked because we unsuccessfully tried changing devices.
   */
  if (dev->IsBlocked()) {
    dev->dunblock(DEV_LOCKED);
  } else {
    dev->Unlock(); /* dunblock() unlock the device too */
  }

  Dmsg2(rdebuglevel, "dcr=%p dev=%p\n", dcr, dcr->dev);
  Dmsg2(rdebuglevel, "MediaType dcr=%s dev=%s\n", dcr->media_type,
        dev->device->media_type);

  dev->Unlock_read_acquire();

  Leave(rdebuglevel);

  return retval;
}

/**
 * Acquire device for writing. We permit multiple writers.
 *  If this is the first one, we read the label.
 *
 *  Returns: NULL if failed for any reason
 *           dcr if successful.
 *   Note, normally ReserveDeviceForAppend() is called
 *   before this routine.
 */
DeviceControlRecord* AcquireDeviceForAppend(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  JobControlRecord* jcr = dcr->jcr;
  bool retval = false;
  bool have_vol = false;

  Enter(200);
  InitDeviceWaitTimers(dcr);

  dev->Lock_acquire(); /* only one job at a time */
  dev->Lock();
  Dmsg1(100, "acquire_append device is %s\n", dev->IsTape() ? "tape" : "disk");

  /*
   * With the reservation system, this should not happen
   */
  if (dev->CanRead()) {
    Jmsg1(jcr, M_FATAL, 0,
          _("Want to append, but device %s is busy reading.\n"),
          dev->print_name());
    Dmsg1(200, "Want to append but device %s is busy reading.\n",
          dev->print_name());
    goto get_out;
  }

  dev->ClearUnload();

  /*
   * have_vol defines whether or not MountNextWriteVolume should
   * ask the Director again about what Volume to use.
   */
  if (dev->CanAppend() && dcr->IsSuitableVolumeMounted() &&
      !bstrcmp(dcr->VolCatInfo.VolCatStatus, "Recycle")) {
    Dmsg0(190, "device already in append.\n");
    /*
     * At this point, the correct tape is already mounted, so
     * we do not need to do MountNextWriteVolume(), unless
     * we need to recycle the tape.
     */
    if (dev->num_writers == 0) {
      dev->VolCatInfo = dcr->VolCatInfo; /* structure assignment */
    }
    have_vol = dcr->IsTapePositionOk();
  }

  if (!have_vol) {
    dev->rLock(true);
    BlockDevice(dev, BST_DOING_ACQUIRE);
    dev->Unlock();
    Dmsg1(190, "jid=%u Do mount_next_write_vol\n", (uint32_t)jcr->JobId);
    if (!dcr->MountNextWriteVolume()) {
      if (!JobCanceled(jcr)) {
        /* Reduce "noise" -- don't print if job canceled */
        Jmsg(jcr, M_FATAL, 0, _("Could not ready device %s for append.\n"),
             dev->print_name());
        Dmsg1(200, "Could not ready device %s for append.\n",
              dev->print_name());
      }
      dev->Lock();
      UnblockDevice(dev);
      goto get_out;
    }
    Dmsg2(190, "Output pos=%u:%u\n", dcr->dev->file, dcr->dev->block_num);
    dev->Lock();
    UnblockDevice(dev);
  }

  dev->num_writers++; /* we are now a writer */
  if (jcr->NumWriteVolumes == 0) { jcr->NumWriteVolumes = 1; }
  dev->VolCatInfo.VolCatJobs++; /* increment number of jobs on vol */
  Dmsg4(100, "=== nwriters=%d nres=%d vcatjob=%d dev=%s\n", dev->num_writers,
        dev->NumReserved(), dev->VolCatInfo.VolCatJobs, dev->print_name());
  dcr->DirUpdateVolumeInfo(false, false); /* send Volume info to Director */
  retval = true;

get_out:
  /*
   * Don't plugin close here, we might have multiple writers
   */
  dcr->ClearReserved();
  dev->Unlock();
  dev->Unlock_acquire();
  Leave(200);

  return retval ? dcr : NULL;
}

/**
 * This job is done, so release the device. From a Unix standpoint, the device
 * remains open.
 *
 * Note, if we were spooling, we may enter with the device blocked.
 * We unblock at the end, only if it was us who blocked the device.
 */
bool ReleaseDevice(DeviceControlRecord* dcr)
{
  utime_t now;
  JobControlRecord* jcr = dcr->jcr;
  Device* dev = dcr->dev;
  bool retval = true;
  char tbuf[100];
  int was_blocked = BST_NOT_BLOCKED;

  Jmsg(jcr, M_INFO, 0, "Releasing device %s.\n", dev->print_name());

  /*
   * Capture job statistics now that we are done using this device.
   */
  now = (utime_t)time(NULL);
  UpdateJobStatistics(jcr, now);

  /*
   * Some devices do cache write operations (e.g. droplet_device).
   * Therefore flushing the cache is required to determine
   * if a job have been written successfully.
   * As a flush operation can take quite a long time,
   * this must be done before acquiring locks.
   * A previous implementation did the flush inside dev->close(),
   * which resulted in various locking problems.
   */
  if (!JobCanceled(jcr)) {
    if (!dev->flush(dcr)) {
      Jmsg(jcr, M_FATAL, 0, "Failed to flush device %s.\n", dev->print_name());
    }
  }

  dev->Lock();
  if (!dev->IsBlocked()) {
    BlockDevice(dev, BST_RELEASING);
  } else {
    was_blocked = dev->blocked();
    dev->SetBlocked(BST_RELEASING);
  }
  LockVolumes();
  Dmsg1(100, "releasing device %s\n", dev->print_name());

  /*
   * If device is reserved, job never started, so release the reserve here
   */
  dcr->ClearReserved();

  if (dev->CanRead()) {
    VolumeCatalogInfo* vol = &dev->VolCatInfo;

    dev->ClearRead(); /* clear read bit */
    Dmsg2(150, "dir_update_vol_info. label=%d Vol=%s\n", dev->IsLabeled(),
          vol->VolCatName);
    if (dev->IsLabeled() && vol->VolCatName[0] != 0) {
      dcr->DirUpdateVolumeInfo(false, false); /* send Volume info to Director */
      RemoveReadVolume(jcr, dcr->VolumeName);
      VolumeUnused(dcr);
    }
  } else if (dev->num_writers > 0) {
    /*
     * Note if WEOT is set, we are at the end of the tape and may not be
     * positioned correctly, so the job_media_record and update_vol_info have
     * already been done, which means we skip them here.
     */
    dev->num_writers--;
    Dmsg1(100, "There are %d writers in ReleaseDevice\n", dev->num_writers);
    if (dev->IsLabeled()) {
      Dmsg2(200, "dir_create_jobmedia. Release vol=%s dev=%s\n",
            dev->getVolCatName(), dev->print_name());
      if (!dev->AtWeot() && !dcr->DirCreateJobmediaRecord(false)) {
        Jmsg2(jcr, M_FATAL, 0,
              _("Could not create JobMedia record for Volume=\"%s\" Job=%s\n"),
              dcr->getVolCatName(), jcr->Job);
      }

      /*
       * If no more writers, and no errors, and wrote something, write an EOF
       */
      if (!dev->num_writers && dev->CanWrite() && dev->block_num > 0) {
        dev->weof(1);
        WriteAnsiIbmLabels(dcr, ANSI_EOF_LABEL, dev->VolHdr.VolumeName);
      }
      if (!dev->AtWeot()) {
        dev->VolCatInfo.VolCatFiles = dev->file; /* set number of files */

        /*
         * Note! do volume update before close, which zaps VolCatInfo
         */
        dcr->DirUpdateVolumeInfo(false,
                                 false); /* send Volume info to Director */
        Dmsg2(200, "dir_update_vol_info. Release vol=%s dev=%s\n",
              dev->getVolCatName(), dev->print_name());
      }
      if (dev->num_writers == 0) { /* if not being used */
        VolumeUnused(dcr);         /*  we obviously are not using the volume */
      }
    }

  } else {
    /*
     * If we reach here, it is most likely because the job has failed,
     * since the device is not in read mode and there are no writers.
     * It was probably reserved.
     */
    VolumeUnused(dcr);
  }

  Dmsg3(100, "%d writers, %d reserve, dev=%s\n", dev->num_writers,
        dev->NumReserved(), dev->print_name());

  /*
   * If no writers, close if file or !CAP_ALWAYS_OPEN
   */
  if (dev->num_writers == 0 &&
      (!dev->IsTape() || !dev->HasCap(CAP_ALWAYSOPEN))) {
    dev->close(dcr);
    FreeVolume(dev);
  }

  UnlockVolumes();

  /*
   * Fire off Alert command and include any output
   */
  if (!JobCanceled(jcr)) {
    if (!dcr->device->drive_tapealert_enabled && dcr->device->alert_command) {
      int status = 1;
      POOLMEM *alert, *line;
      Bpipe* bpipe;

      alert = GetPoolMemory(PM_FNAME);
      line = GetPoolMemory(PM_FNAME);

      alert = edit_device_codes(dcr, alert, dcr->device->alert_command, "");

      /*
       * Wait maximum 5 minutes
       */
      bpipe = OpenBpipe(alert, 60 * 5, "r");
      if (bpipe) {
        while (bfgets(line, bpipe->rfd)) {
          Jmsg(jcr, M_ALERT, 0, _("Alert: %s"), line);
        }
        status = CloseBpipe(bpipe);
      } else {
        status = errno;
      }
      if (status != 0) {
        BErrNo be;
        Jmsg(jcr, M_ALERT, 0, _("3997 Bad alert command: %s: ERR=%s.\n"), alert,
             be.bstrerror(status));
      }

      Dmsg1(400, "alert status=%d\n", status);
      FreePoolMemory(alert);
      FreePoolMemory(line);
    } else {
      /*
       * If all reservations are cleared for this device raise an event that SD
       * plugins can register to.
       */
      if (dev->NumReserved() == 0) {
        GeneratePluginEvent(jcr, bsdEventDeviceRelease, dcr);
      }
    }
  }

  pthread_cond_broadcast(&dev->wait_next_vol);
  Dmsg2(100, "JobId=%u broadcast wait_device_release at %s\n",
        (uint32_t)jcr->JobId,
        bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
  ReleaseDeviceCond();

  /*
   * If we are the thread that blocked the device, then unblock it
   */
  if (pthread_equal(dev->no_wait_id, pthread_self())) {
    dev->dunblock(true);
  } else {
    /*
     * Otherwise, reset the prior block status and unlock
     */
    dev->SetBlocked(was_blocked);
    dev->Unlock();
  }

  if (dcr->keep_dcr) {
    DetachDcrFromDev(dcr);
  } else {
    FreeDeviceControlRecord(dcr);
  }

  Dmsg2(100, "Device %s released by JobId=%u\n", dev->print_name(),
        (uint32_t)jcr->JobId);

  return retval;
}

/**
 * Clean up the device for reuse without freeing the memory
 */
bool CleanDevice(DeviceControlRecord* dcr)
{
  bool retval;

  dcr->keep_dcr = true; /* do not free the dcr */
  retval = ReleaseDevice(dcr);
  dcr->keep_dcr = false;

  return retval;
}

/**
 * Setup DeviceControlRecord with a new device.
 */
void SetupNewDcrDevice(JobControlRecord* jcr,
                       DeviceControlRecord* dcr,
                       Device* dev,
                       BlockSizes* blocksizes)
{
  dcr->jcr = jcr; /* point back to jcr */

  /*
   * Set device information, possibly change device
   */
  if (dev) {
    /*
     * Set wanted blocksizes
     */
    if (blocksizes) {
      dev->min_block_size = blocksizes->min_block_size;
      dev->max_block_size = blocksizes->max_block_size;
    }

    if (dcr->block) { FreeBlock(dcr->block); }
    dcr->block = new_block(dev);

    if (dcr->rec) {
      FreeRecord(dcr->rec);
      dcr->rec = NULL;
    }
    dcr->rec = new_record();

    if (dcr->attached_to_dev) { DetachDcrFromDev(dcr); }

    /*
     * Use job spoolsize prior to device spoolsize
     */
    if (jcr && jcr->spool_size) {
      dcr->max_job_spool_size = jcr->spool_size;
    } else {
      dcr->max_job_spool_size = dev->device->max_job_spool_size;
    }

    dcr->device = dev->device;
    dcr->SetDev(dev);
    AttachDcrToDev(dcr);

    /*
     * Initialize the auto deflation/inflation which can
     * be disabled per DeviceControlRecord when we want to. e.g. when we want to
     * send the data as part of a replication stream in which we
     * don't want to first inflate the data to then again
     * do deflation for sending it to the other storage daemon.
     */
    dcr->autodeflate = dcr->device->autodeflate;
    dcr->autoinflate = dcr->device->autoinflate;
  }
}

static void AttachDcrToDev(DeviceControlRecord* dcr)
{
  Device* dev;
  JobControlRecord* jcr;

  P(dcr->mutex_);
  dev = dcr->dev;
  jcr = dcr->jcr;
  if (jcr) Dmsg1(500, "JobId=%u enter AttachDcrToDev\n", (uint32_t)jcr->JobId);
  /* ***FIXME*** return error if dev not initiated */
  if (!dcr->attached_to_dev && dev->initiated && jcr &&
      jcr->getJobType() != JT_SYSTEM) {
    dev->Lock();
    Dmsg4(200, "Attach Jid=%d dcr=%p size=%d dev=%s\n", (uint32_t)jcr->JobId,
          dcr, dev->attached_dcrs->size(), dev->print_name());
    dev->attached_dcrs->append(dcr); /* attach dcr to device */
    dev->Unlock();
    dcr->attached_to_dev = true;
  }
  V(dcr->mutex_);
}

/**
 * DeviceControlRecord is locked before calling this routine
 */
static void LockedDetachDcrFromDev(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  Dmsg0(500, "Enter DetachDcrFromDev\n"); /* jcr is NULL in some cases */

  /* Detach this dcr only if attached */
  if (dcr->attached_to_dev && dev) {
    dcr->UnreserveDevice();
    dev->Lock();
    Dmsg4(200, "Detach Jid=%d dcr=%p size=%d to dev=%s\n",
          (uint32_t)dcr->jcr->JobId, dcr, dev->attached_dcrs->size(),
          dev->print_name());
    dcr->attached_to_dev = false;
    if (dev->attached_dcrs->size()) {
      dev->attached_dcrs->remove(dcr); /* detach dcr from device */
    }
    //    RemoveDcrFromDcrs(dcr);      /* remove dcr from jcr list */
    dev->Unlock();
  }
  dcr->attached_to_dev = false;
}


static void DetachDcrFromDev(DeviceControlRecord* dcr)
{
  P(dcr->mutex_);
  LockedDetachDcrFromDev(dcr);
  V(dcr->mutex_);
}

/**
 * Free up all aspects of the given dcr -- i.e. dechain it,
 *  release allocated memory, zap pointers, ...
 */
void FreeDeviceControlRecord(DeviceControlRecord* dcr)
{
  JobControlRecord* jcr;

  P(dcr->mutex_);
  jcr = dcr->jcr;

  LockedDetachDcrFromDev(dcr);

  if (dcr->block) { FreeBlock(dcr->block); }

  if (dcr->rec) { FreeRecord(dcr->rec); }

  if (jcr && jcr->dcr == dcr) { jcr->dcr = NULL; }

  if (jcr && jcr->read_dcr == dcr) { jcr->read_dcr = NULL; }

  V(dcr->mutex_);

  pthread_mutex_destroy(&dcr->mutex_);
  pthread_mutex_destroy(&dcr->r_mutex);

  delete dcr;
}

static void SetDcrFromVol(DeviceControlRecord* dcr, VolumeList* vol)
{
  /*
   * Note, if we want to be able to work from a .bsr file only
   *  for disaster recovery, we must "simulate" reading the catalog
   */
  bstrncpy(dcr->VolumeName, vol->VolumeName, sizeof(dcr->VolumeName));
  dcr->setVolCatName(vol->VolumeName);
  bstrncpy(dcr->media_type, vol->MediaType, sizeof(dcr->media_type));
  dcr->VolCatInfo.Slot = vol->Slot;
  dcr->VolCatInfo.InChanger = vol->Slot > 0;
}

} /* namespace storagedaemon */
