/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March MMI
 */
/**
 * @file
 * handles finding the next volume for append.
 *
 * Split out of catreq.c August MMIII catalog request from the Storage daemon.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/autoprune.h"
#include "dird/autorecycle.h"
#include "dird/next_vol.h"
#include "dird/newvol.h"
#include "dird/ua_db.h"
#include "dird/ua_server.h"
#include "dird/ua_prune.h"
#include "dird/ua_purge.h"
#include "lib/edit.h"

namespace directordaemon {

static int const debuglevel = 50; /* debug level */

/**
 * Set storage id if possible
 */
void SetStorageidInMr(StorageResource* store, MediaDbRecord* mr)
{
  if (store != NULL) { mr->StorageId = store->StorageId; }
}

/**
 *  Items needed:
 *
 *  mr.PoolId must be set
 *  mr.ScratchPoolId could be set (used if create==true)
 *  jcr->write_storage
 *  jcr->db
 *  jcr->pool
 *  MediaDbRecord mr with PoolId set
 *  unwanted_volumes -- list of volumes we don't want
 *  create -- whether or not to create a new volume
 *  prune -- whether or not to prune volumes
 */
int FindNextVolumeForAppend(JobControlRecord* jcr,
                            MediaDbRecord* mr,
                            int index,
                            const char* unwanted_volumes,
                            bool create,
                            bool prune)
{
  int retry = 0;
  bool ok;
  bool InChanger;
  StorageResource* store = jcr->res.write_storage;

  bstrncpy(mr->MediaType, store->media_type, sizeof(mr->MediaType));
  Dmsg3(debuglevel,
        "find_next_vol_for_append: JobId=%u PoolId=%d, MediaType=%s\n",
        (uint32_t)jcr->JobId, (int)mr->PoolId, mr->MediaType);

  /*
   * If we are using an Autochanger, restrict Volume search to the Autochanger
   * on the first pass
   */
  InChanger = store->autochanger;

  /*
   * Find the Next Volume for Append
   */
  DbLock(jcr->db);
  while (1) {
    /*
     *  1. Look for volume with "Append" status.
     */
    SetStorageidInMr(store, mr);

    bstrncpy(mr->VolStatus, "Append", sizeof(mr->VolStatus));
    ok = jcr->db->FindNextVolume(jcr, index, InChanger, mr, unwanted_volumes);
    if (!ok) {
      /*
       * No volume found, apply algorithm
       */
      Dmsg4(debuglevel,
            "after find_next_vol ok=%d index=%d InChanger=%d Vstat=%s\n", ok,
            index, InChanger, mr->VolStatus);

      /*
       * 2. Try finding a recycled volume
       */
      ok = FindRecycledVolume(jcr, InChanger, mr, store, unwanted_volumes);
      SetStorageidInMr(store, mr);
      Dmsg2(debuglevel, "FindRecycledVolume ok=%d FW=%d\n", ok,
            mr->FirstWritten);
      if (!ok) {
        /*
         * 3. Try recycling any purged volume
         */
        ok = RecycleOldestPurgedVolume(jcr, InChanger, mr, store,
                                       unwanted_volumes);
        SetStorageidInMr(store, mr);
        if (!ok) {
          /*
           * 4. Try pruning Volumes
           */
          if (prune) {
            Dmsg0(debuglevel, "Call PruneVolumes\n");
            PruneVolumes(jcr, InChanger, mr, store);
          }
          ok = RecycleOldestPurgedVolume(jcr, InChanger, mr, store,
                                         unwanted_volumes);
          SetStorageidInMr(store, mr); /* put StorageId in new record */
          if (!ok && create) {
            Dmsg4(debuglevel,
                  "after prune volumes_vol ok=%d index=%d InChanger=%d "
                  "Vstat=%s\n",
                  ok, index, InChanger, mr->VolStatus);
            /*
             * 5. Try pulling a volume from the Scratch pool
             */
            ok = GetScratchVolume(jcr, InChanger, mr, store);
            SetStorageidInMr(store, mr); /* put StorageId in new record */
            Dmsg4(debuglevel,
                  "after get scratch volume ok=%d index=%d InChanger=%d "
                  "Vstat=%s\n",
                  ok, index, InChanger, mr->VolStatus);
          }
          /*
           * If we are using an Autochanger and have not found
           * a volume, retry looking for any volume.
           */
          if (!ok && InChanger) {
            InChanger = false;
            continue; /* retry again accepting any volume */
          }
        }
      }

      if (!ok && create) {
        /*
         * 6. Try "creating" a new Volume
         */
        ok = newVolume(jcr, mr, store);
      }

      /*
       *  Look at more drastic ways to find an Appendable Volume
       */
      if (!ok && (jcr->res.pool->purge_oldest_volume ||
                  jcr->res.pool->recycle_oldest_volume)) {
        Dmsg2(debuglevel,
              "No next volume found. PurgeOldest=%d\n RecyleOldest=%d",
              jcr->res.pool->purge_oldest_volume,
              jcr->res.pool->recycle_oldest_volume);

        /*
         * Find oldest volume to recycle
         */
        SetStorageidInMr(store, mr);
        ok = jcr->db->FindNextVolume(jcr, -1, InChanger, mr, unwanted_volumes);
        SetStorageidInMr(store, mr);
        Dmsg1(debuglevel, "Find oldest=%d Volume\n", ok);
        if (ok && prune) {
          UaContext* ua;
          Dmsg0(debuglevel, "Try purge Volume.\n");
          /*
           * 7.  Try to purging oldest volume only if not UA calling us.
           */
          ua = new_ua_context(jcr);
          if (jcr->res.pool->purge_oldest_volume && create) {
            Jmsg(jcr, M_INFO, 0, _("Purging oldest volume \"%s\"\n"),
                 mr->VolumeName);
            ok = PurgeJobsFromVolume(ua, mr);
          } else if (jcr->res.pool->recycle_oldest_volume) {
            /*
             * 8. Try recycling the oldest volume
             */
            Jmsg(jcr, M_INFO, 0, _("Pruning oldest volume \"%s\"\n"),
                 mr->VolumeName);
            ok = PruneVolume(ua, mr);
          }
          FreeUaContext(ua);

          if (ok) {
            ok = RecycleVolume(jcr, mr);
            Dmsg1(debuglevel, "Recycle after purge oldest=%d\n", ok);
          }
        }
      }
    }

    Dmsg2(debuglevel, "VolJobs=%d FirstWritten=%d\n", mr->VolJobs,
          mr->FirstWritten);
    if (ok) {
      /*
       * If we can use the volume, check if it is expired
       */
      if (bstrcmp(mr->VolStatus, "Append") && HasVolumeExpired(jcr, mr)) {
        if (retry++ < 200) { /* sanity check */
          continue;          /* try again from the top */
        } else {
          Jmsg(jcr, M_ERROR, 0,
               _("We seem to be looping trying to find the next volume. I give "
                 "up.\n"));
        }
      }
    }

    break;
  }

  DbUnlock(jcr->db);
  Dmsg1(debuglevel, "return ok=%d find_next_vol\n", ok);

  return ok;
}

