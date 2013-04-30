/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2002-2009 Free Software Foundation Europe e.V.

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
 *   Bacula Director -- User Agent Database prune Command
 *      Applies retention periods
 *
 *     Kern Sibbald, February MMII
 *
 */

#include "bacula.h"
#include "dird.h"

/* Imported functions */

/* Forward referenced functions */
static bool grow_del_list(struct del_ctx *del);

/*
 * Called here to count entries to be deleted
 */
int del_count_handler(void *ctx, int num_fields, char **row)
{
   struct s_count_ctx *cnt = (struct s_count_ctx *)ctx;

   if (row[0]) {
      cnt->count = str_to_int64(row[0]);
   } else {
      cnt->count = 0;
   }
   return 0;
}


/*
 * Called here to make in memory list of JobIds to be
 *  deleted and the associated PurgedFiles flag.
 *  The in memory list will then be transversed
 *  to issue the SQL DELETE commands.  Note, the list
 *  is allowed to get to MAX_DEL_LIST_LEN to limit the
 *  maximum malloc'ed memory.
 */
int job_delete_handler(void *ctx, int num_fields, char **row)
{
   struct del_ctx *del = (struct del_ctx *)ctx;

   if (!grow_del_list(del)) {
      return 1;
   }
   del->JobId[del->num_ids] = (JobId_t)str_to_int64(row[0]);
   Dmsg2(60, "job_delete_handler row=%d val=%d\n", del->num_ids, del->JobId[del->num_ids]);
   del->PurgedFiles[del->num_ids++] = (char)str_to_int64(row[1]);
   return 0;
}

int file_delete_handler(void *ctx, int num_fields, char **row)
{
   struct del_ctx *del = (struct del_ctx *)ctx;

   if (!grow_del_list(del)) {
      return 1;
   }
   del->JobId[del->num_ids++] = (JobId_t)str_to_int64(row[0]);
// Dmsg2(150, "row=%d val=%d\n", del->num_ids-1, del->JobId[del->num_ids-1]);
   return 0;
}

/*
 *   Prune records from database
 *
 *    prune files (from) client=xxx [pool=yyy]
 *    prune jobs (from) client=xxx [pool=yyy]
 *    prune volume=xxx
 *    prune stats
 */
int prunecmd(UAContext *ua, const char *cmd)
{
   DIRRES *dir;
   CLIENT *client;
   POOL *pool;
   POOL_DBR pr;
   MEDIA_DBR mr;
   utime_t retention;
   int kw;

   static const char *keywords[] = {
      NT_("Files"),
      NT_("Jobs"),
      NT_("Volume"),
      NT_("Stats"),
      NULL};

   if (!open_client_db(ua)) {
      return false;
   }

   /* First search args */
   kw = find_arg_keyword(ua, keywords);
   if (kw < 0 || kw > 3) {
      /* no args, so ask user */
      kw = do_keyword_prompt(ua, _("Choose item to prune"), keywords);
   }

   switch (kw) {
   case 0:  /* prune files */
      if (!(client = get_client_resource(ua))) {
         return false;
      }
      if (find_arg_with_value(ua, "pool") >= 0) {
         pool = get_pool_resource(ua);
      } else {
         pool = NULL;
      }
      /* Pool File Retention takes precedence over client File Retention */
      if (pool && pool->FileRetention > 0) {
         if (!confirm_retention(ua, &pool->FileRetention, "File")) {
            return false;
         }
      } else if (!confirm_retention(ua, &client->FileRetention, "File")) {
         return false;
      }
      prune_files(ua, client, pool);
      return true;
   case 1:  /* prune jobs */
      if (!(client = get_client_resource(ua))) {
         return false;
      }
      if (find_arg_with_value(ua, "pool") >= 0) {
         pool = get_pool_resource(ua);
      } else {
         pool = NULL;
      }
      /* Pool Job Retention takes precedence over client Job Retention */
      if (pool && pool->JobRetention > 0) {
         if (!confirm_retention(ua, &pool->JobRetention, "Job")) {
            return false;
         }
      } else if (!confirm_retention(ua, &client->JobRetention, "Job")) {
         return false;
      }
      /* ****FIXME**** allow user to select JobType */
      prune_jobs(ua, client, pool, JT_BACKUP);
      return 1;
   case 2:  /* prune volume */
      if (!select_pool_and_media_dbr(ua, &pr, &mr)) {
         return false;
      }
      if (mr.Enabled == 2) {
         ua->error_msg(_("Cannot prune Volume \"%s\" because it is archived.\n"),
            mr.VolumeName);
         return false;
      }
      if (!confirm_retention(ua, &mr.VolRetention, "Volume")) {
         return false;
      }
      prune_volume(ua, &mr);
      return true;
   case 3:  /* prune stats */
      dir = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
      if (!dir->stats_retention) {
         return false;
      }
      retention = dir->stats_retention;
      if (!confirm_retention(ua, &retention, "Statistics")) {
         return false;
      }
      prune_stats(ua, retention);
      return true;
   default:
      break;
   }

   return true;
}

