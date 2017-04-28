/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2010 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"
#include "lib/htable.h"
#include "bvfs.h"

#define dbglevel 10
#define dbglevel_sql 15

static int result_handler(void *ctx, int fields, char **row)
{
   if (fields == 4) {
      Pmsg4(0, "%s\t%s\t%s\t%s\n",
            row[0], row[1], row[2], row[3]);
   } else if (fields == 5) {
      Pmsg5(0, "%s\t%s\t%s\t%s\t%s\n",
            row[0], row[1], row[2], row[3], row[4]);
   } else if (fields == 6) {
      Pmsg6(0, "%s\t%s\t%s\t%s\t%s\t%s\n",
            row[0], row[1], row[2], row[3], row[4], row[5]);
   } else if (fields == 7) {
      Pmsg7(0, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
            row[0], row[1], row[2], row[3], row[4], row[5], row[6]);
   }
   return 0;
}

Bvfs::Bvfs(JCR *j, B_DB *mdb) {
   jcr = j;
   jcr->inc_use_count();
   db = mdb;                 /* need to inc ref count */
   jobids = get_pool_memory(PM_NAME);
   prev_dir = get_pool_memory(PM_NAME);
   pattern = get_pool_memory(PM_NAME);
   *jobids = *prev_dir = *pattern = 0;
   dir_filenameid = pwd_id = offset = 0;
   see_copies = see_all_versions = false;
   limit = 1000;
   attr = new_attr(jcr);
   list_entries = result_handler;
   user_data = this;
}

Bvfs::~Bvfs() {
   free_pool_memory(jobids);
   free_pool_memory(pattern);
   free_pool_memory(prev_dir);
   free_attr(attr);
   jcr->dec_use_count();
}

void Bvfs::set_jobid(JobId_t id)
{
   Mmsg(jobids, "%lld", (uint64_t)id);
}

void Bvfs::set_jobids(char *ids)
{
   pm_strcpy(jobids, ids);
}

/*
 * TODO: Find a way to let the user choose how he wants to display
 * files and directories
 */


/*
 * Working Object to store PathId already seen (avoid
 * database queries), equivalent to %cache_ppathid in perl
 */

#define NITEMS 50000
class pathid_cache {
private:
   hlink *nodes;
   int nb_node;
   int max_node;
   alist *table_node;
   htable *cache_ppathid;

public:
   pathid_cache() {
      hlink link;
      cache_ppathid = (htable *)malloc(sizeof(htable));
      cache_ppathid->init(&link, &link, NITEMS);
      max_node = NITEMS;
      nodes = (hlink *) malloc(max_node * sizeof (hlink));
      nb_node = 0;
      table_node = New(alist(5, owned_by_alist));
      table_node->append(nodes);
   }

   hlink *get_hlink() {
      if (++nb_node >= max_node) {
         nb_node = 0;
         nodes = (hlink *)malloc(max_node * sizeof(hlink));
         table_node->append(nodes);
      }
      return nodes + nb_node;
   }

   bool lookup(char *pathid) {
      return (cache_ppathid->lookup(pathid) != NULL);
   }

   void insert(char *pathid) {
      hlink *h = get_hlink();
      cache_ppathid->insert(pathid, h);
   }

   ~pathid_cache() {
      cache_ppathid->destroy();
      free(cache_ppathid);
      delete table_node;
   }
private:
   pathid_cache(const pathid_cache &); /* prohibit pass by value */
   pathid_cache &operator= (const pathid_cache &);/* prohibit class assignment*/
} ;

/* Return the parent_dir with the trailing /  (update the given string)
 * TODO: see in the rest of bareos if we don't have already this function
 * dir=/tmp/toto/
 * dir=/tmp/
 * dir=/
 * dir=
 */
char *bvfs_parent_dir(char *path)
{
   char *p = path;
   int len = strlen(path) - 1;

   /* windows directory / */
   if (len == 2 && B_ISALPHA(path[0])
                && path[1] == ':'
                && path[2] == '/')
   {
      len = 0;
      path[0] = '\0';
   }

   if (len >= 0 && path[len] == '/') {      /* if directory, skip last / */
      path[len] = '\0';
   }

   if (len > 0) {
      p += len;
      while (p > path && !IsPathSeparator(*p)) {
         p--;
      }
      p[1] = '\0';
   }
   return path;
}

