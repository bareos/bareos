/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2015 Bareos GmbH & Co. KG

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
 * Kern Sibbald, September MM
 */
/**
 * @file
 * BAREOS Director -- User Agent database handling.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/ua_db.h"
#include "cats/sql_pooling.h"
#include "dird/ua_select.h"

/* Imported subroutines */

/**
 * This call explicitly checks for a catalog=catalog-name and
 * if given, opens that catalog.  It also checks for
 * client=client-name and if found, opens the catalog
 * corresponding to that client. If we still don't
 * have a catalog, look for a Job keyword and get the
 * catalog from its client record.
 */
bool OpenClientDb(UaContext *ua, bool use_private)
{
   int i;
   CatalogResource *catalog;
   ClientResource *client;
   JobResource *job;

   /*
    * Try for catalog keyword
    */
   i = FindArgWithValue(ua, NT_("catalog"));
   if (i >= 0) {
      catalog = ua->GetCatalogResWithName(ua->argv[i]);
      if (catalog) {
         if (ua->catalog && ua->catalog != catalog) {
            CloseDb(ua);
         }
         ua->catalog = catalog;
         return OpenDb(ua, use_private);
      }
   }

   /*
    * Try for client keyword
    */
   i = FindArgWithValue(ua, NT_("client"));
   if (i >= 0) {
      client = ua->GetClientResWithName(ua->argv[i]);
      if (client) {
         catalog = client->catalog;
         if (ua->catalog && ua->catalog != catalog) {
            CloseDb(ua);
         }
         if (!ua->AclAccessOk(Catalog_ACL, catalog->name(), true)) {
            ua->ErrorMsg(_("No authorization for Catalog \"%s\"\n"), catalog->name());
            return false;
         }
         ua->catalog = catalog;
         return OpenDb(ua, use_private);
      }
   }

   /*
    * Try for Job keyword
    */
   i = FindArgWithValue(ua, NT_("job"));
   if (i >= 0) {
      job = ua->GetJobResWithName(ua->argv[i]);
      if (job && job->client) {
         catalog = job->client->catalog;
         if (ua->catalog && ua->catalog != catalog) {
            CloseDb(ua);
         }
         if (!ua->AclAccessOk(Catalog_ACL, catalog->name(), true)) {
            ua->ErrorMsg(_("No authorization for Catalog \"%s\"\n"), catalog->name());
            return false;
         }
         ua->catalog = catalog;
         return OpenDb(ua, use_private);
      }
   }

   return OpenDb(ua, use_private);
}

/**
 * Open the catalog database.
 */
bool OpenDb(UaContext *ua, bool use_private)
{
   bool mult_db_conn;

   /*
    * See if we need to do any work at all.
    * Point the current used db e.g. ua->db to the correct database connection.
    */
   if (use_private) {
      if (ua->private_db) {
         ua->db = ua->private_db;
         return true;
      }
   } else if (ua->shared_db) {
      ua->db = ua->shared_db;
      return true;
   }

   /*
    * Select the right catalog to use.
    */
   if (!ua->catalog) {
      ua->catalog = get_catalog_resource(ua);
      if (!ua->catalog) {
         ua->ErrorMsg(_("Could not find a Catalog resource\n"));
         return false;
      }
   }

   /*
    * Some modules like bvfs need their own private catalog connection
    */
   mult_db_conn = ua->catalog->mult_db_connections;
   if (use_private) {
      mult_db_conn = true;
   }

   ua->jcr->res.catalog = ua->catalog;
   Dmsg0(100, "UA Open database\n");
   ua->db = DbSqlGetPooledConnection(ua->jcr,
                                         ua->catalog->db_driver,
                                         ua->catalog->db_name,
                                         ua->catalog->db_user,
                                         ua->catalog->db_password.value,
                                         ua->catalog->db_address,
                                         ua->catalog->db_port,
                                         ua->catalog->db_socket,
                                         mult_db_conn,
                                         ua->catalog->disable_batch_insert,
                                         ua->catalog->try_reconnect,
                                         ua->catalog->exit_on_fatal,
                                         use_private);
   if (ua->db == NULL) {
      ua->ErrorMsg(_("Could not open catalog database \"%s\".\n"), ua->catalog->db_name);
      return false;
   }
   ua->jcr->db = ua->db;

   /*
    * Save the new database connection under the right label e.g. shared or private.
    */
   if (use_private) {
      ua->private_db = ua->db;
   } else {
      ua->shared_db = ua->db;
   }

   if (!ua->api && !ua->runscript) {
      ua->SendMsg(_("Using Catalog \"%s\"\n"), ua->catalog->name());
   }

   Dmsg1(150, "DB %s opened\n", ua->catalog->db_name);
   return true;
}

