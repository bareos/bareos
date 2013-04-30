/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.

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
 *   Bacula Director -- next_vol -- handles finding the next
 *    volume for append.  Split out of catreq.c August MMIII
 *    catalog request from the Storage daemon.

 *     Kern Sibbald, March MMI
 *
 */

#include "bacula.h"
#include "dird.h"

static int const dbglvl = 50;   /* debug level */

/* Set storage id if possible */
void set_storageid_in_mr(STORE *store, MEDIA_DBR *mr)
{
   if (store != NULL) {
      mr->StorageId = store->StorageId;
   }
}

/*
 *  Items needed:
 *   mr.PoolId must be set
 *   mr.ScratchPoolId could be set (used if create==true)
 *   jcr->wstore
 *   jcr->db
 *   jcr->pool
 *   MEDIA_DBR mr with PoolId set
 *   create -- whether or not to create a new volume
 */
int find_next_volume_for_append(JCR *jcr, MEDIA_DBR *mr, int index,             
                                bool create, bool prune)
{
   int retry = 0;
   bool ok;
   bool InChanger;
   STORE *store = jcr->wstore;

   bstrncpy(mr->MediaType, store->media_type, sizeof(mr->MediaType));
   Dmsg3(dbglvl, "find_next_vol_for_append: JobId=%u PoolId=%d, MediaType=%s\n", 
         (uint32_t)jcr->JobId, (int)mr->PoolId, mr->MediaType);
   /*
    * If we are using an Autochanger, restrict Volume
    *   search to the Autochanger on the first pass
    */
   InChanger = store->autochanger;
   /*
    * Find the Next Volume for Append
    */
   db_lock(jcr->db);
   for ( ;; ) {
      bstrncpy(mr->VolStatus, "Append", sizeof(mr->VolStatus));  /* want only appendable volumes */
      /*
       *  1. Look for volume with "Append" status.
       */
      set_storageid_in_mr(store, mr);  /* put StorageId in new record */
      ok = db_find_next_volume(jcr, jcr->db, index, InChanger, mr);

      if (!ok) {
         /*
          * No volume found, apply algorithm
          */
         Dmsg4(dbglvl, "after find_next_vol ok=%d index=%d InChanger=%d Vstat=%s\n",
               ok, index, InChanger, mr->VolStatus);
         /*
          * 2. Try finding a recycled volume
          */
         ok = find_recycled_volume(jcr, InChanger, mr, store);
         set_storageid_in_mr(store, mr);  /* put StorageId in new record */
         Dmsg2(dbglvl, "find_recycled_volume ok=%d FW=%d\n", ok, mr->FirstWritten);
         if (!ok) {
            /*
             * 3. Try recycling any purged volume
             */
            ok = recycle_oldest_purged_volume(jcr, InChanger, mr, store);
            set_storageid_in_mr(store, mr);  /* put StorageId in new record */
            if (!ok) {
               /*
                * 4. Try pruning Volumes
                */
               if (prune) {
                  Dmsg0(dbglvl, "Call prune_volumes\n");
                  prune_volumes(jcr, InChanger, mr, store);
               }
               ok = recycle_oldest_purged_volume(jcr, InChanger, mr, store);
               set_storageid_in_mr(store, mr);  /* put StorageId in new record */
               if (!ok && create) {
                  Dmsg4(dbglvl, "after prune volumes_vol ok=%d index=%d InChanger=%d Vstat=%s\n",
                        ok, index, InChanger, mr->VolStatus);
                  /*
                   * 5. Try pulling a volume from the Scratch pool
                   */ 
                  ok = get_scratch_volume(jcr, InChanger, mr, store);
                  set_storageid_in_mr(store, mr);  /* put StorageId in new record */
                  Dmsg4(dbglvl, "after get scratch volume ok=%d index=%d InChanger=%d Vstat=%s\n",
                        ok, index, InChanger, mr->VolStatus);
               }
               /*
                * If we are using an Autochanger and have not found
                * a volume, retry looking for any volume. 
                */
               if (!ok && InChanger) {
                  InChanger = false;
                  continue;           /* retry again accepting any volume */
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
         if (!ok && (jcr->pool->purge_oldest_volume ||
                     jcr->pool->recycle_oldest_volume)) {
            Dmsg2(dbglvl, "No next volume found. PurgeOldest=%d\n RecyleOldest=%d",
                jcr->pool->purge_oldest_volume, jcr->pool->recycle_oldest_volume);
            /* Find oldest volume to recycle */
            set_storageid_in_mr(store, mr);   /* update storage id */
            ok = db_find_next_volume(jcr, jcr->db, -1, InChanger, mr);
            set_storageid_in_mr(store, mr);  /* update storageid */
            Dmsg1(dbglvl, "Find oldest=%d Volume\n", ok);
            if (ok && prune) {
               UAContext *ua;
               Dmsg0(dbglvl, "Try purge Volume.\n");
               /*
                * 7.  Try to purging oldest volume only if not UA calling us.
                */
               ua = new_ua_context(jcr);
               if (jcr->pool->purge_oldest_volume && create) {
                  Jmsg(jcr, M_INFO, 0, _("Purging oldest volume \"%s\"\n"), mr->VolumeName);
                  ok = purge_jobs_from_volume(ua, mr);
               /*
                * 8. or try recycling the oldest volume
                */
               } else if (jcr->pool->recycle_oldest_volume) {
                  Jmsg(jcr, M_INFO, 0, _("Pruning oldest volume \"%s\"\n"), mr->VolumeName);
                  ok = prune_volume(ua, mr);
               }
               free_ua_context(ua);
               if (ok) {
                  ok = recycle_volume(jcr, mr);
                  Dmsg1(dbglvl, "Recycle after purge oldest=%d\n", ok);
               }
            }
         }
      }
      Dmsg2(dbglvl, "VolJobs=%d FirstWritten=%d\n", mr->VolJobs, mr->FirstWritten);
      if (ok) {
         /* If we can use the volume, check if it is expired */
         if (has_volume_expired(jcr, mr)) {
            if (retry++ < 200) {            /* sanity check */
               continue;                    /* try again from the top */
            } else {
               Jmsg(jcr, M_ERROR, 0, _(
"We seem to be looping trying to find the next volume. I give up.\n"));
            }
         }
      }
      break;
   } /* end for loop */
   db_unlock(jcr->db);
   Dmsg1(dbglvl, "return ok=%d find_next_vol\n", ok);
   return ok;
}

/*
 * Check if any time limits or use limits have expired
 *   if so, set the VolStatus appropriately.
 */
bool has_volume_expired(JCR *jcr, MEDIA_DBR *mr)
{
   bool expired = false;
   char ed1[50];
   /*
    * Check limits and expirations if "Append" and it has been used
    * i.e. mr->VolJobs > 0
    *
    */
   if (strcmp(mr->VolStatus, "Append") == 0 && mr->VolJobs > 0) {
      /* First handle Max Volume Bytes */
      if ((mr->MaxVolBytes > 0 && mr->VolBytes >= mr->MaxVolBytes)) {
         Jmsg(jcr, M_INFO, 0, _("Max Volume bytes=%s exceeded. "
             "Marking Volume \"%s\" as Full.\n"), 
             edit_uint64_with_commas(mr->MaxVolBytes, ed1), mr->VolumeName);
         bstrncpy(mr->VolStatus, "Full", sizeof(mr->VolStatus));
         expired = true;

      /* Now see if Volume should only be used once */
      } else if (mr->VolBytes > 0 && jcr->pool->use_volume_once) {
         Jmsg(jcr, M_INFO, 0, _("Volume used once. "
             "Marking Volume \"%s\" as Used.\n"), mr->VolumeName);
         bstrncpy(mr->VolStatus, "Used", sizeof(mr->VolStatus));
         expired = true;

      /* Now see if Max Jobs written to volume */
      } else if (mr->MaxVolJobs > 0 && mr->MaxVolJobs <= mr->VolJobs) {
         Jmsg(jcr, M_INFO, 0, _("Max Volume jobs=%s exceeded. "
             "Marking Volume \"%s\" as Used.\n"), 
             edit_uint64_with_commas(mr->MaxVolJobs, ed1), mr->VolumeName);
         Dmsg3(dbglvl, "MaxVolJobs=%d JobId=%d Vol=%s\n", mr->MaxVolJobs,
               (uint32_t)jcr->JobId, mr->VolumeName);
         bstrncpy(mr->VolStatus, "Used", sizeof(mr->VolStatus));
         expired = true;

      /* Now see if Max Files written to volume */
      } else if (mr->MaxVolFiles > 0 && mr->MaxVolFiles <= mr->VolFiles) {
         Jmsg(jcr, M_INFO, 0, _("Max Volume files=%s exceeded. "
             "Marking Volume \"%s\" as Used.\n"), 
             edit_uint64_with_commas(mr->MaxVolFiles, ed1), mr->VolumeName);
         bstrncpy(mr->VolStatus, "Used", sizeof(mr->VolStatus));
         expired = true;

      /* Finally, check Use duration expiration */
      } else if (mr->VolUseDuration > 0) {
         utime_t now = time(NULL);
         /* See if Vol Use has expired */
         if (mr->VolUseDuration <= (now - mr->FirstWritten)) {
            Jmsg(jcr, M_INFO, 0, _("Max configured use duration=%s sec. exceeded. "
               "Marking Volume \"%s\" as Used.\n"), 
               edit_uint64_with_commas(mr->VolUseDuration, ed1), mr->VolumeName);
            bstrncpy(mr->VolStatus, "Used", sizeof(mr->VolStatus));
            expired = true;
         }
      }
   }
   if (expired) {
      /* Need to update media */
      Dmsg1(dbglvl, "Vol=%s has expired update media record\n", mr->VolumeName);
      set_storageid_in_mr(NULL, mr);
      if (!db_update_media_record(jcr, jcr->db, mr)) {
         Jmsg(jcr, M_ERROR, 0, _("Catalog error updating volume \"%s\". ERR=%s"),
              mr->VolumeName, db_strerror(jcr->db));
      }
   }
   Dmsg2(dbglvl, "Vol=%s expired=%d\n", mr->VolumeName, expired);
   return expired;
}

/*
 * Try hard to recycle the current volume
 *
 *  Returns: on failure - reason = NULL
 *           on success - reason - pointer to reason
 */
void check_if_volume_valid_or_recyclable(JCR *jcr, MEDIA_DBR *mr, const char **reason)
{
   int ok;

   *reason = NULL;

   /*  Check if a duration or limit has expired */
   if (has_volume_expired(jcr, mr)) {
      *reason = _("volume has expired");
      /* Keep going because we may be able to recycle volume */
   }

   /*
    * Now see if we can use the volume as is
    */
   if (strcmp(mr->VolStatus, "Append") == 0 ||
       strcmp(mr->VolStatus, "Recycle") == 0) {
      *reason = NULL;
      return;
   }

   /*
    * Check if the Volume is already marked for recycling
    */
   if (strcmp(mr->VolStatus, "Purged") == 0) {
      if (recycle_volume(jcr, mr)) {
         Jmsg(jcr, M_INFO, 0, _("Recycled current volume \"%s\"\n"), mr->VolumeName);
         *reason = NULL;
         return;
      } else {
         /* In principle this shouldn't happen */
         *reason = _("and recycling of current volume failed");
         return;
      }
   }

   /* At this point, the volume is not valid for writing */
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
    * Check retention period from last written, but recycle to within
    *   a minute to try to catch close calls ...
    */
   if ((mr->LastWritten + mr->VolRetention - 60) < (utime_t)time(NULL)
         && jcr->pool->recycle_current_volume
         && (strcmp(mr->VolStatus, "Full") == 0 ||
            strcmp(mr->VolStatus, "Used") == 0)) {
      /*
       * Attempt prune of current volume to see if we can
       * recycle it for use.
       */
      UAContext *ua;

      ua = new_ua_context(jcr);
      ok = prune_volume(ua, mr);
      free_ua_context(ua);

      if (ok) {
         /* If fully purged, recycle current volume */
         if (recycle_volume(jcr, mr)) {
            Jmsg(jcr, M_INFO, 0, _("Recycled current volume \"%s\"\n"), mr->VolumeName);
            *reason = NULL;
         } else {
            *reason = _("but should be Append, Purged or Recycle (recycling of the "
               "current volume failed)");
         }
      } else {
         *reason = _("but should be Append, Purged or Recycle (cannot automatically "
            "recycle current volume, as it still contains unpruned data "
            "or the Volume Retention time has not expired.)");
      }
   }
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

bool get_scratch_volume(JCR *jcr, bool InChanger, MEDIA_DBR *mr,
                        STORE *store)
{
   MEDIA_DBR smr;                        /* for searching scratch pool */
   POOL_DBR spr, pr;
   bool ok = false;
   bool found = false;

   /* Only one thread at a time can pull from the scratch pool */
   P(mutex);
   /* 
    * Get Pool record for Scratch Pool
    * choose between ScratchPoolId and Scratch
    * db_get_pool_record will first try ScratchPoolId, 
    * and then try the pool named Scratch
    */
   memset(&spr, 0, sizeof(spr));
   bstrncpy(spr.Name, "Scratch", sizeof(spr.Name));
   spr.PoolId = mr->ScratchPoolId;
   if (db_get_pool_record(jcr, jcr->db, &spr)) {
      smr.PoolId = spr.PoolId;
      if (InChanger) {       
         smr.StorageId = mr->StorageId;  /* want only Scratch Volumes in changer */
      }
      bstrncpy(smr.VolStatus, "Append", sizeof(smr.VolStatus));  /* want only appendable volumes */
      bstrncpy(smr.MediaType, mr->MediaType, sizeof(smr.MediaType));

      /*
       * If we do not find a valid Scratch volume, try
       *  recycling any existing purged volumes, then
       *  try to take the oldest volume.
       */
      set_storageid_in_mr(store, &smr);  /* put StorageId in new record */
      if (db_find_next_volume(jcr, jcr->db, 1, InChanger, &smr)) {
         found = true;

      } else if (find_recycled_volume(jcr, InChanger, &smr, store)) {
         found = true;

      } else if (recycle_oldest_purged_volume(jcr, InChanger, &smr, store)) {
         found = true;
      }

      if (found) {
         POOL_MEM query(PM_MESSAGE);

         /*   
          * Get pool record where the Scratch Volume will go to ensure
          * that we can add a Volume.
          */
         memset(&pr, 0, sizeof(pr));
         bstrncpy(pr.Name, jcr->pool->name(), sizeof(pr.Name));

         if (!db_get_pool_record(jcr, jcr->db, &pr)) {
            Jmsg(jcr, M_WARNING, 0, _("Unable to get Pool record: ERR=%s"), 
                 db_strerror(jcr->db));
            goto bail_out;
         }
         
         /* Make sure there is room for another volume */
         if (pr.MaxVols > 0 && pr.NumVols >= pr.MaxVols) {
            Jmsg(jcr, M_WARNING, 0, _("Unable add Scratch Volume, Pool \"%s\" full MaxVols=%d\n"),
                 jcr->pool->name(), pr.MaxVols);
            goto bail_out;
         }

         mr->copy(&smr);
         set_storageid_in_mr(store, mr);

         /* Set default parameters from current pool */
         set_pool_dbr_defaults_in_media_dbr(mr, &pr);

         /*
          * set_pool_dbr_defaults_in_media_dbr set VolStatus to Append,
          *   we could have Recycled media, also, we retain the old
          *   RecyclePoolId.
          */
         bstrncpy(mr->VolStatus, smr.VolStatus, sizeof(smr.VolStatus));
         mr->RecyclePoolId = smr.RecyclePoolId;

         if (!db_update_media_record(jcr, jcr->db, mr)) {
            Jmsg(jcr, M_WARNING, 0, _("Failed to move Scratch Volume. ERR=%s\n"),
                 db_strerror(jcr->db));
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