/* Return the basename of the with the trailing /
 * TODO: see in the rest of bareos if we don't have
 * this function already
 */
char *bvfs_basename_dir(char *path)
{
   char *p = path;
   int len = strlen(path) - 1;

   if (path[len] == '/') {      /* if directory, skip last / */
      len -= 1;
   }

   if (len > 0) {
      p += len;
      while (p > path && !IsPathSeparator(*p)) {
         p--;
      }
      if (*p == '/') {
         p++;                  /* skip first / */
      }
   }
   return p;
}

static void build_path_hierarchy(JCR *jcr, B_DB *mdb,
                                 pathid_cache &ppathid_cache,
                                 char *org_pathid, char *path)
{
   Dmsg1(dbglevel, "build_path_hierarchy(%s)\n", path);
   char pathid[50];
   ATTR_DBR parent;
   char *bkp = mdb->path;
   bstrncpy(pathid, org_pathid, sizeof(pathid));

   /* Does the ppathid exist for this ? we use a memory cache...  In order to
    * avoid the full loop, we consider that if a dir is allready in the
    * PathHierarchy table, then there is no need to calculate all the
    * hierarchy
    */
   while (path && *path)
   {
      if (!ppathid_cache.lookup(pathid))
      {
         Mmsg(mdb->cmd,
              "SELECT PPathId FROM PathHierarchy WHERE PathId = %s",
              pathid);

         if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
            goto bail_out;      /* Query failed, just leave */
         }

         /* Do we have a result ? */
         if (sql_num_rows(mdb) > 0) {
            ppathid_cache.insert(pathid);
            /* This dir was in the db ...
             * It means we can leave, the tree has allready been built for
             * this dir
             */
            goto bail_out;
         } else {
            /* search or create parent PathId in Path table */
            mdb->path = bvfs_parent_dir(path);
            mdb->pnl = strlen(mdb->path);
            if (!db_create_path_record(jcr, mdb, &parent)) {
               goto bail_out;
            }
            ppathid_cache.insert(pathid);

            /* Prevent from ERROR:  duplicate key value violates unique
             * constraint "pathhierarchy_pkey" when multiple .bvfs_update
             * to prevent from unique key violation when multiple .bvfs_update
             * are run parallel.
             * The syntax of the following quere works for PostgreSQL, MySQL
             * and SQLite
             */
            Mmsg(mdb->cmd,
                 "INSERT INTO PathHierarchy (PathId,PPathId) "
                    "SELECT ph.PathId,ph.PPathId FROM "
                       "(SELECT %s AS PathId, %lld AS PPathId) ph "
                       "WHERE NOT EXISTS "
                       "(SELECT 1 FROM PathHierarchy phi WHERE phi.PathId = ph.PathId)",
                 pathid, (uint64_t) parent.PathId);

            if (!INSERT_DB(jcr, mdb, mdb->cmd)) {
               goto bail_out;   /* Can't insert the record, just leave */
            }

            edit_uint64(parent.PathId, pathid);
            path = mdb->path;   /* already done */
         }
      } else {
         /* It's already in the cache.  We can leave, no time to waste here,
          * all the parent dirs have allready been done
          */
         goto bail_out;
      }
   }

bail_out:
   mdb->path = bkp;
   mdb->fnl = 0;
}

/*
 * Internal function to update path_hierarchy cache with a shared pathid cache
 * return Error 0
 *        OK    1
 */