/* Prune Job stat records from the database. 
 *
 */
int prune_stats(UAContext *ua, utime_t retention)
{
   char ed1[50];
   POOL_MEM query(PM_MESSAGE);
   utime_t now = (utime_t)time(NULL);

   db_lock(ua->db);
   Mmsg(query, "DELETE FROM JobHisto WHERE JobTDate < %s", 
        edit_int64(now - retention, ed1));
   db_sql_query(ua->db, query.c_str(), NULL, NULL);
   db_unlock(ua->db);

   ua->info_msg(_("Pruned Jobs from JobHisto catalog.\n"));

   return true;
}

/* 
 * Use pool and client specified by user to select jobs to prune 
 * returns add_from string to add in FROM clause
 *         add_where string to add in WHERE clause
 */
bool prune_set_filter(UAContext *ua, CLIENT *client, POOL *pool, utime_t period,
                      POOL_MEM *add_from, POOL_MEM *add_where)
{
   utime_t now;
   char ed1[50], ed2[MAX_ESCAPE_NAME_LENGTH]; 
   POOL_MEM tmp(PM_MESSAGE);

   now = (utime_t)time(NULL);
   edit_int64(now - period, ed1);
   Dmsg3(150, "now=%lld period=%lld JobTDate=%s\n", now, period, ed1);
   Mmsg(tmp, " AND JobTDate < %s ", ed1);
   pm_strcat(*add_where, tmp.c_str());

   db_lock(ua->db);
   if (client) { 
      db_escape_string(ua->jcr, ua->db, ed2, 
                       client->name(), strlen(client->name()));
      Mmsg(tmp, " AND Client.Name = '%s' ", ed2);
      pm_strcat(*add_where, tmp.c_str());
      pm_strcat(*add_from, " JOIN Client USING (ClientId) ");
   }

   if (pool) { 
      db_escape_string(ua->jcr, ua->db, ed2, 
                       pool->name(), strlen(pool->name()));
      Mmsg(tmp, " AND Pool.Name = '%s' ", ed2);
      pm_strcat(*add_where, tmp.c_str());
      /* Use ON() instead of USING for some old SQLite */
      pm_strcat(*add_from, " JOIN Pool ON (Job.PoolId = Pool.PoolId) ");
   }
   Dmsg2(150, "f=%s w=%s\n", add_from->c_str(), add_where->c_str());
   db_unlock(ua->db);
   return true;
}

/*
 * Prune File records from the database. For any Job which
 * is older than the retention period, we unconditionally delete
 * all File records for that Job.  This is simple enough that no
 * temporary tables are needed. We simply make an in memory list of
 * the JobIds meeting the prune conditions, then delete all File records
 * pointing to each of those JobIds.
 *
 * This routine assumes you want the pruning to be done. All checking
 *  must be done before calling this routine.
 *
 * Note: client or pool can possibly be NULL (not both).
 */
