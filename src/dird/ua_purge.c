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
 *   Bacula Director -- User Agent Database Purge Command
 *
 *      Purges Files from specific JobIds
 * or
 *      Purges Jobs from Volumes
 *
 *     Kern Sibbald, February MMII
 *
 */

#include "bacula.h"
#include "dird.h"

/* Forward referenced functions */
static int purge_files_from_client(UAContext *ua, CLIENT *client);
static int purge_jobs_from_client(UAContext *ua, CLIENT *client);
static int action_on_purge_cmd(UAContext *ua, const char *cmd);

static const char *select_jobsfiles_from_client =
   "SELECT JobId FROM Job "
   "WHERE ClientId=%s "
   "AND PurgedFiles=0";

static const char *select_jobs_from_client =
   "SELECT JobId, PurgedFiles FROM Job "
   "WHERE ClientId=%s";

/*
 *   Purge records from database
 *
 *     Purge Files (from) [Job|JobId|Client|Volume]
 *     Purge Jobs  (from) [Client|Volume]
 *
 *  N.B. Not all above is implemented yet.
 */
int purgecmd(UAContext *ua, const char *cmd)
{
   int i;
   CLIENT *client;
   MEDIA_DBR mr;
   JOB_DBR  jr;
   static const char *keywords[] = {
      NT_("files"),
      NT_("jobs"),
      NT_("volume"),
      NULL};

   static const char *files_keywords[] = {
      NT_("Job"),
      NT_("JobId"),
      NT_("Client"),
      NT_("Volume"),
      NULL};

   static const char *jobs_keywords[] = {
      NT_("Client"),
      NT_("Volume"),
      NULL};

   ua->warning_msg(_(
      "\nThis command can be DANGEROUS!!!\n\n"
      "It purges (deletes) all Files from a Job,\n"
      "JobId, Client or Volume; or it purges (deletes)\n"
      "all Jobs from a Client or Volume without regard\n"
      "to retention periods. Normally you should use the\n"
      "PRUNE command, which respects retention periods.\n"));

   if (!open_db(ua)) {
      return 1;
   }
   switch (find_arg_keyword(ua, keywords)) {
   /* Files */
   case 0:
      switch(find_arg_keyword(ua, files_keywords)) {
      case 0:                         /* Job */
      case 1:                         /* JobId */
         if (get_job_dbr(ua, &jr)) {
            char jobid[50];
            edit_int64(jr.JobId, jobid);
            purge_files_from_jobs(ua, jobid);
         }
         return 1;
      case 2:                         /* client */
         client = get_client_resource(ua);
         if (client) {
            purge_files_from_client(ua, client);
         }
         return 1;
      case 3:                         /* Volume */
         if (select_media_dbr(ua, &mr)) {
            purge_files_from_volume(ua, &mr);
         }
         return 1;
      }
   /* Jobs */
   case 1:
      switch(find_arg_keyword(ua, jobs_keywords)) {
      case 0:                         /* client */
         client = get_client_resource(ua);
         if (client) {
            purge_jobs_from_client(ua, client);
         }
         return 1;
      case 1:                         /* Volume */
         if (select_media_dbr(ua, &mr)) {
            purge_jobs_from_volume(ua, &mr, /*force*/true);
         }
         return 1;
      }
   /* Volume */
   case 2:
      /* Perform ActionOnPurge (action=truncate) */
      if (find_arg(ua, "action") >= 0) {
         return action_on_purge_cmd(ua, ua->cmd);
      }

      while ((i=find_arg(ua, NT_("volume"))) >= 0) {
         if (select_media_dbr(ua, &mr)) {
            purge_jobs_from_volume(ua, &mr, /*force*/true);
         }
         *ua->argk[i] = 0;            /* zap keyword already seen */
         ua->send_msg("\n");
      }
      return 1;
   default:
      break;
   }
   switch (do_keyword_prompt(ua, _("Choose item to purge"), keywords)) {
   case 0:                            /* files */
      client = get_client_resource(ua);
      if (client) {
         purge_files_from_client(ua, client);
      }
      break;
   case 1:                            /* jobs */
      client = get_client_resource(ua);
      if (client) {
         purge_jobs_from_client(ua, client);
      }
      break;
   case 2:                            /* Volume */
      if (select_media_dbr(ua, &mr)) {
         purge_jobs_from_volume(ua, &mr, /*force*/true);
      }
      break;
   }
   return 1;
}