static bool update_path_hierarchy_cache(JCR *jcr,
                                        B_DB *mdb,
                                        pathid_cache &ppathid_cache,
                                        JobId_t JobId)
{
   Dmsg0(dbglevel, "update_path_hierarchy_cache()\n");
   bool retval = false;
   uint32_t num;
   char jobid[50];
   edit_uint64(JobId, jobid);

   db_lock(mdb);
   db_start_transaction(jcr, mdb);

   Mmsg(mdb->cmd, "SELECT 1 FROM Job WHERE JobId = %s AND HasCache=1", jobid);

   if (!QUERY_DB(jcr, mdb, mdb->cmd) || sql_num_rows(mdb) > 0) {
      Dmsg1(dbglevel, "already computed %d\n", (uint32_t)JobId );
      retval = true;
      goto bail_out;
   }

   /* prevent from DB lock waits when already in progress */
   Mmsg(mdb->cmd, "SELECT 1 FROM Job WHERE JobId = %s AND HasCache=-1", jobid);

   if (!QUERY_DB(jcr, mdb, mdb->cmd) || sql_num_rows(mdb) > 0) {
      Dmsg1(dbglevel, "already in progress %d\n", (uint32_t)JobId );
      retval = false;
      goto bail_out;
   }

   /* set HasCache to -1 in Job (in progress) */
   Mmsg(mdb->cmd, "UPDATE Job SET HasCache=-1 WHERE JobId=%s", jobid);
   UPDATE_DB(jcr, mdb, mdb->cmd);

   /* need to COMMIT here to ensure that other concurrent .bvfs_update runs
    * see the current HasCache value. A new transaction must only be started
    * after having finished PathHierarchy processing, otherwise prevention
    * from duplicate key violations in build_path_hierarchy() will not work.
    */
   db_end_transaction(jcr, mdb);

   /* Inserting path records for JobId */
   Mmsg(mdb->cmd, "INSERT INTO PathVisibility (PathId, JobId) "
                   "SELECT DISTINCT PathId, JobId "
                     "FROM (SELECT PathId, JobId FROM File WHERE JobId = %s "
                           "UNION "
                           "SELECT PathId, BaseFiles.JobId "
                             "FROM BaseFiles JOIN File AS F USING (FileId) "
                            "WHERE BaseFiles.JobId = %s) AS B",
        jobid, jobid);

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      Dmsg1(dbglevel, "Can't fill PathVisibility %d\n", (uint32_t)JobId );
      goto bail_out;
   }

   /* Now we have to do the directory recursion stuff to determine missing
    * visibility We try to avoid recursion, to be as fast as possible We also
    * only work on not allready hierarchised directories...
    */
   Mmsg(mdb->cmd,
     "SELECT PathVisibility.PathId, Path "
       "FROM PathVisibility "
            "JOIN Path ON( PathVisibility.PathId = Path.PathId) "
            "LEFT JOIN PathHierarchy "
         "ON (PathVisibility.PathId = PathHierarchy.PathId) "
      "WHERE PathVisibility.JobId = %s "
        "AND PathHierarchy.PathId IS NULL "
      "ORDER BY Path", jobid);
   Dmsg1(dbglevel_sql, "q=%s\n", mdb->cmd);

   if (!QUERY_DB(jcr, mdb, mdb->cmd)) {
      Dmsg1(dbglevel, "Can't get new Path %d\n", (uint32_t)JobId );
      goto bail_out;
   }

   /* TODO: I need to reuse the DB connection without emptying the result
    * So, now i'm copying the result in memory to be able to query the
    * catalog descriptor again.
    */
   num = sql_num_rows(mdb);
   if (num > 0) {
      char **result = (char **)malloc (num * 2 * sizeof(char *));

      SQL_ROW row;
      int i=0;
      while((row = sql_fetch_row(mdb))) {
         result[i++] = bstrdup(row[0]);
         result[i++] = bstrdup(row[1]);
      }

      i=0;
      while (num > 0) {
         build_path_hierarchy(jcr, mdb, ppathid_cache, result[i], result[i+1]);
         free(result[i++]);
         free(result[i++]);
         num--;
      }
      free(result);
   }

   db_start_transaction(jcr, mdb);

   if (mdb->db_get_type_index() == SQL_TYPE_SQLITE3) {
      Mmsg(mdb->cmd,
 "INSERT INTO PathVisibility (PathId, JobId) "
   "SELECT DISTINCT h.PPathId AS PathId, %s "
     "FROM PathHierarchy AS h "
    "WHERE h.PathId IN (SELECT PathId FROM PathVisibility WHERE JobId=%s) "
      "AND h.PPathId NOT IN (SELECT PathId FROM PathVisibility WHERE JobId=%s)",
           jobid, jobid, jobid );

   } else {
      Mmsg(mdb->cmd,
  "INSERT INTO PathVisibility (PathId, JobId)  "
   "SELECT a.PathId,%s "
   "FROM ( "
     "SELECT DISTINCT h.PPathId AS PathId "
       "FROM PathHierarchy AS h "
       "JOIN  PathVisibility AS p ON (h.PathId=p.PathId) "
      "WHERE p.JobId=%s) AS a "
     "LEFT JOIN PathVisibility AS b "
       "ON (b.JobId=%s and a.PathId = b.PathId) "
   "WHERE b.PathId IS NULL",  jobid, jobid, jobid);
   }

   do {
      retval = QUERY_DB(jcr, mdb, mdb->cmd);
   } while (retval && sql_affected_rows(mdb) > 0);

   Mmsg(mdb->cmd, "UPDATE Job SET HasCache=1 WHERE JobId=%s", jobid);
   UPDATE_DB(jcr, mdb, mdb->cmd);