/**
 * Check if any time limits or use limits have expired if so,
 * set the VolStatus appropriately.
 */
bool HasVolumeExpired(JobControlRecord* jcr, MediaDbRecord* mr)
{
  bool expired = false;
  char ed1[50];

  /*
   * Check limits and expirations if "Append" and it has been used i.e.
   * mr->VolJobs > 0
   */
  if (bstrcmp(mr->VolStatus, "Append") && mr->VolJobs > 0) {
    /*
     * First handle Max Volume Bytes
     */
    if ((mr->MaxVolBytes > 0 && mr->VolBytes >= mr->MaxVolBytes)) {
      Jmsg(jcr, M_INFO, 0,
           _("Max Volume bytes=%s exceeded. Marking Volume \"%s\" as Full.\n"),
           edit_uint64_with_commas(mr->MaxVolBytes, ed1), mr->VolumeName);
      bstrncpy(mr->VolStatus, "Full", sizeof(mr->VolStatus));
      expired = true;
    } else if (mr->VolBytes > 0 && jcr->res.pool->use_volume_once) {
      /*
       * Volume should only be used once
       */
      Jmsg(jcr, M_INFO, 0,
           _("Volume used once. Marking Volume \"%s\" as Used.\n"),
           mr->VolumeName);
      bstrncpy(mr->VolStatus, "Used", sizeof(mr->VolStatus));
      expired = true;
    } else if (mr->MaxVolJobs > 0 && mr->MaxVolJobs <= mr->VolJobs) {
      /*
       * Max Jobs written to volume
       */
      Jmsg(jcr, M_INFO, 0,
           _("Max Volume jobs=%s exceeded. Marking Volume \"%s\" as Used.\n"),
           edit_uint64_with_commas(mr->MaxVolJobs, ed1), mr->VolumeName);
      Dmsg3(debuglevel, "MaxVolJobs=%d JobId=%d Vol=%s\n", mr->MaxVolJobs,
            (uint32_t)jcr->JobId, mr->VolumeName);
      bstrncpy(mr->VolStatus, "Used", sizeof(mr->VolStatus));
      expired = true;
    } else if (mr->MaxVolFiles > 0 && mr->MaxVolFiles <= mr->VolFiles) {
      /*
       * Max Files written to volume
       */
      Jmsg(jcr, M_INFO, 0,
           _("Max Volume files=%s exceeded. Marking Volume \"%s\" as Used.\n"),
           edit_uint64_with_commas(mr->MaxVolFiles, ed1), mr->VolumeName);
      bstrncpy(mr->VolStatus, "Used", sizeof(mr->VolStatus));
      expired = true;
    } else if (mr->VolUseDuration > 0) {
      /*
       * Use duration expiration
       */
      utime_t now = time(NULL);
      if (mr->VolUseDuration <= (now - mr->FirstWritten)) {
        Jmsg(jcr, M_INFO, 0,
             _("Max configured use duration=%s sec. exceeded. Marking Volume "
               "\"%s\" as Used.\n"),
             edit_uint64_with_commas(mr->VolUseDuration, ed1), mr->VolumeName);
        bstrncpy(mr->VolStatus, "Used", sizeof(mr->VolStatus));
        expired = true;
      }
    }
  }

  if (expired) {
    /*
     * Need to update media
     */
    Dmsg1(debuglevel, "Vol=%s has expired update media record\n",
          mr->VolumeName);
    SetStorageidInMr(NULL, mr);
    if (!jcr->db->UpdateMediaRecord(jcr, mr)) {
      Jmsg(jcr, M_ERROR, 0, _("Catalog error updating volume \"%s\". ERR=%s"),
           mr->VolumeName, jcr->db->strerror());
    }
  }
  Dmsg2(debuglevel, "Vol=%s expired=%d\n", mr->VolumeName, expired);

  return expired;
}

