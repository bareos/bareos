/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 *   Bacula Director -- Update command processing
 *     Split from ua_cmds.c March 2005
 *
 *     Kern Sibbald, September MM
 *
 */

#include "bacula.h"
#include "dird.h"

/* Forward referenced functions */
static int update_volume(UAContext *ua);
static bool update_pool(UAContext *ua);
static bool update_job(UAContext *ua);
static bool update_stats(UAContext *ua);

/*
 * Update a Pool Record in the database.
 *  It is always updated from the Resource record.
 *
 *    update pool=<pool-name>
 *         updates pool from Pool resource
 *    update media pool=<pool-name> volume=<volume-name>
 *         changes pool info for volume
 *    update slots [scan=...]
 *         updates autochanger slots
 *    update stats [days=...]
 *         updates long term statistics
 */
int update_cmd(UAContext *ua, const char *cmd)
{
   static const char *kw[] = {
      NT_("media"),  /* 0 */
      NT_("volume"), /* 1 */
      NT_("pool"),   /* 2 */
      NT_("slots"),  /* 3 */
      NT_("slot"),   /* 4 */
      NT_("jobid"),  /* 5 */
      NT_("stats"),  /* 6 */
      NULL};

   if (!open_client_db(ua)) {
      return 1;
   }

   switch (find_arg_keyword(ua, kw)) {
   case 0:
   case 1:
      update_volume(ua);
      return 1;
   case 2:
      update_pool(ua);
      return 1;
   case 3:
   case 4:
      update_slots(ua);
      return 1;
   case 5:
      update_job(ua);
      return 1;
   case 6:
      update_stats(ua);
      return 1;
   default:
      break;
   }

   start_prompt(ua, _("Update choice:\n"));
   add_prompt(ua, _("Volume parameters"));
   add_prompt(ua, _("Pool from resource"));
   add_prompt(ua, _("Slots from autochanger"));
   add_prompt(ua, _("Long term statistics"));
   switch (do_prompt(ua, _("item"), _("Choose catalog item to update"), NULL, 0)) {
   case 0:
      update_volume(ua);
      break;
   case 1:
      update_pool(ua);
      break;
   case 2:
      update_slots(ua);
      break;
   case 3:
      update_stats(ua);
      break;
   default:
      break;
   }
   return 1;
}

static void update_volstatus(UAContext *ua, const char *val, MEDIA_DBR *mr)
{
   POOL_MEM query(PM_MESSAGE);
   const char *kw[] = {
      NT_("Append"),
      NT_("Archive"),
      NT_("Disabled"),
      NT_("Full"),
      NT_("Used"),
      NT_("Cleaning"),
      NT_("Recycle"),
      NT_("Read-Only"),
      NT_("Error"),
      NULL};
   bool found = false;
   int i;

   for (i=0; kw[i]; i++) {
      if (strcasecmp(val, kw[i]) == 0) {
         found = true;
         break;
      }
   }
   if (!found) {
      ua->error_msg(_("Invalid VolStatus specified: %s\n"), val);
   } else {
      char ed1[50];
      bstrncpy(mr->VolStatus, kw[i], sizeof(mr->VolStatus));
      Mmsg(query, "UPDATE Media SET VolStatus='%s' WHERE MediaId=%s",
         mr->VolStatus, edit_int64(mr->MediaId,ed1));
      if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
         ua->error_msg("%s", db_strerror(ua->db));
      } else {
         ua->info_msg(_("New Volume status is: %s\n"), mr->VolStatus);
      }
   }
}

static void update_volretention(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   char ed1[150], ed2[50];
   POOL_MEM query(PM_MESSAGE);
   if (!duration_to_utime(val, &mr->VolRetention)) {
      ua->error_msg(_("Invalid retention period specified: %s\n"), val);
      return;
   }
   Mmsg(query, "UPDATE Media SET VolRetention=%s WHERE MediaId=%s",
      edit_uint64(mr->VolRetention, ed1), edit_int64(mr->MediaId,ed2));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New retention period is: %s\n"),
         edit_utime(mr->VolRetention, ed1, sizeof(ed1)));
   }
}

