/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Subroutines to handle Catalog reqests sent to the Director
 * Reqests/commands from the Director are handled in dircmd.c
 *
 * Kern Sibbald, December 2000
 */

#include "bareos.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */
#include "lib/crypto_cache.h"

static const int dbglvl = 200;
static pthread_mutex_t vol_info_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Requests sent to the Director */
static char Find_media[] =
   "CatReq Job=%s FindMedia=%d pool_name=%s media_type=%s\n";
static char Get_Vol_Info[] =
   "CatReq Job=%s GetVolInfo VolName=%s write=%d\n";
static char Update_media[] =
   "CatReq Job=%s UpdateMedia VolName=%s"
   " VolJobs=%u VolFiles=%u VolBlocks=%u VolBytes=%s VolMounts=%u"
   " VolErrors=%u VolWrites=%u MaxVolBytes=%s EndTime=%s VolStatus=%s"
   " Slot=%d relabel=%d InChanger=%d VolReadTime=%s VolWriteTime=%s"
   " VolFirstWritten=%s\n";
static char Create_job_media[] =
   "CatReq Job=%s CreateJobMedia"
   " FirstIndex=%u LastIndex=%u StartFile=%u EndFile=%u"
   " StartBlock=%u EndBlock=%u Copy=%d Strip=%d MediaId=%s\n";
static char FileAttributes[] =
   "UpdCat Job=%s FileAttributes ";

/* Responses received from the Director */
static char OK_media[] =
   "1000 OK VolName=%127s VolJobs=%u VolFiles=%lu"
   " VolBlocks=%lu VolBytes=%lld VolMounts=%lu VolErrors=%lu VolWrites=%lu"
   " MaxVolBytes=%lld VolCapacityBytes=%lld VolStatus=%20s"
   " Slot=%ld MaxVolJobs=%lu MaxVolFiles=%lu InChanger=%ld"
   " VolReadTime=%lld VolWriteTime=%lld EndFile=%lu EndBlock=%lu"
   " LabelType=%ld MediaId=%lld EncryptionKey=%127s"
   " MinBlocksize=%lu MaxBlocksize=%lu\n";
static char OK_create[] =
   "1000 OK CreateJobMedia\n";

#ifdef needed
static char Device_update[] =
   "DevUpd Job=%s device=%s "
   "append=%d read=%d num_writers=%d "
   "open=%d labeled=%d offline=%d "
   "reserved=%d max_writers=%d "
   "autoselect=%d autochanger=%d "
   "changer_name=%s media_type=%s volume_name=%s\n";

/*
 * Send update information about a device to Director
 */
bool SD_DCR::dir_update_device(JCR *jcr, DEVICE *dev)
{
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM dev_name, VolumeName, MediaType, ChangerName;
   DEVRES *device = dev->device;
   bool ok;

   pm_strcpy(dev_name, device->hdr.name);
   bash_spaces(dev_name);
   if (dev->is_labeled()) {
      pm_strcpy(VolumeName, dev->VolHdr.VolumeName);
   } else {
      pm_strcpy(VolumeName, "*");
   }
   bash_spaces(VolumeName);
   pm_strcpy(MediaType, device->media_type);
   bash_spaces(MediaType);
   if (device->changer_res) {
      pm_strcpy(ChangerName, device->changer_res->hdr.name);
      bash_spaces(ChangerName);
   } else {
      pm_strcpy(ChangerName, "*");
   }
   ok = dir->fsend(Device_update,
      jcr->Job,
      dev_name.c_str(),
      dev->can_append()!=0,
      dev->can_read()!=0, dev->num_writers,
      dev->is_open()!=0, dev->is_labeled()!=0,
      dev->is_offline()!=0, dev->reserved_device,
      dev->is_tape()?100000:1,
      dev->autoselect, 0,
      ChangerName.c_str(), MediaType.c_str(), VolumeName.c_str());
   Dmsg1(dbglvl, ">dird: %s\n", dir->msg);
   return ok;
}