/*
 * Purge File records from the database. For any Job which
 * is older than the retention period, we unconditionally delete
 * all File records for that Job.  This is simple enough that no
 * temporary tables are needed. We simply make an in memory list of
 * the JobIds meeting the prune conditions, then delete all File records
 * pointing to each of those JobIds.
 */
static int purge_files_from_client(UAContext *ua, CLIENT *client)
{
   struct del_ctx del;
   POOL_MEM query(PM_MESSAGE);
   CLIENT_DBR cr;
   char ed1[50];

   memset(&cr, 0, sizeof(cr));
   bstrncpy(cr.Name, client->name(), sizeof(cr.Name));
   if (!db_create_client_record(ua->jcr, ua->db, &cr)) {
      return 0;
   }

   memset(&del, 0, sizeof(del));
   del.max_ids = 1000;
   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);

   ua->info_msg(_("Begin purging files for Client \"%s\"\n"), cr.Name);

   Mmsg(query, select_jobsfiles_from_client, edit_int64(cr.ClientId, ed1));
   Dmsg1(050, "select sql=%s\n", query.c_str());
   db_sql_query(ua->db, query.c_str(), file_delete_handler, (void *)&del);

   purge_files_from_job_list(ua, del);

   if (del.num_ids == 0) {
      ua->warning_msg(_("No Files found for client %s to purge from %s catalog.\n"),
         client->name(), client->catalog->name());
   } else {
      ua->info_msg(_("Files for %d Jobs for client \"%s\" purged from %s catalog.\n"), del.num_ids,
         client->name(), client->catalog->name());
   }

   if (del.JobId) {
      free(del.JobId);
   }
   return 1;
}



/*
 * Purge Job records from the database. For any Job which
 * is older than the retention period, we unconditionally delete
 * it and all File records for that Job.  This is simple enough that no
 * temporary tables are needed. We simply make an in memory list of
 * the JobIds then delete the Job, Files, and JobMedia records in that list.
 */
static int purge_jobs_from_client(UAContext *ua, CLIENT *client)
{
   struct del_ctx del;
   POOL_MEM query(PM_MESSAGE);
   CLIENT_DBR cr;
   char ed1[50];

   memset(&cr, 0, sizeof(cr));

   bstrncpy(cr.Name, client->name(), sizeof(cr.Name));
   if (!db_create_client_record(ua->jcr, ua->db, &cr)) {
      return 0;
   }

   memset(&del, 0, sizeof(del));
   del.max_ids = 1000;
   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);
   del.PurgedFiles = (char *)malloc(del.max_ids);
   
   ua->info_msg(_("Begin purging jobs from Client \"%s\"\n"), cr.Name);

   Mmsg(query, select_jobs_from_client, edit_int64(cr.ClientId, ed1));
   Dmsg1(150, "select sql=%s\n", query.c_str());
   db_sql_query(ua->db, query.c_str(), job_delete_handler, (void *)&del);

   purge_job_list_from_catalog(ua, del);

   if (del.num_ids == 0) {
      ua->warning_msg(_("No Files found for client %s to purge from %s catalog.\n"),
         client->name(), client->catalog->name());
   } else {
      ua->info_msg(_("%d Jobs for client %s purged from %s catalog.\n"), del.num_ids,
         client->name(), client->catalog->name());
   }

   if (del.JobId) {
      free(del.JobId);
   }
   if (del.PurgedFiles) {
      free(del.PurgedFiles);
   }
   return 1;
}


/*
 * Remove File records from a list of JobIds
 */
void purge_files_from_jobs(UAContext *ua, char *jobs)
{
   POOL_MEM query(PM_MESSAGE);

   Mmsg(query, "DELETE FROM File WHERE JobId IN (%s)", jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);
   Dmsg1(050, "Delete File sql=%s\n", query.c_str());

   Mmsg(query, "DELETE FROM BaseFiles WHERE JobId IN (%s)", jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);
   Dmsg1(050, "Delete BaseFiles sql=%s\n", query.c_str());

   /*
    * Now mark Job as having files purged. This is necessary to
    * avoid having too many Jobs to process in future prunings. If
    * we don't do this, the number of JobId's in our in memory list
    * could grow very large.
    */
   Mmsg(query, "UPDATE Job SET PurgedFiles=1 WHERE JobId IN (%s)", jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);
   Dmsg1(050, "Mark purged sql=%s\n", query.c_str());
}

/*
 * Delete jobs (all records) from the catalog in groups of 1000
 *  at a time.
 */