static void update_voluseduration(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   char ed1[150], ed2[50];
   POOL_MEM query(PM_MESSAGE);

   if (!duration_to_utime(val, &mr->VolUseDuration)) {
      ua->error_msg(_("Invalid use duration specified: %s\n"), val);
      return;
   }
   Mmsg(query, "UPDATE Media SET VolUseDuration=%s WHERE MediaId=%s",
      edit_uint64(mr->VolUseDuration, ed1), edit_int64(mr->MediaId,ed2));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New use duration is: %s\n"),
         edit_utime(mr->VolUseDuration, ed1, sizeof(ed1)));
   }
}

static void update_volmaxjobs(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOL_MEM query(PM_MESSAGE);
   char ed1[50];
   Mmsg(query, "UPDATE Media SET MaxVolJobs=%s WHERE MediaId=%s",
      val, edit_int64(mr->MediaId,ed1));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New max jobs is: %s\n"), val);
   }
}

static void update_volmaxfiles(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOL_MEM query(PM_MESSAGE);
   char ed1[50];
   Mmsg(query, "UPDATE Media SET MaxVolFiles=%s WHERE MediaId=%s",
      val, edit_int64(mr->MediaId, ed1));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New max files is: %s\n"), val);
   }
}

static void update_volmaxbytes(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   uint64_t maxbytes;
   char ed1[50], ed2[50];
   POOL_MEM query(PM_MESSAGE);

   if (!size_to_uint64(val, strlen(val), &maxbytes)) {
      ua->error_msg(_("Invalid max. bytes specification: %s\n"), val);
      return;
   }
   Mmsg(query, "UPDATE Media SET MaxVolBytes=%s WHERE MediaId=%s",
      edit_uint64(maxbytes, ed1), edit_int64(mr->MediaId, ed2));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New Max bytes is: %s\n"), edit_uint64(maxbytes, ed1));
   }
}

static void update_volrecycle(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   int recycle;
   char ed1[50];

   POOL_MEM query(PM_MESSAGE);
   if (!is_yesno(val, &recycle)) {
      ua->error_msg(_("Invalid value. It must be yes or no.\n"));
      return;
   }
   Mmsg(query, "UPDATE Media SET Recycle=%d WHERE MediaId=%s",
      recycle, edit_int64(mr->MediaId, ed1));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New Recycle flag is: %s\n"),
         recycle==1?_("yes"):_("no"));
   }
}

static void update_volinchanger(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   int InChanger;
   char ed1[50];

   POOL_MEM query(PM_MESSAGE);
   if (!is_yesno(val, &InChanger)) {
      ua->error_msg(_("Invalid value. It must be yes or no.\n"));
      return;
   }
   Mmsg(query, "UPDATE Media SET InChanger=%d WHERE MediaId=%s",
      InChanger, edit_int64(mr->MediaId, ed1));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New InChanger flag is: %s\n"),
         InChanger==1?_("yes"):_("no"));
   }
}


static void update_volslot(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOL_DBR pr;

   memset(&pr, 0, sizeof(POOL_DBR));
   pr.PoolId = mr->PoolId;
   if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
      ua->error_msg("%s", db_strerror(ua->db));
      return;
   }
   mr->Slot = atoi(val);
   if (pr.MaxVols > 0 && mr->Slot > (int)pr.MaxVols) {
      ua->error_msg(_("Invalid slot, it must be between 0 and MaxVols=%d\n"),
         pr.MaxVols);
      return;
   }
   /*
    * Make sure to use db_update... rather than doing this directly,
    *   so that any Slot is handled correctly.
    */
   set_storageid_in_mr(NULL, mr);
   if (!db_update_media_record(ua->jcr, ua->db, mr)) {
      ua->error_msg(_("Error updating media record Slot: ERR=%s"), db_strerror(ua->db));
   } else {
      ua->info_msg(_("New Slot is: %d\n"), mr->Slot);
   }
}