bail_out:
   db_end_transaction(jcr, mdb);
   db_unlock(mdb);
   return retval;
}

/*
 * Find an store the filename descriptor for empty directories Filename.Name=''
 */
DBId_t Bvfs::get_dir_filenameid()
{
   uint32_t id;
   if (dir_filenameid) {
      return dir_filenameid;
   }
   POOL_MEM q;
   Mmsg(q, "SELECT FilenameId FROM Filename WHERE Name = ''");
   db_sql_query(db, q.c_str(), db_int_handler, &id);
   dir_filenameid = id;
   return dir_filenameid;
}

void bvfs_update_cache(JCR *jcr, B_DB *mdb)
{
   uint32_t nb=0;
   db_list_ctx jobids_list;

   db_lock(mdb);

   Mmsg(mdb->cmd,
 "SELECT JobId from Job "
  "WHERE HasCache = 0 "
    "AND Type IN ('B') AND JobStatus IN ('T', 'W', 'f', 'A') "
  "ORDER BY JobId");

   db_sql_query(mdb, mdb->cmd, db_list_handler, &jobids_list);

   bvfs_update_path_hierarchy_cache(jcr, mdb, jobids_list.list);

   db_start_transaction(jcr, mdb);
   Dmsg0(dbglevel, "Cleaning pathvisibility\n");
   Mmsg(mdb->cmd,
        "DELETE FROM PathVisibility "
         "WHERE NOT EXISTS "
        "(SELECT 1 FROM Job WHERE JobId=PathVisibility.JobId)");
   nb = DELETE_DB(jcr, mdb, mdb->cmd);
   Dmsg1(dbglevel, "Affected row(s) = %d\n", nb);

   db_end_transaction(jcr, mdb);
   db_unlock(mdb);
}

/*
 * Update the bvfs cache for given jobids (1,2,3,4)
 */
bool bvfs_update_path_hierarchy_cache(JCR *jcr, B_DB *mdb, char *jobids)
{
   char *p;
   int status;
   JobId_t JobId;
   bool retval = true;
   pathid_cache ppathid_cache;

   p = jobids;
   while (1) {
      status = get_next_jobid_from_list(&p, &JobId);
      if (status < 0) {
         goto bail_out;
      }

      if (status == 0) {
         /*
          * We reached the end of the list.
          */
         goto bail_out;
      }

      Dmsg1(dbglevel, "Updating cache for %lld\n", (uint64_t)JobId);
      if (!update_path_hierarchy_cache(jcr, mdb, ppathid_cache, JobId)) {
         retval = false;
      }
   }

bail_out:
   return retval;
}

/*
 * Update the bvfs cache for current jobids
 */
void Bvfs::update_cache()
{
   bvfs_update_path_hierarchy_cache(jcr, db, jobids);
}

/* Change the current directory, returns true if the path exists */
bool Bvfs::ch_dir(const char *path)
{
   pm_strcpy(db->path, path);
   db->pnl = strlen(db->path);
   db_lock(db);
   ch_dir(db_get_path_record(jcr, db));
   db_unlock(db);
   return pwd_id != 0;
}

/*
 * Get all file versions for a specified client
 * TODO: Handle basejobs using different client
 */
