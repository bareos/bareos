/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Routines for handling the autochanger.
 */

#include "bareos.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */
#include "stored/autochanger.h"
#include "stored/wait.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/util.h"

/* Forward referenced functions */
static bool lock_changer(DeviceControlRecord *dcr);
static bool unlock_changer(DeviceControlRecord *dcr);
static bool unload_other_drive(DeviceControlRecord *dcr, slot_number_t slot, bool lock_set);
static char *transfer_edit_device_codes(DeviceControlRecord *dcr, POOLMEM *&omsg, const char *imsg, const char *cmd,
                                        slot_number_t src_slot, slot_number_t dst_slot);

/**
 * Init all the autochanger resources found
 */
bool init_autochangers()
{
   bool OK = true;
   AutochangerResource *changer;
   drive_number_t logical_drive_number;

   /*
    * Ensure that the media_type for each device is the same
    */
   foreach_res(changer, R_AUTOCHANGER) {
      DeviceResource *device;

      logical_drive_number = 0;
      foreach_alist(device, changer->device) {
         /*
          * If the device does not have a changer name or changer command
          * defined, used the one from the Autochanger resource
          */
         if (!device->changer_name && changer->changer_name) {
            device->changer_name = bstrdup(changer->changer_name);
         }

         if (!device->changer_command && changer->changer_command) {
            device->changer_command = bstrdup(changer->changer_command);
         }

         if (!device->changer_name) {
            Jmsg(NULL, M_ERROR, 0,
                 _("No Changer Name given for device %s. Cannot continue.\n"),
                 device->name());
            OK = false;
         }

         if (!device->changer_command) {
            Jmsg(NULL, M_ERROR, 0,
                 _("No Changer Command given for device %s. Cannot continue.\n"),
                 device->name());
            OK = false;
         }

         /*
          * Give the drive in the autochanger a logical drive number.
          */
         device->drive = logical_drive_number++;
      }
   }

   return OK;
}

/**
 * Called here to do an autoload using the autochanger, if
 * configured, and if a Slot has been defined for this Volume.
 * On success this routine loads the indicated tape, but the
 * label is not read, so it must be verified.
 *
 * Note if dir is not NULL, it is the console requesting the
 * autoload for labeling, so we respond directly to the dir bsock.
 *
 * Returns: 1 on success
 *          0 on failure (no changer available)
 *         -1 on error on autochanger
 *         -2 on error locking the autochanger
 */