void CloseDb(UaContext *ua)
{
   if (ua->jcr) {
      ua->jcr->db = NULL;
   }

   if (ua->shared_db) {
      DbSqlClosePooledConnection(ua->jcr, ua->shared_db);
      ua->shared_db = NULL;
   }

   if (ua->private_db) {
      DbSqlClosePooledConnection(ua->jcr, ua->private_db);
      ua->private_db = NULL;
   }
}

/**
 * Create a pool record from a given Pool resource
 *
 * Returns: -1  on error
 *           0  record already exists
 *           1  record created
 */
int CreatePool(JobControlRecord *jcr, BareosDb *db, PoolResource *pool, e_pool_op op)
{
   PoolDbRecord  pr;

   memset(&pr, 0, sizeof(pr));

   bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));

   if (db->GetPoolRecord(jcr, &pr)) {
      /*
       * Pool Exists
       */
      if (op == POOL_OP_UPDATE) {  /* update request */
         SetPooldbrFromPoolres(&pr, pool, op);
         SetPooldbrReferences(jcr, db, &pr, pool);
         db->UpdatePoolRecord(jcr, &pr);
      }
      return 0;                       /* exists */
   }

   SetPooldbrFromPoolres(&pr, pool, op);
   SetPooldbrReferences(jcr, db, &pr, pool);

   if (!db->CreatePoolRecord(jcr, &pr)) {
      return -1;                      /* error */
   }
   return 1;
}

/**
 * This is a common routine used to stuff the Pool DB record defaults
 * into the Media DB record just before creating a media (Volume) record.
 */
void SetPoolDbrDefaultsInMediaDbr(MediaDbRecord *mr, PoolDbRecord *pr)
{
   mr->PoolId = pr->PoolId;
   bstrncpy(mr->VolStatus, NT_("Append"), sizeof(mr->VolStatus));
   mr->Recycle = pr->Recycle;
   mr->VolRetention = pr->VolRetention;
   mr->VolUseDuration = pr->VolUseDuration;
   mr->ActionOnPurge = pr->ActionOnPurge;
   mr->RecyclePoolId = pr->RecyclePoolId;
   mr->MaxVolJobs = pr->MaxVolJobs;
   mr->MaxVolFiles = pr->MaxVolFiles;
   mr->MaxVolBytes = pr->MaxVolBytes;
   mr->LabelType = pr->LabelType;
   mr->MinBlocksize = pr->MinBlocksize;
   mr->MaxBlocksize = pr->MaxBlocksize;
   mr->Enabled = VOL_ENABLED;
}

/**
 * Set PoolDbRecord.RecyclePoolId and PoolDbRecord.ScratchPoolId from Pool resource
 * works with set_pooldbr_from_poolres
 */