void Bvfs::get_all_file_versions(DBId_t pathid, DBId_t fnid, const char *client)
{
   Dmsg3(dbglevel, "get_all_file_versions(%lld, %lld, %s)\n", (uint64_t)pathid,
         (uint64_t)fnid, client);
   char ed1[50], ed2[50];
   POOL_MEM q;
   if (see_copies) {
      Mmsg(q, " AND Job.Type IN ('C', 'B') ");
   } else {
      Mmsg(q, " AND Job.Type = 'B' ");
   }

   POOL_MEM query;

   Mmsg(query,//    1           2              3
"SELECT 'V', File.PathId, File.FilenameId,  File.Md5, "
//         4          5           6
        "File.JobId, File.LStat, File.FileId, "
//         7                    8
       "Media.VolumeName, Media.InChanger "
"FROM File, Job, Client, JobMedia, Media "
"WHERE File.FilenameId = %s "
  "AND File.PathId=%s "
  "AND File.JobId = Job.JobId "
  "AND Job.JobId = JobMedia.JobId "
  "AND File.FileIndex >= JobMedia.FirstIndex "
  "AND File.FileIndex <= JobMedia.LastIndex "
  "AND JobMedia.MediaId = Media.MediaId "
  "AND Job.ClientId = Client.ClientId "
  "AND Client.Name = '%s' "
  "%s ORDER BY FileId LIMIT %d OFFSET %d"
        ,edit_uint64(fnid, ed1), edit_uint64(pathid, ed2), client, q.c_str(),
        limit, offset);
   Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());
   db_sql_query(db, query.c_str(), list_entries, user_data);
}

DBId_t Bvfs::get_root()
{
   int p;
   *db->path = 0;
   db_lock(db);
   p = db_get_path_record(jcr, db);
   db_unlock(db);
   return p;
}

static int path_handler(void *ctx, int fields, char **row)
{
   Bvfs *fs = (Bvfs *) ctx;
   return fs->_handle_path(ctx, fields, row);
}

int Bvfs::_handle_path(void *ctx, int fields, char **row)
{
   if (bvfs_is_dir(row)) {
      /* can have the same path 2 times */
      if (!bstrcmp(row[BVFS_Name], prev_dir)) {
         pm_strcpy(prev_dir, row[BVFS_Name]);
         return list_entries(user_data, fields, row);
      }
   }
   return 0;
}

/*
 * Retrieve . and .. information
 */
void Bvfs::ls_special_dirs()
{
   Dmsg1(dbglevel, "ls_special_dirs(%lld)\n", (uint64_t)pwd_id);
   char ed1[50], ed2[50];
   if (*jobids == 0) {
      return;
   }
   if (!dir_filenameid) {
      get_dir_filenameid();
   }

   /* Will fetch directories  */
   *prev_dir = 0;

   POOL_MEM query;
   Mmsg(query,
"(SELECT PPathId AS PathId, '..' AS Path "
    "FROM  PathHierarchy "
   "WHERE  PathId = %s "
"UNION "
 "SELECT %s AS PathId, '.' AS Path)",
        edit_uint64(pwd_id, ed1), ed1);

   POOL_MEM query2;
   Mmsg(query2,// 1      2     3        4     5       6
"SELECT 'D', tmp.PathId, 0, tmp.Path, JobId, LStat, FileId "
  "FROM %s AS tmp  LEFT JOIN ( " // get attributes if any
       "SELECT File1.PathId AS PathId, File1.JobId AS JobId, "
              "File1.LStat AS LStat, File1.FileId AS FileId FROM File AS File1 "
       "WHERE File1.FilenameId = %s "
       "AND File1.JobId IN (%s)) AS listfile1 "
  "ON (tmp.PathId = listfile1.PathId) "
  "ORDER BY tmp.Path, JobId DESC ",
        query.c_str(), edit_uint64(dir_filenameid, ed2), jobids);

   Dmsg1(dbglevel_sql, "q=%s\n", query2.c_str());
   db_sql_query(db, query2.c_str(), path_handler, this);
}