int autoload_device(DeviceControlRecord *dcr, int writing, BareosSocket *dir)
{
   POOLMEM *changer;
   int rtn_stat = -1;                 /* error status */
   slot_number_t slot;
   JobControlRecord *jcr = dcr->jcr;
   drive_number_t drive;
   Device * volatile dev = dcr->dev;

   if (!dev->is_autochanger()) {
      Dmsg1(100, "Device %s is not an autochanger\n", dev->print_name());
      return 0;
   }

   /*
    * An empty ChangerCommand => virtual disk autochanger
    */
   if (dcr->device->changer_command && dcr->device->changer_command[0] == 0) {
      Dmsg0(100, "ChangerCommand=0, virtual disk changer\n");
      return 1;                       /* nothing to load */
   }

   drive = dev->drive_index;
   slot = dcr->VolCatInfo.InChanger ? dcr->VolCatInfo.Slot : 0;
   Dmsg3(100, "autoload: slot=%hd InChgr=%d Vol=%s\n", dcr->VolCatInfo.Slot,
         dcr->VolCatInfo.InChanger, dcr->getVolCatName());

   /*
    * Handle autoloaders here.  If we cannot autoload it, we will return 0 so that
    * the sysop will be asked to load it.
    */
   if (writing && slot <= 0) {
      if (dir) {
         return 0;                    /* For user, bail out right now */
      }

      /*
       * ***FIXME*** this really should not be here
       */
      if (dcr->dir_find_next_appendable_volume()) {
         slot = dcr->VolCatInfo.InChanger ? dcr->VolCatInfo.Slot : 0;
      } else {
         slot = 0;
      }
   }
   Dmsg1(400, "Want changer slot=%hd\n", slot);

   changer = get_pool_memory(PM_FNAME);
   if (slot <= 0) {
      /*
       * Suppress info when polling
       */
      if (!dev->poll) {
         Jmsg(jcr, M_INFO, 0, _("No slot defined in catalog (slot=%hd) for Volume \"%s\" on %s.\n"),
              slot, dcr->getVolCatName(), dev->print_name());
         Jmsg(jcr, M_INFO, 0, _("Cartridge change or \"update slots\" may be required.\n"));
      }
      rtn_stat = 0;
   } else if (!dcr->device->changer_name) {
      /*
       * Suppress info when polling
       */
      if (!dev->poll) {
         Jmsg(jcr, M_INFO, 0, _("No \"Changer Device\" for %s. Manual load of Volume may be required.\n"),
              dev->print_name());
      }
      rtn_stat = 0;
  } else if (!dcr->device->changer_command) {
      /*
       * Suppress info when polling
       */
      if (!dev->poll) {
         Jmsg(jcr, M_INFO, 0, _("No \"Changer Command\" for %s. Manual load of Volume may be required.\n"), dev->print_name());
      }
      rtn_stat = 0;
  } else {
      uint32_t timeout = dcr->device->max_changer_wait;
      int status;
      slot_number_t loaded;

      /*
       * Attempt to load the Volume
       */
      loaded = get_autochanger_loaded_slot(dcr);
      if (loaded != slot) {
         PoolMem results(PM_MESSAGE);

         if (!lock_changer(dcr)) {
            rtn_stat = -2;
            goto bail_out;
         }

         /*
          * Unload anything in our drive
          */
         if (!unload_autochanger(dcr, loaded, true)) {
            unlock_changer(dcr);
            goto bail_out;
         }

         /*
          * Make sure desired slot is unloaded
          */
         if (!unload_other_drive(dcr, slot, true)) {
            unlock_changer(dcr);
            goto bail_out;
         }

         /*
          * Load the desired volume.
          */
         Dmsg2(100, "Doing changer load slot %hd %s\n", slot, dev->print_name());
         Jmsg(jcr, M_INFO, 0,
              _("3304 Issuing autochanger \"load slot %hd, drive %hd\" command.\n"),
              slot, drive);
         dcr->VolCatInfo.Slot = slot;    /* slot to be loaded */
         changer = edit_device_codes(dcr, changer, dcr->device->changer_command, "load");
         dev->close(dcr);
         Dmsg1(200, "Run program=%s\n", changer);
         status = run_program_full_output(changer, timeout, results.addr());
         if (status == 0) {
            Jmsg(jcr, M_INFO, 0, _("3305 Autochanger \"load slot %hd, drive %hd\", status is OK.\n"),
                    slot, drive);
            Dmsg2(100, "load slot %hd, drive %hd, status is OK.\n", slot, drive);
            dev->set_slot(slot);      /* set currently loaded slot */
            if (dev->vol) {
               /*
                * We just swapped this Volume so it cannot be swapping any more
                */
               dev->vol->clear_swapping();
            }
         } else {
            berrno be;
            be.set_errno(status);
            Dmsg3(100, "load slot %hd, drive %hd, bad stats=%s.\n", slot, drive,
               be.bstrerror());
            Jmsg(jcr, M_FATAL, 0, _("3992 Bad autochanger \"load slot %hd, drive %hd\": "
                 "ERR=%s.\nResults=%s\n"),
                 slot, drive, be.bstrerror(), results.c_str());
            rtn_stat = -1;            /* hard error */
            dev->set_slot(-1);        /* mark unknown */
         }
         Dmsg2(100, "load slot %hd status=%d\n", slot, status);
         unlock_changer(dcr);
      } else {
         status = 0;                  /* we got what we want */
         dev->set_slot(slot);         /* set currently loaded slot */
      }

      Dmsg1(100, "After changer, status=%d\n", status);

      if (status == 0) {              /* did we succeed? */
         rtn_stat = 1;                /* tape loaded by changer */
      }
   }

bail_out:
   free_pool_memory(changer);
   return rtn_stat;
}