int prune_files(UAContext *ua, CLIENT *client, POOL *pool)
{
   struct del_ctx del;
   struct s_count_ctx cnt;
   POOL_MEM query(PM_MESSAGE);
   POOL_MEM sql_where(PM_MESSAGE);
   POOL_MEM sql_from(PM_MESSAGE);
   utime_t period;
   char ed1[50];
   
   memset(&del, 0, sizeof(del));

   if (pool && pool->FileRetention > 0) {
      period = pool->FileRetention;

   } else if (client) {
      period = client->FileRetention;

   } else {                     /* should specify at least pool or client */
      return false;
   }

   db_lock(ua->db);
   /* Specify JobTDate and Pool.Name= and/or Client.Name= in the query */
   if (!prune_set_filter(ua, client, pool, period, &sql_from, &sql_where)) {
      goto bail_out;
   }

//   edit_utime(now-period, ed1, sizeof(ed1));
//   Jmsg(ua->jcr, M_INFO, 0, _("Begin pruning Jobs older than %s secs.\n"), ed1);
   Jmsg(ua->jcr, M_INFO, 0, _("Begin pruning Files.\n"));
   /* Select Jobs -- for counting */ 
   Mmsg(query, 
        "SELECT COUNT(1) FROM Job %s WHERE PurgedFiles=0 %s", 
        sql_from.c_str(), sql_where.c_str());
   Dmsg1(050, "select sql=%s\n", query.c_str());
   cnt.count = 0;
   if (!db_sql_query(ua->db, query.c_str(), del_count_handler, (void *)&cnt)) {
      ua->error_msg("%s", db_strerror(ua->db));
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }

   if (cnt.count == 0) {
      if (ua->verbose) {
         ua->warning_msg(_("No Files found to prune.\n"));
      }
      goto bail_out;
   }

   if (cnt.count < MAX_DEL_LIST_LEN) {
      del.max_ids = cnt.count + 1;
   } else {
      del.max_ids = MAX_DEL_LIST_LEN;
   }
   del.tot_ids = 0;

   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);

   /* Now process same set but making a delete list */
   Mmsg(query, "SELECT JobId FROM Job %s WHERE PurgedFiles=0 %s", 
        sql_from.c_str(), sql_where.c_str());
   Dmsg1(050, "select sql=%s\n", query.c_str());
   db_sql_query(ua->db, query.c_str(), file_delete_handler, (void *)&del);

   purge_files_from_job_list(ua, del);

   edit_uint64_with_commas(del.num_del, ed1);
   ua->info_msg(_("Pruned Files from %s Jobs for client %s from catalog.\n"),
      ed1, client->name());

bail_out:
   db_unlock(ua->db);
   if (del.JobId) {
      free(del.JobId);
   }
   return 1;
}


static void drop_temp_tables(UAContext *ua)
{
   int i;
   for (i=0; drop_deltabs[i]; i++) {
      db_sql_query(ua->db, drop_deltabs[i], NULL, (void *)NULL);
   }
}

static bool create_temp_tables(UAContext *ua)
{
   /* Create temp tables and indicies */
   if (!db_sql_query(ua->db, create_deltabs[db_get_type_index(ua->db)], NULL, (void *)NULL)) {
      ua->error_msg("%s", db_strerror(ua->db));
      Dmsg0(050, "create DelTables table failed\n");
      return false;
   }
   if (!db_sql_query(ua->db, create_delindex, NULL, (void *)NULL)) {
       ua->error_msg("%s", db_strerror(ua->db));
       Dmsg0(050, "create DelInx1 index failed\n");
       return false;
   }
   return true;
}

static bool grow_del_list(struct del_ctx *del)
{
   if (del->num_ids == MAX_DEL_LIST_LEN) {
      return false;
   }

   if (del->num_ids == del->max_ids) {
      del->max_ids = (del->max_ids * 3) / 2;
      del->JobId = (JobId_t *)brealloc(del->JobId, sizeof(JobId_t) *
         del->max_ids);
      del->PurgedFiles = (char *)brealloc(del->PurgedFiles, del->max_ids);
   }
   return true;
}