/* Returns true if we have dirs to read */
bool Bvfs::ls_dirs()
{
   Dmsg1(dbglevel, "ls_dirs(%lld)\n", (uint64_t)pwd_id);
   char ed1[50], ed2[50];
   if (*jobids == 0) {
      return false;
   }

   POOL_MEM query;
   POOL_MEM filter;
   if (*pattern) {
      Mmsg(filter, " AND Path2.Path %s '%s' ",
           match_query[db_get_type_index(db)], pattern);
   }

   if (!dir_filenameid) {
      get_dir_filenameid();
   }

   /* the sql query displays same directory multiple time, take the first one */
   *prev_dir = 0;

   /* Let's retrieve the list of the visible dirs in this dir ...
    * First, I need the empty filenameid to locate efficiently
    * the dirs in the file table
    * my $dir_filenameid = $self->get_dir_filenameid();
    */
   /* Then we get all the dir entries from File ... */
   Mmsg(query,
//       0     1     2   3      4     5       6
"SELECT 'D', PathId, 0, Path, JobId, LStat, FileId FROM ( "
    "SELECT Path1.PathId AS PathId, Path1.Path AS Path, "
           "lower(Path1.Path) AS lpath, "
           "listfile1.JobId AS JobId, listfile1.LStat AS LStat, "
           "listfile1.FileId AS FileId "
    "FROM ( "
      "SELECT DISTINCT PathHierarchy1.PathId AS PathId "
      "FROM PathHierarchy AS PathHierarchy1 "
      "JOIN Path AS Path2 "
        "ON (PathHierarchy1.PathId = Path2.PathId) "
      "JOIN PathVisibility AS PathVisibility1 "
        "ON (PathHierarchy1.PathId = PathVisibility1.PathId) "
      "WHERE PathHierarchy1.PPathId = %s "
      "AND PathVisibility1.JobId IN (%s) "
      "AND PathVisibility1.PathId NOT IN ( "
          "SELECT PathId FROM File "
          "WHERE FilenameId = %s "
                "AND JobId = ( "
                    "SELECT MAX(JobId) FROM PathVisibility "
                    "WHERE PathId = PathVisibility1.PathId "
                          "AND JobId IN (%s)) "
                "AND FileIndex = 0) "
           "%s "
     ") AS listpath1 "
   "JOIN Path AS Path1 ON (listpath1.PathId = Path1.PathId) "

   "LEFT JOIN ( " /* get attributes if any */
       "SELECT File1.PathId AS PathId, File1.JobId AS JobId, "
              "File1.LStat AS LStat, File1.FileId AS FileId FROM File AS File1 "
       "WHERE File1.FilenameId = %s "
       "AND File1.JobId IN (%s)) AS listfile1 "
       "ON (listpath1.PathId = listfile1.PathId) "
    ") AS A ORDER BY 2,3 DESC LIMIT %d OFFSET %d",
        edit_uint64(pwd_id, ed1),
        jobids,
        edit_uint64(dir_filenameid, ed2),
        jobids,
        filter.c_str(),
        edit_uint64(dir_filenameid, ed2),
        jobids,
        limit, offset);

   Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());

   db_lock(db);
   db_sql_query(db, query.c_str(), path_handler, this);
   nb_record = sql_num_rows(db);
   db_unlock(db);

   return nb_record == limit;
}

void build_ls_files_query(B_DB *db, POOL_MEM &query,
                          const char *JobId, const char *PathId,
                          const char *filter, int64_t limit, int64_t offset)
{
   if (db_get_type_index(db) == SQL_TYPE_POSTGRESQL) {
      Mmsg(query, sql_bvfs_list_files[db_get_type_index(db)],
           JobId, PathId, JobId, PathId,
           filter, limit, offset);
   } else {
      Mmsg(query, sql_bvfs_list_files[db_get_type_index(db)],
           JobId, PathId, JobId, PathId,
           limit, offset, filter, JobId, JobId);
   }
}

/* Returns true if we have files to read */
bool Bvfs::ls_files()
{
   POOL_MEM query;
   POOL_MEM filter;
   char pathid[50];

   Dmsg1(dbglevel, "ls_files(%lld)\n", (uint64_t)pwd_id);
   if (*jobids == 0) {
      return false;
   }

   if (!pwd_id) {
      ch_dir(get_root());
   }

   edit_uint64(pwd_id, pathid);
   if (*pattern) {
      Mmsg(filter, " AND Filename.Name %s '%s' ",
           match_query[db_get_type_index(db)], pattern);
   }

   build_ls_files_query(db, query,
                        jobids, pathid, filter.c_str(),
                        limit, offset);

   Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());

   db_lock(db);
   db_sql_query(db, query.c_str(), list_entries, user_data);
   nb_record = sql_num_rows(db);
   db_unlock(db);

   return nb_record == limit;
}