/**
 * Returns: -1 if error from changer command
 *          slot otherwise
 *
 * Note, this is safe to do without releasing the drive since it does not attempt load/unload a slot.
 */
slot_number_t get_autochanger_loaded_slot(DeviceControlRecord *dcr, bool lock_set)
{
   int status;
   POOLMEM *changer;
   JobControlRecord *jcr = dcr->jcr;
   slot_number_t loaded;
   Device *dev = dcr->dev;
   PoolMem results(PM_MESSAGE);
   uint32_t timeout = dcr->device->max_changer_wait;
   drive_number_t drive = dcr->dev->drive;

   if (!dev->is_autochanger()) {
      return -1;
   }

   if (!dcr->device->changer_command) {
      return -1;
   }

   if (dev->get_slot() > 0) {
      return dev->get_slot();
   }

   /*
    * Virtual disk autochanger
    */
   if (dcr->device->changer_command[0] == 0) {
      return 1;
   }

   /*
    * Only lock the changer if the lock_set is false e.g. changer not locked by calling function.
    */
   if (!lock_set) {
      if (!lock_changer(dcr)) {
         return -1;
      }
   }

   /*
    * Find out what is loaded, zero means device is unloaded
    * Suppress info when polling
    */
   if (!dev->poll && debug_level >= 1) {
      Jmsg(jcr, M_INFO, 0, _("3301 Issuing autochanger \"loaded? drive %hd\" command.\n"), drive);
   }

   changer = get_pool_memory(PM_FNAME);
   changer = edit_device_codes(dcr, changer, dcr->device->changer_command, "loaded");
   Dmsg1(100, "Run program=%s\n", changer);
   status = run_program_full_output(changer, timeout, results.addr());
   Dmsg3(100, "run_prog: %s stat=%d result=%s", changer, status, results.c_str());

   if (status == 0) {
      loaded = str_to_int16(results.c_str());
      if (loaded > 0) {
         /*
          * Suppress info when polling
          */
         if (!dev->poll && debug_level >= 1) {
            Jmsg(jcr, M_INFO, 0, _("3302 Autochanger \"loaded? drive %hd\", result is Slot %hd.\n"), drive, loaded);
         }
         dev->set_slot(loaded);
      } else {
         /*
          * Suppress info when polling
          */
         if (!dev->poll && debug_level >= 1) {
            Jmsg(jcr, M_INFO, 0, _("3302 Autochanger \"loaded? drive %hd\", result: nothing loaded.\n"),
                 drive);
         }
         if (loaded == 0) {      /* no slot loaded */
            dev->set_slot(0);
         } else {                /* probably some error */
            dev->clear_slot();   /* unknown */
         }
      }
   } else {
      berrno be;
      be.set_errno(status);
      Jmsg(jcr, M_INFO, 0, _("3991 Bad autochanger \"loaded? drive %hd\" command: "
                             "ERR=%s.\nResults=%s\n"), drive, be.bstrerror(), results.c_str());
      loaded = -1;              /* force unload */
   }

   if (!lock_set) {
      unlock_changer(dcr);
   }

   free_pool_memory(changer);

   return loaded;
}

static bool lock_changer(DeviceControlRecord *dcr)
{
   AutochangerResource *changer_res = dcr->device->changer_res;

   if (changer_res) {
      int errstat;
      Dmsg1(200, "Locking changer %s\n", changer_res->name());
      if ((errstat = rwl_writelock(&changer_res->changer_lock)) != 0) {
         berrno be;
         Jmsg(dcr->jcr, M_ERROR_TERM, 0, _("Lock failure on autochanger. ERR=%s\n"), be.bstrerror(errstat));
      }

      /*
       * We just locked the changer for exclusive use so let any plugin know we have.
       */
      if (generate_plugin_event(dcr->jcr, bsdEventChangerLock, dcr) != bRC_OK) {
         Dmsg0(100, "Locking changer: bsdEventChangerLock failed\n");
         rwl_writeunlock(&changer_res->changer_lock);
         return false;
      }
   }

   return true;
}

