/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
// Kern Sibbald, MM, MMI
/**
 * @file
 * Higher Level Device routines.
 * Knows about Bareos tape labels and such
 *
 * NOTE! In general, subroutines that have the word
 * "device" in the name do locking. Subroutines
 * that have the word "dev" in the name do not
 * do locking.  Thus if xxx_device() calls
 * yyy_dev(), all is OK, but if xxx_device()
 * calls yyy_device(), everything will hang.
 * Obviously, no zzz_dev() is allowed to call
 * a www_device() or everything falls apart.
 *
 * Concerning the routines dev->r_lock()() and BlockDevice()
 * see the end of this module for details.  In general,
 * blocking a device leaves it in a state where all threads
 * other than the current thread block when they attempt to
 * lock the device. They remain suspended (blocked) until the device
 * is unblocked. So, a device is blocked during an operation
 * that takes a long time (initialization, mounting a new
 * volume, ...) locking a device is done for an operation
 * that takes a short time such as writing data to the
 * device.
 */

#include "include/bareos.h" /* pull in global headers */
#include "stored/bsr.h"
#include "stored/stored.h" /* pull in Storage Daemon headers */
#include "stored/device.h"
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/match_bsr.h"
#include "lib/edit.h"
#include "include/jcr.h"
#include "lib/berrno.h"

namespace storagedaemon {

/**
 * This is the dreaded moment. We either have an end of
 * medium condition or worse, an error condition.
 * Attempt to "recover" by obtaining a new Volume.
 *
 * Here are a few things to know:
 *  dcr->VolCatInfo contains the info on the "current" tape for this job.
 *  dev->VolCatInfo contains the info on the tape in the drive.
 *    The tape in the drive could have changed several times since
 *    the last time the job used it (jcr->VolCatInfo).
 *  dcr->VolumeName is the name of the current/desired tape in the drive.
 *
 * We enter with device locked, and
 *     exit with device locked.
 *
 * Note, we are called only from one place in block.c for the daemons.
 *     The btape utility calls it from btape.c.
 *
 *  Returns: true  on success
 *           false on failure
 */
bool FixupDeviceBlockWriteError(DeviceControlRecord* dcr, int retries)
{
  char PrevVolName[MAX_NAME_LENGTH];
  DeviceBlock* block;
  char b1[30], b2[30];
  time_t wait_time;
  char dt[MAX_TIME_LENGTH];
  JobControlRecord* jcr = dcr->jcr;
  Device* dev = dcr->dev;
  int blocked = dev->blocked(); /* save any previous blocked status */
  bool ok = false;

  wait_time = time(NULL);

  Dmsg0(100, "=== Enter FixupDeviceBlockWriteError\n");

  // If we are blocked at entry, unblock it, and set our own block status
  if (blocked != BST_NOT_BLOCKED) { UnblockDevice(dev); }
  BlockDevice(dev, BST_DOING_ACQUIRE);

  /* Continue unlocked, but leave BLOCKED */
  dev->Unlock();

  bstrncpy(PrevVolName, dev->getVolCatName(), sizeof(PrevVolName));
  bstrncpy(dev->VolHdr.PrevVolumeName, PrevVolName,
           sizeof(dev->VolHdr.PrevVolumeName));

  /* Save the old block and create a new temporary label block */
  block = dcr->block;
  dcr->block = new_block(dev);

  /* Inform User about end of medium */
  Jmsg(jcr, M_INFO, 0,
       T_("End of medium on Volume \"%s\" Bytes=%s Blocks=%s at %s.\n"),
       PrevVolName, edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, b1),
       edit_uint64_with_commas(dev->VolCatInfo.VolCatBlocks, b2),
       bstrftime(dt, sizeof(dt), time(NULL)));

  Dmsg1(050, "SetUnload dev=%s\n", dev->print_name());
  dev->SetUnload();
  if (!dcr->MountNextWriteVolume()) {
    FreeBlock(dcr->block);
    dcr->block = block;
    dev->Lock();
    goto bail_out;
  }
  Dmsg2(050, "MustUnload=%d dev=%s\n", dev->MustUnload(), dev->print_name());
  dev->Lock(); /* lock again */

  dev->VolCatInfo.VolCatJobs++; /* increment number of jobs on vol */
  dcr->DirUpdateVolumeInfo(
      is_labeloperation::False); /* send Volume info to Director */

  Jmsg(jcr, M_INFO, 0, T_("New volume \"%s\" mounted on device %s at %s.\n"),
       dcr->VolumeName, dev->print_name(),
       bstrftime(dt, sizeof(dt), time(NULL)));

