/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *  Higher Level Device routines.
 *  Knows about Bacula tape labels and such
 *
 *  NOTE! In general, subroutines that have the word
 *        "device" in the name do locking.  Subroutines
 *        that have the word "dev" in the name do not
 *        do locking.  Thus if xxx_device() calls
 *        yyy_dev(), all is OK, but if xxx_device()
 *        calls yyy_device(), everything will hang.
 *        Obviously, no zzz_dev() is allowed to call
 *        a www_device() or everything falls apart.
 *
 * Concerning the routines dev->r_lock()() and block_device()
 *  see the end of this module for details.  In general,
 *  blocking a device leaves it in a state where all threads
 *  other than the current thread block when they attempt to
 *  lock the device. They remain suspended (blocked) until the device
 *  is unblocked. So, a device is blocked during an operation
 *  that takes a long time (initialization, mounting a new
 *  volume, ...) locking a device is done for an operation
 *  that takes a short time such as writing data to the
 *  device.
 *
 *
 *   Kern Sibbald, MM, MMI
 *
 *   Version $Id$
 */

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Forward referenced functions */

/*
 * This is the dreaded moment. We either have an end of
 * medium condition or worse, and error condition.
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
bool fixup_device_block_write_error(DCR *dcr, int retries)
{
   char PrevVolName[MAX_NAME_LENGTH];
   DEV_BLOCK *label_blk;
   DEV_BLOCK *block = dcr->block;
   char b1[30], b2[30];
   time_t wait_time;
   char dt[MAX_TIME_LENGTH];
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;
   int blocked = dev->blocked();         /* save any previous blocked status */
   bool ok = false;

   wait_time = time(NULL);

   Dmsg0(100, "=== Enter fixup_device_block_write_error\n");

   /*
    * If we are blocked at entry, unblock it, and set our own block status
    */
   if (blocked != BST_NOT_BLOCKED) {
      unblock_device(dev);
   }
   block_device(dev, BST_DOING_ACQUIRE);

   /* Continue unlocked, but leave BLOCKED */
   dev->Unlock();

   bstrncpy(PrevVolName, dev->getVolCatName(), sizeof(PrevVolName));
   bstrncpy(dev->VolHdr.PrevVolumeName, PrevVolName, sizeof(dev->VolHdr.PrevVolumeName));

   label_blk = new_block(dev);
   dcr->block = label_blk;

   /* Inform User about end of medium */
   Jmsg(jcr, M_INFO, 0, _("End of medium on Volume \"%s\" Bytes=%s Blocks=%s at %s.\n"),
        PrevVolName, edit_uint64_with_commas(dev->VolCatInfo.VolCatBytes, b1),
        edit_uint64_with_commas(dev->VolCatInfo.VolCatBlocks, b2),
        bstrftime(dt, sizeof(dt), time(NULL)));

   Dmsg1(050, "set_unload dev=%s\n", dev->print_name());
   dev->set_unload();
   if (!dcr->mount_next_write_volume()) {
      free_block(label_blk);
      dcr->block = block;
      dev->Lock();  
      goto bail_out;
   }
   Dmsg2(050, "must_unload=%d dev=%s\n", dev->must_unload(), dev->print_name());
   dev->Lock();                    /* lock again */

   dev->VolCatInfo.VolCatJobs++;              /* increment number of jobs on vol */
   dir_update_volume_info(dcr, false, false); /* send Volume info to Director */

   Jmsg(jcr, M_INFO, 0, _("New volume \"%s\" mounted on device %s at %s.\n"),
      dcr->VolumeName, dev->print_name(), bstrftime(dt, sizeof(dt), time(NULL)));

   /*
    * If this is a new tape, the label_blk will contain the
    *  label, so write it now. If this is a previously
    *  used tape, mount_next_write_volume() will return an
    *  empty label_blk, and nothing will be written.
    */
   Dmsg0(190, "write label block to dev\n");
   if (!dcr->write_block_to_dev()) {
      berrno be;
      Pmsg1(0, _("write_block_to_device Volume label failed. ERR=%s"),
        be.bstrerror(dev->dev_errno));
      free_block(label_blk);
      dcr->block = block;
      goto bail_out;
   }
   free_block(label_blk);
   dcr->block = block;

   /*
    * Walk through all attached jcrs indicating the volume has changed
    */
   Dmsg1(100, "Notify vol change. Volume=%s\n", dev->getVolCatName());
   DCR *mdcr;
   foreach_dlist(mdcr, dev->attached_dcrs) {
      JCR *mjcr = mdcr->jcr;
      if (mjcr->JobId == 0) {
         continue;                 /* ignore console */
      }
      mdcr->NewVol = true;
      if (jcr != mjcr) {
         bstrncpy(mdcr->VolumeName, dcr->VolumeName, sizeof(mdcr->VolumeName));
      }
   }

   /* Clear NewVol now because dir_get_volume_info() already done */
   jcr->dcr->NewVol = false;
   set_new_volume_parameters(dcr);

   jcr->run_time += time(NULL) - wait_time; /* correct run time for mount wait */

   /* Write overflow block to device */
   Dmsg0(190, "Write overflow block to dev\n");
   if (!dcr->write_block_to_dev()) {
      berrno be;
      Dmsg1(0, _("write_block_to_device overflow block failed. ERR=%s"),
        be.bstrerror(dev->dev_errno));
      /* Note: recursive call */
      if (retries-- <= 0 || !fixup_device_block_write_error(dcr, retries)) {
         Jmsg2(jcr, M_FATAL, 0, 
              _("Catastrophic error. Cannot write overflow block to device %s. ERR=%s"),
              dev->print_name(), be.bstrerror(dev->dev_errno));
         goto bail_out;
      }
   }
   ok = true;