static bool unlock_changer(DeviceControlRecord *dcr)
{
   AutochangerResource *changer_res = dcr->device->changer_res;

   if (changer_res) {
      int errstat;

      generate_plugin_event(dcr->jcr, bsdEventChangerUnlock, dcr);

      Dmsg1(200, "Unlocking changer %s\n", changer_res->name());
      if ((errstat = rwl_writeunlock(&changer_res->changer_lock)) != 0) {
         berrno be;
         Jmsg(dcr->jcr, M_ERROR_TERM, 0, _("Unlock failure on autochanger. ERR=%s\n"), be.bstrerror(errstat));
      }
   }

   return true;
}

/**
 * Unload the volume, if any, in this drive
 * On entry: loaded == 0 -- nothing to do
 *           loaded  < 0 -- check if anything to do
 *           loaded  > 0 -- load slot == loaded
 */
bool unload_autochanger(DeviceControlRecord *dcr, slot_number_t loaded, bool lock_set)
{
   Device *dev = dcr->dev;
   JobControlRecord *jcr = dcr->jcr;
   slot_number_t slot;
   uint32_t timeout = dcr->device->max_changer_wait;
   bool retval = true;

   if (loaded == 0) {
      return true;
   }

   if (!dev->is_autochanger() ||
       !dcr->device->changer_name ||
       !dcr->device->changer_command) {
      return false;
   }

   /*
    * Virtual disk autochanger
    */
   if (dcr->device->changer_command[0] == 0) {
      dev->clear_unload();
      return true;
   }

   /*
    * Only lock the changer if the lock_set is false e.g. changer not locked by calling function.
    */
   if (!lock_set) {
      if (!lock_changer(dcr)) {
         return false;
      }
   }

   if (loaded < 0) {
      loaded = get_autochanger_loaded_slot(dcr, true);
   }

   if (loaded > 0) {
      int status;
      PoolMem results(PM_MESSAGE);
      POOLMEM *changer = get_pool_memory(PM_FNAME);

      Jmsg(jcr, M_INFO, 0,
           _("3307 Issuing autochanger \"unload slot %hd, drive %hd\" command.\n"),
           loaded, dev->drive);
      slot = dcr->VolCatInfo.Slot;
      dcr->VolCatInfo.Slot = loaded;
      changer = edit_device_codes(dcr, changer,
                                  dcr->device->changer_command, "unload");
      dev->close(dcr);
      Dmsg1(100, "Run program=%s\n", changer);
      status = run_program_full_output(changer, timeout, results.addr());
      dcr->VolCatInfo.Slot = slot;
      if (status != 0) {
         berrno be;

         be.set_errno(status);
         Jmsg(jcr, M_INFO, 0, _("3995 Bad autochanger \"unload slot %hd, drive %hd\": "
                                "ERR=%s\nResults=%s\n"),
              loaded, dev->drive, be.bstrerror(), results.c_str());
         retval = false;
         dev->clear_slot();        /* unknown */
      } else {
         dev->set_slot(0);         /* nothing loaded */
      }

      free_pool_memory(changer);
   }

   /*
    * Only unlock the changer if the lock_set is false e.g. changer not locked by calling function.
    */
   if (!lock_set) {
      unlock_changer(dcr);
   }

   if (loaded > 0) {           /* free_volume outside from changer lock */
      free_volume(dev);        /* Free any volume associated with this drive */
   }

   if (retval) {
      dev->clear_unload();
   }

   return retval;
}

/**
 * Unload the slot if mounted in a different drive
 */