bool SD_DCR::dir_update_changer(JCR *jcr, AUTOCHANGER *changer)
{
   BSOCK *dir = jcr->dir_bsock;
   POOL_MEM dev_name, MediaType;
   DEVRES *device;
   bool ok;

   pm_strcpy(dev_name, changer->hdr.name);
   bash_spaces(dev_name);
   device = (DEVRES *)changer->device->first();
   pm_strcpy(MediaType, device->media_type);
   bash_spaces(MediaType);
   /* This is mostly to indicate that we are here */
   ok = dir->fsend(Device_update,
      jcr->Job,
      dev_name.c_str(),         /* Changer name */
      0, 0, 0,                  /* append, read, num_writers */
      0, 0, 0,                  /* is_open, is_labeled, offline */
      0, 0,                     /* reserved, max_writers */
      0,                        /* Autoselect */
      changer->device->size(),  /* Number of devices */
      "0",                      /* PoolId */
      "*",                      /* ChangerName */
      MediaType.c_str(),        /* MediaType */
      "*");                     /* VolName */
   Dmsg1(dbglvl, ">dird: %s\n", dir->msg);
   return ok;
}
#endif

/**
 * Common routine for:
 *   dir_get_volume_info()
 * and
 *   dir_find_next_appendable_volume()
 *
 *  NOTE!!! All calls to this routine must be protected by
 *          locking vol_info_mutex before calling it so that
 *          we don't have one thread modifying the parameters
 *          and another reading them.
 *
 *  Returns: true  on success and vol info in dcr->VolCatInfo
 *           false on failure
 */
static bool do_get_volume_info(DCR *dcr)
{
    JCR *jcr = dcr->jcr;
    BSOCK *dir = jcr->dir_bsock;
    VOLUME_CAT_INFO vol;
    int n;
    int32_t InChanger;

    dcr->setVolCatInfo(false);
    if (dir->recv() <= 0) {
       Dmsg0(dbglvl, "getvolname error bnet_recv\n");
       Mmsg(jcr->errmsg, _("Network error on bnet_recv in req_vol_info.\n"));
       return false;
    }
    memset(&vol, 0, sizeof(vol));
    Dmsg1(dbglvl, "<dird %s", dir->msg);
    n = sscanf(dir->msg, OK_media, vol.VolCatName,
               &vol.VolCatJobs, &vol.VolCatFiles,
               &vol.VolCatBlocks, &vol.VolCatBytes,
               &vol.VolCatMounts, &vol.VolCatErrors,
               &vol.VolCatWrites, &vol.VolCatMaxBytes,
               &vol.VolCatCapacityBytes, vol.VolCatStatus,
               &vol.Slot, &vol.VolCatMaxJobs, &vol.VolCatMaxFiles,
               &InChanger, &vol.VolReadTime, &vol.VolWriteTime,
               &vol.EndFile, &vol.EndBlock, &vol.LabelType,
               &vol.VolMediaId, vol.VolEncrKey,
               &vol.VolMinBlocksize, &vol.VolMaxBlocksize);
    if (n != 24) {
       Dmsg3(dbglvl, "Bad response from Dir fields=%d, len=%d: %s",
             n, dir->msglen, dir->msg);
       Mmsg(jcr->errmsg, _("Error getting Volume info: %s"), dir->msg);
       return false;
    }
    vol.InChanger = InChanger;        /* bool in structure */
    vol.is_valid = true;
    unbash_spaces(vol.VolCatName);
    bstrncpy(dcr->VolumeName, vol.VolCatName, sizeof(dcr->VolumeName));
    dcr->VolCatInfo = vol;            /* structure assignment */

    /*
     * If we received a new crypto key update the cache and write out the new cache on a change.
     */
    if (*vol.VolEncrKey) {
       if (update_crypto_cache(vol.VolCatName, vol.VolEncrKey)) {
          write_crypto_cache(me->working_directory, "bareos-sd",
                             get_first_port_host_order(me->SDaddrs));
       }
    }

    Dmsg4(dbglvl, "do_get_volume_info return true slot=%d Volume=%s, "
                  "VolminBlocksize=%u VolMaxBlocksize=%u\n",
          vol.Slot, vol.VolCatName, vol.VolMinBlocksize, vol.VolMaxBlocksize);
    Dmsg2(dbglvl, "setting dcr->VolMinBlocksize(%u) to vol.VolMinBlocksize(%u)\n",
          dcr->VolMinBlocksize, vol.VolMinBlocksize);
    Dmsg2(dbglvl, "setting dcr->VolMaxBlocksize(%u) to vol.VolMaxBlocksize(%u)\n",
          dcr->VolMaxBlocksize, vol.VolMaxBlocksize);

    /*
     * Assign the volcatinfo to the dcr.
     */
    dcr->VolMinBlocksize = vol.VolMinBlocksize;
    dcr->VolMaxBlocksize = vol.VolMaxBlocksize;

    return true;
}

