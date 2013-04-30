/*
 * Bacula Catalog Database Delete record interface routines
 *
 *    Kern Sibbald, December 2000
 *
 *    Version $Id: sql_delete.c 7380 2008-07-14 10:42:59Z kerns $
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

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

/* *****FIXME**** fix fixed length of select_cmd[] and insert_cmd[] */

#include "bacula.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/*
 * Delete Pool record, must also delete all associated
 *  Media records.
 *
 *  Returns: 0 on error
 *           1 on success
 *           PoolId = number of Pools deleted (should be 1)
 *           NumVols = number of Media records deleted
 */
int
db_delete_pool_record(JCR *jcr, B_DB *mdb, POOL_DBR *pr)
{
   SQL_ROW row;
   int num_rows;
   char esc[MAX_ESCAPE_NAME_LENGTH];

   db_lock(mdb);
   mdb->db_escape_string(jcr, esc, pr->Name, strlen(pr->Name));
   Mmsg(mdb->cmd, "SELECT PoolId FROM Pool WHERE Name='%s'", esc);
   Dmsg1(10, "selectpool: %s\n", mdb->cmd);

   pr->PoolId = pr->NumVols = 0;

   if (QUERY_DB(jcr, mdb, mdb->cmd)) {

      num_rows = sql_num_rows(mdb);
      if (num_rows == 0) {
         Mmsg(mdb->errmsg, _("No pool record %s exists\n"), pr->Name);
         sql_free_result(mdb);
         db_unlock(mdb);
         return 0;
      } else if (num_rows != 1) {
         Mmsg(mdb->errmsg, _("Expecting one pool record, got %d\n"), num_rows);
         sql_free_result(mdb);
         db_unlock(mdb);
         return 0;
      }
      if ((row = sql_fetch_row(mdb)) == NULL) {
         Mmsg1(&mdb->errmsg, _("Error fetching row %s\n"), sql_strerror(mdb));
         db_unlock(mdb);
         return 0;
      }
      pr->PoolId = str_to_int64(row[0]);
      sql_free_result(mdb);
   }

   /* Delete Media owned by this pool */
   Mmsg(mdb->cmd,
"DELETE FROM Media WHERE Media.PoolId = %d", pr->PoolId);

   pr->NumVols = DELETE_DB(jcr, mdb, mdb->cmd);
   Dmsg1(200, "Deleted %d Media records\n", pr->NumVols);

   /* Delete Pool */
   Mmsg(mdb->cmd,
"DELETE FROM Pool WHERE Pool.PoolId = %d", pr->PoolId);
   pr->PoolId = DELETE_DB(jcr, mdb, mdb->cmd);
   Dmsg1(200, "Deleted %d Pool records\n", pr->PoolId);

   db_unlock(mdb);
   return 1;
}

#define MAX_DEL_LIST_LEN 1000000

struct s_del_ctx {
   JobId_t *JobId;
   int num_ids;                       /* ids stored */
   int max_ids;                       /* size of array */
   int num_del;                       /* number deleted */
   int tot_ids;                       /* total to process */
};

/*
 * Called here to make in memory list of JobIds to be
 *  deleted. The in memory list will then be transversed
 *  to issue the SQL DELETE commands.  Note, the list
 *  is allowed to get to MAX_DEL_LIST_LEN to limit the
 *  maximum malloc'ed memory.
 */
static int delete_handler(void *ctx, int num_fields, char **row)
{
   struct s_del_ctx *del = (struct s_del_ctx *)ctx;

   if (del->num_ids == MAX_DEL_LIST_LEN) {
      return 1;
   }
   if (del->num_ids == del->max_ids) {
      del->max_ids = (del->max_ids * 3) / 2;
      del->JobId = (JobId_t *)brealloc(del->JobId, sizeof(JobId_t) *
         del->max_ids);
   }
   del->JobId[del->num_ids++] = (JobId_t)str_to_int64(row[0]);
   return 0;
}


/*
 * This routine will purge (delete) all records
 * associated with a particular Volume. It will
 * not delete the media record itself.
 * TODO: This function is broken and it doesn't purge
 *       File, BaseFiles, Log, ...
 *       We call it from relabel and delete volume=, both ensure
 *       that the volume is properly purged.
 */
static int do_media_purge(B_DB *mdb, MEDIA_DBR *mr)
{
   POOLMEM *query = get_pool_memory(PM_MESSAGE);
   struct s_del_ctx del;
   char ed1[50];
   int i;

   del.num_ids = 0;
   del.tot_ids = 0;
   del.num_del = 0;
   del.max_ids = 0;
   Mmsg(mdb->cmd, "SELECT JobId from JobMedia WHERE MediaId=%d", mr->MediaId);
   del.max_ids = mr->VolJobs;
   if (del.max_ids < 100) {
      del.max_ids = 100;
   } else if (del.max_ids > MAX_DEL_LIST_LEN) {
      del.max_ids = MAX_DEL_LIST_LEN;
   }
   del.JobId = (JobId_t *)malloc(sizeof(JobId_t) * del.max_ids);
   db_sql_query(mdb, mdb->cmd, delete_handler, (void *)&del);

   for (i=0; i < del.num_ids; i++) {
      Dmsg1(400, "Delete JobId=%d\n", del.JobId[i]);
      Mmsg(query, "DELETE FROM Job WHERE JobId=%s", edit_int64(del.JobId[i], ed1));
      db_sql_query(mdb, query, NULL, (void *)NULL);
      Mmsg(query, "DELETE FROM File WHERE JobId=%s", edit_int64(del.JobId[i], ed1));
      db_sql_query(mdb, query, NULL, (void *)NULL);
      Mmsg(query, "DELETE FROM JobMedia WHERE JobId=%s", edit_int64(del.JobId[i], ed1));
      db_sql_query(mdb, query, NULL, (void *)NULL);
   }
   free(del.JobId);
   free_pool_memory(query);
   return 1;
}

/* Delete Media record and all records that
 * are associated with it.
 */
int db_delete_media_record(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr)
{
   db_lock(mdb);
   if (mr->MediaId == 0 && !db_get_media_record(jcr, mdb, mr)) {
      db_unlock(mdb);
      return 0;
   }
   /* Do purge if not already purged */
   if (strcmp(mr->VolStatus, "Purged") != 0) {
      /* Delete associated records */
      do_media_purge(mdb, mr);
   }

   Mmsg(mdb->cmd, "DELETE FROM Media WHERE MediaId=%d", mr->MediaId);
   db_sql_query(mdb, mdb->cmd, NULL, (void *)NULL);
   db_unlock(mdb);
   return 1;
}

/*
 * Purge all records associated with a
 * media record. This does not delete the
 * media record itself. But the media status
 * is changed to "Purged".
 */
int db_purge_media_record(JCR *jcr, B_DB *mdb, MEDIA_DBR *mr)
{
   db_lock(mdb);
   if (mr->MediaId == 0 && !db_get_media_record(jcr, mdb, mr)) {
      db_unlock(mdb);
      return 0;
   }
   /* Delete associated records */
   do_media_purge(mdb, mr);           /* Note, always purge */

   /* Mark Volume as purged */
   strcpy(mr->VolStatus, "Purged");
   if (!db_update_media_record(jcr, mdb, mr)) {
      db_unlock(mdb);
      return 0;
   }

   db_unlock(mdb);
   return 1;
}


#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES */
