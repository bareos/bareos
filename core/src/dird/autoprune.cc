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
 * Kern Sibbald, May MMII
 */
/**
 * @file
 * Automatic Pruning Applies retention periods
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/next_vol.h"
#include "dird/ua_server.h"
#include "dird/ua_prune.h"
#include "dird/ua_purge.h"
#include "lib/edit.h"

/* Forward referenced functions */

/**
 * Auto Prune Jobs and Files. This is called at the end of every
 *   Job.  We do not prune volumes here.
 */
void DoAutoprune(JobControlRecord *jcr)
{
   UaContext *ua;
   JobResource *job;
   ClientResource *client;
   PoolResource *pool;
   bool pruned;

   if (!jcr->res.client) {            /* temp -- remove me */
      return;
   }

   ua = new_ua_context(jcr);
   job = jcr->res.job;
   client = jcr->res.client;
   pool = jcr->res.pool;

   if (job->PruneJobs || client->AutoPrune) {
      PruneJobs(ua, client, pool, jcr->getJobType());
      pruned = true;
   } else {
      pruned = false;
   }

   if (job->PruneFiles || client->AutoPrune) {
      PruneFiles(ua, client, pool);
      pruned = true;
   }
   if (pruned) {
      Jmsg(jcr, M_INFO, 0, _("End auto prune.\n\n"));
   }
   FreeUaContext(ua);
   return;
}

/**
 * Prune at least one Volume in current Pool. This is called from catreq.c => next_vol.c
 * when the Storage daemon is asking for another volume and no appendable volumes are available.
 */