struct accurate_check_ctx {
   DBId_t ClientId;                   /* Id of client */
   DBId_t FileSetId;                  /* Id of FileSet */ 
};

/* row: Job.Name, FileSet, Client.Name, FileSetId, ClientId, Type */
static int job_select_handler(void *ctx, int num_fields, char **row)
{
   alist *lst = (alist *)ctx;
   struct accurate_check_ctx *res;
   ASSERT(num_fields == 6);

   /* Quick fix for #5507, avoid locking res_head after db_lock() */

#ifdef bug5507
   /* If this job doesn't exist anymore in the configuration, delete it */
   if (GetResWithName(R_JOB, row[0]) == NULL) {
      return 0;
   }

   /* If this fileset doesn't exist anymore in the configuration, delete it */
   if (GetResWithName(R_FILESET, row[1]) == NULL) {
      return 0;
   }

   /* If this client doesn't exist anymore in the configuration, delete it */
   if (GetResWithName(R_CLIENT, row[2]) == NULL) {
      return 0;
   }
#endif

   /* Don't compute accurate things for Verify jobs */
   if (*row[5] == 'V') {
      return 0;
   }

   res = (struct accurate_check_ctx*) malloc(sizeof(struct accurate_check_ctx));
   res->FileSetId = str_to_int64(row[3]);
   res->ClientId = str_to_int64(row[4]);
   lst->append(res);

// Dmsg2(150, "row=%d val=%d\n", del->num_ids-1, del->JobId[del->num_ids-1]);
   return 0;
}

/*
 * Pruning Jobs is a bit more complicated than purging Files
 * because we delete Job records only if there is a more current
 * backup of the FileSet. Otherwise, we keep the Job record.
 * In other words, we never delete the only Job record that
 * contains a current backup of a FileSet. This prevents the
 * Volume from being recycled and destroying a current backup.
 *
 * For Verify Jobs, we do not delete the last InitCatalog.
 *
 * For Restore Jobs there are no restrictions.
 */