bail_out:
   /*
    * At this point, the device is locked and blocked.
    * Unblock the device, restore any entry blocked condition, then
    *   return leaving the device locked (as it was on entry).
    */
   unblock_device(dev);
   if (blocked != BST_NOT_BLOCKED) {
      block_device(dev, blocked);
   }
   return ok;                               /* device locked */
}

void set_start_vol_position(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   /* Set new start position */
   if (dev->is_tape()) {
      dcr->StartBlock = dev->block_num;
      dcr->StartFile = dev->file;
   } else {
      dcr->StartBlock = (uint32_t)dev->file_addr;
      dcr->StartFile  = (uint32_t)(dev->file_addr >> 32);
   }
}

/*
 * We have a new Volume mounted, so reset the Volume parameters
 *  concerning this job.  The global changes were made earlier
 *  in the dev structure.
 */
void set_new_volume_parameters(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   if (dcr->NewVol && !dir_get_volume_info(dcr, GET_VOL_INFO_FOR_WRITE)) {
      Jmsg1(jcr, M_ERROR, 0, "%s", jcr->errmsg);
   }
   set_new_file_parameters(dcr);
   jcr->NumWriteVolumes++;
   dcr->NewVol = false;
}

/*
 * We are now in a new Volume file, so reset the Volume parameters
 *  concerning this job.  The global changes were made earlier
 *  in the dev structure.
 */
void set_new_file_parameters(DCR *dcr)
{
   set_start_vol_position(dcr);

   /* Reset indicies */
   dcr->VolFirstIndex = 0;
   dcr->VolLastIndex = 0;
   dcr->NewFile = false;
   dcr->WroteVol = false;
}



/*
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
bool first_open_device(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   bool ok = true;

   Dmsg0(120, "start open_output_device()\n");
   if (!dev) {
      return false;
   }

   dev->rLock();

   /* Defer opening files */
   if (!dev->is_tape()) {
      Dmsg0(129, "Device is file, deferring open.\n");
      goto bail_out;
   }

    int mode;
    if (dev->has_cap(CAP_STREAM)) {
       mode = OPEN_WRITE_ONLY;
    } else {
       mode = OPEN_READ_ONLY;
    }
   Dmsg0(129, "Opening device.\n");
   if (!dev->open(dcr, mode)) {
      Emsg1(M_FATAL, 0, _("dev open failed: %s\n"), dev->errmsg);
      ok = false;
      goto bail_out;
   }
   Dmsg1(129, "open dev %s OK\n", dev->print_name());

bail_out:
   dev->Unlock();
   return ok;
}

/*
 * Make sure device is open, if not do so
 */
bool open_device(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   /* Open device */
   int mode;
   if (dev->has_cap(CAP_STREAM)) {
      mode = OPEN_WRITE_ONLY;
   } else {
      mode = OPEN_READ_WRITE;
   }
   if (!dev->open(dcr, mode)) {
      /* If polling, ignore the error */
      /* If DVD, also ignore the error, very often you cannot open the device
       * (when there is no DVD, or when the one inserted is a wrong one) */
      if (!dev->poll && !dev->is_dvd() && !dev->is_removable()) {
         Jmsg2(dcr->jcr, M_FATAL, 0, _("Unable to open device %s: ERR=%s\n"),
            dev->print_name(), dev->bstrerror());
         Pmsg2(000, _("Unable to open archive %s: ERR=%s\n"), 
            dev->print_name(), dev->bstrerror());
      }
      return false;
   }
   return true;
}