  /* If this is a new tape, the label_blk will contain the
   *  label, so write it now. If this is a previously
   *  used tape, MountNextWriteVolume() will return an
   *  empty label_blk, and nothing will be written. */
  Dmsg0(190, "write label block to dev\n");
  if (!dcr->WriteBlockToDev()) {
    BErrNo be;
    Pmsg1(0, T_("WriteBlockToDevice Volume label failed. ERR=%s"),
          be.bstrerror(dev->dev_errno));
    FreeBlock(dcr->block);
    dcr->block = block;
    goto bail_out;
  }
  FreeBlock(dcr->block);
  dcr->block = block;

  // Walk through all attached jcrs indicating the volume has changed
  Dmsg1(100, "Notify vol change. Volume=%s\n", dev->getVolCatName());
  for (auto mdcr : dev->attached_dcrs) {
    JobControlRecord* mjcr = mdcr->jcr;
    if (mjcr->JobId == 0) { continue; /* ignore console */ }
    mdcr->NewVol = true;
    if (jcr != mjcr) {
      bstrncpy(mdcr->VolumeName, dcr->VolumeName, sizeof(mdcr->VolumeName));
    }
  }

  /* Clear NewVol now because DirGetVolumeInfo() already done */
  jcr->sd_impl->dcr->NewVol = false;
  SetNewVolumeParameters(dcr);

  jcr->run_time += time(NULL) - wait_time; /* correct run time for mount wait */

  /* Write overflow block to device */
  Dmsg0(190, "Write overflow block to dev\n");
  if (!dcr->WriteBlockToDev()) {
    BErrNo be;
    Dmsg1(0, T_("WriteBlockToDevice overflow block failed. ERR=%s\n"),
          be.bstrerror(dev->dev_errno));
    /* Note: recursive call */
    if (retries-- <= 0 || !FixupDeviceBlockWriteError(dcr, retries)) {
      Jmsg2(jcr, M_FATAL, 0,
            T_("Catastrophic error. Cannot write overflow block to device %s. "
               "ERR=%s\n"),
            dev->print_name(), be.bstrerror(dev->dev_errno));
      goto bail_out;
    }
  }
  ok = true;

bail_out:
  /* At this point, the device is locked and blocked.
   * Unblock the device, restore any entry blocked condition, then
   *   return leaving the device locked (as it was on entry). */
  UnblockDevice(dev);
  if (blocked != BST_NOT_BLOCKED) { BlockDevice(dev, blocked); }
  return ok; /* device locked */
}

void SetStartVolPosition(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  /* Set new start position */
  if (dev->GetSeekMode() == SeekMode::FILE_BLOCK) {
    dcr->StartBlock = dev->block_num;
    dcr->StartFile = dev->file;
  } else {
    dcr->StartBlock = (uint32_t)dev->file_addr;
    dcr->StartFile = (uint32_t)(dev->file_addr >> 32);
  }
}

/**
 * We have a new Volume mounted, so reset the Volume parameters
 *  concerning this job.  The global changes were made earlier
 *  in the dev structure.
 */
void SetNewVolumeParameters(DeviceControlRecord* dcr)
{
  JobControlRecord* jcr = dcr->jcr;
  if (dcr->NewVol && !dcr->DirGetVolumeInfo(GET_VOL_INFO_FOR_WRITE)) {
    Jmsg1(jcr, M_ERROR, 0, "%s", jcr->errmsg);
  }
  SetNewFileParameters(dcr);
  jcr->sd_impl->NumWriteVolumes++;
  dcr->NewVol = false;
}

/**
 * We are now in a new Volume file, so reset the Volume parameters
 *  concerning this job.  The global changes were made earlier
 *  in the dev structure.
 */
void SetNewFileParameters(DeviceControlRecord* dcr)
{
  SetStartVolPosition(dcr);

  /* Reset indicies */
  dcr->VolFirstIndex = 0;
  dcr->VolLastIndex = 0;
  dcr->NewFile = false;
  dcr->WroteVol = false;
}

/**
 *   First Open of the device. Expect dev to already be initialized.
 *
 *   This routine is used only when the Storage daemon starts
 *   and always_open is set, and in the stand-alone utility
 *   routines such as bextract.
 *
 *   Note, opening of a normal file is deferred to later so
 *    that we can get the filename; the device_name for
 *    a file is the directory only.
 *
 *   Returns: false on failure
 *            true  on success
 */