int prune_jobs(UAContext *ua, CLIENT *client, POOL *pool, int JobType)
{
   POOL_MEM query(PM_MESSAGE);
   POOL_MEM sql_where(PM_MESSAGE);
   POOL_MEM sql_from(PM_MESSAGE);
   utime_t period;
   char ed1[50];
   alist *jobids_check=NULL;
   struct accurate_check_ctx *elt;
   db_list_ctx jobids, tempids;
   JOB_DBR jr;
   struct del_ctx del;
   memset(&del, 0, sizeof(del));

   if (pool && pool->JobRetention > 0) {
      period = pool->JobRetention;

   } else if (client) {
      period = client->JobRetention;

   } else {                     /* should specify at least pool or client */
      return false;
   }

   db_lock(ua->db);
   if (!prune_set_filter(ua, client, pool, period, &sql_from, &sql_where)) {
      goto bail_out;
   }

   /* Drop any previous temporary tables still there */
   drop_temp_tables(ua);

   /* Create temp tables and indicies */
   if (!create_temp_tables(ua)) {
      goto bail_out;
   }

   edit_utime(period, ed1, sizeof(ed1));
   Jmsg(ua->jcr, M_INFO, 0, _("Begin pruning Jobs older than %s.\n"), ed1);

   del.max_ids = 100;
   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);
   del.PurgedFiles = (char *)malloc(del.max_ids);

   /*
    * Select all files that are older than the JobRetention period
    *  and add them into the "DeletionCandidates" table.
    */
   Mmsg(query, 
        "INSERT INTO DelCandidates "
          "SELECT JobId,PurgedFiles,FileSetId,JobFiles,JobStatus "
            "FROM Job %s "      /* JOIN Pool/Client */
           "WHERE Type IN ('B', 'C', 'M', 'V',  'D', 'R', 'c', 'm', 'g') "
             " %s ",            /* Pool/Client + JobTDate */
        sql_from.c_str(), sql_where.c_str());

   Dmsg1(050, "select sql=%s\n", query.c_str());
   if (!db_sql_query(ua->db, query.c_str(), NULL, (void *)NULL)) {
      if (ua->verbose) {
         ua->error_msg("%s", db_strerror(ua->db));
      }
      goto bail_out;
   }

   /* Now, for the selection, we discard some of them in order to be always
    * able to restore files. (ie, last full, last diff, last incrs)
    * Note: The DISTINCT could be more useful if we don't get FileSetId
    */
   jobids_check = New(alist(10, owned_by_alist));
   Mmsg(query, 
"SELECT DISTINCT Job.Name, FileSet, Client.Name, Job.FileSetId, "
                "Job.ClientId, Job.Type "
  "FROM DelCandidates "
       "JOIN Job USING (JobId) "
       "JOIN Client USING (ClientId) "
       "JOIN FileSet ON (Job.FileSetId = FileSet.FileSetId) "
 "WHERE Job.Type IN ('B') "               /* Look only Backup jobs */
   "AND Job.JobStatus IN ('T', 'W') "     /* Look only useful jobs */
      );

   /* The job_select_handler will skip jobs or filesets that are no longer
    * in the configuration file. Interesting ClientId/FileSetId will be
    * added to jobids_check (currently disabled in 6.0.7b)
    */
   if (!db_sql_query(ua->db, query.c_str(), job_select_handler, jobids_check)) {
      ua->error_msg("%s", db_strerror(ua->db));
   }

   /* For this selection, we exclude current jobs used for restore or
    * accurate. This will prevent to prune the last full backup used for
    * current backup & restore
    */
   memset(&jr, 0, sizeof(jr));
   /* To find useful jobs, we do like an incremental */
   jr.JobLevel = L_INCREMENTAL; 
   foreach_alist(elt, jobids_check) {
      jr.ClientId = elt->ClientId;   /* should be always the same */
      jr.FileSetId = elt->FileSetId;
      db_accurate_get_jobids(ua->jcr, ua->db, &jr, &tempids);
      jobids.add(tempids);
   }

   /* Discard latest Verify level=InitCatalog job 
    * TODO: can have multiple fileset
    */
   Mmsg(query, 
        "SELECT JobId, JobTDate "
          "FROM Job %s "                         /* JOIN Client/Pool */
         "WHERE Type='V'    AND Level='V' "
              " %s "                             /* Pool, JobTDate, Client */
         "ORDER BY JobTDate DESC LIMIT 1", 
        sql_from.c_str(), sql_where.c_str());

   if (!db_sql_query(ua->db, query.c_str(), db_list_handler, &jobids)) {
      ua->error_msg("%s", db_strerror(ua->db));
   }

   /* If we found jobs to exclude from the DelCandidates list, we should
    * also remove BaseJobs that can be linked with them
    */
   if (jobids.count > 0) {
      Dmsg1(60, "jobids to exclude before basejobs = %s\n", jobids.list);
      /* We also need to exclude all basejobs used */
      db_get_used_base_jobids(ua->jcr, ua->db, jobids.list, &jobids);

      /* Removing useful jobs from the DelCandidates list */
      Mmsg(query, "DELETE FROM DelCandidates "
                   "WHERE JobId IN (%s) "        /* JobId used in accurate */
                     "AND JobFiles!=0",          /* Discard when JobFiles=0 */
           jobids.list);

      if (!db_sql_query(ua->db, query.c_str(), NULL, NULL)) {
         ua->error_msg("%s", db_strerror(ua->db));
         goto bail_out;         /* Don't continue if the list isn't clean */
      }
      Dmsg1(60, "jobids to exclude = %s\n", jobids.list);
   }

   /* We use DISTINCT because we can have two times the same job */
   Mmsg(query, 
        "SELECT DISTINCT DelCandidates.JobId,DelCandidates.PurgedFiles "
          "FROM DelCandidates");
   if (!db_sql_query(ua->db, query.c_str(), job_delete_handler, (void *)&del)) {
      ua->error_msg("%s", db_strerror(ua->db));
   }

   purge_job_list_from_catalog(ua, del);

   if (del.num_del > 0) {
      ua->info_msg(_("Pruned %d %s for client %s from catalog.\n"), del.num_del,
         del.num_del==1?_("Job"):_("Jobs"), client->name());
    } else if (ua->verbose) {
       ua->info_msg(_("No Jobs found to prune.\n"));
    }

