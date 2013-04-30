/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2012 Free Software Foundation Europe e.V.

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
 *   dvd.c  -- Routines specific to DVD devices (and
 *             possibly other removable hard media). 
 *
 *    Nicolas Boichat, MMV
 *
 */

#include "bacula.h"
#include "stored.h"

/* Forward referenced functions */
static void add_file_and_part_name(DEVICE *dev, POOL_MEM &archive_name);

/* 
 * Write the current volume/part filename to archive_name.
 */
void make_mounted_dvd_filename(DEVICE *dev, POOL_MEM &archive_name) 
{
   pm_strcpy(archive_name, dev->device->mount_point);
   add_file_and_part_name(dev, archive_name);
}

void make_spooled_dvd_filename(DEVICE *dev, POOL_MEM &archive_name)
{
   /* Use the working directory if spool directory is not defined */
   if (dev->device->spool_directory) {
      pm_strcpy(archive_name, dev->device->spool_directory);
   } else {
      pm_strcpy(archive_name, working_directory);
   }
   add_file_and_part_name(dev, archive_name);
}      

static void add_file_and_part_name(DEVICE *dev, POOL_MEM &archive_name)
{
   char partnumber[20];

   if (!IsPathSeparator(archive_name.c_str()[strlen(archive_name.c_str())-1])) {
      pm_strcat(archive_name, "/");
   }

   pm_strcat(archive_name, dev->getVolCatName());
   /* if part > 1, append .# to the filename (where # is the part number) */
   if (dev->part > 1) {
      pm_strcat(archive_name, ".");
      bsnprintf(partnumber, sizeof(partnumber), "%d", dev->part);
      pm_strcat(archive_name, partnumber);
   }
   Dmsg2(400, "Exit add_file_part_name: arch=%s, part=%d\n",
                  archive_name.c_str(), dev->part);
}  

/* Update the free space on the device */
bool DEVICE::update_freespace() 
{
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM* results;
   char* icmd;
   int timeout;
   uint64_t free;
   char ed1[50];
   bool ok = false;
   int status;

   if (!is_dvd() || is_freespace_ok()) {
      return true;
   }
   
   /* The device must be mounted in order to dvd-freespace to work */
   mount(1);
   
   Dsm_check(400);
   icmd = device->free_space_command;
   
   if (!icmd) {
      free_space = 0;
      free_space_errno = 0;
      clear_freespace_ok();              /* No valid freespace */
      clear_media();
      Dmsg2(29, "ERROR: update_free_space_dev: free_space=%s, free_space_errno=%d (!icmd)\n", 
            edit_uint64(free_space, ed1), free_space_errno);
      Mmsg(errmsg, _("No FreeSpace command defined.\n"));
      return false;
   }
   
   edit_mount_codes(ocmd, icmd);
   
   Dmsg1(29, "update_freespace: cmd=%s\n", ocmd.c_str());

   results = get_pool_memory(PM_MESSAGE);
   
   /* Try at most 3 times to get the free space on the device. This should perhaps be configurable. */
   timeout = 3;
   
   while (1) {
      berrno be;
      Dmsg1(20, "Run freespace prog=%s\n", ocmd.c_str());
      status = run_program_full_output(ocmd.c_str(), max_open_wait/2, results);
      Dmsg2(500, "Freespace status=%d result=%s\n", status, results);
      if (status == 0) {
         free = str_to_int64(results);
         Dmsg1(400, "Free space program run: Freespace=%s\n", results);
         if (free >= 0) {
            free_space = free;
            free_space_errno = 0;
            set_freespace_ok();     /* have valid freespace */
            set_media();
            Mmsg(errmsg, "");
            ok = true;
            break;
         }
      }
      free_space = 0;
      free_space_errno = EPIPE;
      clear_freespace_ok();         /* no valid freespace */
      Mmsg2(errmsg, _("Cannot run free space command. Results=%s ERR=%s\n"), 
            results, be.bstrerror(status));
      
      if (--timeout > 0) {
         Dmsg4(40, "Cannot get free space on device %s. free_space=%s, "
            "free_space_errno=%d ERR=%s\n", print_name(), 
               edit_uint64(free_space, ed1), free_space_errno, 
               errmsg);
         bmicrosleep(1, 0);
         continue;
      }

      dev_errno = free_space_errno;
      Dmsg4(40, "Cannot get free space on device %s. free_space=%s, "
         "free_space_errno=%d ERR=%s\n",
            print_name(), edit_uint64(free_space, ed1),
            free_space_errno, errmsg);
      break;
   }
   
   free_pool_memory(results);
   Dmsg4(29, "leave update_freespace: free_space=%s freespace_ok=%d free_space_errno=%d have_media=%d\n", 
      edit_uint64(free_space, ed1), !!is_freespace_ok(), free_space_errno, !!have_media());
   Dsm_check(400);
   return ok;
}