/**
 * Get Volume info for a specific volume from the Director's Database
 *
 * Returns: true  on success   (Director guarantees that Pool and MediaType
 *                              are correct and VolStatus==Append or
 *                              VolStatus==Recycle)
 *          false on failure
 *
 *          Volume information returned in dcr->VolCatInfo
 */
bool SD_DCR::dir_get_volume_info(enum get_vol_info_rw writing)
{
   bool ok;
   BSOCK *dir = jcr->dir_bsock;

   P(vol_info_mutex);
   setVolCatName(VolumeName);
   bash_spaces(getVolCatName());
   dir->fsend(Get_Vol_Info, jcr->Job, getVolCatName(),
              (writing == GET_VOL_INFO_FOR_WRITE) ? 1 : 0);
   Dmsg1(dbglvl, ">dird %s", dir->msg);
   unbash_spaces(getVolCatName());
   ok = do_get_volume_info(this);
   V(vol_info_mutex);

   return ok;
}

/**
 * Get info on the next appendable volume in the Director's database
 *
 * Returns: true  on success dcr->VolumeName is volume
 *                reserve_volume() called on Volume name
 *          false on failure dcr->VolumeName[0] == 0
 *                also sets dcr->found_in_use if at least one
 *                in use volume was found.
 *
 *          Volume information returned in dcr
 */
bool SD_DCR::dir_find_next_appendable_volume()
{
    BSOCK *dir = jcr->dir_bsock;
    bool rtn;
    char lastVolume[MAX_NAME_LENGTH];

    Dmsg2(dbglvl, "dir_find_next_appendable_volume: reserved=%d Vol=%s\n", is_reserved(), VolumeName);

    /*
     * Try the twenty oldest or most available volumes.  Note,
     * the most available could already be mounted on another
     * drive, so we continue looking for a not in use Volume.
     */
    lock_volumes();
    P(vol_info_mutex);
    clear_found_in_use();
    lastVolume[0] = 0;
    for (int vol_index=1;  vol_index < 20; vol_index++) {
       bash_spaces(media_type);
       bash_spaces(pool_name);
       dir->fsend(Find_media, jcr->Job, vol_index, pool_name, media_type);
       unbash_spaces(media_type);
       unbash_spaces(pool_name);
       Dmsg1(dbglvl, ">dird %s", dir->msg);
       if (do_get_volume_info(this)) {
          /*
           * Give up if we get the same volume name twice
           */
          if (lastVolume[0] && bstrcmp(lastVolume, VolumeName)) {
             Dmsg1(dbglvl, "Got same vol = %s\n", lastVolume);
             break;
          }
          bstrncpy(lastVolume, VolumeName, sizeof(lastVolume));
          if (can_i_write_volume()) {
             Dmsg1(dbglvl, "Call reserve_volume for write. Vol=%s\n", VolumeName);
             if (reserve_volume(this, VolumeName) == NULL) {
                Dmsg2(dbglvl, "Could not reserve volume %s on %s\n", VolumeName, dev->print_name());
                continue;
             }
             Dmsg1(dbglvl, "dir_find_next_appendable_volume return true. vol=%s\n", VolumeName);
             rtn = true;
             goto get_out;
          } else {
             Dmsg1(dbglvl, "Volume %s is in use.\n", VolumeName);
             /* If volume is not usable, it is in use by someone else */
             set_found_in_use();
             continue;
          }
       }
       Dmsg2(dbglvl, "No vol. index %d return false. dev=%s\n", vol_index, dev->print_name());
       break;
    }
    rtn = false;
    VolumeName[0] = 0;

get_out:
    V(vol_info_mutex);
    unlock_volumes();
    return rtn;
}

/**
 * After writing a Volume, send the updated statistics
 * back to the director. The information comes from the
 * dev record.
 */
bool SD_DCR::dir_update_volume_info(bool label, bool update_LastWritten)
{
   BSOCK *dir = jcr->dir_bsock;
   VOLUME_CAT_INFO *vol = &dev->VolCatInfo;
   char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50];
   int InChanger;
   bool ok = false;
   POOL_MEM volume_name;

   /* If system job, do not update catalog */
   if (jcr->is_JobType(JT_SYSTEM)) {
      return true;
   }

   if (vol->VolCatName[0] == 0) {
      Jmsg0(jcr, M_FATAL, 0, _("NULL Volume name. This shouldn't happen!!!\n"));
      Pmsg0(000, _("NULL Volume name. This shouldn't happen!!!\n"));
      return false;
   }

   /* Lock during Volume update */
   P(vol_info_mutex);
   Dmsg1(dbglvl, "Update cat VolBytes=%lld\n", vol->VolCatBytes);
   /* Just labeled or relabeled the tape */
   if (label) {
      bstrncpy(vol->VolCatStatus, "Append", sizeof(vol->VolCatStatus));
   }