static bool unload_other_drive(DeviceControlRecord *dcr, slot_number_t slot, bool lock_set)
{
   Device *dev = NULL;
   Device *dev_save;
   bool found = false;
   AutochangerResource *changer = dcr->dev->device->changer_res;
   DeviceResource *device;
   int retries = 0;                /* wait for device retries */

   if (!changer) {
      return false;
   }
   if (changer->device->size() == 1) {
      return true;
   }

   /*
    * We look for the slot number corresponding to the tape
    * we want in other drives, and if possible, unload it.
    */
   Dmsg0(100, "Wiffle through devices looking for slot\n");
   foreach_alist(device, changer->device) {
      dev = device->dev;
      if (!dev) {
         continue;
      }
      dev_save = dcr->dev;
      dcr->set_dev(dev);
      if (dev->get_slot() <= 0 && get_autochanger_loaded_slot(dcr, lock_set) <= 0) {
         dcr->set_dev(dev_save);
         continue;
      }
      dcr->set_dev(dev_save);
      if (dev->get_slot() == slot) {
         found = true;
         break;
      }
   }
   if (!found) {
      Dmsg1(100, "Slot=%hd not found in another device\n", slot);
      return true;
   } else {
      Dmsg1(100, "Slot=%hd found in another device\n", slot);
   }

   /*
    * The Volume we want is on another device.
    */
   if (dev->is_busy()) {
      Dmsg4(100, "Vol %s for dev=%s in use dev=%s slot=%hd\n",
           dcr->VolumeName, dcr->dev->print_name(),
           dev->print_name(), slot);
   }

   for (int i = 0; i < 3; i++) {
      if (dev->is_busy()) {
         wait_for_device(dcr->jcr, retries);
         continue;
      }
      break;
   }

   if (dev->is_busy()) {
      Jmsg(dcr->jcr, M_WARNING, 0, _("Volume \"%s\" wanted on %s is in use by device %s\n"),
           dcr->VolumeName, dcr->dev->print_name(), dev->print_name());
      Dmsg4(100, "Vol %s for dev=%s is busy dev=%s slot=%hd\n",
           dcr->VolumeName, dcr->dev->print_name(), dev->print_name(), dev->get_slot());
      Dmsg2(100, "num_writ=%d reserv=%d\n", dev->num_writers, dev->num_reserved());
      volume_unused(dcr);

      return false;
   }

   return unload_dev(dcr, dev, lock_set);
}

/**
 * Unconditionally unload a specified drive
 */
bool unload_dev(DeviceControlRecord *dcr, Device *dev, bool lock_set)
{
   int status;
   Device *save_dev;
   bool retval = true;
   JobControlRecord *jcr = dcr->jcr;
   slot_number_t save_slot;
   uint32_t timeout = dcr->device->max_changer_wait;
   AutochangerResource *changer = dcr->dev->device->changer_res;

   if (!changer) {
      return false;
   }

   save_dev = dcr->dev;               /* save dcr device */
   dcr->set_dev(dev);                 /* temporarily point dcr at other device */

   /*
    * Update slot if not set or not always_open
    */
   if (dev->get_slot() <= 0 || !dev->has_cap(CAP_ALWAYSOPEN)) {
      get_autochanger_loaded_slot(dcr, lock_set);
   }

   /*
    * Fail if we have no slot to unload
    */
   if (dev->get_slot() <= 0) {
      dcr->set_dev(save_dev);
      return false;
   }

   /*
    * Only lock the changer if the lock_set is false e.g. changer not locked by calling function.
    */
   if (!lock_set) {
      if (!lock_changer(dcr)) {
         dcr->set_dev(save_dev);
         return false;
      }
   }

   save_slot = dcr->VolCatInfo.Slot;
   dcr->VolCatInfo.Slot = dev->get_slot();

   POOLMEM *changer_cmd = get_pool_memory(PM_FNAME);
   PoolMem results(PM_MESSAGE);

   Jmsg(jcr, M_INFO, 0,
        _("3307 Issuing autochanger \"unload slot %hd, drive %hd\" command.\n"),
        dev->get_slot(), dev->drive);

   Dmsg2(100, "Issuing autochanger \"unload slot %hd, drive %hd\" command.\n",
        dev->get_slot(), dev->drive);

   changer_cmd = edit_device_codes(dcr, changer_cmd,
                                   dcr->device->changer_command, "unload");
   dev->close(dcr);

   Dmsg2(200, "close dev=%s reserve=%d\n", dev->print_name(), dev->num_reserved());
   Dmsg1(100, "Run program=%s\n", changer_cmd);

   status = run_program_full_output(changer_cmd, timeout, results.addr());
   dcr->VolCatInfo.Slot = save_slot;
   dcr->set_dev(save_dev);
   if (status != 0) {
      berrno be;
      be.set_errno(status);
      Jmsg(jcr, M_INFO, 0, _("3997 Bad autochanger \"unload slot %hd, drive %hd\": ERR=%s.\n"),
           dev->get_slot(), dev->drive, be.bstrerror());

      Dmsg3(100, "Bad autochanger \"unload slot %hd, drive %hd\": ERR=%s.\n",
              dev->get_slot(), dev->drive, be.bstrerror());
      retval = false;
      dev->clear_slot();          /* unknown */
   } else {
      Dmsg2(100, "Slot %hd unloaded %s\n", dev->get_slot(), dev->print_name());
      dev->set_slot(0);           /* nothing loaded */
   }
   if (retval) {
      dev->clear_unload();
   }

   /*
    * Only unlock the changer if the lock_set is false e.g. changer not locked by calling function.
    */
   if (!lock_set) {
      unlock_changer(dcr);
   }

   free_volume(dev);               /* Free any volume associated with this drive */
   free_pool_memory(changer_cmd);

   return retval;
}