void purge_job_list_from_catalog(UAContext *ua, del_ctx &del)
{
   POOL_MEM jobids(PM_MESSAGE);
   char ed1[50];

   for (int i=0; del.num_ids; ) {
      Dmsg1(150, "num_ids=%d\n", del.num_ids);
      pm_strcat(jobids, "");
      for (int j=0; j<1000 && del.num_ids>0; j++) {
         del.num_ids--;
         if (del.JobId[i] == 0 || ua->jcr->JobId == del.JobId[i]) {
            Dmsg2(150, "skip JobId[%d]=%d\n", i, (int)del.JobId[i]);
            i++;
            continue;
         }
         if (*jobids.c_str() != 0) {
            pm_strcat(jobids, ",");
         }
         pm_strcat(jobids, edit_int64(del.JobId[i++], ed1));
         Dmsg1(150, "Add id=%s\n", ed1);
         del.num_del++;
      }
      Dmsg1(150, "num_ids=%d\n", del.num_ids);
      purge_jobs_from_catalog(ua, jobids.c_str());
   }
}

/*
 * Delete files from a list of jobs in groups of 1000
 *  at a time.
 */
void purge_files_from_job_list(UAContext *ua, del_ctx &del)
{
   POOL_MEM jobids(PM_MESSAGE);
   char ed1[50];
   /*
    * OK, now we have the list of JobId's to be pruned, send them
    *   off to be deleted batched 1000 at a time.
    */
   for (int i=0; del.num_ids; ) {
      pm_strcat(jobids, "");
      for (int j=0; j<1000 && del.num_ids>0; j++) {
         del.num_ids--;
         if (del.JobId[i] == 0 || ua->jcr->JobId == del.JobId[i]) {
            Dmsg2(150, "skip JobId[%d]=%d\n", i, (int)del.JobId[i]);
            i++;
            continue;
         }
         if (*jobids.c_str() != 0) {
            pm_strcat(jobids, ",");
         }
         pm_strcat(jobids, edit_int64(del.JobId[i++], ed1));
         Dmsg1(150, "Add id=%s\n", ed1);
         del.num_del++;
      }
      purge_files_from_jobs(ua, jobids.c_str());
   }
}

/*
 * Change the type of the next copy job to backup.
 * We need to upgrade the next copy of a normal job,
 * and also upgrade the next copy when the normal job
 * already have been purged.
 *
 *   JobId: 1   PriorJobId: 0    (original)
 *   JobId: 2   PriorJobId: 1    (first copy)
 *   JobId: 3   PriorJobId: 1    (second copy)
 *
 *   JobId: 2   PriorJobId: 1    (first copy, now regular backup)
 *   JobId: 3   PriorJobId: 1    (second copy)
 *
 *  => Search through PriorJobId in jobid and
 *                    PriorJobId in PriorJobId (jobid)
 */
void upgrade_copies(UAContext *ua, char *jobs)
{
   POOL_MEM query(PM_MESSAGE);
   
   db_lock(ua->db);

   /* Do it in two times for mysql */
   Mmsg(query, uap_upgrade_copies_oldest_job[db_get_type_index(ua->db)], JT_JOB_COPY, jobs, jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);
   Dmsg1(050, "Upgrade copies Log sql=%s\n", query.c_str());

   /* Now upgrade first copy to Backup */
   Mmsg(query, "UPDATE Job SET Type='B' "      /* JT_JOB_COPY => JT_BACKUP  */
                "WHERE JobId IN ( SELECT JobId FROM cpy_tmp )");

   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);

   Mmsg(query, "DROP TABLE cpy_tmp");
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);

   db_unlock(ua->db);
}

/*
 * Remove all records from catalog for a list of JobIds
 */
void purge_jobs_from_catalog(UAContext *ua, char *jobs)
{
   POOL_MEM query(PM_MESSAGE);

   /* Delete (or purge) records associated with the job */
   purge_files_from_jobs(ua, jobs);

   Mmsg(query, "DELETE FROM JobMedia WHERE JobId IN (%s)", jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);
   Dmsg1(050, "Delete JobMedia sql=%s\n", query.c_str());

   Mmsg(query, "DELETE FROM Log WHERE JobId IN (%s)", jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);
   Dmsg1(050, "Delete Log sql=%s\n", query.c_str());

   Mmsg(query, "DELETE FROM RestoreObject WHERE JobId IN (%s)", jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);
   Dmsg1(050, "Delete RestoreObject sql=%s\n", query.c_str());

   Mmsg(query, "DELETE FROM PathVisibility WHERE JobId IN (%s)", jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);
   Dmsg1(050, "Delete PathVisibility sql=%s\n", query.c_str());

   upgrade_copies(ua, jobs);

   /* Now remove the Job record itself */
   Mmsg(query, "DELETE FROM Job WHERE JobId IN (%s)", jobs);
   db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL);

   Dmsg1(050, "Delete Job sql=%s\n", query.c_str());
}

void purge_files_from_volume(UAContext *ua, MEDIA_DBR *mr )
{} /* ***FIXME*** implement */

/*
 * Returns: 1 if Volume purged
 *          0 if Volume not purged
 */
bool purge_jobs_from_volume(UAContext *ua, MEDIA_DBR *mr, bool force)
{
   POOL_MEM query(PM_MESSAGE);
   db_list_ctx lst;
   char *jobids=NULL;
   int i;
   bool purged = false;
   bool stat;

   stat = strcmp(mr->VolStatus, "Append") == 0 ||
          strcmp(mr->VolStatus, "Full")   == 0 ||
          strcmp(mr->VolStatus, "Used")   == 0 ||
          strcmp(mr->VolStatus, "Error")  == 0;
   if (!stat) {
      ua->error_msg(_("\nVolume \"%s\" has VolStatus \"%s\" and cannot be purged.\n"
                     "The VolStatus must be: Append, Full, Used, or Error to be purged.\n"),
                     mr->VolumeName, mr->VolStatus);
      return 0;
   }

   /*
    * Check if he wants to purge a single jobid
    */
   i = find_arg_with_value(ua, "jobid");
   if (i >= 0 && is_a_number_list(ua->argv[i])) {
      jobids = ua->argv[i];
   } else {
      /*
       * Purge ALL JobIds
       */
      if (!db_get_volume_jobids(ua->jcr, ua->db, mr, &lst)) {
         ua->error_msg("%s", db_strerror(ua->db));
         Dmsg0(050, "Count failed\n");
         goto bail_out;
      }
      jobids = lst.list;
   }

   if (*jobids) {
      purge_jobs_from_catalog(ua, jobids);
   }

   ua->info_msg(_("%d File%s on Volume \"%s\" purged from catalog.\n"), 
                lst.count, lst.count<=1?"":"s", mr->VolumeName);

   purged = is_volume_purged(ua, mr, force); 

bail_out:
   return purged;
}

/*
 * This routine will check the JobMedia records to see if the
 *   Volume has been purged. If so, it marks it as such and
 *
 * Returns: true if volume purged
 *          false if not
 *
 * Note, we normally will not purge a volume that has Firstor LastWritten
 *   zero, because it means the volume is most likely being written
 *   however, if the user manually purges using the purge command in
 *   the console, he has been warned, and we go ahead and purge
 *   the volume anyway, if possible).
 */
bool is_volume_purged(UAContext *ua, MEDIA_DBR *mr, bool force)
{
   POOL_MEM query(PM_MESSAGE);
   struct s_count_ctx cnt;
   bool purged = false;
   char ed1[50];

   if (!force && (mr->FirstWritten == 0 || mr->LastWritten == 0)) {
      goto bail_out;               /* not written cannot purge */
   }

   if (strcmp(mr->VolStatus, "Purged") == 0) {
      purged = true;
      goto bail_out;
   }

   /* If purged, mark it so */
   cnt.count = 0;
   Mmsg(query, "SELECT 1 FROM JobMedia WHERE MediaId=%s LIMIT 1", 
        edit_int64(mr->MediaId, ed1));
   if (!db_sql_query(ua->db, query.c_str(), del_count_handler, (void *)&cnt)) {
      ua->error_msg("%s", db_strerror(ua->db));
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }

   if (cnt.count == 0) {
      ua->warning_msg(_("There are no more Jobs associated with Volume \"%s\". Marking it purged.\n"),
         mr->VolumeName);
      if (!(purged = mark_media_purged(ua, mr))) {
         ua->error_msg("%s", db_strerror(ua->db));
      }
   }
bail_out:
   return purged;
}

static BSOCK *open_sd_bsock(UAContext *ua)
{
   STORE *store = ua->jcr->wstore;

   if (!ua->jcr->store_bsock) {
      ua->send_msg(_("Connecting to Storage daemon %s at %s:%d ...\n"),
         store->name(), store->address, store->SDport);
      if (!connect_to_storage_daemon(ua->jcr, 10, SDConnectTimeout, 1)) {
         ua->error_msg(_("Failed to connect to Storage daemon.\n"));
         return NULL;
      }
   }
   return ua->jcr->store_bsock;
}