/*
 * Return next Id from comma separated list
 *
 * Returns:
 *   1 if next Id returned
 *   0 if no more Ids are in list
 *  -1 there is an error
 * TODO: merge with get_next_jobid_from_list() and get_next_dbid_from_list()
 */
static int get_next_id_from_list(char **p, int64_t *Id)
{
   const int maxlen = 30;
   char id[maxlen+1];
   char *q = *p;

   id[0] = 0;
   for (int i=0; i<maxlen; i++) {
      if (*q == 0) {
         break;
      } else if (*q == ',') {
         q++;
         break;
      }
      id[i] = *q++;
      id[i+1] = 0;
   }
   if (id[0] == 0) {
      return 0;
   } else if (!is_a_number(id)) {
      return -1;                      /* error */
   }
   *p = q;
   *Id = str_to_int64(id);
   return 1;
}

static int get_path_handler(void *ctx, int fields, char **row)
{
   POOL_MEM *buf = (POOL_MEM *) ctx;
   pm_strcpy(*buf, row[0]);
   return 0;
}

static bool check_temp(char *output_table)
{
   if (output_table[0] == 'b' &&
       output_table[1] == '2' &&
       is_an_integer(output_table + 2))
   {
      return true;
   }
   return false;
}

void Bvfs::clear_cache()
{
   db_sql_query(db, "BEGIN");
   db_sql_query(db, "UPDATE Job SET HasCache=0");
   db_sql_query(db, "TRUNCATE PathHierarchy");
   db_sql_query(db, "TRUNCATE PathVisibility");
   db_sql_query(db, "COMMIT");
}

bool Bvfs::drop_restore_list(char *output_table)
{
   POOL_MEM query;
   if (check_temp(output_table)) {
      Mmsg(query, "DROP TABLE %s", output_table);
      db_sql_query(db, query.c_str());
      return true;
   }
   return false;
}