void PruneVolumes(JobControlRecord *jcr, bool InChanger,
                   MediaDbRecord *mr, StorageResource *store)
{
   int i;
   int count;
   PoolDbRecord spr;
   UaContext *ua;
   dbid_list ids;
   struct del_ctx prune_list;
   PoolMem query(PM_MESSAGE);
   char ed1[50], ed2[100], ed3[50];

   Dmsg1(100, "Prune volumes PoolId=%d\n", jcr->jr.PoolId);
   if (!jcr->res.job->PruneVolumes && !jcr->res.pool->AutoPrune) {
      Dmsg0(100, "AutoPrune not set in Pool.\n");
      return;
   }

   memset(&prune_list, 0, sizeof(prune_list));
   prune_list.max_ids = 10000;
   prune_list.JobId = (JobId_t *)malloc(sizeof(JobId_t) * prune_list.max_ids);

   ua = new_ua_context(jcr);
   DbLock(jcr->db);

   /*
    * Edit PoolId
    */
   edit_int64(mr->PoolId, ed1);

   /*
    * Get Pool record for Scratch Pool
    */
   memset(&spr, 0, sizeof(spr));
   bstrncpy(spr.Name, "Scratch", sizeof(spr.Name));
   if (jcr->db->GetPoolRecord(jcr, &spr)) {
      edit_int64(spr.PoolId, ed2);
      bstrncat(ed2, ",", sizeof(ed2));
   } else {
      ed2[0] = 0;
   }

   if (mr->ScratchPoolId) {
      edit_int64(mr->ScratchPoolId, ed3);
      bstrncat(ed2, ed3, sizeof(ed2));
      bstrncat(ed2, ",", sizeof(ed2));
   }

   Dmsg1(100, "Scratch pool(s)=%s\n", ed2);
   /*
    * ed2 ends up with scratch poolid and current poolid or
    *   just current poolid if there is no scratch pool
    */
   bstrncat(ed2, ed1, sizeof(ed2));

   /*
    * Get the List of all media ids in the current Pool or whose
    *  RecyclePoolId is the current pool or the scratch pool
    */
   const char *select = "SELECT DISTINCT MediaId,LastWritten FROM Media WHERE "
        "(PoolId=%s OR RecyclePoolId IN (%s)) AND MediaType='%s' %s"
        "ORDER BY LastWritten ASC,MediaId";

   if (InChanger) {
      char changer[100];
      /* Ensure it is in this autochanger */
      Bsnprintf(changer, sizeof(changer), "AND InChanger=1 AND StorageId=%s ",
         edit_int64(mr->StorageId, ed3));
      Mmsg(query, select, ed1, ed2, mr->MediaType, changer);
   } else {
      Mmsg(query, select, ed1, ed2, mr->MediaType, "");
   }

   Dmsg1(100, "query=%s\n", query.c_str());
   if (!jcr->db->GetQueryDbids(ua->jcr, query, ids)) {
      Jmsg(jcr, M_ERROR, 0, "%s", jcr->db->strerror());
      goto bail_out;
   }

   Dmsg1(100, "Volume prune num_ids=%d\n", ids.num_ids);

   /* Visit each Volume and Prune it until we find one that is purged */
   for (i=0; i<ids.num_ids; i++) {
      MediaDbRecord lmr;

      memset(&lmr, 0, sizeof(lmr));
      lmr.MediaId = ids.DBId[i];
      Dmsg1(100, "Get record MediaId=%d\n", (int)lmr.MediaId);
      if (!jcr->db->GetMediaRecord(jcr, &lmr)) {
         Jmsg(jcr, M_ERROR, 0, "%s", jcr->db->strerror());
         continue;
      }
      Dmsg1(100, "Examine vol=%s\n", lmr.VolumeName);
      /* Don't prune archived volumes */
      if (lmr.Enabled == VOL_ARCHIVED) {
         Dmsg1(100, "Vol=%s disabled\n", lmr.VolumeName);
         continue;
      }
      /* Prune only Volumes with status "Full", or "Used" */
      if (bstrcmp(lmr.VolStatus, "Full") ||
          bstrcmp(lmr.VolStatus, "Used")) {
         Dmsg2(100, "Add prune list MediaId=%d Volume %s\n", (int)lmr.MediaId, lmr.VolumeName);
         count = GetPruneListForVolume(ua, &lmr, &prune_list);
         Dmsg1(100, "Num pruned = %d\n", count);
         if (count != 0) {
            PurgeJobListFromCatalog(ua, prune_list);
            prune_list.num_ids = 0;             /* reset count */
         }
         if (!IsVolumePurged(ua, &lmr)) {
            Dmsg1(050, "Vol=%s not pruned\n", lmr.VolumeName);
            continue;
         }
         Dmsg1(050, "Vol=%s is purged\n", lmr.VolumeName);

         /*
          * Since we are also pruning the Scratch pool, continue until and check if
          * this volume is available (InChanger + StorageId) If not, just skip this
          * volume and try the next one
          */
         if (InChanger) {
            if (!lmr.InChanger || (lmr.StorageId != mr->StorageId)) {
               Dmsg1(100, "Vol=%s not inchanger or correct StoreId\n", lmr.VolumeName);
               continue;                  /* skip this volume, ie not loadable */
            }
         }
         if (!lmr.Recycle) {
            Dmsg1(100, "Vol=%s not recyclable\n", lmr.VolumeName);
            continue;
         }

         if (HasVolumeExpired(jcr, &lmr)) {
            Dmsg1(100, "Vol=%s has expired\n", lmr.VolumeName);
            continue;                     /* Volume not usable */
         }

         /*
          * If purged and not moved to another Pool, then we stop pruning and take this volume.
          */
         if (lmr.PoolId == mr->PoolId) {
            Dmsg2(100, "Got Vol=%s MediaId=%d purged.\n", lmr.VolumeName, (int)lmr.MediaId);
            memcpy(mr, &lmr, sizeof(MediaDbRecord));
            SetStorageidInMr(store, mr);
            break;                        /* got a volume */
         }
      }
   }

bail_out:
   Dmsg0(100, "Leave prune volumes\n");
   DbUnlock(jcr->db);
   FreeUaContext(ua);
   if (prune_list.JobId) {
      free(prune_list.JobId);
   }
   return;
}