/* Modify the Pool in which this Volume is located */
void update_vol_pool(UAContext *ua, char *val, MEDIA_DBR *mr, POOL_DBR *opr)
{
   POOL_DBR pr;
   POOL_MEM query(PM_MESSAGE);
   char ed1[50], ed2[50];

   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, val, sizeof(pr.Name));
   if (!get_pool_dbr(ua, &pr)) {
      return;
   }
   mr->PoolId = pr.PoolId;            /* set new PoolId */
   /*
    */
   db_lock(ua->db);
   Mmsg(query, "UPDATE Media SET PoolId=%s WHERE MediaId=%s",
      edit_int64(mr->PoolId, ed1), edit_int64(mr->MediaId, ed2));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New Pool is: %s\n"), pr.Name);
      opr->NumVols--;
      if (!db_update_pool_record(ua->jcr, ua->db, opr)) {
         ua->error_msg("%s", db_strerror(ua->db));
      }
      pr.NumVols++;
      if (!db_update_pool_record(ua->jcr, ua->db, &pr)) {
         ua->error_msg("%s", db_strerror(ua->db));
      }
   }
   db_unlock(ua->db);
}

/* Modify the RecyclePool of a Volume */
void update_vol_recyclepool(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOL_DBR pr;
   POOL_MEM query(PM_MESSAGE);
   char ed1[50], ed2[50], *poolname;

   if(val && *val) { /* update volume recyclepool="Scratch" */
     /* If a pool name is given, look up the PoolId */
     memset(&pr, 0, sizeof(pr));
     bstrncpy(pr.Name, val, sizeof(pr.Name));
     if (!get_pool_dbr(ua, &pr, NT_("recyclepool"))) {
        return;
     }
     /* pool = select_pool_resource(ua);  */
     mr->RecyclePoolId = pr.PoolId;            /* get the PoolId */
     poolname = pr.Name;

  } else { /* update volume recyclepool="" */
    /* If no pool name is given, set the PoolId to 0 (the default) */
     mr->RecyclePoolId = 0;
     poolname = _("*None*");
  }

   db_lock(ua->db);
   Mmsg(query, "UPDATE Media SET RecyclePoolId=%s WHERE MediaId=%s",
      edit_int64(mr->RecyclePoolId, ed1), edit_int64(mr->MediaId, ed2));
   if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
   } else {
      ua->info_msg(_("New RecyclePool is: %s\n"), poolname);
   }
   db_unlock(ua->db);
}

/*
 * Refresh the Volume information from the Pool record
 */
static void update_vol_from_pool(UAContext *ua, MEDIA_DBR *mr)
{
   POOL_DBR pr;

   memset(&pr, 0, sizeof(pr));
   pr.PoolId = mr->PoolId;
   if (!db_get_pool_record(ua->jcr, ua->db, &pr) ||
       !acl_access_ok(ua, Pool_ACL, pr.Name)) {
      return;
   }
   set_pool_dbr_defaults_in_media_dbr(mr, &pr);
   if (!db_update_media_defaults(ua->jcr, ua->db, mr)) {
      ua->error_msg(_("Error updating Volume record: ERR=%s"), db_strerror(ua->db));
   } else {
      ua->info_msg(_("Volume defaults updated from \"%s\" Pool record.\n"),
         pr.Name);
   }
}

/*
 * Refresh the Volume information from the Pool record
 *   for all Volumes
 */
static void update_all_vols_from_pool(UAContext *ua, const char *pool_name) 
{
   POOL_DBR pr;
   MEDIA_DBR mr;

   memset(&pr, 0, sizeof(pr));

   bstrncpy(pr.Name, pool_name, sizeof(pr.Name));
   if (!get_pool_dbr(ua, &pr)) {
      return;
   }
   set_pool_dbr_defaults_in_media_dbr(&mr, &pr);
   mr.PoolId = pr.PoolId;
   if (!db_update_media_defaults(ua->jcr, ua->db, &mr)) {
      ua->error_msg(_("Error updating Volume records: ERR=%s"), db_strerror(ua->db));
   } else {
      ua->info_msg(_("All Volume defaults updated from \"%s\" Pool record.\n"),
         pr.Name);
   }
}