/**
 * Try hard to recycle the current volume
 *
 * Returns: on failure - reason = NULL
 *          on success - reason - pointer to reason
 */
void CheckIfVolumeValidOrRecyclable(JobControlRecord* jcr,
                                    MediaDbRecord* mr,
                                    const char** reason)
{
  int ok;

  *reason = NULL;

  /*
   * Check if a duration or limit has expired
   */
  if (bstrcmp(mr->VolStatus, "Append") && HasVolumeExpired(jcr, mr)) {
    *reason = _("volume has expired");
    /*
     * Keep going because we may be able to recycle volume
     */
  }

  /*
   * Now see if we can use the volume as is
   */
  if (bstrcmp(mr->VolStatus, "Append") || bstrcmp(mr->VolStatus, "Recycle")) {
    *reason = NULL;
    return;
  }

  /*
   * Check if the Volume is already marked for recycling
   */
  if (bstrcmp(mr->VolStatus, "Purged")) {
    if (RecycleVolume(jcr, mr)) {
      Jmsg(jcr, M_INFO, 0, _("Recycled current volume \"%s\"\n"),
           mr->VolumeName);
      *reason = NULL;
      return;
    } else {
      /*
       * In principle this shouldn't happen
       */
      *reason = _("and recycling of current volume failed");
      return;
    }
  }

  /*
   * At this point, the volume is not valid for writing
   */
  *reason = _("but should be Append, Purged or Recycle");

  /*
   * What we're trying to do here is see if the current volume is
   * "recyclable" - ie. if we prune all expired jobs off it, is
   * it now possible to reuse it for the job that it is currently
   * needed for?
   */
  if (!mr->Recycle) {
    *reason = _("volume has recycling disabled");
    return;
  }

  /*
   * Check retention period from last written, but recycle to within a minute to
   * try to catch close calls ...
   */
  if ((mr->LastWritten + mr->VolRetention - 60) < (utime_t)time(NULL) &&
      jcr->res.pool->recycle_current_volume &&
      (bstrcmp(mr->VolStatus, "Full") || bstrcmp(mr->VolStatus, "Used"))) {
    /*
     * Attempt prune of current volume to see if we can recycle it for use.
     */
    UaContext* ua;

    ua = new_ua_context(jcr);
    ok = PruneVolume(ua, mr);
    FreeUaContext(ua);

    if (ok) {
      /*
       * If fully purged, recycle current volume
       */
      if (RecycleVolume(jcr, mr)) {
        Jmsg(jcr, M_INFO, 0, _("Recycled current volume \"%s\"\n"),
             mr->VolumeName);
        *reason = NULL;
      } else {
        *reason =
            _("but should be Append, Purged or Recycle (recycling of the "
              "current volume failed)");
      }
    } else {
      *reason =
          _("but should be Append, Purged or Recycle (cannot automatically "
            "recycle current volume, as it still contains unpruned data "
            "or the Volume Retention time has not expired.)");
    }
  }
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

bool GetScratchVolume(JobControlRecord* jcr,
                      bool InChanger,
                      MediaDbRecord* mr,
                      StorageResource* store)
{
  MediaDbRecord smr; /* for searching scratch pool */
  PoolDbRecord spr;
  bool ok = false;
  bool found = false;

  /*
   * Only one thread at a time can pull from the scratch pool
   */
  P(mutex);

  /*
   * Get Pool record for Scratch Pool
   * choose between ScratchPoolId and Scratch
   * GetPoolRecord will first try ScratchPoolId,
   * and then try the pool named Scratch
   */
  bstrncpy(spr.Name, "Scratch", sizeof(spr.Name));
  spr.PoolId = mr->ScratchPoolId;
  if (jcr->db->GetPoolRecord(jcr, &spr)) {
    smr.PoolId = spr.PoolId;
    if (InChanger) {
      smr.StorageId = mr->StorageId; /* want only Scratch Volumes in changer */
    }

    bstrncpy(smr.VolStatus, "Append",
             sizeof(smr.VolStatus)); /* want only appendable volumes */
    bstrncpy(smr.MediaType, mr->MediaType, sizeof(smr.MediaType));

    /*
     * If we do not find a valid Scratch volume, try recycling any existing
     * purged volumes, then try to take the oldest volume.
     */
    SetStorageidInMr(store, &smr); /* put StorageId in new record */
    if (jcr->db->FindNextVolume(jcr, 1, InChanger, &smr, NULL)) {
      found = true;
    } else if (FindRecycledVolume(jcr, InChanger, &smr, store, NULL)) {
      found = true;
    } else if (RecycleOldestPurgedVolume(jcr, InChanger, &smr, store, NULL)) {
      found = true;
    }

    if (found) {
      PoolMem query(PM_MESSAGE);

      /*
       * Get pool record where the Scratch Volume will go to ensure that we can
       * add a Volume.
       */
      PoolDbRecord pr;
      bstrncpy(pr.Name, jcr->res.pool->resource_name_, sizeof(pr.Name));

      if (!jcr->db->GetPoolRecord(jcr, &pr)) {
        Jmsg(jcr, M_WARNING, 0, _("Unable to get Pool record: ERR=%s"),
             jcr->db->strerror());
        goto bail_out;
      }

      /*
       * Make sure there is room for another volume
       */
      if (pr.MaxVols > 0 && pr.NumVols >= pr.MaxVols) {
        Jmsg(jcr, M_WARNING, 0,
             _("Unable add Scratch Volume, Pool \"%s\" full MaxVols=%d\n"),
             jcr->res.pool->resource_name_, pr.MaxVols);
        goto bail_out;
      }

      memcpy(mr, &smr, sizeof(MediaDbRecord));
      SetStorageidInMr(store, mr);

      /*
       * Set default parameters from current pool
       */
      SetPoolDbrDefaultsInMediaDbr(mr, &pr);

      /*
       * SetPoolDbrDefaultsInMediaDbr set VolStatus to Append, we could have
       * Recycled media, also, we retain the old RecyclePoolId.
       */
      bstrncpy(mr->VolStatus, smr.VolStatus, sizeof(smr.VolStatus));
      mr->RecyclePoolId = smr.RecyclePoolId;

      if (!jcr->db->UpdateMediaRecord(jcr, mr)) {
        Jmsg(jcr, M_WARNING, 0, _("Failed to move Scratch Volume. ERR=%s\n"),
             jcr->db->strerror());
        goto bail_out;
      }

      Jmsg(jcr, M_INFO, 0, _("Using Volume \"%s\" from 'Scratch' pool.\n"),
           mr->VolumeName);

      ok = true;
    }
  }

bail_out:
  V(mutex);
  return ok;
}
} /* namespace directordaemon */