/*
 * Note!!!! Part numbers now begin at 1. The part number is
 *  suppressed from the first part, which is just the Volume
 *  name. Each subsequent part is the Volumename.partnumber.
 *
 * Write a part (Vol, Vol.2, ...) from the spool to the DVD   
 * This routine does not update the part number, so normally, you
 *  should call open_next_part()
 *
 * It is also called from truncate_dvd to "blank" the medium, as
 *  well as from block.c when the DVD is full to write the last part.
 */
bool dvd_write_part(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   POOL_MEM archive_name(PM_FNAME);
   
   /*
    * Don't write empty part files.
    * This is only useful when growisofs does not support write beyond
    * the 4GB boundary.
    * Example :
    *   - 3.9 GB on the volume, dvd-freespace reports 0.4 GB free
    *   - Write 0.2 GB on the volume, Bacula thinks it could still
    *     append data, it creates a new empty part.
    *   - dvd-freespace reports 0 GB free, as the 4GB boundary has
    *     been crossed
    *   - Bacula thinks he must finish to write to the device, so it
    *     tries to write the last part (0-byte), but dvd-writepart fails...
    *
    * There is one exception: when recycling a volume, we write a blank part
    * file, so, then, we need to accept to write it.
    */
   if (dev->part_size == 0 && !dev->truncating) {
      Dmsg2(29, "dvd_write_part: device is %s, won't write blank part %d\n", dev->print_name(), dev->part);
      /* Delete spool file */
      make_spooled_dvd_filename(dev, archive_name);
      unlink(archive_name.c_str());
      dev->set_part_spooled(false);
      Dmsg1(29, "========= unlink(%s)\n", archive_name.c_str());
      Dsm_check(400);
      return true;
   }
   
   POOL_MEM ocmd(PM_FNAME);
   POOL_MEM results(PM_MESSAGE);
   char* icmd;
   int status;
   int timeout;
   char ed1[50];
   
   dev->clear_freespace_ok();             /* need to update freespace */

   Dsm_check(400);
   Dmsg3(29, "dvd_write_part: device is %s, part is %d, is_mounted=%d\n", dev->print_name(), dev->part, dev->is_mounted());
   icmd = dev->device->write_part_command;
   
   dev->edit_mount_codes(ocmd, icmd);
      
   /*
    * original line follows
    * timeout = dev->max_open_wait + (dev->max_part_size/(1350*1024/2));
    * I modified this for a longer timeout; pre-formatting, blanking and
    * writing can take quite a while
    */

   /* Explanation of the timeout value, when writing the first part,
    *  by Arno Lehmann :
    * 9 GB, write speed 1x: 6990 seconds (almost 2 hours...)
    * Overhead: 900 seconds (starting, initializing, finalizing,probably 
    *   reloading 15 minutes)
    * Sum: 15780.
    * A reasonable last-exit timeout would be 16000 seconds. Quite long - 
    * almost 4.5 hours, but hopefully, that timeout will only ever be needed 
    * in case of a serious emergency.
    */

   if (dev->part == 1) {
      timeout = 16000;
   } else {
      timeout = dev->max_open_wait + (dev->part_size/(1350*1024/4));
   }

   Dmsg2(20, "Write part: cmd=%s timeout=%d\n", ocmd.c_str(), timeout);
   status = run_program_full_output(ocmd.c_str(), timeout, results.addr());
   Dmsg2(20, "Write part status=%d result=%s\n", status, results.c_str());

   dev->blank_dvd = false;
   if (status != 0) {
      Jmsg2(dcr->jcr, M_FATAL, 0, _("Error writing part %d to the DVD: ERR=%s\n"),
         dev->part, results.c_str());
      Mmsg1(dev->errmsg, _("Error while writing current part to the DVD: %s"), 
            results.c_str());
      Dmsg1(100, "%s\n", dev->errmsg);
      dev->dev_errno = EIO;
      if (!dev->truncating) {
         dcr->mark_volume_in_error();
      }
      Dsm_check(400);
      return false;
   }
   Jmsg(dcr->jcr, M_INFO, 0, _("Part %d (%lld bytes) written to DVD.\n"), dev->part, dev->part_size);
   Dmsg3(400, "dvd_write_part: Part %d (%lld bytes) written to DVD\nResults: %s\n",
            dev->part, dev->part_size, results.c_str());
    
   dev->num_dvd_parts++;            /* there is now one more part on DVD */
   dev->VolCatInfo.VolCatParts = dev->num_dvd_parts;
   dcr->VolCatInfo.VolCatParts = dev->num_dvd_parts;
   Dmsg1(100, "Update num_parts=%d\n", dev->num_dvd_parts);

   /* Delete spool file */
   make_spooled_dvd_filename(dev, archive_name);
   unlink(archive_name.c_str());
   dev->set_part_spooled(false);
   Dmsg1(29, "========= unlink(%s)\n", archive_name.c_str());
   Dsm_check(400);
   
   /* growisofs umounted the device, so remount it (it will update the free space) */
   dev->clear_mounted();
   dev->mount(1);
   Jmsg(dcr->jcr, M_INFO, 0, _("Remaining free space %s on %s\n"), 
      edit_uint64_with_commas(dev->free_space, ed1), dev->print_name());
   Dsm_check(400);
   return true;
}