/* 
 * Called here to send the appropriate commands to the SD
 *  to do truncate on purge.
 */
static void do_truncate_on_purge(UAContext *ua, MEDIA_DBR *mr, 
                                 char *pool, char *storage,
                                 int drive, BSOCK *sd)
{
   int dvd;
   bool ok=false;
   uint64_t VolBytes = 0;
   
   /* TODO: Return if not mr->Recyle ? */
   if (!mr->Recycle) {
      return;
   }

   /* Do it only if action on purge = truncate is set */
   if (!(mr->ActionOnPurge & ON_PURGE_TRUNCATE)) {
      return;
   }
   /*
    * Send the command to truncate the volume after purge. If this feature
    * is disabled for the specific device, this will be a no-op.
    */

   /* Protect us from spaces */
   bash_spaces(mr->VolumeName);
   bash_spaces(mr->MediaType);
   bash_spaces(pool);
   bash_spaces(storage);
      
   /* Do it by relabeling the Volume, which truncates it */
   sd->fsend("relabel %s OldName=%s NewName=%s PoolName=%s "
             "MediaType=%s Slot=%d drive=%d\n",
                storage,
                mr->VolumeName, mr->VolumeName,
                pool, mr->MediaType, mr->Slot, drive);
      
   unbash_spaces(mr->VolumeName);
   unbash_spaces(mr->MediaType);
   unbash_spaces(pool);
   unbash_spaces(storage);

   /* Send relabel command, and check for valid response */
   while (sd->recv() >= 0) {
      ua->send_msg("%s", sd->msg);
      if (sscanf(sd->msg, "3000 OK label. VolBytes=%llu DVD=%d ", &VolBytes, &dvd) == 2) {
         ok = true;
      }
   }

   if (ok) {
      mr->VolBytes = VolBytes;
      mr->VolFiles = 0;
      set_storageid_in_mr(NULL, mr);
      if (!db_update_media_record(ua->jcr, ua->db, mr)) {
         ua->error_msg(_("Can't update volume size in the catalog\n"));
      }
      ua->send_msg(_("The volume \"%s\" has been truncated\n"), mr->VolumeName);
   } else {
      ua->warning_msg(_("Unable to truncate volume \"%s\"\n"), mr->VolumeName);
   }
}

/* 
 * Implement Bacula bconsole command  purge action
 *     purge action= pool= volume= storage= devicetype= 
 */
static int action_on_purge_cmd(UAContext *ua, const char *cmd)
{
   bool allpools = false;
   int drive = -1;
   int nb = 0;
   uint32_t *results = NULL;
   const char *action = "all";
   STORE *store = NULL;
   POOL *pool = NULL;
   MEDIA_DBR mr;
   POOL_DBR pr;
   BSOCK *sd = NULL;
   
   memset(&pr, 0, sizeof(pr));

   /* Look at arguments */
   for (int i=1; i<ua->argc; i++) {
      if (strcasecmp(ua->argk[i], NT_("allpools")) == 0) {
         allpools = true;
            
      } else if (strcasecmp(ua->argk[i], NT_("volume")) == 0 
                 && is_name_valid(ua->argv[i], NULL)) {
         bstrncpy(mr.VolumeName, ua->argv[i], sizeof(mr.VolumeName));

      } else if (strcasecmp(ua->argk[i], NT_("devicetype")) == 0 
                 && ua->argv[i]) {
         bstrncpy(mr.MediaType, ua->argv[i], sizeof(mr.MediaType));
         
      } else if (strcasecmp(ua->argk[i], NT_("drive")) == 0 && ua->argv[i]) {
         drive = atoi(ua->argv[i]);

      } else if (strcasecmp(ua->argk[i], NT_("action")) == 0 
                 && is_name_valid(ua->argv[i], NULL)) {
         action=ua->argv[i];
      }
   }

   /* Choose storage */
   ua->jcr->wstore = store =  get_storage_resource(ua, false);
   if (!store) {
      goto bail_out;
   }

   if (!open_db(ua)) {
      Dmsg0(100, "Can't open db\n");
      goto bail_out;
   }

   if (!allpools) {
      /* force pool selection */
      pool = get_pool_resource(ua);
      if (!pool) {
         Dmsg0(100, "Can't get pool resource\n");
         goto bail_out;
      }
      bstrncpy(pr.Name, pool->name(), sizeof(pr.Name));
      if (!db_get_pool_record(ua->jcr, ua->db, &pr)) {
         Dmsg0(100, "Can't get pool record\n");
         goto bail_out;
      }
      mr.PoolId = pr.PoolId;
   }

   /* 
    * Look for all Purged volumes that can be recycled, are enabled and
    *  have more the 10,000 bytes.
    */
   mr.Recycle = 1;
   mr.Enabled = 1;
   mr.VolBytes = 10000;
   set_storageid_in_mr(store, &mr);
   bstrncpy(mr.VolStatus, "Purged", sizeof(mr.VolStatus));
   if (!db_get_media_ids(ua->jcr, ua->db, &mr, &nb, &results)) {
      Dmsg0(100, "No results from db_get_media_ids\n");
      goto bail_out;
   }
   
   if (!nb) {
      ua->send_msg(_("No Volumes found to perform %s action.\n"), action);
      goto bail_out;
   }

   if ((sd=open_sd_bsock(ua)) == NULL) {
      Dmsg0(100, "Can't open connection to sd\n");
      goto bail_out;
   }

   /*
    * Loop over the candidate Volumes and actually truncate them
    */
   for (int i=0; i < nb; i++) {
      mr.clear();
      mr.MediaId = results[i];
      if (db_get_media_record(ua->jcr, ua->db, &mr)) {         
         /* TODO: ask for drive and change Pool */
         if (!strcasecmp("truncate", action) || !strcasecmp("all", action)) {
            do_truncate_on_purge(ua, &mr, pr.Name, store->dev_name(), drive, sd);
         }
      } else {
         Dmsg1(0, "Can't find MediaId=%lld\n", (uint64_t) mr.MediaId);
      }
   }

bail_out:
   close_db(ua);
   if (sd) {
      sd->signal(BNET_TERMINATE);
      sd->close();
      ua->jcr->store_bsock = NULL;
   }
   ua->jcr->wstore = NULL;
   if (results) {
      free(results);
   }

   return 1;
}