// if (update_LastWritten) {
      vol->VolLastWritten = time(NULL);
// }
   pm_strcpy(volume_name, vol->VolCatName);
   bash_spaces(volume_name);
   InChanger = vol->InChanger;
   dir->fsend(Update_media, jcr->Job,
              volume_name.c_str(), vol->VolCatJobs, vol->VolCatFiles,
              vol->VolCatBlocks, edit_uint64(vol->VolCatBytes, ed1),
              vol->VolCatMounts, vol->VolCatErrors,
              vol->VolCatWrites, edit_uint64(vol->VolCatMaxBytes, ed2),
              edit_uint64(vol->VolLastWritten, ed6),
              vol->VolCatStatus, vol->Slot, label,
              InChanger,                      /* bool in structure */
              edit_int64(vol->VolReadTime, ed3),
              edit_int64(vol->VolWriteTime, ed4),
              edit_uint64(vol->VolFirstWritten, ed5));
   Dmsg1(dbglvl, ">dird %s", dir->msg);

   /* Do not lock device here because it may be locked from label */
   if (!jcr->is_canceled()) {
      if (!do_get_volume_info(this)) {
         Jmsg(jcr, M_FATAL, 0, "%s", jcr->errmsg);
         Dmsg2(dbglvl, _("Didn't get vol info vol=%s: ERR=%s"),
            vol->VolCatName, jcr->errmsg);
         goto bail_out;
      }
      Dmsg1(420, "get_volume_info() %s", dir->msg);
      /* Update dev Volume info in case something changed (e.g. expired) */
      dev->VolCatInfo = VolCatInfo;
      ok = true;
   }

bail_out:
   V(vol_info_mutex);
   return ok;
}

/**
 * After writing a Volume, create the JobMedia record.
 */
bool SD_DCR::dir_create_jobmedia_record(bool zero)
{
   BSOCK *dir = jcr->dir_bsock;
   char ed1[50];

   /* If system job, do not update catalog */
   if (jcr->is_JobType(JT_SYSTEM)) {
      return true;
   }

   /* Throw out records where FI is zero -- i.e. nothing done */
   if (!zero && VolFirstIndex == 0 &&
        (StartBlock != 0 || EndBlock != 0)) {
      Dmsg0(dbglvl, "JobMedia FI=0 StartBlock!=0 record suppressed\n");
      return true;
   }

   if (!WroteVol) {
      return true;                    /* nothing written to tape */
   }

   WroteVol = false;
   if (zero) {
      /* Send dummy place holder to avoid purging */
      dir->fsend(Create_job_media, jcr->Job,
         0 , 0, 0, 0, 0, 0, 0, 0, edit_uint64(VolMediaId, ed1));
   } else {
      dir->fsend(Create_job_media, jcr->Job,
                 VolFirstIndex, VolLastIndex,
                 StartFile, EndFile,
                 StartBlock, EndBlock,
                 Copy, Stripe,
                 edit_uint64(VolMediaId, ed1));
   }
   Dmsg1(dbglvl, ">dird %s", dir->msg);
   if (dir->recv() <= 0) {
      Dmsg0(dbglvl, "create_jobmedia error bnet_recv\n");
      Jmsg(jcr, M_FATAL, 0, _("Error creating JobMedia record: ERR=%s\n"),
           dir->bstrerror());
      return false;
   }
   Dmsg1(dbglvl, "<dird %s", dir->msg);
   if (!bstrcmp(dir->msg, OK_create)) {
      Dmsg1(dbglvl, "Bad response from Dir: %s\n", dir->msg);
      Jmsg(jcr, M_FATAL, 0, _("Error creating JobMedia record: %s\n"), dir->msg);
      return false;
   }
   return true;
}

