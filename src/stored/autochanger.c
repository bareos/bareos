/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.

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
 *  Routines for handling the autochanger.
 *
 *   Kern Sibbald, August MMII
 *                            
 */

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

/* Forward referenced functions */
static void lock_changer(DCR *dcr);
static void unlock_changer(DCR *dcr);
static bool unload_other_drive(DCR *dcr, int slot);

/* Init all the autochanger resources found */
bool init_autochangers()
{
   bool OK = true;
   AUTOCHANGER *changer;
   /* Ensure that the media_type for each device is the same */
   foreach_res(changer, R_AUTOCHANGER) {
      DEVRES *device;
      foreach_alist(device, changer->device) {
         /*
          * If the device does not have a changer name or changer command
          *   defined, used the one from the Autochanger resource 
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
               device->hdr.name);
            OK = false;
         }   
         if (!device->changer_command) {
            Jmsg(NULL, M_ERROR, 0, 
               _("No Changer Command given for device %s. Cannot continue.\n"),
               device->hdr.name);
            OK = false;
         }   
      }
   }
   return OK;
}


/*
 * Called here to do an autoload using the autochanger, if
 *  configured, and if a Slot has been defined for this Volume.
 *  On success this routine loads the indicated tape, but the
 *  label is not read, so it must be verified.
 *
 *  Note if dir is not NULL, it is the console requesting the
 *   autoload for labeling, so we respond directly to the
 *   dir bsock.
 *
 *  Returns: 1 on success
 *           0 on failure (no changer available)
 *          -1 on error on autochanger
 */
int autoload_device(DCR *dcr, int writing, BSOCK *dir)
{
   JCR *jcr = dcr->jcr;
   DEVICE * volatile dev = dcr->dev;
   int slot;
   int drive = dev->drive_index;
   int rtn_stat = -1;                 /* error status */
   POOLMEM *changer;

   if (!dev->is_autochanger()) {
      Dmsg1(100, "Device %s is not an autochanger\n", dev->print_name());
      return 0;
   }

   /* An empty ChangerCommand => virtual disk autochanger */
   if (dcr->device->changer_command && dcr->device->changer_command[0] == 0) {
      Dmsg0(100, "ChangerCommand=0, virtual disk changer\n");
      return 1;                       /* nothing to load */
   }

   slot = dcr->VolCatInfo.InChanger ? dcr->VolCatInfo.Slot : 0;
   Dmsg3(100, "autoload: slot=%d InChgr=%d Vol=%s\n", dcr->VolCatInfo.Slot,
         dcr->VolCatInfo.InChanger, dcr->getVolCatName());
   /*
    * Handle autoloaders here.  If we cannot autoload it, we
    *  will return 0 so that the sysop will be asked to load it.
    */
   if (writing && slot <= 0) {
      if (dir) {
         return 0;                    /* For user, bail out right now */
      }
      /* ***FIXME*** this really should not be here */
      if (dir_find_next_appendable_volume(dcr)) {
         slot = dcr->VolCatInfo.InChanger ? dcr->VolCatInfo.Slot : 0;
      } else {
         slot = 0;
      }
   }
   Dmsg1(400, "Want changer slot=%d\n", slot);

   changer = get_pool_memory(PM_FNAME);
   if (slot <= 0) {
      /* Suppress info when polling */
      if (!dev->poll) {
         Jmsg(jcr, M_INFO, 0, _("No slot defined in catalog (slot=%d) for Volume \"%s\" on %s.\n"), 
              slot, dcr->getVolCatName(), dev->print_name());
         Jmsg(jcr, M_INFO, 0, _("Cartridge change or \"update slots\" may be required.\n"));
      }
      rtn_stat = 0;
   } else if (!dcr->device->changer_name) {
      /* Suppress info when polling */
      if (!dev->poll) {
         Jmsg(jcr, M_INFO, 0, _("No \"Changer Device\" for %s. Manual load of Volume may be required.\n"),
              dev->print_name());
      }
      rtn_stat = 0;
  } else if (!dcr->device->changer_command) {
      /* Suppress info when polling */
      if (!dev->poll) {
         Jmsg(jcr, M_INFO, 0, _("No \"Changer Command\" for %s. Manual load of Volume may be requird.\n"),
              dev->print_name());
      }
      rtn_stat = 0;
  } else {
      /* Attempt to load the Volume */

      uint32_t timeout = dcr->device->max_changer_wait;
      int loaded, status;

      loaded = get_autochanger_loaded_slot(dcr);

      if (loaded != slot) {
         POOL_MEM results(PM_MESSAGE);

         /* Unload anything in our drive */
         if (!unload_autochanger(dcr, loaded)) {
            goto bail_out;
         }
            
         /* Make sure desired slot is unloaded */
         if (!unload_other_drive(dcr, slot)) {
            goto bail_out;
         }

         /*
          * Load the desired cassette
          */
         lock_changer(dcr);
         Dmsg2(100, "Doing changer load slot %d %s\n", slot, dev->print_name());
         Jmsg(jcr, M_INFO, 0,
              _("3304 Issuing autochanger \"load slot %d, drive %d\" command.\n"),
              slot, drive);
         dcr->VolCatInfo.Slot = slot;    /* slot to be loaded */
         changer = edit_device_codes(dcr, changer, dcr->device->changer_command, "load");
         dev->close();
         Dmsg1(200, "Run program=%s\n", changer);
         status = run_program_full_output(changer, timeout, results.addr());
         if (status == 0) {
            Jmsg(jcr, M_INFO, 0, _("3305 Autochanger \"load slot %d, drive %d\", status is OK.\n"),
                    slot, drive);
            Dmsg2(100, "load slot %d, drive %d, status is OK.\n", slot, drive);
            dev->set_slot(slot);      /* set currently loaded slot */
            if (dev->vol) {
               /* We just swapped this Volume so it cannot be swapping any more */
               dev->vol->clear_swapping();
            }
         } else {
            berrno be;
            be.set_errno(status);
            Dmsg3(100, "load slot %d, drive %d, bad stats=%s.\n", slot, drive,
               be.bstrerror());
            Jmsg(jcr, M_FATAL, 0, _("3992 Bad autochanger \"load slot %d, drive %d\": "
                 "ERR=%s.\nResults=%s\n"),
                    slot, drive, be.bstrerror(), results.c_str());
            rtn_stat = -1;            /* hard error */
            dev->set_slot(-1);        /* mark unknown */
         }
         Dmsg2(100, "load slot %d status=%d\n", slot, status);
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
   free_pool_memory(changer);
   return rtn_stat;

bail_out:
   free_pool_memory(changer);
   return -1;

}

/*
 * Returns: -1 if error from changer command
 *          slot otherwise
 *  Note, this is safe to do without releasing the drive
 *   since it does not attempt load/unload a slot.
 */
int get_autochanger_loaded_slot(DCR *dcr)
{
   JCR *jcr = dcr->jcr;
   DEVICE *dev = dcr->dev;
   int status, loaded;
   uint32_t timeout = dcr->device->max_changer_wait;
   int drive = dcr->dev->drive_index;
   POOL_MEM results(PM_MESSAGE);
   POOLMEM *changer;

   if (!dev->is_autochanger()) {
      return -1;
   }
   if (!dcr->device->changer_command) {
      return -1;
   }
   if (dev->get_slot() > 0) {
      return dev->get_slot();
   }

   /* Virtual disk autochanger */
   if (dcr->device->changer_command[0] == 0) {
      return 1;
   }

   /* Find out what is loaded, zero means device is unloaded */
   changer = get_pool_memory(PM_FNAME);
   lock_changer(dcr);
   /* Suppress info when polling */
   if (!dev->poll && debug_level >= 1) {
      Jmsg(jcr, M_INFO, 0, _("3301 Issuing autochanger \"loaded? drive %d\" command.\n"),
           drive);
   }
   changer = edit_device_codes(dcr, changer, dcr->device->changer_command, "loaded");
   Dmsg1(100, "Run program=%s\n", changer);
   status = run_program_full_output(changer, timeout, results.addr());
   Dmsg3(100, "run_prog: %s stat=%d result=%s", changer, status, results.c_str());
   if (status == 0) {
      loaded = str_to_int32(results.c_str());
      if (loaded > 0) {
         /* Suppress info when polling */
         if (!dev->poll && debug_level >= 1) {
            Jmsg(jcr, M_INFO, 0, _("3302 Autochanger \"loaded? drive %d\", result is Slot %d.\n"),
                 drive, loaded);
         }
         dev->set_slot(loaded);
      } else {
         /* Suppress info when polling */
         if (!dev->poll && debug_level >= 1) {
            Jmsg(jcr, M_INFO, 0, _("3302 Autochanger \"loaded? drive %d\", result: nothing loaded.\n"),
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
      Jmsg(jcr, M_INFO, 0, _("3991 Bad autochanger \"loaded? drive %d\" command: "
           "ERR=%s.\nResults=%s\n"), drive, be.bstrerror(), results.c_str());
      loaded = -1;              /* force unload */
   }
   unlock_changer(dcr);
   free_pool_memory(changer);
   return loaded;
}

static void lock_changer(DCR *dcr)
{
   AUTOCHANGER *changer_res = dcr->device->changer_res;
   if (changer_res) {
      int errstat;
      Dmsg1(200, "Locking changer %s\n", changer_res->hdr.name);
      if ((errstat=rwl_writelock(&changer_res->changer_lock)) != 0) {
         berrno be;
         Jmsg(dcr->jcr, M_ERROR_TERM, 0, _("Lock failure on autochanger. ERR=%s\n"),
              be.bstrerror(errstat));
      }
   }
}

static void unlock_changer(DCR *dcr)
{
   AUTOCHANGER *changer_res = dcr->device->changer_res;
   if (changer_res) {
      int errstat;
      Dmsg1(200, "Unlocking changer %s\n", changer_res->hdr.name);
      if ((errstat=rwl_writeunlock(&changer_res->changer_lock)) != 0) {
         berrno be;
         Jmsg(dcr->jcr, M_ERROR_TERM, 0, _("Unlock failure on autochanger. ERR=%s\n"),
              be.bstrerror(errstat));
      }
   }
}

/*
 * Unload the volume, if any, in this drive
 *  On entry: loaded == 0 -- nothing to do
 *            loaded  < 0 -- check if anything to do
 *            loaded  > 0 -- load slot == loaded
 */
bool unload_autochanger(DCR *dcr, int loaded)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   int slot;
   uint32_t timeout = dcr->device->max_changer_wait;
   bool ok = true;

   if (loaded == 0) {
      return true;
   }

   if (!dev->is_autochanger() || !dcr->device->changer_name ||
       !dcr->device->changer_command) {
      return false;
   }

   /* Virtual disk autochanger */
   if (dcr->device->changer_command[0] == 0) {
      dev->clear_unload();
      return true;
   }

   lock_changer(dcr);
   if (loaded < 0) {
      loaded = get_autochanger_loaded_slot(dcr);
   }

   if (loaded > 0) {
      POOL_MEM results(PM_MESSAGE);
      POOLMEM *changer = get_pool_memory(PM_FNAME);
      Jmsg(jcr, M_INFO, 0,
           _("3307 Issuing autochanger \"unload slot %d, drive %d\" command.\n"),
           loaded, dev->drive_index);
      slot = dcr->VolCatInfo.Slot;
      dcr->VolCatInfo.Slot = loaded;
      changer = edit_device_codes(dcr, changer, 
                   dcr->device->changer_command, "unload");
      dev->close();
      Dmsg1(100, "Run program=%s\n", changer);
      int stat = run_program_full_output(changer, timeout, results.addr());
      dcr->VolCatInfo.Slot = slot;
      if (stat != 0) {
         berrno be;
         be.set_errno(stat);
         Jmsg(jcr, M_INFO, 0, _("3995 Bad autochanger \"unload slot %d, drive %d\": "
              "ERR=%s\nResults=%s\n"),
                 loaded, dev->drive_index, be.bstrerror(), results.c_str());
         ok = false;
         dev->clear_slot();        /* unknown */
      } else {
         dev->set_slot(0);         /* nothing loaded */
      }

      free_pool_memory(changer);
   }
   unlock_changer(dcr);

   if (loaded > 0) {           /* free_volume outside from changer lock */
      free_volume(dev);        /* Free any volume associated with this drive */
   }

   if (ok) {
      dev->clear_unload();
   }
   return ok;
}

/*
 * Unload the slot if mounted in a different drive
 */
static bool unload_other_drive(DCR *dcr, int slot)
{
   DEVICE *dev = NULL;
   DEVICE *dev_save;
   bool found = false;
   AUTOCHANGER *changer = dcr->dev->device->changer_res;
   DEVRES *device;
   int retries = 0;                /* wait for device retries */

   if (!changer) {
      return false;
   }
   if (changer->device->size() == 1) {
      return true;
   }

   /*
    * We look for the slot number corresponding to the tape
    *   we want in other drives, and if possible, unload
    *   it.
    */
   Dmsg0(100, "Wiffle through devices looking for slot\n");
   foreach_alist(device, changer->device) {
      dev = device->dev;
      if (!dev) {
         continue;
      }
      dev_save = dcr->dev;
      dcr->set_dev(dev);
      if (dev->get_slot() <= 0 && get_autochanger_loaded_slot(dcr) <= 0) {
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
      Dmsg1(100, "Slot=%d not found in another device\n", slot);
      return true;
   } else {
      Dmsg1(100, "Slot=%d found in another device\n", slot);
   }   

   /* The Volume we want is on another device. */
   if (dev->is_busy()) {
      Dmsg4(100, "Vol %s for dev=%s in use dev=%s slot=%d\n",
           dcr->VolumeName, dcr->dev->print_name(),
           dev->print_name(), slot);
   }   
   for (int i=0; i < 3; i++) {
      if (dev->is_busy()) {
         wait_for_device(dcr->jcr, retries);
         continue;
      }
      break;
   }
   if (dev->is_busy()) {
      Jmsg(dcr->jcr, M_WARNING, 0, _("Volume \"%s\" wanted on %s is in use by device %s\n"),
           dcr->VolumeName, dcr->dev->print_name(), dev->print_name());
      Dmsg4(100, "Vol %s for dev=%s is busy dev=%s slot=%d\n",
           dcr->VolumeName, dcr->dev->print_name(), dev->print_name(), dev->get_slot());
      Dmsg2(100, "num_writ=%d reserv=%d\n", dev->num_writers, dev->num_reserved());
      volume_unused(dcr);
      return false;
   }
   return unload_dev(dcr, dev);
}

/*
 * Unconditionally unload a specified drive
 */
bool unload_dev(DCR *dcr, DEVICE *dev)
{
   JCR *jcr = dcr->jcr;
   bool ok = true;
   uint32_t timeout = dcr->device->max_changer_wait;
   AUTOCHANGER *changer = dcr->dev->device->changer_res;
   DEVICE *save_dev;
   int save_slot;

   if (!changer) {
      return false;
   }

   save_dev = dcr->dev;               /* save dcr device */
   dcr->set_dev(dev);                 /* temporarily point dcr at other device */

   /* Update slot if not set or not always_open */
   if (dev->get_slot() <= 0 || !dev->has_cap(CAP_ALWAYSOPEN)) {
      get_autochanger_loaded_slot(dcr);
   }

   /* Fail if we have no slot to unload */
   if (dev->get_slot() <= 0) {
      dcr->set_dev(save_dev);
      return false;
   }
   
   save_slot = dcr->VolCatInfo.Slot;
   dcr->VolCatInfo.Slot = dev->get_slot();

//   dev->dlock();

   POOLMEM *changer_cmd = get_pool_memory(PM_FNAME);
   POOL_MEM results(PM_MESSAGE);
   lock_changer(dcr);
   Jmsg(jcr, M_INFO, 0,
        _("3307 Issuing autochanger \"unload slot %d, drive %d\" command.\n"),
        dev->get_slot(), dev->drive_index);

   Dmsg2(100, "Issuing autochanger \"unload slot %d, drive %d\" command.\n",
        dev->get_slot(), dev->drive_index);

   changer_cmd = edit_device_codes(dcr, changer_cmd, 
                dcr->device->changer_command, "unload");
   dev->close();
   Dmsg2(200, "close dev=%s reserve=%d\n", dev->print_name(), 
      dev->num_reserved());
   Dmsg1(100, "Run program=%s\n", changer_cmd);
   int stat = run_program_full_output(changer_cmd, timeout, results.addr());
   dcr->VolCatInfo.Slot = save_slot;
   dcr->set_dev(save_dev);
   if (stat != 0) {
      berrno be;
      be.set_errno(stat);
      Jmsg(jcr, M_INFO, 0, _("3997 Bad autochanger \"unload slot %d, drive %d\": ERR=%s.\n"),
              dev->get_slot(), dev->drive_index, be.bstrerror());

      Dmsg3(100, "Bad autochanger \"unload slot %d, drive %d\": ERR=%s.\n",
              dev->get_slot(), dev->drive_index, be.bstrerror());
      ok = false;
      dev->clear_slot();          /* unknown */
   } else {
      Dmsg2(100, "Slot %d unloaded %s\n", dev->get_slot(), dev->print_name());
      dev->set_slot(0);           /* nothing loaded */
   }
   if (ok) {
      dev->clear_unload();
   }
   unlock_changer(dcr);

//   dev->dunlock();

   free_volume(dev);               /* Free any volume associated with this drive */
   free_pool_memory(changer_cmd);
   return ok;
}



/*
 * List the Volumes that are in the autoloader possibly
 *   with their barcodes.
 *   We assume that it is always the Console that is calling us.
 */
bool autochanger_cmd(DCR *dcr, BSOCK *dir, const char *cmd)  
{
   DEVICE *dev = dcr->dev;
   uint32_t timeout = dcr->device->max_changer_wait;
   POOLMEM *changer;
   BPIPE *bpipe;
   int len = sizeof_pool_memory(dir->msg) - 1;
   int stat;

   if (!dev->is_autochanger() || !dcr->device->changer_name ||
       !dcr->device->changer_command) {
      if (strcmp(cmd, "drives") == 0) {
         dir->fsend("drives=1\n");
      }
      dir->fsend(_("3993 Device %s not an autochanger device.\n"),
         dev->print_name());
      return false;
   }

   if (strcmp(cmd, "drives") == 0) {
      AUTOCHANGER *changer_res = dcr->device->changer_res;
      int drives = 1;
      if (changer_res) {
         drives = changer_res->device->size();
      }
      dir->fsend("drives=%d\n", drives);
      Dmsg1(100, "drives=%d\n", drives);
      return true;
   }

   /* If listing, reprobe changer */
   if (bstrcmp(cmd, "list") || bstrcmp(cmd, "listall")) {
      dcr->dev->set_slot(0);
      get_autochanger_loaded_slot(dcr);
   }

   changer = get_pool_memory(PM_FNAME);
   lock_changer(dcr);
   /* Now issue the command */
   changer = edit_device_codes(dcr, changer, 
                 dcr->device->changer_command, cmd);
   dir->fsend(_("3306 Issuing autochanger \"%s\" command.\n"), cmd);
   bpipe = open_bpipe(changer, timeout, "r");
   if (!bpipe) {
      dir->fsend(_("3996 Open bpipe failed.\n"));
      goto bail_out;            /* TODO: check if we need to return false */
   }
   if (bstrcmp(cmd, "list") || bstrcmp(cmd, "listall")) {
      /* Get output from changer */
      while (fgets(dir->msg, len, bpipe->rfd)) {
         dir->msglen = strlen(dir->msg);
         Dmsg1(100, "<stored: %s\n", dir->msg);
         bnet_send(dir);
      }
   } else if (strcmp(cmd, "slots") == 0 ) {
      char buf[100], *p;
      /* For slots command, read a single line */
      buf[0] = 0;
      fgets(buf, sizeof(buf)-1, bpipe->rfd);
      buf[sizeof(buf)-1] = 0;
      /* Strip any leading space in front of # of slots */
      for (p=buf; B_ISSPACE(*p); p++)
        { }
      dir->fsend("slots=%s", p);
      Dmsg1(100, "<stored: %s", dir->msg);
   } 
                 
   stat = close_bpipe(bpipe);
   if (stat != 0) {
      berrno be;
      be.set_errno(stat);
      dir->fsend(_("Autochanger error: ERR=%s\n"), be.bstrerror());
   }

bail_out:
   unlock_changer(dcr);
   free_pool_memory(changer);
   return true;
}


/*
 * Edit codes into ChangerCommand
 *  %% = %
 *  %a = archive device name
 *  %c = changer device name
 *  %d = changer drive index
 *  %f = Client's name
 *  %j = Job name
 *  %o = command
 *  %s = Slot base 0
 *  %S = Slot base 1
 *  %v = Volume name
 *
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *  cmd = command string (load, unload, ...)
 *
 */
char *edit_device_codes(DCR *dcr, char *omsg, const char *imsg, const char *cmd)
{
   const char *p;
   const char *str;
   char add[20];

   *omsg = 0;
   Dmsg1(1800, "edit_device_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
         switch (*++p) {
         case '%':
            str = "%";
            break;
         case 'a':
            str = dcr->dev->archive_name();
            break;
         case 'c':
            str = NPRT(dcr->device->changer_name);
            break;
         case 'd':
            sprintf(add, "%d", dcr->dev->drive_index);
            str = add;
            break;
         case 'o':
            str = NPRT(cmd);
            break;
         case 's':
            sprintf(add, "%d", dcr->VolCatInfo.Slot - 1);
            str = add;
            break;
         case 'S':
            sprintf(add, "%d", dcr->VolCatInfo.Slot);
            str = add;
            break;
         case 'j':                    /* Job name */
            str = dcr->jcr->Job;
            break;
         case 'v':
            if (dcr->VolCatInfo.VolCatName[0]) {
               str = dcr->VolCatInfo.VolCatName;
            } else if (dcr->VolumeName[0]) {
               str = dcr->VolumeName;
            } else if (dcr->dev->vol && dcr->dev->vol->vol_name) {
               str = dcr->dev->vol->vol_name;
            } else {
               str = dcr->dev->VolHdr.VolumeName;
            }
            break;
         case 'f':
            str = NPRT(dcr->jcr->client_name);
            break;

         default:
            add[0] = '%';
            add[1] = *p;
            add[2] = 0;
            str = add;
            break;
         }
      } else {
         add[0] = *p;
         add[1] = 0;
         str = add;
      }
      Dmsg1(1900, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg);
   }
   Dmsg1(800, "omsg=%s\n", omsg);
   return omsg;
}