/**
 * List the Volumes that are in the autoloader possibly with their barcodes.
 * We assume that it is always the Console that is calling us.
 */
bool autochanger_cmd(DeviceControlRecord *dcr, BareosSocket *dir, const char *cmd)
{
   Device *dev = dcr->dev;
   uint32_t timeout = dcr->device->max_changer_wait;
   POOLMEM *changer;
   Bpipe *bpipe;
   int len = sizeof_pool_memory(dir->msg) - 1;
   int status;
   int retries = 1; /* Number of retries on failing slot count */

   if (!dev->is_autochanger() ||
       !dcr->device->changer_name ||
       !dcr->device->changer_command) {
      if (bstrcmp(cmd, "drives")) {
         dir->fsend("drives=1\n");
      }
      dir->fsend(_("3993 Device %s not an autochanger device.\n"), dev->print_name());
      return false;
   }

   if (bstrcmp(cmd, "drives")) {
      AutochangerResource *changer_res = dcr->device->changer_res;
      drive_number_t drives = 1;
      if (changer_res) {
         drives = changer_res->device->size();
      }
      dir->fsend("drives=%hd\n", drives);
      Dmsg1(100, "drives=%hd\n", drives);
      return true;
   }

   /*
    * If listing, reprobe changer
    */
   if (bstrcmp(cmd, "list") || bstrcmp(cmd, "listall")) {
      dcr->dev->set_slot(0);
      get_autochanger_loaded_slot(dcr);
   }

   changer = get_pool_memory(PM_FNAME);
   lock_changer(dcr);
   changer = edit_device_codes(dcr,
                               changer,
                               dcr->device->changer_command,
                               cmd);
   dir->fsend(_("3306 Issuing autochanger \"%s\" command.\n"), cmd);

   /*
    * Now issue the command
    */
retry_changercmd:
   bpipe = open_bpipe(changer, timeout, "r");
   if (!bpipe) {
      dir->fsend(_("3996 Open bpipe failed.\n"));
      goto bail_out;            /* TODO: check if we need to return false */
   }

   if (bstrcmp(cmd, "list") || bstrcmp(cmd, "listall")) {
      /*
       * Get output from changer
       */
      while (fgets(dir->msg, len, bpipe->rfd)) {
         dir->msglen = strlen(dir->msg);
         Dmsg1(100, "<stored: %s", dir->msg);
         bnet_send(dir);
      }
   } else if (bstrcmp(cmd, "slots")) {
      slot_number_t slots;
      char buf[100], *p;

      /*
       * For slots command, read a single line
       */
      buf[0] = 0;
      fgets(buf, sizeof(buf)-1, bpipe->rfd);
      buf[sizeof(buf)-1] = 0;

      /*
       * Strip any leading space in front of # of slots
       */
      for (p = buf; B_ISSPACE(*p); p++) { }

      /*
       * Validate slot count. If slots == 0 retry retries more times.
       */
      slots = str_to_int16(p);
      if (slots == 0 && retries-- >= 0) {
         close_bpipe(bpipe);
         goto retry_changercmd;
      }

      dir->fsend("slots=%hd", slots);
      Dmsg1(100, "<stored: %s", dir->msg);
   }

   status = close_bpipe(bpipe);
   if (status != 0) {
      berrno be;
      be.set_errno(status);
      dir->fsend(_("3998 Autochanger error: ERR=%s\n"), be.bstrerror());
   }

bail_out:
   unlock_changer(dcr);
   free_pool_memory(changer);
   return true;
}