/**
 * Update File Attribute data
 * We do the following:
 *  1. expand the bsock buffer to be large enough
 *  2. Write a "header" into the buffer with serialized data
 *    VolSessionId
 *    VolSeesionTime
 *    FileIndex
 *    Stream
 *    data length that follows
 *    start of raw byte data from the Device record.
 * Note, this is primarily for Attribute data, but can
 *   also handle any device record. The Director must know
 *   the raw byte data format that is defined for each Stream.
 * Now Restore Objects pass through here STREAM_RESTORE_OBJECT
 */
bool SD_DCR::dir_update_file_attributes(DEV_RECORD *record)
{
   BSOCK *dir = jcr->dir_bsock;
   ser_declare;

#ifdef NO_ATTRIBUTES_TEST
   return true;
#endif

   dir->msg = check_pool_memory_size(dir->msg, sizeof(FileAttributes) +
                MAX_NAME_LENGTH + sizeof(DEV_RECORD) + record->data_len + 1);
   dir->msglen = bsnprintf(dir->msg, sizeof(FileAttributes) +
                MAX_NAME_LENGTH + 1, FileAttributes, jcr->Job);
   ser_begin(dir->msg + dir->msglen, 0);
   ser_uint32(record->VolSessionId);
   ser_uint32(record->VolSessionTime);
   ser_int32(record->FileIndex);
   ser_int32(record->Stream);
   ser_uint32(record->data_len);
   ser_bytes(record->data, record->data_len);
   dir->msglen = ser_length(dir->msg);
   Dmsg1(1800, ">dird %s\n", dir->msg);    /* Attributes */
   return dir->send();
}

/**
 *   Request the sysop to create an appendable volume
 *
 *   Entered with device blocked.
 *   Leaves with device blocked.
 *
 *   Returns: true  on success (operator issues a mount command)
 *            false on failure
 *              Note, must create dev->errmsg on error return.
 *
 *    On success, dcr->VolumeName and dcr->VolCatInfo contain
 *      information on suggested volume, but this may not be the
 *      same as what is actually mounted.
 *
 *    When we return with success, the correct tape may or may not
 *      actually be mounted. The calling routine must read it and
 *      verify the label.
 */
bool SD_DCR::dir_ask_sysop_to_create_appendable_volume()
{
   int status = W_TIMEOUT;
   bool got_vol = false;

   if (job_canceled(jcr)) {
      return false;
   }
   Dmsg0(dbglvl, "enter dir_ask_sysop_to_create_appendable_volume\n");
   ASSERT(dev->blocked());
   for ( ;; ) {
      if (job_canceled(jcr)) {
         Mmsg(dev->errmsg,
              _("Job %s canceled while waiting for mount on Storage Device \"%s\".\n"),
              jcr->Job, dev->print_name());
         Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
         return false;
      }
      got_vol = dir_find_next_appendable_volume();   /* get suggested volume */
      if (got_vol) {
         goto get_out;
      } else {
         if (status == W_TIMEOUT || status == W_MOUNT) {
            Mmsg(dev->errmsg, _(
"Job %s is waiting. Cannot find any appendable volumes.\n"
"Please use the \"label\" command to create a new Volume for:\n"
"    Storage:      %s\n"
"    Pool:         %s\n"
"    Media type:   %s\n"),
               jcr->Job,
               dev->print_name(),
               pool_name,
               media_type);
            Jmsg(jcr, M_MOUNT, 0, "%s", dev->errmsg);
            Dmsg1(dbglvl, "%s", dev->errmsg);
         }
      }

      jcr->sendJobStatus(JS_WaitMedia);

      status = wait_for_sysop(this);
      Dmsg1(dbglvl, "Back from wait_for_sysop status=%d\n", status);
      if (dev->poll) {
         Dmsg1(dbglvl, "Poll timeout in create append vol on device %s\n", dev->print_name());
         continue;
      }

      if (status == W_TIMEOUT) {
         if (!double_dev_wait_time(dev)) {
            Mmsg(dev->errmsg, _("Max time exceeded waiting to mount Storage Device %s for Job %s\n"),
               dev->print_name(), jcr->Job);
            Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
            Dmsg1(dbglvl, "Gave up waiting on device %s\n", dev->print_name());
            return false;             /* exceeded maximum waits */
         }
         continue;
      }
      if (status == W_ERROR) {
         berrno be;
         Mmsg0(dev->errmsg, _("pthread error in mount_next_volume.\n"));
         Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
         return false;
      }
      Dmsg1(dbglvl, "Someone woke me for device %s\n", dev->print_name());
   }

get_out:
   jcr->sendJobStatus(JS_Running);
   Dmsg0(dbglvl, "leave dir_ask_sysop_to_mount_create_appendable_volume\n");
   return true;
}

