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

#include "bareos.h"
#include "dird.h"

/* Imported subroutines */

/**
 * This call explicitly checks for a catalog=catalog-name and
 * if given, opens that catalog.  It also checks for
 * client=client-name and if found, opens the catalog
 * corresponding to that client. If we still don't
 * have a catalog, look for a Job keyword and get the
 * catalog from its client record.
 */
bool open_client_db(UAContext *ua, bool use_private)
{
   int i;
   CATRES *catalog;
   CLIENTRES *client;
   JOBRES *job;

   /*
    * Try for catalog keyword
    */
   i = find_arg_with_value(ua, NT_("catalog"));
   if (i >= 0) {
      catalog = ua->GetCatalogResWithName(ua->argv[i]);
      if (catalog) {
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         ua->catalog = catalog;
         return open_db(ua, use_private);
      }
   }

   /*
    * Try for client keyword
    */
   i = find_arg_with_value(ua, NT_("client"));
   if (i >= 0) {
      client = ua->GetClientResWithName(ua->argv[i]);
      if (client) {
         catalog = client->catalog;
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         if (!ua->acl_access_ok(Catalog_ACL, catalog->name(), true)) {
            ua->error_msg(_("No authorization for Catalog \"%s\"\n"), catalog->name());
            return false;
         }
         ua->catalog = catalog;
         return open_db(ua, use_private);
      }
   }

   /*
    * Try for Job keyword
    */
   i = find_arg_with_value(ua, NT_("job"));
   if (i >= 0) {
      job = ua->GetJobResWithName(ua->argv[i]);
      if (job && job->client) {
         catalog = job->client->catalog;
         if (ua->catalog && ua->catalog != catalog) {
            close_db(ua);
         }
         if (!ua->acl_access_ok(Catalog_ACL, catalog->name(), true)) {
            ua->error_msg(_("No authorization for Catalog \"%s\"\n"), catalog->name());
            return false;
         }
         ua->catalog = catalog;
         return open_db(ua, use_private);
      }
   }

   return open_db(ua, use_private);
}

/**
 * Open the catalog database.
 */
bool open_db(UAContext *ua, bool use_private)
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
         ua->error_msg(_("Could not find a Catalog resource\n"));
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
   ua->db = db_sql_get_pooled_connection(ua->jcr,
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
      ua->error_msg(_("Could not open catalog database \"%s\".\n"), ua->catalog->db_name);
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
      ua->send_msg(_("Using Catalog \"%s\"\n"), ua->catalog->name());
   }

   Dmsg1(150, "DB %s opened\n", ua->catalog->db_name);
   return true;
}

void close_db(UAContext *ua)
{
   if (ua->jcr) {
      ua->jcr->db = NULL;
   }

   if (ua->shared_db) {
      db_sql_close_pooled_connection(ua->jcr, ua->shared_db);
      ua->shared_db = NULL;
   }

   if (ua->private_db) {
      db_sql_close_pooled_connection(ua->jcr, ua->private_db);
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
int create_pool(JCR *jcr, B_DB *db, POOLRES *pool, e_pool_op op)
{
   POOL_DBR  pr;

   memset(&pr, 0, sizeof(pr));

   bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));

   if (db->get_pool_record(jcr, &pr)) {
      /*
       * Pool Exists
       */
      if (op == POOL_OP_UPDATE) {  /* update request */
         set_pooldbr_from_poolres(&pr, pool, op);
         set_pooldbr_references(jcr, db, &pr, pool);
         db->update_pool_record(jcr, &pr);
      }
      return 0;                       /* exists */
   }

   set_pooldbr_from_poolres(&pr, pool, op);
   set_pooldbr_references(jcr, db, &pr, pool);

   if (!db->create_pool_record(jcr, &pr)) {
      return -1;                      /* error */
   }
   return 1;
}

/**
 * This is a common routine used to stuff the Pool DB record defaults
 * into the Media DB record just before creating a media (Volume) record.
 */
void set_pool_dbr_defaults_in_media_dbr(MEDIA_DBR *mr, POOL_DBR *pr)
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
 * Set POOL_DBR.RecyclePoolId and POOL_DBR.ScratchPoolId from Pool resource
 * works with set_pooldbr_from_poolres
 */
bool set_pooldbr_references(JCR *jcr, B_DB *db, POOL_DBR *pr, POOLRES *pool)
{
   POOL_DBR rpool;
   bool ret = true;

   if (pool->RecyclePool) {
      memset(&rpool, 0, sizeof(rpool));

      bstrncpy(rpool.Name, pool->RecyclePool->name(), sizeof(rpool.Name));
      if (db->get_pool_record(jcr, &rpool)) {
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
      if (db->get_pool_record(jcr, &rpool)) {
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
void set_pooldbr_from_poolres(POOL_DBR *pr, POOLRES *pool, e_pool_op op)
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
int update_pool_references(JCR *jcr, B_DB *db, POOLRES *pool)
{
   POOL_DBR pr;

   if (!pool->RecyclePool && !pool->ScratchPool) {
      return true;
   }

   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));

   if (!db->get_pool_record(jcr, &pr)) {
      return -1;                       /* not exists in database */
   }

   set_pooldbr_from_poolres(&pr, pool, POOL_OP_UPDATE);

   if (!set_pooldbr_references(jcr, db, &pr, pool)) {
      return -1;                      /* error */
   }

   if (!db->update_pool_record(jcr, &pr)) {
      return -1;                      /* error */
   }
   return true;
}
