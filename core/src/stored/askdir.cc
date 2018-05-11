/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * Kern Sibbald, December 2000
 */
/**
 * @file
 * Subroutines to handle Catalog reqests sent to the Director
 * Reqests/commands from the Director are handled in dircmd.c
 */

#include "include/bareos.h"                   /* pull in global headers */
#include "stored/stored.h"                   /* pull in Storage Daemon headers */

#include "include/jcr.h"
#include "lib/crypto_cache.h"
#include "stored/wait.h"
#include "lib/edit.h"
#include "lib/util.h"

static const int debuglevel = 50;
static pthread_mutex_t vol_info_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Requests sent to the Director */
static char Find_media[] =
   "CatReq Job=%s FindMedia=%d pool_name=%s media_type=%s unwanted_volumes=%s\n";
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

/**
 * Send update information about a device to Director
 */
bool StorageDaemonDeviceControlRecord::dir_update_device(JobControlRecord *jcr, Device *dev)
{
   BareosSocket *dir = jcr->dir_bsock;
   PoolMem dev_name, VolumeName, MediaType, ChangerName;
   DeviceResource *device = dev->device;
   bool ok;

   PmStrcpy(dev_name, device->name());
   BashSpaces(dev_name);
   if (dev->IsLabeled()) {
      PmStrcpy(VolumeName, dev->VolHdr.VolumeName);
   } else {
      PmStrcpy(VolumeName, "*");
   }
   BashSpaces(VolumeName);
   PmStrcpy(MediaType, device->media_type);
   BashSpaces(MediaType);
   if (device->changer_res) {
      PmStrcpy(ChangerName, device->changer_res->name());
      BashSpaces(ChangerName);
   } else {
      PmStrcpy(ChangerName, "*");
   }
   ok = dir->fsend(Device_update,
                   jcr->Job,
                   dev_name.c_str(),
                   dev->CanAppend() != 0,
                   dev->CanRead() != 0, dev->num_writers,
                   dev->IsOpen() != 0, dev->IsLabeled() != 0,
                   dev->IsOffline() != 0, dev->reserved_device,
                   dev->IsTape() ? 100000 : 1,
                   dev->autoselect, 0,
                   ChangerName.c_str(), MediaType.c_str(), VolumeName.c_str());
   Dmsg1(debuglevel, ">dird: %s", dir->msg);

   return ok;
}

bool StorageDaemonDeviceControlRecord::dir_update_changer(JobControlRecord *jcr, AUTOCHANGER *changer)
{
   BareosSocket *dir = jcr->dir_bsock;
   PoolMem dev_name, MediaType;
   DeviceResource *device;
   bool ok;

   PmStrcpy(dev_name, changer->name());
   BashSpaces(dev_name);
   device = (DeviceResource *)changer->device->first();
   PmStrcpy(MediaType, device->media_type);
   BashSpaces(MediaType);
   /* This is mostly to indicate that we are here */
   ok = dir->fsend(Device_update,
                   jcr->Job,
                   dev_name.c_str(),         /* Changer name */
                   0, 0, 0,                  /* append, read, num_writers */
                   0, 0, 0,                  /* IsOpen, IsLabeled, offline */
                   0, 0,                     /* reserved, max_writers */
                   0,                        /* Autoselect */
                   changer->device->size(),  /* Number of devices */
                   "0",                      /* PoolId */
                   "*",                      /* ChangerName */
                   MediaType.c_str(),        /* MediaType */
                   "*");                     /* VolName */
   Dmsg1(debuglevel, ">dird: %s", dir->msg);

   return ok;
}
#endif

/**
 * Common routine for:
 *   DirGetVolumeInfo()
 * and
 *   DirFindNextAppendableVolume()
 *
 *  NOTE!!! All calls to this routine must be protected by
 *          locking vol_info_mutex before calling it so that
 *          we don't have one thread modifying the parameters
 *          and another reading them.
 *
 *  Returns: true  on success and vol info in dcr->VolCatInfo
 *           false on failure
 */