static void update_all_vols(UAContext *ua)
{
   int i, num_pools;
   uint32_t *ids;
   POOL_DBR pr;
   MEDIA_DBR mr;

   memset(&pr, 0, sizeof(pr));
   
   if (!db_get_pool_ids(ua->jcr, ua->db, &num_pools, &ids)) {
      ua->error_msg(_("Error obtaining pool ids. ERR=%s\n"), db_strerror(ua->db));
      return;
   }

   for (i=0; i<num_pools; i++) {
      pr.PoolId = ids[i];
      if (!db_get_pool_record(ua->jcr, ua->db, &pr)) { /* ***FIXME*** use acl? */
         ua->warning_msg(_("Updating all pools, but skipped PoolId=%d. ERR=%s\n"), db_strerror(ua->db));
         continue;
      }

      set_pool_dbr_defaults_in_media_dbr(&mr, &pr);
      mr.PoolId = pr.PoolId;

      if (!db_update_media_defaults(ua->jcr, ua->db, &mr)) {
         ua->error_msg(_("Error updating Volume records: ERR=%s"), db_strerror(ua->db));
      } else {
         ua->info_msg(_("All Volume defaults updated from \"%s\" Pool record.\n"),
            pr.Name);
      }
   }

   free(ids);
}

static void update_volenabled(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   mr->Enabled = get_enabled(ua, val);
   if (mr->Enabled < 0) {
      return;
   }
   set_storageid_in_mr(NULL, mr);
   if (!db_update_media_record(ua->jcr, ua->db, mr)) {
      ua->error_msg(_("Error updating media record Enabled: ERR=%s"),
                    db_strerror(ua->db));
   } else {
      ua->info_msg(_("New Enabled is: %d\n"), mr->Enabled);
   }
}

static void update_vol_actiononpurge(UAContext *ua, char *val, MEDIA_DBR *mr)
{
   POOL_MEM ret;
   if (strcasecmp(val, "truncate") == 0) {
      mr->ActionOnPurge = ON_PURGE_TRUNCATE;
   } else {
      mr->ActionOnPurge = 0;
   }
   
   set_storageid_in_mr(NULL, mr);
   if (!db_update_media_record(ua->jcr, ua->db, mr)) {
      ua->error_msg(_("Error updating media record ActionOnPurge: ERR=%s"),
                    db_strerror(ua->db));
   } else {
      ua->info_msg(_("New ActionOnPurge is: %s\n"), 
                   action_on_purge_to_string(mr->ActionOnPurge, ret));
   }
}

/*
 * Update a media record -- allows you to change the
 *  Volume status. E.g. if you want Bacula to stop
 *  writing on the volume, set it to anything other
 *  than Append.
 */