/*
 * Open the next part file.
 *  - Close the fd
 *  - Increment part number 
 *  - Reopen the device
 */
int dvd_open_next_part(DCR *dcr)
{
   DEVICE *dev = dcr->dev;

   Dmsg6(29, "Enter: == open_next_part part=%d npart=%d dev=%s vol=%s mode=%d file_addr=%d\n", 
      dev->part, dev->num_dvd_parts, dev->print_name(),
         dev->getVolCatName(), dev->openmode, dev->file_addr);
   if (!dev->is_dvd()) {
      Dmsg1(100, "Device %s is not dvd!!!!\n", dev->print_name()); 
      return -1;
   }
   
   /* When appending, do not open a new part if the current is empty */
   if (dev->can_append() && (dev->part > dev->num_dvd_parts) && 
       (dev->part_size == 0)) {
      Dmsg0(29, "open_next_part exited immediately (dev->part_size == 0).\n");
      return dev->fd();
   }

   dev->close_part(dcr);               /* close current part */
   
   /*
    * If we have a spooled part open, write it to the
    *  DVD before opening the next part.
    */
   if (dev->is_part_spooled()) {
      Dmsg2(100, "Before open next write previous. part=%d num_parts=%d\n",
         dev->part, dev->num_dvd_parts);
      if (!dvd_write_part(dcr)) {
         Dmsg0(29, "Error in dvd_write part.\n");
         return -1;
      }
   }
     
   dev->part_start += dev->part_size;
   dev->part++;
   Dmsg2(29, "Inc part=%d num_dvd_parts=%d\n", dev->part, dev->num_dvd_parts);

   /* Are we working on a part past what is written in the DVD? */
   if (dev->num_dvd_parts < dev->part) {
      POOL_MEM archive_name(PM_FNAME);
      struct stat buf;
      /* 
       * First check what is on DVD.  If our part is there, we
       *   are in trouble, so bail out.
       * NB: This is however not a problem if we are writing the first part.
       * It simply means that we are over writing an existing volume...
       */
      if (dev->num_dvd_parts > 0) {
         make_mounted_dvd_filename(dev, archive_name);   /* makes dvd name */
         Dmsg1(100, "Check if part on DVD: %s\n", archive_name.c_str());
         if (stat(archive_name.c_str(), &buf) == 0) {
            /* bad news bail out */
            dev->set_part_spooled(false);
            Mmsg1(&dev->errmsg, _("Next Volume part already exists on DVD. Cannot continue: %s\n"),
               archive_name.c_str());
            return -1;
         }
      }
   }

   Dmsg2(400, "Call dev->open(vol=%s, mode=%d)\n", dcr->getVolCatName(), 
         dev->openmode);

   /* Open next part.  Note, this sets part_size for part opened. */
   if (!dev->open(dcr, OPEN_READ_ONLY)) {
      return -1;
   } 
   dev->set_labeled();                   /* all next parts are "labeled" */
   
   return dev->fd();
}