bool SetPooldbrReferences(JobControlRecord *jcr, BareosDb *db, PoolDbRecord *pr, PoolResource *pool)
{
   PoolDbRecord rpool;
   bool ret = true;

   if (pool->RecyclePool) {
      memset(&rpool, 0, sizeof(rpool));

      bstrncpy(rpool.Name, pool->RecyclePool->name(), sizeof(rpool.Name));
      if (db->GetPoolRecord(jcr, &rpool)) {
        pr->RecyclePoolId = rpool.PoolId;
      } else {
        Jmsg(jcr, M_WARNING, 0,
        _("Can't set %s RecyclePool to %s, %s is not in database.\n" \
          "Try to update it with 'update pool=%s'\n"),
        pool->name(), rpool.Name, rpool.Name,pool->name());

        ret = false;
      }
   } else {                    /* no RecyclePool used, set it to 0 */
      pr->RecyclePoolId = 0;
   }

   if (pool->ScratchPool) {
      memset(&rpool, 0, sizeof(rpool));

      bstrncpy(rpool.Name, pool->ScratchPool->name(), sizeof(rpool.Name));
      if (db->GetPoolRecord(jcr, &rpool)) {
        pr->ScratchPoolId = rpool.PoolId;
      } else {
        Jmsg(jcr, M_WARNING, 0,
        _("Can't set %s ScratchPool to %s, %s is not in database.\n" \
          "Try to update it with 'update pool=%s'\n"),
        pool->name(), rpool.Name, rpool.Name,pool->name());
        ret = false;
      }
   } else {                    /* no ScratchPool used, set it to 0 */
      pr->ScratchPoolId = 0;
   }

   return ret;
}

/**
 * This is a common routine to create or update a
 * Pool DB base record from a Pool Resource. We handle
 * the setting of MaxVols and NumVols slightly differently
 * depending on if we are creating the Pool or we are
 * simply bringing it into agreement with the resource (update).
 *
 * Caution : RecyclePoolId isn't setup in this function.
 *           You can use set_pooldbr_recyclepoolid();
 */
void SetPooldbrFromPoolres(PoolDbRecord *pr, PoolResource *pool, e_pool_op op)
{
   bstrncpy(pr->PoolType, pool->pool_type, sizeof(pr->PoolType));
   if (op == POOL_OP_CREATE) {
      pr->MaxVols = pool->max_volumes;
      pr->NumVols = 0;
   } else {
      /*
       * Update pool
       */
      if (pr->MaxVols != pool->max_volumes) {
         pr->MaxVols = pool->max_volumes;
      }
      if (pr->MaxVols != 0 && pr->MaxVols < pr->NumVols) {
         pr->MaxVols = pr->NumVols;
      }
   }
   pr->LabelType = pool->LabelType;
   pr->UseOnce = pool->use_volume_once;
   pr->UseCatalog = pool->use_catalog;
   pr->Recycle = pool->Recycle;
   pr->VolRetention = pool->VolRetention;
   pr->VolUseDuration = pool->VolUseDuration;
   pr->MaxVolJobs = pool->MaxVolJobs;
   pr->MaxVolFiles = pool->MaxVolFiles;
   pr->MaxVolBytes = pool->MaxVolBytes;
   pr->AutoPrune = pool->AutoPrune;
   pr->ActionOnPurge = pool->action_on_purge;
   pr->ActionOnPurge = pool->action_on_purge;
   pr->MinBlocksize = pool->MinBlocksize;
   pr->MaxBlocksize = pool->MaxBlocksize;
   pr->Recycle = pool->Recycle;
   if (pool->label_format) {
      bstrncpy(pr->LabelFormat, pool->label_format, sizeof(pr->LabelFormat));
   } else {
      bstrncpy(pr->LabelFormat, "*", sizeof(pr->LabelFormat));    /* none */
   }
}

/**
 * set/update Pool.RecyclePoolId and Pool.ScratchPoolId in Catalog
 */
int UpdatePoolReferences(JobControlRecord *jcr, BareosDb *db, PoolResource *pool)
{
   PoolDbRecord pr;

   if (!pool->RecyclePool && !pool->ScratchPool) {
      return true;
   }

   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));

   if (!db->GetPoolRecord(jcr, &pr)) {
      return -1;                       /* not exists in database */
   }

   SetPooldbrFromPoolres(&pr, pool, POOL_OP_UPDATE);

   if (!SetPooldbrReferences(jcr, db, &pr, pool)) {
      return -1;                      /* error */
   }

   if (!db->UpdatePoolRecord(jcr, &pr)) {
      return -1;                      /* error */
   }
   return true;
}