static int update_volume(UAContext *ua)
{
   MEDIA_DBR mr;
   POOL *pool;
   POOL_DBR pr;
   POOLMEM *query;
   POOL_MEM ret;
   char buf[1000];
   char ed1[130];
   bool done = false;
   int i;
   const char *kw[] = {
      NT_("VolStatus"),                /* 0 */
      NT_("VolRetention"),             /* 1 */
      NT_("VolUse"),                   /* 2 */
      NT_("MaxVolJobs"),               /* 3 */
      NT_("MaxVolFiles"),              /* 4 */
      NT_("MaxVolBytes"),              /* 5 */
      NT_("Recycle"),                  /* 6 */
      NT_("InChanger"),                /* 7 */
      NT_("Slot"),                     /* 8 */
      NT_("Pool"),                     /* 9 */
      NT_("FromPool"),                 /* 10 */
      NT_("AllFromPool"),              /* 11 !!! see below !!! */
      NT_("Enabled"),                  /* 12 */
      NT_("RecyclePool"),              /* 13 */
      NT_("ActionOnPurge"),            /* 14 */
      NULL };

#define AllFromPool 11               /* keep this updated with above */

   for (i=0; kw[i]; i++) {
      int j;
      POOL_DBR pr;

      if ((j=find_arg_with_value(ua, kw[i])) > 0) {
         /* If all from pool don't select a media record */
         if (i != AllFromPool && !select_media_dbr(ua, &mr)) {
            return 0;
         }
         switch (i) {
         case 0:
            update_volstatus(ua, ua->argv[j], &mr);
            break;
         case 1:
            update_volretention(ua, ua->argv[j], &mr);
            break;
         case 2:
            update_voluseduration(ua, ua->argv[j], &mr);
            break;
         case 3:
            update_volmaxjobs(ua, ua->argv[j], &mr);
            break;
         case 4:
            update_volmaxfiles(ua, ua->argv[j], &mr);
            break;
         case 5:
            update_volmaxbytes(ua, ua->argv[j], &mr);
            break;
         case 6:
            update_volrecycle(ua, ua->argv[j], &mr);
            break;
         case 7:
            update_volinchanger(ua, ua->argv[j], &mr);
            break;
         case 8:
            update_volslot(ua, ua->argv[j], &mr);
            break;
         case 9:
            memset(&pr, 0, sizeof(POOL_DBR));
            pr.PoolId = mr.PoolId;
            if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
               ua->error_msg("%s", db_strerror(ua->db));
               break;
            }
            update_vol_pool(ua, ua->argv[j], &mr, &pr);
            break;
         case 10:
            update_vol_from_pool(ua, &mr);
            return 1;
         case 11:
            update_all_vols_from_pool(ua, ua->argv[j]);
            return 1;
         case 12:
            update_volenabled(ua, ua->argv[j], &mr);
            break;
         case 13:
            update_vol_recyclepool(ua, ua->argv[j], &mr);
            break;
         case 14:
            update_vol_actiononpurge(ua, ua->argv[j], &mr);
            break;
         }
         done = true;
      }
   }

   /* Allow user to simply update all volumes */
   if (find_arg(ua, NT_("fromallpools")) > 0) {
      update_all_vols(ua);
      return 1;
   }

   for ( ; !done; ) {
      start_prompt(ua, _("Parameters to modify:\n"));
      add_prompt(ua, _("Volume Status"));              /* 0 */
      add_prompt(ua, _("Volume Retention Period"));    /* 1 */
      add_prompt(ua, _("Volume Use Duration"));        /* 2 */
      add_prompt(ua, _("Maximum Volume Jobs"));        /* 3 */
      add_prompt(ua, _("Maximum Volume Files"));       /* 4 */
      add_prompt(ua, _("Maximum Volume Bytes"));       /* 5 */
      add_prompt(ua, _("Recycle Flag"));               /* 6 */
      add_prompt(ua, _("Slot"));                       /* 7 */
      add_prompt(ua, _("InChanger Flag"));             /* 8 */
      add_prompt(ua, _("Volume Files"));               /* 9 */
      add_prompt(ua, _("Pool"));                       /* 10 */
      add_prompt(ua, _("Volume from Pool"));           /* 11 */
      add_prompt(ua, _("All Volumes from Pool"));      /* 12 */
      add_prompt(ua, _("All Volumes from all Pools")); /* 13 */
      add_prompt(ua, _("Enabled")),                    /* 14 */
      add_prompt(ua, _("RecyclePool")),                /* 15 */
      add_prompt(ua, _("Action On Purge")),            /* 16 */
      add_prompt(ua, _("Done"));                       /* 17 */
      i = do_prompt(ua, "", _("Select parameter to modify"), NULL, 0);  

      /* For All Volumes, All Volumes from Pool, and Done, we don't need
           * a Volume record */
      if ( i != 12 && i != 13 && i != 17) {
         if (!select_media_dbr(ua, &mr)) {  /* Get Volume record */
            return 0;
         }
         ua->info_msg(_("Updating Volume \"%s\"\n"), mr.VolumeName);
      }
      switch (i) {
      case 0:                         /* Volume Status */
         /* Modify Volume Status */
         ua->info_msg(_("Current Volume status is: %s\n"), mr.VolStatus);
         start_prompt(ua, _("Possible Values are:\n"));
         add_prompt(ua, NT_("Append")); 
         add_prompt(ua, NT_("Archive"));
         add_prompt(ua, NT_("Disabled"));
         add_prompt(ua, NT_("Full"));
         add_prompt(ua, NT_("Used"));
         add_prompt(ua, NT_("Cleaning"));
         if (strcmp(mr.VolStatus, NT_("Purged")) == 0) {
            add_prompt(ua, NT_("Recycle"));
         }
         add_prompt(ua, NT_("Read-Only"));
         if (do_prompt(ua, "", _("Choose new Volume Status"), ua->cmd, sizeof(mr.VolStatus)) < 0) {
            return 1;
         }
         update_volstatus(ua, ua->cmd, &mr);
         break;
      case 1:                         /* Retention */
         ua->info_msg(_("Current retention period is: %s\n"),
            edit_utime(mr.VolRetention, ed1, sizeof(ed1)));
         if (!get_cmd(ua, _("Enter Volume Retention period: "))) {
            return 0;
         }
         update_volretention(ua, ua->cmd, &mr);
         break;

      case 2:                         /* Use Duration */
         ua->info_msg(_("Current use duration is: %s\n"),
            edit_utime(mr.VolUseDuration, ed1, sizeof(ed1)));
         if (!get_cmd(ua, _("Enter Volume Use Duration: "))) {
            return 0;
         }
         update_voluseduration(ua, ua->cmd, &mr);
         break;

      case 3:                         /* Max Jobs */
         ua->info_msg(_("Current max jobs is: %u\n"), mr.MaxVolJobs);
         if (!get_pint(ua, _("Enter new Maximum Jobs: "))) {
            return 0;
         }
         update_volmaxjobs(ua, ua->cmd, &mr);
         break;

      case 4:                         /* Max Files */
         ua->info_msg(_("Current max files is: %u\n"), mr.MaxVolFiles);
         if (!get_pint(ua, _("Enter new Maximum Files: "))) {
            return 0;
         }
         update_volmaxfiles(ua, ua->cmd, &mr);
         break;

      case 5:                         /* Max Bytes */
         ua->info_msg(_("Current value is: %s\n"), edit_uint64(mr.MaxVolBytes, ed1));
         if (!get_cmd(ua, _("Enter new Maximum Bytes: "))) {
            return 0;
         }
         update_volmaxbytes(ua, ua->cmd, &mr);
         break;


      case 6:                         /* Recycle */
         ua->info_msg(_("Current recycle flag is: %s\n"),
            mr.Recycle==1?_("yes"):_("no"));
         if (!get_yesno(ua, _("Enter new Recycle status: "))) {
            return 0;
         }
         update_volrecycle(ua, ua->cmd, &mr);
         break;

      case 7:                         /* Slot */
         ua->info_msg(_("Current Slot is: %d\n"), mr.Slot);
         if (!get_pint(ua, _("Enter new Slot: "))) {
            return 0;
         }
         update_volslot(ua, ua->cmd, &mr);
         break;
         
      case 8:                         /* InChanger */
         ua->info_msg(_("Current InChanger flag is: %d\n"), mr.InChanger);
         bsnprintf(buf, sizeof(buf), _("Set InChanger flag for Volume \"%s\": yes/no: "),
            mr.VolumeName);
         if (!get_yesno(ua, buf)) {
            return 0;
         }
         mr.InChanger = ua->pint32_val;
         /*
          * Make sure to use db_update... rather than doing this directly,
          *   so that any Slot is handled correctly.
          */
         set_storageid_in_mr(NULL, &mr);
         if (!db_update_media_record(ua->jcr, ua->db, &mr)) {
            ua->error_msg(_("Error updating media record Slot: ERR=%s"), db_strerror(ua->db));
         } else {
            ua->info_msg(_("New InChanger flag is: %d\n"), mr.InChanger);
         }
         break;


      case 9:                         /* Volume Files */
         int32_t VolFiles;
         ua->warning_msg(_("Warning changing Volume Files can result\n"
                        "in loss of data on your Volume\n\n"));
         ua->info_msg(_("Current Volume Files is: %u\n"), mr.VolFiles);
         if (!get_pint(ua, _("Enter new number of Files for Volume: "))) {
            return 0;
         }
         VolFiles = ua->pint32_val;
         if (VolFiles != (int)(mr.VolFiles + 1)) {
            ua->warning_msg(_("Normally, you should only increase Volume Files by one!\n"));
            if (!get_yesno(ua, _("Increase Volume Files? (yes/no): ")) || ua->pint32_val == 0) {
               break;
            }
         }
         query = get_pool_memory(PM_MESSAGE);
         Mmsg(query, "UPDATE Media SET VolFiles=%u WHERE MediaId=%s",
            VolFiles, edit_int64(mr.MediaId, ed1));
         if (!db_sql_query(ua->db, query, NULL, NULL)) {
            ua->error_msg("%s", db_strerror(ua->db));
         } else {
            ua->info_msg(_("New Volume Files is: %u\n"), VolFiles);
         }
         free_pool_memory(query);
         break;

      case 10:                        /* Volume's Pool */
         memset(&pr, 0, sizeof(POOL_DBR));
         pr.PoolId = mr.PoolId;
         if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
            ua->error_msg("%s", db_strerror(ua->db));
            return 0;
         }
         ua->info_msg(_("Current Pool is: %s\n"), pr.Name);
         if (!get_cmd(ua, _("Enter new Pool name: "))) {
            return 0;
         }
         update_vol_pool(ua, ua->cmd, &mr, &pr);
         return 1;

      case 11:
         update_vol_from_pool(ua, &mr);
         return 1;
      case 12:
         pool = select_pool_resource(ua);
         if (pool) {
            update_all_vols_from_pool(ua, pool->name());
         }
         return 1;

      case 13:
         update_all_vols(ua);
         return 1;

      case 14:
         ua->info_msg(_("Current Enabled is: %d\n"), mr.Enabled);
         if (!get_cmd(ua, _("Enter new Enabled: "))) {
            return 0;
         }
         if (strcasecmp(ua->cmd, "yes") == 0 || strcasecmp(ua->cmd, "true") == 0) {
            mr.Enabled = 1;
         } else if (strcasecmp(ua->cmd, "no") == 0 || strcasecmp(ua->cmd, "false") == 0) {
            mr.Enabled = 0;
         } else if (strcasecmp(ua->cmd, "archived") == 0) { 
            mr.Enabled = 2;
         } else {
            mr.Enabled = atoi(ua->cmd);
         }
         update_volenabled(ua, ua->cmd, &mr);
         break;

      case 15:
         memset(&pr, 0, sizeof(POOL_DBR));
         pr.PoolId = mr.RecyclePoolId;
         if (db_get_pool_record(ua->jcr, ua->db, &pr)) {
            ua->info_msg(_("Current RecyclePool is: %s\n"), pr.Name);
         } else {
            ua->info_msg(_("No current RecyclePool\n"));
         }
         if (!select_pool_dbr(ua, &pr, NT_("recyclepool"))) {
            return 0;
         }
         update_vol_recyclepool(ua, pr.Name, &mr);
         return 1;

      case 16:
         pm_strcpy(ret, "");
         ua->info_msg(_("Current ActionOnPurge is: %s\n"), 
                      action_on_purge_to_string(mr.ActionOnPurge, ret));
         if (!get_cmd(ua, _("Enter new ActionOnPurge (one of: Truncate, None): "))) {
            return 0;
         }

         update_vol_actiononpurge(ua, ua->cmd, &mr);
         break;

      default:                        /* Done or error */
         ua->info_msg(_("Selection terminated.\n"));
         return 1;
      }
   }
   return 1;
}

/*
 * Update long term statistics
 */
static bool update_stats(UAContext *ua)
{
   int i = find_arg_with_value(ua, NT_("days"));
   utime_t since=0;

   if (i >= 0) {
      since = atoi(ua->argv[i]) * 24*60*60;
   }

   int nb = db_update_stats(ua->jcr, ua->db, since);
   ua->info_msg(_("Updating %i job(s).\n"), nb);

   return true;
}

/*
 * Update pool record -- pull info from current POOL resource
 */
static bool update_pool(UAContext *ua)
{
   POOL_DBR  pr;
   int id;
   POOL *pool;
   POOLMEM *query;
   char ed1[50];

   pool = get_pool_resource(ua);
   if (!pool) {
      return false;
   }

   memset(&pr, 0, sizeof(pr));
   bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));
   if (!get_pool_dbr(ua, &pr)) {
      return false;
   }

   set_pooldbr_from_poolres(&pr, pool, POOL_OP_UPDATE); /* update */
   set_pooldbr_references(ua->jcr, ua->db, &pr, pool);

   id = db_update_pool_record(ua->jcr, ua->db, &pr);
   if (id <= 0) {
      ua->error_msg(_("db_update_pool_record returned %d. ERR=%s\n"),
         id, db_strerror(ua->db));
   }
   query = get_pool_memory(PM_MESSAGE);
   Mmsg(query, list_pool, edit_int64(pr.PoolId, ed1));
   db_list_sql_query(ua->jcr, ua->db, query, prtit, ua, 1, HORZ_LIST);
   free_pool_memory(query);
   ua->info_msg(_("Pool DB record updated from resource.\n"));
   return true;
}