/*
 * Open the first part file.
 *  - Close the fd
 *  - Reopen the device
 */
static bool dvd_open_first_part(DCR *dcr, int mode)
{
   DEVICE *dev = dcr->dev;

   Dmsg5(29, "Enter: ==== open_first_part dev=%s Vol=%s mode=%d num_dvd_parts=%d append=%d\n", dev->print_name(), 
         dev->getVolCatName(), dev->openmode, dev->num_dvd_parts, dev->can_append());


   dev->close_part(dcr);

   Dmsg2(400, "Call dev->open(vol=%s, mode=%d)\n", dcr->getVolCatName(), 
         mode);
   Dmsg0(100, "Set part=1\n");
   dev->part = 1;
   dev->part_start = 0;

   if (!dev->open(dcr, mode)) {
      Dmsg0(400, "open dev() failed\n");
      return false;
   }
   Dmsg2(400, "Leave open_first_part state=%s append=%d\n", dev->is_open()?"open":"not open", dev->can_append());
   
   return true;
}


/* 
 * Do an lseek on a DVD handling all the different parts
 */
boffset_t lseek_dvd(DCR *dcr, boffset_t offset, int whence)
{
   DEVICE *dev;
   boffset_t pos;
   char ed1[50], ed2[50];

   if (!dcr) {                  /* can be NULL when called from rewind(NULL) */
      return -1;
   }
   dev = dcr->dev;
   
   Dmsg5(400, "Enter lseek_dvd fd=%d off=%s w=%d part=%d nparts=%d\n", dev->fd(),
      edit_int64(offset, ed1), whence, dev->part, dev->num_dvd_parts);

   switch(whence) {
   case SEEK_SET:
      Dmsg2(400, "lseek_dvd SEEK_SET to %s (part_start=%s)\n",
         edit_int64(offset, ed1), edit_uint64(dev->part_start, ed2));
      if ((uint64_t)offset >= dev->part_start) {
         if ((uint64_t)offset == dev->part_start || 
             (uint64_t)offset < dev->part_start+dev->part_size) {
            /* We are staying in the current part, just seek */
#if defined(HAVE_WIN32)
            pos = _lseeki64(dev->fd(), offset-dev->part_start, SEEK_SET);
#else
            pos = lseek(dev->fd(), offset-dev->part_start, SEEK_SET);
#endif
            if (pos < 0) {
               return pos;
            } else {
               return pos + dev->part_start;
            }
         } else {
            /* Load next part, and start again */
            Dmsg0(100, "lseek open next part\n");
            if (dvd_open_next_part(dcr) < 0) {
               Dmsg0(400, "lseek_dvd failed while trying to open the next part\n");
               return -1;
            }
            Dmsg2(100, "Recurse lseek after open next part=%d num_part=%d\n",
               dev->part, dev->num_dvd_parts);
            return lseek_dvd(dcr, offset, SEEK_SET);
         }
      } else {
         /*
          * pos < dev->part_start :
          * We need to access a previous part, 
          * so just load the first one, and seek again
          * until the right one is loaded
          */
         Dmsg0(100, "lseek open first part\n");
         if (!dvd_open_first_part(dcr, dev->openmode)) {
            Dmsg0(400, "lseek_dvd failed while trying to open the first part\n");
            return -1;
         }
         Dmsg2(100, "Recurse lseek after open first part=%d num_part=%d\n",
               dev->part, dev->num_dvd_parts);
         return lseek_dvd(dcr, offset, SEEK_SET); /* system lseek */
      }
      break;
   case SEEK_CUR:
      Dmsg1(400, "lseek_dvd SEEK_CUR to %s\n", edit_int64(offset, ed1));
      if ((pos = lseek(dev->fd(), 0, SEEK_CUR)) < 0) {
         Dmsg0(400, "Seek error.\n");
         return pos;                  
      }
      pos += dev->part_start;
      if (offset == 0) {
         Dmsg1(400, "lseek_dvd SEEK_CUR returns %s\n", edit_uint64(pos, ed1));
         return pos;
      } else { 
         Dmsg1(400, "do lseek_dvd SEEK_SET %s\n", edit_uint64(pos, ed1));
         return lseek_dvd(dcr, pos, SEEK_SET);
      }
      break;
   case SEEK_END:
      Dmsg1(400, "lseek_dvd SEEK_END to %s\n", edit_int64(offset, ed1));
      /*
       * Bacula does not use offsets for SEEK_END
       *  Also, Bacula uses seek_end only when it wants to
       *  append to the volume, so for a dvd that means
       *  that the volume must be spooled since the DVD
       *  itself is read-only (as currently implemented).
       */
      if (offset > 0) { /* Not used by bacula */
         Dmsg1(400, "lseek_dvd SEEK_END called with an invalid offset %s\n", 
            edit_uint64(offset, ed1));
         errno = EINVAL;
         return -1;
      }
      /* If we are already on a spooled part and have the
       *  right part number, simply seek
       */
      if (dev->is_part_spooled() && dev->part > dev->num_dvd_parts) {
         if ((pos = lseek(dev->fd(), 0, SEEK_END)) < 0) {
            return pos;   
         } else {
            Dmsg1(400, "lseek_dvd SEEK_END returns %s\n", 
                  edit_uint64(pos + dev->part_start, ed1));
            return pos + dev->part_start;
         }
      } else {
         /*
          * Load the first part, then load the next until we reach the last one.
          * This is the only way to be sure we compute the right file address.
          *
          * Save previous openmode, and open all but last part read-only 
          * (useful for DVDs) 
          */
         int modesave = dev->openmode;
         if (!dvd_open_first_part(dcr, OPEN_READ_ONLY)) {
            Dmsg0(400, "lseek_dvd failed while trying to open the first part\n");
            return -1;
         }
         if (dev->num_dvd_parts > 0) {
            while (dev->part < dev->num_dvd_parts) {
               if (dvd_open_next_part(dcr) < 0) {
                  Dmsg0(400, "lseek_dvd failed while trying to open the next part\n");
                  return -1;
               }
            }
            dev->openmode = modesave;
            if (dvd_open_next_part(dcr) < 0) {
               Dmsg0(400, "lseek_dvd failed while trying to open the next part\n");
               return -1;
            }
         }
         return lseek_dvd(dcr, 0, SEEK_END);
      }
      break;
   default:
      Dmsg0(400, "Seek call error.\n");
      errno = EINVAL;
      return -1;
   }
}