bool FirstOpenDevice(DeviceControlRecord* dcr)
{
  Device* dev = dcr->dev;
  bool ok = true;

  Dmsg0(120, "start open_output_device()\n");
  if (!dev) { return false; }

  dev->rLock();

  /* Defer opening files */
  if (!dev->IsTape()) {
    Dmsg0(129, "Device is file, deferring open.\n");
    goto bail_out;
  }

  DeviceMode mode;
  if (dev->HasCap(CAP_STREAM)) {
    mode = DeviceMode::OPEN_WRITE_ONLY;
  } else {
    mode = DeviceMode::OPEN_READ_ONLY;
  }
  Dmsg0(129, "Opening device.\n");
  if (!dev->open(dcr, mode)) {
    Emsg1(M_FATAL, 0, T_("dev open failed: %s\n"), dev->errmsg);
    ok = false;
    goto bail_out;
  }
  Dmsg1(129, "open dev %s OK\n", dev->print_name());

bail_out:
  dev->Unlock();
  return ok;
}

// Position to the first file on this volume
BootStrapRecord* PositionDeviceToFirstFile(JobControlRecord* jcr,
                                           BootStrapRecord** current,
                                           DeviceControlRecord* dcr)
{
  BootStrapRecord* bsr = NULL;
  Device* dev = dcr->dev;
  uint32_t file, block;
  /* Now find and position to first file and block
   *   on this tape. */
  if (jcr->sd_impl->read_session.bsr) {
    if (!*current) {
      *current = jcr->sd_impl->read_session.bsr;
      Dmsg2(300, "Switching to bsr { sessid = %lu, volume = %s }\n",
            (*current)->sessid->sessid, (*current)->volume->VolumeName);
    }


    jcr->sd_impl->read_session.bsr->Reposition = true;
    bsr = find_next_bsr(jcr->sd_impl->read_session.bsr, *current, dev);
    if (bsr && bsr != *current) {
      *current = bsr;
      Dmsg2(300, "Switching to bsr { sessid = %lu, volume = %s }\n",
            (*current)->sessid->sessid, (*current)->volume->VolumeName);
    }
    if (GetBsrStartAddr(bsr, &file, &block) > 0) {
      Jmsg(jcr, M_INFO, 0,
           T_("Forward spacing Volume \"%s\" to file:block %u:%u.\n"),
           dev->VolHdr.VolumeName, file, block);
      dev->Reposition(dcr, file, block);
    }
  }
  return bsr;
}

/**
 * See if we can Reposition.
 *
 * Returns: true  if at end of volume
 *          false otherwise
 */
bool TryDeviceRepositioning(JobControlRecord* jcr,
                            BootStrapRecord** current,
                            DeviceRecord* rec,
                            DeviceControlRecord* dcr)
{
  BootStrapRecord* bsr;
  Device* dev = dcr->dev;

  bsr = find_next_bsr(jcr->sd_impl->read_session.bsr, *current, dev);
  if (bsr == NULL && jcr->sd_impl->read_session.bsr->mount_next_volume) {
    Dmsg0(500, "Would mount next volume here\n");
    Dmsg2(500, "Current position (file:block) %u:%u\n", dev->file,
          dev->block_num);
    jcr->sd_impl->read_session.bsr->mount_next_volume = false;
    if (!dev->AtEot()) {
      /* Set EOT flag to force mount of next Volume */
      jcr->sd_impl->read_session.mount_next_volume = true;
      dev->SetEot();
    }
    rec->Block = 0;
    return true;
  }
  if (bsr && bsr != *current) {
    *current = bsr;
    Dmsg2(300, "Switching to bsr { sessid = %lu, volume = %s }\n",
          (*current)->sessid->sessid, (*current)->volume->VolumeName);

    /* ***FIXME*** gross kludge to make disk seeking work.  Remove
     *   when find_next_bsr() is fixed not to return a bsr already
     *   completed. */
    uint32_t block, file;
    /* TODO: use dev->file_addr ? */
    uint64_t dev_addr = (((uint64_t)dev->file) << 32) | dev->block_num;
    uint64_t bsr_addr = GetBsrStartAddr(bsr, &file, &block);

    if (dev_addr > bsr_addr) { return false; }
    Dmsg4(500, "Try_Reposition from (file:block) %u:%u to %u:%u\n", dev->file,
          dev->block_num, file, block);
    dev->Reposition(dcr, file, block);
    rec->Block = 0;
  }
  return false;
}

} /* namespace storagedaemon */