/**
 * Transfer a volume from src_slot to dst_slot.
 * We assume that it is always the Console that is calling us.
 */
bool autochanger_transfer_cmd(DeviceControlRecord *dcr, BareosSocket *dir, slot_number_t src_slot, slot_number_t dst_slot)
{
   Device *dev = dcr->dev;
   uint32_t timeout = dcr->device->max_changer_wait;
   POOLMEM *changer;
   Bpipe *bpipe;
   int len = sizeof_pool_memory(dir->msg) - 1;
   int status;

   if (!dev->is_autochanger() ||
       !dcr->device->changer_name ||
       !dcr->device->changer_command) {
      dir->fsend(_("3993 Device %s not an autochanger device.\n"),
         dev->print_name());
      return false;
   }

   changer = get_pool_memory(PM_FNAME);
   lock_changer(dcr);
   changer = transfer_edit_device_codes(dcr,
                                        changer,
                                        dcr->device->changer_command,
                                        "transfer",
                                        src_slot,
                                        dst_slot);
   dir->fsend(_("3306 Issuing autochanger transfer command.\n"));

   /*
    * Now issue the command
    */
   bpipe = open_bpipe(changer, timeout, "r");
   if (!bpipe) {
      dir->fsend(_("3996 Open bpipe failed.\n"));
      goto bail_out;            /* TODO: check if we need to return false */
   }

   /*
    * Get output from changer
    */
   while (fgets(dir->msg, len, bpipe->rfd)) {
      dir->msglen = strlen(dir->msg);
      Dmsg1(100, "<stored: %s\n", dir->msg);
      bnet_send(dir);
   }

   status = close_bpipe(bpipe);
   if (status != 0) {
      berrno be;
      be.set_errno(status);
      dir->fsend(_("3998 Autochanger error: ERR=%s\n"), be.bstrerror());
   } else {
      dir->fsend(_("3308 Successfully transferred volume from slot %hd to %hd.\n"),
                 src_slot, dst_slot);
   }

bail_out:
   unlock_changer(dcr);
   free_pool_memory(changer);
   return true;
}

/**
 * Special version of edit_device_codes for use with transfer subcommand.
 * Edit codes into ChangerCommand
 *  %% = %
 *  %a = destination slot
 *  %c = changer device name
 *  %d = none
 *  %f = none
 *  %j = none
 *  %o = command
 *  %s = source slot
 *  %S = source slot
 *  %v = none
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *  cmd = command string (transfer)
 */
static char *transfer_edit_device_codes(DeviceControlRecord *dcr, POOLMEM *&omsg, const char *imsg, const char *cmd,
                                        slot_number_t src_slot, slot_number_t dst_slot)
{
   const char *p;
   const char *str;
   char ed1[50];

   *omsg = 0;
   Dmsg1(1800, "transfer_edit_device_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
         switch (*++p) {
         case '%':
            str = "%";
            break;
         case 'a':
            str = edit_int64(dst_slot, ed1);
            break;
         case 'c':
            str = NPRT(dcr->device->changer_name);
            break;
         case 'o':
            str = NPRT(cmd);
            break;
         case 's':
         case 'S':
            str = edit_int64(src_slot, ed1);
            break;
         default:
            continue;
         }
      } else {
         ed1[0] = *p;
         ed1[1] = 0;
         str = ed1;
      }
      Dmsg1(1900, "add_str %s\n", str);
      pm_strcat(omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg);
   }
   Dmsg1(800, "omsg=%s\n", omsg);

   return omsg;
}