/*
 * Update a Job record -- allows you to change the
 *  date fields in a Job record. This helps when
 *  providing migration from other vendors.
 */
static bool update_job(UAContext *ua)
{
   int i;
   char ed1[50], ed2[50];
   POOL_MEM cmd(PM_MESSAGE);
   JOB_DBR jr;
   CLIENT_DBR cr;
   utime_t StartTime;
   char *client_name = NULL;
   char *start_time = NULL;
   const char *kw[] = {
      NT_("starttime"),                   /* 0 */
      NT_("client"),                      /* 1 */
      NULL };

   Dmsg1(200, "cmd=%s\n", ua->cmd);
   i = find_arg_with_value(ua, NT_("jobid"));
   if (i < 0) {
      ua->error_msg(_("Expect JobId keyword, not found.\n"));
      return false;
   }
   memset(&jr, 0, sizeof(jr));
   memset(&cr, 0, sizeof(cr));
   jr.JobId = str_to_int64(ua->argv[i]);
   if (!db_get_job_record(ua->jcr, ua->db, &jr)) {
      ua->error_msg("%s", db_strerror(ua->db));
      return false;
   }

   for (i=0; kw[i]; i++) {
      int j;
      if ((j=find_arg_with_value(ua, kw[i])) >= 0) {
         switch (i) {
         case 0:                         /* start time */
            start_time = ua->argv[j];
            break;
         case 1:                         /* Client name */
            client_name = ua->argv[j];
            break;
         }
      }
   }
   if (!client_name && !start_time) {
      ua->error_msg(_("Neither Client nor StartTime specified.\n"));
      return 0;
   }
   if (client_name) {
      if (!get_client_dbr(ua, &cr)) {
         return false;
      }
      jr.ClientId = cr.ClientId;
   }
   if (start_time) {
      utime_t delta_start;

      StartTime = str_to_utime(start_time);
      if (StartTime == 0) {
         ua->error_msg(_("Improper date format: %s\n"), ua->argv[i]);
         return false;
      }
      delta_start = StartTime - jr.StartTime;
      Dmsg3(200, "ST=%lld jr.ST=%lld delta=%lld\n", StartTime, 
            (utime_t)jr.StartTime, delta_start);
      jr.StartTime = (time_t)StartTime;
      jr.SchedTime += (time_t)delta_start;
      jr.EndTime += (time_t)delta_start;
      jr.JobTDate += delta_start;
      /* Convert to DB times */
      bstrutime(jr.cStartTime, sizeof(jr.cStartTime), jr.StartTime);
      bstrutime(jr.cSchedTime, sizeof(jr.cSchedTime), jr.SchedTime);
      bstrutime(jr.cEndTime, sizeof(jr.cEndTime), jr.EndTime);
   }
   Mmsg(cmd, "UPDATE Job SET ClientId=%s,StartTime='%s',SchedTime='%s',"
             "EndTime='%s',JobTDate=%s WHERE JobId=%s", 
             edit_int64(jr.ClientId, ed1), 
             jr.cStartTime,
             jr.cSchedTime,
             jr.cEndTime,
             edit_uint64(jr.JobTDate, ed1), 
             edit_int64(jr.JobId, ed2));
   if (!db_sql_query(ua->db, cmd.c_str(), NULL, NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
      return false;
   }
   return true;
}