static bool DoGetVolumeInfo(DeviceControlRecord *dcr)
{
    JobControlRecord *jcr = dcr->jcr;
    BareosSocket *dir = jcr->dir_bsock;
    VolumeCatalogInfo vol;
    int n;
    int32_t InChanger;

    dcr->setVolCatInfo(false);
    if (dir->recv() <= 0) {
       Dmsg0(debuglevel, "getvolname error BnetRecv\n");
       Mmsg(jcr->errmsg, _("Network error on BnetRecv in req_vol_info.\n"));
       return false;
    }
    memset(&vol, 0, sizeof(vol));
    Dmsg1(debuglevel, "<dird %s", dir->msg);
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
       Dmsg3(debuglevel, "Bad response from Dir fields=%d, len=%d: %s",
             n, dir->message_length, dir->msg);
       Mmsg(jcr->errmsg, _("Error getting Volume info: %s"), dir->msg);
       return false;
    }
    vol.InChanger = InChanger;        /* bool in structure */
    vol.is_valid = true;
    UnbashSpaces(vol.VolCatName);
    bstrncpy(dcr->VolumeName, vol.VolCatName, sizeof(dcr->VolumeName));
    dcr->VolCatInfo = vol;            /* structure assignment */

    /*
     * If we received a new crypto key update the cache and write out the new cache on a change.
     */
    if (*vol.VolEncrKey) {
       if (UpdateCryptoCache(vol.VolCatName, vol.VolEncrKey)) {
          WriteCryptoCache(me->working_directory, "bareos-sd",
                             GetFirstPortHostOrder(me->SDaddrs));
       }
    }

    Dmsg4(debuglevel, "DoGetVolumeInfo return true slot=%d Volume=%s, "
                  "VolminBlocksize=%u VolMaxBlocksize=%u\n",
          vol.Slot, vol.VolCatName, vol.VolMinBlocksize, vol.VolMaxBlocksize);
    Dmsg2(debuglevel, "setting dcr->VolMinBlocksize(%u) to vol.VolMinBlocksize(%u)\n",
          dcr->VolMinBlocksize, vol.VolMinBlocksize);
    Dmsg2(debuglevel, "setting dcr->VolMaxBlocksize(%u) to vol.VolMaxBlocksize(%u)\n",
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
 * Volume information returned in dcr->VolCatInfo
 */
bool StorageDaemonDeviceControlRecord::DirGetVolumeInfo(enum get_vol_info_rw writing)
{
   bool ok;
   BareosSocket *dir = jcr->dir_bsock;

   P(vol_info_mutex);
   setVolCatName(VolumeName);
   BashSpaces(getVolCatName());
   dir->fsend(Get_Vol_Info, jcr->Job, getVolCatName(),
              (writing == GET_VOL_INFO_FOR_WRITE) ? 1 : 0);
   Dmsg1(debuglevel, ">dird %s", dir->msg);
   UnbashSpaces(getVolCatName());
   ok = DoGetVolumeInfo(this);
   V(vol_info_mutex);

   return ok;
}

/**
 * Get info on the next appendable volume in the Director's database
 *
 * Returns: true  on success dcr->VolumeName is volume
 *                reserve_volume() called on Volume name
 *          false on failure dcr->VolumeName[0] == 0
 *                also sets dcr->FoundInUse if at least one
 *                in use volume was found.
 *
 * Volume information returned in dcr
 */
bool StorageDaemonDeviceControlRecord::DirFindNextAppendableVolume()
{
    bool retval;
    BareosSocket *dir = jcr->dir_bsock;
    PoolMem unwanted_volumes(PM_MESSAGE);

    Dmsg2(debuglevel, "DirFindNextAppendableVolume: reserved=%d Vol=%s\n", IsReserved(), VolumeName);

    /*
     * Try the twenty oldest or most available volumes. Note,
     * the most available could already be mounted on another
     * drive, so we continue looking for a not in use Volume.
     */
    LockVolumes();
    P(vol_info_mutex);
    ClearFoundInUse();

    PmStrcpy(unwanted_volumes, "");
    for (int vol_index = 1; vol_index < 20; vol_index++) {
       BashSpaces(media_type);
       BashSpaces(pool_name);
       BashSpaces(unwanted_volumes.c_str());
       dir->fsend(Find_media, jcr->Job, vol_index, pool_name, media_type, unwanted_volumes.c_str());
       UnbashSpaces(media_type);
       UnbashSpaces(pool_name);
       UnbashSpaces(unwanted_volumes.c_str());
       Dmsg1(debuglevel, ">dird %s", dir->msg);

       if (DoGetVolumeInfo(this)) {
          if (vol_index == 1) {
             PmStrcpy(unwanted_volumes, VolumeName);
          } else {
             PmStrcat(unwanted_volumes, ",");
             PmStrcat(unwanted_volumes, VolumeName);
          }

          if (Can_i_write_volume()) {
             Dmsg1(debuglevel, "Call reserve_volume for write. Vol=%s\n", VolumeName);
             if (reserve_volume(this, VolumeName) == NULL) {
                Dmsg2(debuglevel, "Could not reserve volume %s on %s\n", VolumeName, dev->print_name());
                continue;
             }
             Dmsg1(debuglevel, "DirFindNextAppendableVolume return true. vol=%s\n", VolumeName);
             retval = true;
             goto get_out;
          } else {
             Dmsg1(debuglevel, "Volume %s is in use.\n", VolumeName);

             /*
              * If volume is not usable, it is in use by someone else
              */
             SetFoundInUse();
             continue;
          }
       }
       Dmsg2(debuglevel, "No vol. index %d return false. dev=%s\n", vol_index, dev->print_name());
       break;
    }
    retval = false;
    VolumeName[0] = 0;

get_out:
    V(vol_info_mutex);
    UnlockVolumes();

    return retval;
}

/**
 * After writing a Volume, send the updated statistics
 * back to the director. The information comes from the
 * dev record.
 */
bool StorageDaemonDeviceControlRecord::DirUpdateVolumeInfo(bool label, bool update_LastWritten)
{
   BareosSocket *dir = jcr->dir_bsock;
   VolumeCatalogInfo *vol = &dev->VolCatInfo;
   char ed1[50], ed2[50], ed3[50], ed4[50], ed5[50], ed6[50];
   int InChanger;
   bool ok = false;
   PoolMem volume_name;

   /*
    * If system job, do not update catalog
    */
   if (jcr->is_JobType(JT_SYSTEM)) {
      return true;
   }

   if (vol->VolCatName[0] == 0) {
      Jmsg0(jcr, M_FATAL, 0, _("NULL Volume name. This shouldn't happen!!!\n"));
      Pmsg0(000, _("NULL Volume name. This shouldn't happen!!!\n"));
      return false;
   }

   /*
    * Lock during Volume update
    */
   P(vol_info_mutex);
   Dmsg1(debuglevel, "Update cat VolBytes=%lld\n", vol->VolCatBytes);

   /*
    * Just labeled or relabeled the tape
    */
   if (label) {
      bstrncpy(vol->VolCatStatus, "Append", sizeof(vol->VolCatStatus));
   }
// if (update_LastWritten) {
      vol->VolLastWritten = time(NULL);
// }
   PmStrcpy(volume_name, vol->VolCatName);
   BashSpaces(volume_name);
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
   Dmsg1(debuglevel, ">dird %s", dir->msg);

   /*
    * Do not lock device here because it may be locked from label
    */
   if (!jcr->IsCanceled()) {
      if (!DoGetVolumeInfo(this)) {
         Jmsg(jcr, M_FATAL, 0, "%s", jcr->errmsg);
         Dmsg2(debuglevel, _("Didn't get vol info vol=%s: ERR=%s"),
            vol->VolCatName, jcr->errmsg);
         goto bail_out;
      }
      Dmsg1(420, "get_volume_info() %s", dir->msg);

      /*
       * Update dev Volume info in case something changed (e.g. expired)
       */
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
bool StorageDaemonDeviceControlRecord::DirCreateJobmediaRecord(bool zero)
{
   BareosSocket *dir = jcr->dir_bsock;
   char ed1[50];

   /*
    * If system job, do not update catalog
    */
   if (jcr->is_JobType(JT_SYSTEM)) {
      return true;
   }

   /*
    * Throw out records where FI is zero -- i.e. nothing done
    */
   if (!zero && VolFirstIndex == 0 &&
        (StartBlock != 0 || EndBlock != 0)) {
      Dmsg0(debuglevel, "JobMedia FI=0 StartBlock!=0 record suppressed\n");
      return true;
   }

   if (!WroteVol) {
      return true;                    /* nothing written to tape */
   }

   WroteVol = false;
   if (zero) {
      /*
       * Send dummy place holder to avoid purging
       */
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
   Dmsg1(debuglevel, ">dird %s", dir->msg);

   if (dir->recv() <= 0) {
      Dmsg0(debuglevel, "create_jobmedia error BnetRecv\n");
      Jmsg(jcr, M_FATAL, 0, _("Error creating JobMedia record: ERR=%s\n"),
           dir->bstrerror());
      return false;
   }
   Dmsg1(debuglevel, "<dird %s", dir->msg);

   if (!bstrcmp(dir->msg, OK_create)) {
      Dmsg1(debuglevel, "Bad response from Dir: %s\n", dir->msg);
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
 *     VolSessionId
 *     VolSeesionTime
 *     FileIndex
 *     Stream
 *     data length that follows
 *     start of raw byte data from the Device record.
 *
 * Note, this is primarily for Attribute data, but can
 * also handle any device record. The Director must know
 * the raw byte data format that is defined for each Stream.
 *
 * Now Restore Objects pass through here STREAM_RESTORE_OBJECT
 */
bool StorageDaemonDeviceControlRecord::DirUpdateFileAttributes(DeviceRecord *record)
{
   BareosSocket *dir = jcr->dir_bsock;
   ser_declare;

#ifdef NO_ATTRIBUTES_TEST
   return true;
#endif

   dir->msg = CheckPoolMemorySize(dir->msg, sizeof(FileAttributes) +
                MAX_NAME_LENGTH + sizeof(DeviceRecord) + record->data_len + 1);
   dir->message_length = Bsnprintf(dir->msg, sizeof(FileAttributes) +
                MAX_NAME_LENGTH + 1, FileAttributes, jcr->Job);
   SerBegin(dir->msg + dir->message_length, 0);
   ser_uint32(record->VolSessionId);
   ser_uint32(record->VolSessionTime);
   ser_int32(record->FileIndex);
   ser_int32(record->Stream);
   ser_uint32(record->data_len);
   SerBytes(record->data, record->data_len);
   dir->message_length = SerLength(dir->msg);
   Dmsg1(1800, ">dird %s", dir->msg);    /* Attributes */

   return dir->send();
}

/**
 * Request the sysop to create an appendable volume
 *
 * Entered with device blocked.
 * Leaves with device blocked.
 *
 * Returns: true  on success (operator issues a mount command)
 *          false on failure
 *
 * Note, must create dev->errmsg on error return.
 *
 * On success, dcr->VolumeName and dcr->VolCatInfo contain
 * information on suggested volume, but this may not be the
 * same as what is actually mounted.
 *
 * When we return with success, the correct tape may or may not
 * actually be mounted. The calling routine must read it and
 * verify the label.
 */
bool StorageDaemonDeviceControlRecord::DirAskSysopToCreateAppendableVolume()
{
   int status = W_TIMEOUT;
   bool got_vol = false;

   if (JobCanceled(jcr)) {
      return false;
   }

   Dmsg0(debuglevel, "enter DirAskSysopToCreateAppendableVolume\n");
   ASSERT(dev->blocked());
   for ( ;; ) {
      if (JobCanceled(jcr)) {
         Mmsg(dev->errmsg,
              _("Job %s canceled while waiting for mount on Storage Device \"%s\".\n"),
              jcr->Job, dev->print_name());
         Jmsg(jcr, M_INFO, 0, "%s", dev->errmsg);
         return false;
      }
      got_vol = DirFindNextAppendableVolume();   /* get suggested volume */
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
            Dmsg1(debuglevel, "%s", dev->errmsg);
         }
      }

      jcr->sendJobStatus(JS_WaitMedia);

      status = WaitForSysop(this);
      Dmsg1(debuglevel, "Back from WaitForSysop status=%d\n", status);
      if (dev->poll) {
         Dmsg1(debuglevel, "Poll timeout in create append vol on device %s\n", dev->print_name());
         continue;
      }

      if (status == W_TIMEOUT) {
         if (!DoubleDevWaitTime(dev)) {
            Mmsg(dev->errmsg, _("Max time exceeded waiting to mount Storage Device %s for Job %s\n"),
               dev->print_name(), jcr->Job);
            Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
            Dmsg1(debuglevel, "Gave up waiting on device %s\n", dev->print_name());
            return false;             /* exceeded maximum waits */
         }
         continue;
      }

      if (status == W_ERROR) {
         BErrNo be;
         Mmsg0(dev->errmsg, _("pthread error in mount_next_volume.\n"));
         Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
         return false;
      }
      Dmsg1(debuglevel, "Someone woke me for device %s\n", dev->print_name());
   }

get_out:
   jcr->sendJobStatus(JS_Running);
   Dmsg0(debuglevel, "leave dir_ask_sysop_to_mount_create_appendable_volume\n");

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
bool StorageDaemonDeviceControlRecord::DirAskSysopToMountVolume(int mode)
{
   int status = W_TIMEOUT;

   Dmsg0(debuglevel, "enter DirAskSysopToMountVolume\n");
   if (!VolumeName[0]) {
      Mmsg0(dev->errmsg, _("Cannot request another volume: no volume name given.\n"));
      return false;
   }
   ASSERT(dev->blocked());
   while (1) {
      if (JobCanceled(jcr)) {
         Mmsg(dev->errmsg, _("Job %s canceled while waiting for mount on Storage Device %s.\n"),
              jcr->Job, dev->print_name());
         return false;
      }

      /*
       * If we are not polling, and the wait timeout or the user explicitly did a mount,
       * send him the message. Otherwise skip it.
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
         Dmsg3(debuglevel, "Mount \"%s\" on device \"%s\" for Job %s\n",
               VolumeName, dev->print_name(), jcr->Job);
      }

      jcr->sendJobStatus(JS_WaitMount);

      status = WaitForSysop(this);         /* wait on device */
      Dmsg1(debuglevel, "Back from WaitForSysop status=%d\n", status);
      if (dev->poll) {
         Dmsg1(debuglevel, "Poll timeout in mount vol on device %s\n", dev->print_name());
         Dmsg1(debuglevel, "Blocked=%s\n", dev->print_blocked());
         goto get_out;
      }

      if (status == W_TIMEOUT) {
         if (!DoubleDevWaitTime(dev)) {
            Mmsg(dev->errmsg, _("Max time exceeded waiting to mount Storage Device %s for Job %s\n"),
               dev->print_name(), jcr->Job);
            Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
            Dmsg1(debuglevel, "Gave up waiting on device %s\n", dev->print_name());
            return false;             /* exceeded maximum waits */
         }
         continue;
      }

      if (status == W_ERROR) {
         BErrNo be;
         Mmsg(dev->errmsg, _("pthread error in mount_volume\n"));
         Jmsg(jcr, M_FATAL, 0, "%s", dev->errmsg);
         return false;
      }
      Dmsg1(debuglevel, "Someone woke me for device %s\n", dev->print_name());
      break;
   }

get_out:
   jcr->sendJobStatus(JS_Running);
   Dmsg0(debuglevel, "leave DirAskSysopToMountVolume\n");
   return true;
}

DeviceControlRecord *StorageDaemonDeviceControlRecord::get_new_spooling_dcr()
{
   DeviceControlRecord *dcr;

   dcr = New(StorageDaemonDeviceControlRecord);

   return dcr;
}

/**
 * Dummy methods for everything but SD and BTAPE.
 */
bool DeviceControlRecord::DirAskSysopToMountVolume(int /*mode*/)
{
   fprintf(stderr, _("Mount Volume \"%s\" on device %s and press return when ready: "),
           VolumeName, dev->print_name());
   dev->close(this);
   getchar();
   return true;
}

bool DeviceControlRecord::DirGetVolumeInfo(enum get_vol_info_rw writing)
{
   Dmsg0(100, "Fake DirGetVolumeInfo\n");
   setVolCatName(VolumeName);
   Dmsg1(500, "Vol=%s\n", getVolCatName());
   return 1;
}

DeviceControlRecord *DeviceControlRecord::get_new_spooling_dcr()
{
   DeviceControlRecord *dcr;

   dcr = New(StorageDaemonDeviceControlRecord);

   return dcr;
}