bool dvd_close_job(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   JCR *jcr = dcr->jcr;
   bool ok = true;

   /*
    * If the device is a dvd and WritePartAfterJob
    * is set to yes, open the next part, so, in case of a device
    * that requires mount, it will be written to the device.
    */
   if (dev->is_dvd() && jcr->write_part_after_job && (dev->part_size > 0)) {
      Dmsg1(400, "Writing last part=%d write_partafter_job is set.\n",
         dev->part);
      if (dev->part < dev->num_dvd_parts+1) {
         Jmsg3(jcr, M_FATAL, 0, _("Error writing. Current part less than total number of parts (%d/%d, device=%s)\n"),
               dev->part, dev->num_dvd_parts, dev->print_name());
         dev->dev_errno = EIO;
         ok = false;
      }
      
      if (ok && !dvd_write_part(dcr)) {
         Jmsg2(jcr, M_FATAL, 0, _("Unable to write last on %s: ERR=%s\n"),
               dev->print_name(), dev->bstrerror());
         dev->dev_errno = EIO;
         ok = false;
      }
   }
   return ok;
}

void dvd_remove_empty_part(DCR *dcr) 
{
   DEVICE *dev = dcr->dev;

   /* Remove the last part file if it is empty */
   if (dev->is_dvd() && dev->num_dvd_parts > 0) {
      struct stat statp;
      uint32_t part_save = dev->part;
      POOL_MEM archive_name(PM_FNAME);
      int status;

      dev->part = dev->num_dvd_parts;
      make_spooled_dvd_filename(dev, archive_name);
      /* Check that the part file is empty */
      status = stat(archive_name.c_str(), &statp);
      if (status == 0 && statp.st_size == 0) {
         Dmsg3(100, "Unlink empty part in close call make_dvd_filename. part=%d num=%d vol=%s\n", 
                part_save, dev->num_dvd_parts, dev->getVolCatName());
         Dmsg1(100, "unlink(%s)\n", archive_name.c_str());
         unlink(archive_name.c_str());
         if (part_save == dev->part) {
            dev->set_part_spooled(false);  /* no spooled part left */
         }
      } else if (status < 0) {                         
         if (part_save == dev->part) {
            dev->set_part_spooled(false);  /* spool doesn't exit */
         }
      }       
      dev->part = part_save;               /* restore part number */
   }
}