bail_out:
   drop_temp_tables(ua);
   db_unlock(ua->db);
   if (del.JobId) {
      free(del.JobId);
   }
   if (del.PurgedFiles) {
      free(del.PurgedFiles);
   }
   if (jobids_check) {
      delete jobids_check;
   }
   return 1;
}

/*
 * Prune a given Volume
 */
bool prune_volume(UAContext *ua, MEDIA_DBR *mr)
{
   POOL_MEM query(PM_MESSAGE);
   struct del_ctx del;
   bool ok = false;
   int count;

   if (mr->Enabled == 2) {
      return false;                   /* Cannot prune archived volumes */
   }

   memset(&del, 0, sizeof(del));
   del.max_ids = 10000;
   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);

   db_lock(ua->db);

   /* Prune only Volumes with status "Full", or "Used" */
   if (strcmp(mr->VolStatus, "Full")   == 0 ||
       strcmp(mr->VolStatus, "Used")   == 0) {
      Dmsg2(050, "get prune list MediaId=%d Volume %s\n", (int)mr->MediaId, mr->VolumeName);
      count = get_prune_list_for_volume(ua, mr, &del);
      Dmsg1(050, "Num pruned = %d\n", count);
      if (count != 0) {
         purge_job_list_from_catalog(ua, del);
      }
      ok = is_volume_purged(ua, mr);
   }

   db_unlock(ua->db);
   if (del.JobId) {
      free(del.JobId);
   }
   return ok;
}

/*
 * Get prune list for a volume
 */
int get_prune_list_for_volume(UAContext *ua, MEDIA_DBR *mr, del_ctx *del)
{
   POOL_MEM query(PM_MESSAGE);
   int count = 0;
   utime_t now, period;
   char ed1[50], ed2[50];

   if (mr->Enabled == 2) {
      return 0;                    /* cannot prune Archived volumes */
   }

   /*
    * Now add to the  list of JobIds for Jobs written to this Volume
    */
   edit_int64(mr->MediaId, ed1); 
   period = mr->VolRetention;
   now = (utime_t)time(NULL);
   edit_int64(now-period, ed2);
   Mmsg(query, sel_JobMedia, ed1, ed2);
   Dmsg3(250, "Now=%d period=%d now-period=%s\n", (int)now, (int)period,
      ed2);

   Dmsg1(050, "Query=%s\n", query.c_str());
   if (!db_sql_query(ua->db, query.c_str(), file_delete_handler, (void *)del)) {
      if (ua->verbose) {
         ua->error_msg("%s", db_strerror(ua->db));
      }
      Dmsg0(050, "Count failed\n");
      goto bail_out;
   }
   count = exclude_running_jobs_from_list(del);
   
bail_out:
   return count;
}

/*
 * We have a list of jobs to prune or purge. If any of them is
 *   currently running, we set its JobId to zero which effectively
 *   excludes it.
 *
 * Returns the number of jobs that can be prunned or purged.
 *
 */
int exclude_running_jobs_from_list(del_ctx *prune_list)
{
   int count = 0;
   JCR *jcr;
   bool skip;
   int i;          

   /* Do not prune any job currently running */
   for (i=0; i < prune_list->num_ids; i++) {
      skip = false;
      foreach_jcr(jcr) {
         if (jcr->JobId == prune_list->JobId[i]) {
            Dmsg2(050, "skip running job JobId[%d]=%d\n", i, (int)prune_list->JobId[i]);
            prune_list->JobId[i] = 0;
            skip = true;
            break;
         }
      }
      endeach_jcr(jcr);
      if (skip) {
         continue;  /* don't increment count */
      }
      Dmsg2(050, "accept JobId[%d]=%d\n", i, (int)prune_list->JobId[i]);
      count++;
   }
   return count;
}