/**
 * Request to mount specific Volume
 *
 * Entered with device blocked and dcr->VolumeName is desired volume.
 * Leaves with device blocked.
 *
 * Returns: true  on success (operator issues a mount command)
 *          false on failure
 *
 * Note, must create dev->errmsg on error return.
 */
bool SD_DCR::dir_ask_sysop_to_mount_volume(int mode)
{
   int status = W_TIMEOUT;

   Dmsg0(dbglvl, "enter dir_ask_sysop_to_mount_volume\n");
   if (!VolumeName[0]) {
      Mmsg0(dev->errmsg, _("Cannot request another volume: no volume name given.\n"));
      return false;
   }
   ASSERT(dev->blocked());
   for ( ;; ) {
      if (job_canceled(jcr)) {
         Mmsg(dev->errmsg, _("Job %s canceled while waiting for mount on Storage Device %s.\n"),
              jcr->Job, dev->print_name());
         return false;
      }

      /*
       * If we are not polling, and the wait timeout or the
       *   user explicitly did a mount, send him the message.
       *   Otherwise skip it.
       */
      if (!dev->poll && (status == W_TIMEOUT || status == W_MOUNT)) {
         const char *msg;

         if (mode == ST_APPENDREADY) {
            msg = _("Please mount append Volume \"%s\" or label a new one for:\n"
                    "    Job:          %s\n"
                    "    Storage:      %s\n"
                    "    Pool:         %s\n"
                    "    Media type:   %s\n");
         } else {
            msg = _("Please mount read Volume \"%s\" for:\n"
                    "    Job:          %s\n"
                    "    Storage:      %s\n"
                    "    Pool:         %s\n"
                    "    Media type:   %s\n");
         }
         Jmsg(jcr, M_MOUNT, 0, msg, VolumeName, jcr->Job,
              dev->print_name(), pool_name, media_type);
         Dmsg3(dbglvl, "Mount \"%s\" on device \"%s\" for Job %s\n",
               VolumeName, dev->print_name(), jcr->Job);
      }

      jcr->sendJobStatus(JS_WaitMount);

      status = wait_for_sysop(this);         /* wait on device */
      Dmsg1(dbglvl, "Back from wait_for_sysop status=%d\n", status);
      if (dev->poll) {
         Dmsg1(dbglvl, "Poll timeout in mount vol on device %s\n", dev->print_name());
         Dmsg1(dbglvl, "Blocked=%s\n", dev->print_blocked());
         goto get_out;
      }

      if (status == W_TIMEOUT) {
         if (!double_dev_wait_time(dev)) {
            Mmsg(dev->errmsg, _("Max time exceeded waiting to mount Storage Device %s for Job %s\n"),
               dev->print_name(), jcr->Job);
            Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
            Dmsg1(dbglvl, "Gave up waiting on device %s\n", dev->print_name());
            return false;             /* exceeded maximum waits */
         }
         continue;
      }
      if (status == W_ERROR) {
         berrno be;
         Mmsg(dev->errmsg, _("pthread error in mount_volume\n"));
         Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
         return false;
      }
      Dmsg1(dbglvl, "Someone woke me for device %s\n", dev->print_name());
      break;
   }

get_out:
   jcr->sendJobStatus(JS_Running);
   Dmsg0(dbglvl, "leave dir_ask_sysop_to_mount_volume\n");
   return true;
}

DCR *SD_DCR::get_new_spooling_dcr()
{
   DCR *dcr;

   dcr = New(SD_DCR);

   return dcr;
}

/*
 * Dummy methods for everything but SD and BTAPE.
 */
bool DCR::dir_ask_sysop_to_mount_volume(int /*mode*/)
{
   fprintf(stderr, _("Mount Volume \"%s\" on device %s and press return when ready: "),
           VolumeName, dev->print_name());
   dev->close(this);
   getchar();
   return true;
}

bool DCR::dir_get_volume_info(enum get_vol_info_rw writing)
{
   Dmsg0(100, "Fake dir_get_volume_info\n");
   setVolCatName(VolumeName);
   Dmsg1(500, "Vol=%s\n", getVolCatName());
   return 1;
}

DCR *DCR::get_new_spooling_dcr()
{
   DCR *dcr;

   dcr = New(SD_DCR);

   return dcr;
}