bool truncate_dvd(DCR *dcr) 
{
   DEVICE* dev = dcr->dev;

   dev->clear_freespace_ok();             /* need to update freespace */
   dev->close_part(dcr);

   if (!dev->unmount(1)) {
      Dmsg0(400, "truncate_dvd: Failed to unmount DVD\n");
      return false;
   }

   /* If necessary, delete its spool file. */
   if (dev->is_part_spooled()) {
      POOL_MEM archive_name(PM_FNAME);
      /* Delete spool file */
      make_spooled_dvd_filename(dev, archive_name);
      unlink(archive_name.c_str());
      dev->set_part_spooled(false);
   }

   /* Set num_dvd_parts to zero (on disk) */
   dev->part = 0;
   dev->num_dvd_parts = 0;
   dcr->VolCatInfo.VolCatParts = 0;
   dev->VolCatInfo.VolCatParts = 0;
   
   Dmsg0(400, "truncate_dvd: Opening first part (1)...\n");
   
   dev->truncating = true;
   /* This creates a zero length spool file and sets part=1 */
   if (!dvd_open_first_part(dcr, CREATE_READ_WRITE)) {
      Dmsg0(400, "truncate_dvd: Error while opening first part (1).\n");
      dev->truncating = false;
      return false;
   }

   dev->close_part(dcr);
   
   Dmsg0(400, "truncate_dvd: Opening first part (2)...\n");
   
   /* 
    * Now actually truncate the DVD which is done by writing
    *  a zero length part to the DVD/
    */
   if (!dvd_write_part(dcr)) {
      Dmsg0(400, "truncate_dvd: Error while writing to DVD.\n");
      dev->truncating = false;
      return false;
   }
   dev->truncating = false;
   
   /* Set num_dvd_parts to zero (on disk) */
   dev->part = 0;
   dev->num_dvd_parts = 0;
   dcr->VolCatInfo.VolCatParts = 0;
   dev->VolCatInfo.VolCatParts = 0;
   /* Clear the size of the volume */
   dev->VolCatInfo.VolCatBytes = 0;
   dcr->VolCatInfo.VolCatBytes = 0;

   /* Update catalog */
   if (!dir_update_volume_info(dcr, false, true)) {
      return false;
   }
   
   return true;
}

/*
 * Checks if we can write on a non-blank DVD: meaning that it just have been
 * truncated (there is only one zero-sized file on the DVD).
 *  
 * Note!  Normally if we can mount the device, which should be the case
 *   when we get here, it is not a blank DVD.  Hence we check if
 *   if all files are of zero length (i.e. no data), in which case we allow it.
 *
 */