bool Bvfs::compute_restore_list(char *fileid, char *dirid, char *hardlink,
                                char *output_table)
{
   POOL_MEM query;
   POOL_MEM tmp, tmp2;
   int64_t id, jobid, prev_jobid;
   bool init = false;
   bool retval = false;

   /* check args */
   if ((*fileid   && !is_a_number_list(fileid))  ||
       (*dirid    && !is_a_number_list(dirid))   ||
       (*hardlink && !is_a_number_list(hardlink))||
       (!*hardlink && !*fileid && !*dirid && !*hardlink))
   {
      return false;
   }
   if (!check_temp(output_table)) {
      return false;
   }

   db_lock(db);

   /* Cleanup old tables first */
   Mmsg(query, "DROP TABLE btemp%s", output_table);
   db_sql_query(db, query.c_str());

   Mmsg(query, "DROP TABLE %s", output_table);
   db_sql_query(db, query.c_str());

   Mmsg(query, "CREATE TABLE btemp%s AS ", output_table);

   if (*fileid) {               /* Select files with their direct id */
      init=true;
      Mmsg(tmp,"SELECT Job.JobId, JobTDate, FileIndex, FilenameId, "
                      "PathId, FileId "
                 "FROM File JOIN Job USING (JobId) WHERE FileId IN (%s)",
           fileid);
      pm_strcat(query, tmp.c_str());
   }

   /* Add a directory content */
   while (get_next_id_from_list(&dirid, &id) == 1) {
      Mmsg(tmp, "SELECT Path FROM Path WHERE PathId=%lld", id);

      if (!db_sql_query(db, tmp.c_str(), get_path_handler, (void *)&tmp2)) {
         Dmsg0(dbglevel, "Can't search for path\n");
         /* print error */
         goto bail_out;
      }
      if (bstrcmp(tmp2.c_str(), "")) { /* path not found */
         Dmsg3(dbglevel, "Path not found %lld q=%s s=%s\n",
               id, tmp.c_str(), tmp2.c_str());
         break;
      }
      /* escape % and _ for LIKE search */
      tmp.check_size((strlen(tmp2.c_str())+1) * 2);
      char *p = tmp.c_str();
      for (char *s = tmp2.c_str(); *s ; s++) {
         if (*s == '%' || *s == '_' || *s == '\\') {
            *p = '\\';
            p++;
         }
         *p = *s;
         p++;
      }
      *p = '\0';
      tmp.strcat("%");

      size_t len = strlen(tmp.c_str());
      tmp2.check_size((len+1) * 2);
      db_escape_string(jcr, db, tmp2.c_str(), tmp.c_str(), len);

      if (init) {
         query.strcat(" UNION ");
      }

      Mmsg(tmp, "SELECT Job.JobId, JobTDate, File.FileIndex, File.FilenameId, "
                        "File.PathId, FileId "
                   "FROM Path JOIN File USING (PathId) JOIN Job USING (JobId) "
                  "WHERE Path.Path LIKE '%s' AND File.JobId IN (%s) ",
           tmp2.c_str(), jobids);
      query.strcat(tmp.c_str());
      init = true;

      query.strcat(" UNION ");

      /* A directory can have files from a BaseJob */
      Mmsg(tmp, "SELECT File.JobId, JobTDate, BaseFiles.FileIndex, "
                        "File.FilenameId, File.PathId, BaseFiles.FileId "
                   "FROM BaseFiles "
                        "JOIN File USING (FileId) "
                        "JOIN Job ON (BaseFiles.JobId = Job.JobId) "
                        "JOIN Path USING (PathId) "
                  "WHERE Path.Path LIKE '%s' AND BaseFiles.JobId IN (%s) ",
           tmp2.c_str(), jobids);
      query.strcat(tmp.c_str());
   }

   /* expect jobid,fileindex */
   prev_jobid=0;
   while (get_next_id_from_list(&hardlink, &jobid) == 1) {
      if (get_next_id_from_list(&hardlink, &id) != 1) {
         Dmsg0(dbglevel, "hardlink should be two by two\n");
         goto bail_out;
      }
      if (jobid != prev_jobid) { /* new job */
         if (prev_jobid == 0) {  /* first jobid */
            if (init) {
               query.strcat(" UNION ");
            }
         } else {               /* end last job, start new one */
            tmp.strcat(") UNION ");
            query.strcat(tmp.c_str());
         }
         Mmsg(tmp,   "SELECT Job.JobId, JobTDate, FileIndex, FilenameId, "
                            "PathId, FileId "
                       "FROM File JOIN Job USING (JobId) WHERE JobId = %lld "
                        "AND FileIndex IN (%lld", jobid, id);
         prev_jobid = jobid;

      } else {                  /* same job, add new findex */
         Mmsg(tmp2, ", %lld", id);
         tmp.strcat(tmp2.c_str());
      }
   }

   if (prev_jobid != 0) {       /* end last job */
      tmp.strcat(") ");
      query.strcat(tmp.c_str());
      init = true;
   }

   Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());

   if (!db_sql_query(db, query.c_str())) {
      Dmsg0(dbglevel, "Can't execute q\n");
      goto bail_out;
   }

   Mmsg(query, sql_bvfs_select[db_get_type_index(db)],
        output_table, output_table, output_table);

   /* TODO: handle jobid filter */
   Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());
   if (!db_sql_query(db, query.c_str())) {
      Dmsg0(dbglevel, "Can't execute q\n");
      goto bail_out;
   }

   /* MySQL need it */
   if (db_get_type_index(db) == SQL_TYPE_MYSQL) {
      Mmsg(query, "CREATE INDEX idx_%s ON %s (JobId)",
           output_table, output_table);
      Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());
      if (!db_sql_query(db, query.c_str())) {
         Dmsg0(dbglevel, "Can't execute q\n");
         goto bail_out;
      }
   }

   retval = true;

bail_out:
   Mmsg(query, "DROP TABLE btemp%s", output_table);
   db_sql_query(db, query.c_str());
   db_unlock(db);
   return retval;
}

#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