/*
 * IF volume status is Append, Full, Used, or Error, mark it Purged
 *   Purged volumes can then be recycled (if enabled).
 */
bool mark_media_purged(UAContext *ua, MEDIA_DBR *mr)
{
   JCR *jcr = ua->jcr;
   if (strcmp(mr->VolStatus, "Append") == 0 ||
       strcmp(mr->VolStatus, "Full")   == 0 ||
       strcmp(mr->VolStatus, "Used")   == 0 ||
       strcmp(mr->VolStatus, "Error")  == 0) {
      bstrncpy(mr->VolStatus, "Purged", sizeof(mr->VolStatus));
      set_storageid_in_mr(NULL, mr);
      if (!db_update_media_record(jcr, ua->db, mr)) {
         return false;
      }
      pm_strcpy(jcr->VolumeName, mr->VolumeName);
      generate_job_event(jcr, "VolumePurged");
      generate_plugin_event(jcr, bDirEventVolumePurged);
      /*
       * If the RecyclePool is defined, move the volume there
       */
      if (mr->RecyclePoolId && mr->RecyclePoolId != mr->PoolId) {
         POOL_DBR oldpr, newpr;
         memset(&oldpr, 0, sizeof(POOL_DBR));
         memset(&newpr, 0, sizeof(POOL_DBR));
         newpr.PoolId = mr->RecyclePoolId;
         oldpr.PoolId = mr->PoolId;
         if (   db_get_pool_record(jcr, ua->db, &oldpr) 
             && db_get_pool_record(jcr, ua->db, &newpr)) 
         {
            /* check if destination pool size is ok */
            if (newpr.MaxVols > 0 && newpr.NumVols >= newpr.MaxVols) {
               ua->error_msg(_("Unable move recycled Volume in full " 
                              "Pool \"%s\" MaxVols=%d\n"),
                        newpr.Name, newpr.MaxVols);

            } else {            /* move media */
               update_vol_pool(ua, newpr.Name, mr, &oldpr);
            }
         } else {
            ua->error_msg("%s", db_strerror(ua->db));
         }
      }

      /* Send message to Job report, if it is a *real* job */           
      if (jcr && jcr->JobId > 0) {
         Jmsg(jcr, M_INFO, 0, _("All records pruned from Volume \"%s\"; marking it \"Purged\"\n"),
            mr->VolumeName); 
      }
      return true;
   } else {
      ua->error_msg(_("Cannot purge Volume with VolStatus=%s\n"), mr->VolStatus);
   }
   return strcmp(mr->VolStatus, "Purged") == 0;
}