bool check_can_write_on_non_blank_dvd(DCR *dcr) 
{
   DEVICE* dev = dcr->dev;
   DIR* dp;
   struct dirent *entry, *result;
   int name_max;
   struct stat filestat;
   bool ok = true;
      
   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }
   
   if (!(dp = opendir(dev->device->mount_point))) {
      berrno be;
      dev->dev_errno = errno;
      Dmsg3(29, "check_can_write_on_non_blank_dvd: failed to open dir %s (dev=%s), ERR=%s\n", 
            dev->device->mount_point, dev->print_name(), be.bstrerror());
      return false;
   }
   
   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   for ( ;; ) {
      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
         dev->dev_errno = EIO;
         Dmsg2(129, "check_can_write_on_non_blank_dvd: no more files in dir %s (dev=%s)\n", 
               dev->device->mount_point, dev->print_name());
         break;
      } else {
         Dmsg2(99, "check_can_write_on_non_blank_dvd: found %s (versus %s)\n", 
               result->d_name, dev->getVolCatName());
         if (strcmp(result->d_name, ".") && strcmp(result->d_name, "..") &&
             strcmp(result->d_name, ".keep")) {
            /* Found a file, checking it is empty */
            POOL_MEM filename(PM_FNAME);
            pm_strcpy(filename, dev->device->mount_point);
            if (!IsPathSeparator(filename.c_str()[strlen(filename.c_str())-1])) {
               pm_strcat(filename, "/");
            }
            pm_strcat(filename, result->d_name);
            if (stat(filename.c_str(), &filestat) < 0) {
               berrno be;
               dev->dev_errno = errno;
               Dmsg2(29, "check_can_write_on_non_blank_dvd: cannot stat file (file=%s), ERR=%s\n", 
                  filename.c_str(), be.bstrerror());
               ok = false;
               break;
            }
            Dmsg2(99, "check_can_write_on_non_blank_dvd: size of %s is %lld\n", 
               filename.c_str(), filestat.st_size);
            if (filestat.st_size != 0) {
               ok = false;
               break;
            }
         }
      }
   }
   free(entry);
   closedir(dp);
   
   Dmsg1(29, "OK  can_write_on_non_blank_dvd: OK=%d\n", ok);
   return ok;
}

/* 
 * Mount a DVD device, then scan to find out how many parts
 *  there are.
 */
int find_num_dvd_parts(DCR *dcr)
{
   DEVICE *dev = dcr->dev;
   int num_parts = 0;

   if (!dev->is_dvd()) {
      return 0;
   }
   
   if (dev->mount(1)) {
      DIR* dp;
      struct dirent *entry, *result;
      int name_max;
      int len = strlen(dcr->getVolCatName());

      /* Now count the number of parts */
      name_max = pathconf(".", _PC_NAME_MAX);
      if (name_max < 1024) {
         name_max = 1024;
      }
         
      if (!(dp = opendir(dev->device->mount_point))) {
         berrno be;
         dev->dev_errno = errno;
         Dmsg3(29, "find_num_dvd_parts: failed to open dir %s (dev=%s), ERR=%s\n", 
               dev->device->mount_point, dev->print_name(), be.bstrerror());
         goto get_out;
      }
      
      entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);

      Dmsg1(100, "Looking for Vol=%s\n", dcr->getVolCatName());
      for ( ;; ) {
         int flen;
         bool ignore;
         if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
            dev->dev_errno = EIO;
            Dmsg2(129, "find_num_dvd_parts: failed to find suitable file in dir %s (dev=%s)\n", 
                  dev->device->mount_point, dev->print_name());
            break;
         }
         flen = strlen(result->d_name);
         ignore = true;
         if (flen >= len) {
            result->d_name[len] = 0;
            if (strcmp(dcr->getVolCatName(), result->d_name) == 0) {
               num_parts++;
               Dmsg1(100, "find_num_dvd_parts: found part: %s\n", result->d_name);
               ignore = false;
            }
         }
         if (ignore) {
            Dmsg2(129, "find_num_dvd_parts: ignoring %s in %s\n", 
                  result->d_name, dev->device->mount_point);
         }
      }
      free(entry);
      closedir(dp);
      Dmsg1(29, "find_num_dvd_parts = %d\n", num_parts);
   }
   
get_out:
   dev->set_freespace_ok();
   if (dev->is_mounted()) {
      dev->unmount(0);
   }
   return num_parts;
}
