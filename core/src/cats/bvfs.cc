/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2009-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Planets Communications B.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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
/**
 * @file
 * bareos virtual filesystem layer
 */
#include "include/bareos.h"

#if HAVE_POSTGRESQL

#  include "cats/cats.h"
#  include "cats/sql.h"
#  include "lib/htable.h"
#  include "cats/bvfs.h"
#  include "lib/edit.h"

#  include <unordered_set>

#  define dbglevel 10
#  define dbglevel_sql 15

class pathid_cache {
  std::unordered_set<uint64_t> cache;

 public:
  void insert(uint64_t id) { cache.emplace(id); }
  bool lookup(uint64_t id) const { return cache.find(id) != cache.end(); }
};

// Generic path handlers used for database queries.
static int GetPathHandler(void* ctx, int, char** row)
{
  PoolMem* buf = (PoolMem*)ctx;
  PmStrcpy(*buf, row[0]);
  return 0;
}

static int PathHandler(void* ctx, int fields, char** row)
{
  Bvfs* fs = (Bvfs*)ctx;
  return fs->_handlePath(ctx, fields, row);
}

// BVFS specific methods part of the BareosDb database abstraction.
void BareosDb::BuildPathHierarchy(JobControlRecord* jcr,
                                  pathid_cache& ppathid_cache,
                                  char* org_pathid,
                                  char* new_path)
{
  uint64_t pathid = str_to_int64(org_pathid);
  char* bkp = path;

  Dmsg1(dbglevel, "BuildPathHierarchy(%s)\n", new_path);

  /* Does the ppathid exist for this? use a memory cache ...
   * In order to avoid the full loop, we consider that if a dir is already in
   * the PathHierarchy table, then there is no need to calculate all the
   * hierarchy */
  while (new_path && *new_path) {
    if (ppathid_cache.lookup(pathid)) {
      /* It's already in the cache.  We can leave, no time to waste here,
       * all the parent dirs have already been done */
      goto bail_out;
    } else {
      Mmsg(cmd, "SELECT PPathId FROM PathHierarchy WHERE PathId = %" PRIu64,
           pathid);

      if (!QueryDb(jcr, cmd)) { goto bail_out; /* Query failed, just leave */ }

      if (SqlNumRows() > 0) {
        ppathid_cache.insert(pathid);
        /* This dir was in the db ...
         * It means we can leave, the tree has already been built for
         * this dir
         */
        goto bail_out;
      } else {
        // Search or create parent PathId in Path table
        path = bvfs_parent_dir(new_path);
        pnl = strlen(path);

        AttributesDbRecord parent;
        if (!CreatePathRecord(jcr, &parent)) { goto bail_out; }
        ppathid_cache.insert(pathid);

        Mmsg(cmd,
             "INSERT INTO PathHierarchy (PathId, PPathId) VALUES (%" PRIu64
             ",%" PRIu64 ")",
             pathid, (uint64_t)parent.PathId);
        if (!InsertDb(jcr, cmd)) {
          goto bail_out; /* Can't insert the record, just leave */
        }

        pathid = parent.PathId;
        /* continue with parent directory */
        new_path = path;
      }
    }
  }

bail_out:
  path = bkp;
  fnl = 0;
}

/**
 * Internal function to update path_hierarchy cache with a shared pathid cache
 * return Error 0
 *        OK    1
 */
bool BareosDb::UpdatePathHierarchyCache(JobControlRecord* jcr,
                                        pathid_cache& ppathid_cache,
                                        JobId_t JobId)
{
  Dmsg0(dbglevel, "UpdatePathHierarchyCache()\n");
  bool retval = false;
  uint32_t num;
  char jobid[50];
  edit_uint64(JobId, jobid);

  DbLocker _{this};
  StartTransaction(jcr);

  Mmsg(cmd, "SELECT 1 FROM Job WHERE JobId = %s AND HasCache=1", jobid);

  if (!QueryDb(jcr, cmd) || SqlNumRows() > 0) {
    Dmsg1(dbglevel, "Already computed %d\n", (uint32_t)JobId);
    retval = true;
    goto bail_out;
  }

  /* prevent from DB lock waits when already in progress */
  Mmsg(cmd, "SELECT 1 FROM Job WHERE JobId = %s AND HasCache=-1", jobid);

  if (!QueryDb(jcr, cmd) || SqlNumRows() > 0) {
    Dmsg1(dbglevel, "already in progress %d\n", (uint32_t)JobId);
    retval = false;
    goto bail_out;
  }

  /* set HasCache to -1 in Job (in progress) */
  Mmsg(cmd, "UPDATE Job SET HasCache=-1 WHERE JobId=%s", jobid);
  UpdateDb(jcr, cmd);

  /* need to COMMIT here to ensure that other concurrent .bvfs_update runs
   * see the current HasCache value. A new transaction must only be started
   * after having finished PathHierarchy processing, otherwise prevention
   * from duplicate key violations in BuildPathHierarchy() will not work. */
  EndTransaction(jcr);

  /* Inserting path records for JobId */
  Mmsg(cmd,
       "INSERT INTO PathVisibility (PathId, JobId) "
       "SELECT DISTINCT PathId, JobId "
       "FROM (SELECT PathId, JobId FROM File WHERE JobId = %s "
       ") AS B",
       jobid);

  if (!QueryDb(jcr, cmd)) {
    Dmsg1(dbglevel, "Can't fill PathVisibility %d\n", (uint32_t)JobId);
    goto bail_out;
  }

  /* Now we have to do the directory recursion stuff to determine missing
   * visibility.
   * We try to avoid recursion, to be as fast as possible.
   * We also only work on not already hierarchised directories ... */
  Mmsg(cmd,
       "SELECT PathVisibility.PathId, Path "
       "FROM PathVisibility "
       "JOIN Path ON (PathVisibility.PathId = Path.PathId) "
       "LEFT JOIN PathHierarchy "
       "ON (PathVisibility.PathId = PathHierarchy.PathId) "
       "WHERE PathVisibility.JobId = %s "
       "AND PathHierarchy.PathId IS NULL "
       "ORDER BY Path",
       jobid);

  if (!QueryDb(jcr, cmd)) {
    Dmsg1(dbglevel, "Can't get new Path %d\n", (uint32_t)JobId);
    goto bail_out;
  }

  /* TODO: I need to reuse the DB connection without emptying the result
   * So, now i'm copying the result in memory to be able to query the
   * catalog descriptor again.
   */
  num = SqlNumRows();
  if (num > 0) {
    char** result = (char**)malloc(num * 2 * sizeof(char*));

    SQL_ROW row;
    int i = 0;
    while ((row = SqlFetchRow())) {
      result[i++] = strdup(row[0]);
      result[i++] = strdup(row[1]);
    }

    /* The PathHierarchy table needs exclusive write lock here to
     * prevent from unique key constraint violations (PostgreSQL)
     * or duplicate entry errors (MySQL/MariaDB) when multiple
     * bvfs update operations are run simultaneously.
     */
    FillQuery(cmd, SQL_QUERY::bvfs_lock_pathhierarchy_0);
    if (!QueryDb(jcr, cmd)) { goto bail_out; }

    i = 0;
    while (num > 0) {
      BuildPathHierarchy(jcr, ppathid_cache, result[i], result[i + 1]);
      free(result[i++]);
      free(result[i++]);
      num--;
    }
    free(result);

    FillQuery(cmd, SQL_QUERY::bvfs_unlock_tables_0);
    if (!QueryDb(jcr, cmd)) { goto bail_out; }
  }

  StartTransaction(jcr);

  FillQuery(cmd, SQL_QUERY::bvfs_update_path_visibility_3, jobid, jobid, jobid);

  do {
    retval = QueryDb(jcr, cmd);
  } while (retval && SqlAffectedRows() > 0);

  Mmsg(cmd, "UPDATE Job SET HasCache=1 WHERE JobId=%s", jobid);
  UpdateDb(jcr, cmd);

bail_out:
  EndTransaction(jcr);

  return retval;
}

void BareosDb::BvfsUpdateCache(JobControlRecord* jcr)
{
  uint32_t nb = 0;
  db_list_ctx jobids_list;

  DbLocker _{this};

  Mmsg(cmd,
       "SELECT JobId from Job "
       "WHERE HasCache = 0 "
       "AND Type IN ('B','A','a') AND JobStatus IN ('T', 'W', 'f', 'A') "
       "ORDER BY JobId");
  SqlQuery(cmd, DbListHandler, &jobids_list);

  BvfsUpdatePathHierarchyCache(jcr, jobids_list.GetAsString().c_str());

  StartTransaction(jcr);
  Dmsg0(dbglevel, "Cleaning pathvisibility\n");
  Mmsg(cmd,
       "DELETE FROM PathVisibility "
       "WHERE NOT EXISTS "
       "(SELECT 1 FROM Job WHERE JobId=PathVisibility.JobId)");
  nb = DeleteDb(jcr, cmd);
  Dmsg1(dbglevel, "Affected row(s) = %d\n", nb);
  EndTransaction(jcr);
}

// Update the bvfs cache for given jobids (1,2,3,4)
bool BareosDb::BvfsUpdatePathHierarchyCache(JobControlRecord* jcr,
                                            const char* jobids)
{
  const char* p;
  int status;
  JobId_t JobId;
  bool retval = true;
  pathid_cache ppathid_cache;

  p = jobids;
  while (1) {
    status = GetNextJobidFromList(&p, &JobId);
    if (status < 0) { goto bail_out; }

    if (status == 0) {
      // We reached the end of the list.
      goto bail_out;
    }

    Dmsg1(dbglevel, "Updating cache for %" PRIu32 "\n", JobId);
    if (!UpdatePathHierarchyCache(jcr, ppathid_cache, JobId)) {
      retval = false;
    }
  }

bail_out:
  return retval;
}

int BareosDb::BvfsLsDirs(PoolMem& query, void* ctx)
{
  int nb_record = 0;

  Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());

  DbLocker _{this};

  SqlQuery(query.c_str(), PathHandler, ctx);
  // FIXME: SqlNumRows() is always 0 after SqlQuery.
  // nb_record = SqlNumRows();

  return nb_record;
}

int BareosDb::BvfsBuildLsFileQuery(PoolMem& query,
                                   DB_RESULT_HANDLER* ResultHandler,
                                   void* ctx)
{
  int nb_record = 0;

  Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());

  DbLocker _{this};
  SqlQuery(query.c_str(), ResultHandler, ctx);
  // FIXME: SqlNumRows() is always 0 after SqlQuery.
  // nb_record = SqlNumRows();

  return nb_record;
}

static int ResultHandler(void*, int fields, char** row)
{
  Dmsg1(100, "ResultHandler(*,%d,**)", fields);
  if (fields == 4) {
    Pmsg4(0, "%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3]);
  } else if (fields == 5) {
    Pmsg5(0, "%s\t%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3], row[4]);
  } else if (fields == 6) {
    Pmsg6(0, "%s\t%s\t%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3], row[4],
          row[5]);
  } else if (fields == 7) {
    Pmsg7(0, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", row[0], row[1], row[2], row[3],
          row[4], row[5], row[6]);
  }
  return 0;
}

Bvfs::Bvfs(JobControlRecord* j, BareosDb* mdb)
{
  jcr = j;
  jcr->IncUseCount();
  db = mdb; /* need to inc ref count */
  jobids = GetPoolMemory(PM_NAME);
  prev_dir = GetPoolMemory(PM_NAME);
  pattern = GetPoolMemory(PM_NAME);
  *jobids = *prev_dir = *pattern = 0;
  pwd_id = 0;
  see_copies = false;
  see_all_versions = false;
  limit = 1000;
  offset = 0;
  attr = new_attr(jcr);
  list_entries = ResultHandler;
  user_data = this;
}

Bvfs::~Bvfs()
{
  FreePoolMemory(jobids);
  FreePoolMemory(pattern);
  FreePoolMemory(prev_dir);
  FreeAttr(attr);
  jcr->DecUseCount();
}

void Bvfs::SetJobids(char* ids) { PmStrcpy(jobids, ids); }

/*
 * Return the parent_dir with the trailing "/".
 * It updates the given string.
 *
 * Unix:
 * dir=/tmp/toto/
 * dir=/tmp/
 * dir=/
 * dir=
 *
 * Windows:
 * dir=c:/Programs/Bareos/
 * dir=c:/Programs/
 * dir=c:/
 * dir=
 *
 * Plugins:
 * dir=@bpipe@/data.dat
 * dir=@bpipe@/
 * dir=
 */
char* bvfs_parent_dir(char* path)
{
  char* p = path;
  int len = strlen(path) - 1;

  /* windows directory / */
  if (len == 2 && B_ISALPHA(path[0]) && path[1] == ':' && path[2] == '/') {
    len = 0;
    path[0] = '\0';
  }

  /* if path is a directory, remove last / */
  if ((len >= 0) && (path[len] == '/')) { path[len] = '\0'; }

  if (len > 0) {
    p += len;
    while (p > path && !IsPathSeparator(*p)) { p--; }
    if (IsPathSeparator(*p) and (len >= 1)) {
      /* Terminate the string after the "/".
       * Do this instead of overwritting the "/"
       * to keep the root directory "/" as a separate path. */
      p[1] = '\0';
    } else {
      /* path did not start with a "/".
       * This can be the case for plugin results. */
      p[0] = '\0';
    }
  }

  return path;
}

/* Return the basename of the path with the trailing /
 * TODO: see in the rest of bareos if we don't have
 * this function already (e.g. last_path_separator)
 */
char* bvfs_basename_dir(char* path)
{
  char* p = path;
  int len = strlen(path) - 1;

  if (path[len] == '/') { /* if directory, skip last / */
    len -= 1;
  }

  if (len > 0) {
    p += len;
    while (p > path && !IsPathSeparator(*p)) { p--; }
    if (*p == '/') { p++; /* skip first / */ }
  }
  return p;
}

// Change the current directory, returns true if the path exists
bool Bvfs::ChDir(const char* path)
{
  DbLocker _{db};
  ChDir(db->GetPathRecord(jcr, path));

  return pwd_id != 0;
}

void Bvfs::GetAllFileVersions(const char* path,
                              const char* fname,
                              const char* client)
{
  DBId_t pathid = 0;
  char path_esc[MAX_ESCAPE_NAME_LENGTH];

  db->EscapeString(jcr, path_esc, path, strlen(path));
  pathid = db->GetPathRecord(jcr, path_esc);
  GetAllFileVersions(pathid, fname, client);
}

/**
 * Get all file versions for a specified client
 * TODO: Handle basejobs using different client
 */
void Bvfs::GetAllFileVersions(DBId_t pathid,
                              const char* fname,
                              const char* client)
{
  char ed1[50];
  char fname_esc[MAX_ESCAPE_NAME_LENGTH];
  char client_esc[MAX_ESCAPE_NAME_LENGTH];
  PoolMem query(PM_MESSAGE);
  PoolMem filter(PM_MESSAGE);

  Dmsg3(dbglevel, "GetAllFileVersions(%" PRIdbid ", %s, %s)\n", pathid, fname,
        client);

  if (see_copies) {
    Mmsg(filter, " AND Job.Type IN ('C', 'B', 'A', 'a') ");
  } else {
    Mmsg(filter, " AND Job.Type IN ('B', 'A', 'a') ");
  }

  db->EscapeString(jcr, fname_esc, fname, strlen(fname));
  db->EscapeString(jcr, client_esc, client, strlen(client));

  db->FillQuery(query, BareosDb::SQL_QUERY::bvfs_versions_6, fname_esc,
                edit_uint64(pathid, ed1), client_esc, filter.c_str(), limit,
                offset);
  db->SqlQuery(query.c_str(), list_entries, user_data);
}

DBId_t Bvfs::get_root()
{
  int p;

  DbLocker _{db};
  p = db->GetPathRecord(jcr, "");

  return p;
}

int Bvfs::_handlePath(void*, int fields, char** row)
{
  if (BvfsIsDir(row)) {
    // Can have the same path 2 times
    if (!bstrcmp(row[BVFS_Name], prev_dir)) {
      PmStrcpy(prev_dir, row[BVFS_Name]);
      return list_entries(user_data, fields, row);
    }
  }
  return 0;
}

/* Returns true if we have dirs to read */
bool Bvfs::ls_dirs()
{
  char pathid[50];
  PoolMem special_dirs_query(PM_MESSAGE);
  PoolMem filter(PM_MESSAGE);
  PoolMem sub_dirs_query(PM_MESSAGE);
  PoolMem union_query(PM_MESSAGE);

  Dmsg1(dbglevel, "ls_dirs(%" PRIdbid ")\n", pwd_id);

  if (*jobids == 0) { return false; }

  /* convert pathid to string */
  edit_uint64(pwd_id, pathid);

  // The sql query displays same directory multiple time, take the first one
  *prev_dir = 0;

  db->FillQuery(special_dirs_query, BareosDb::SQL_QUERY::bvfs_ls_special_dirs_3,
                pathid, pathid, jobids);

  if (*pattern) {
    db->FillQuery(filter, BareosDb::SQL_QUERY::match_query, pattern);
  }
  db->FillQuery(sub_dirs_query, BareosDb::SQL_QUERY::bvfs_ls_sub_dirs_5, pathid,
                jobids, jobids, filter.c_str(), jobids);

  db->FillQuery(union_query, BareosDb::SQL_QUERY::bvfs_lsdirs_4,
                special_dirs_query.c_str(), sub_dirs_query.c_str(), limit,
                offset);

  /* FIXME: BvfsLsDirs does not return number of results */
  nb_record = db->BvfsLsDirs(union_query, this);

  return true;
}

static void build_ls_files_query(JobControlRecord*,
                                 BareosDb* db,
                                 PoolMem& query,
                                 const char* JobId,
                                 const char* PathId,
                                 const char* filter,
                                 int64_t limit,
                                 int64_t offset)
{
  db->FillQuery(query, BareosDb::SQL_QUERY::bvfs_list_files,
                JobId, PathId,
                filter,
                limit, offset);
}

// Returns true if we have files to read
bool Bvfs::ls_files()
{
  char pathid[50];
  PoolMem filter(PM_MESSAGE);
  PoolMem query(PM_MESSAGE);

  Dmsg1(dbglevel, "ls_files(%" PRIdbid ")\n", pwd_id);
  if (*jobids == 0) { return false; }

  if (!pwd_id) { ChDir(get_root()); }

  edit_uint64(pwd_id, pathid);
  if (*pattern) {
    db->FillQuery(filter, BareosDb::SQL_QUERY::match_query2, pattern);
  }

  build_ls_files_query(jcr, db, query, jobids, pathid, filter.c_str(), limit,
                       offset);
  nb_record = db->BvfsBuildLsFileQuery(query, list_entries, user_data);

  return nb_record == limit;
}

/**
 * Return next Id from comma separated list
 *
 * Returns:
 *   1 if next Id returned
 *   0 if no more Ids are in list
 *  -1 there is an error
 * TODO: merge with GetNextJobidFromList() and GetNextDbidFromList()
 */
static int GetNextIdFromList(char** p, int64_t* Id)
{
  const int maxlen = 30;
  char id[maxlen + 1];
  char* q = *p;

  id[0] = 0;
  for (int i = 0; i < maxlen; i++) {
    if (*q == 0) {
      break;
    } else if (*q == ',') {
      q++;
      break;
    }
    id[i] = *q++;
    id[i + 1] = 0;
  }
  if (id[0] == 0) {
    return 0;
  } else if (!Is_a_number(id)) {
    return -1; /* error */
  }
  *p = q;
  *Id = str_to_int64(id);
  return 1;
}

static bool CheckTemp(char* output_table)
{
  if (output_table[0] == 'b' && output_table[1] == '2'
      && IsAnInteger(output_table + 2)) {
    return true;
  }
  return false;
}

void Bvfs::clear_cache()
{
  db->SqlQuery(BareosDb::SQL_QUERY::bvfs_clear_cache_0);
}

bool Bvfs::DropRestoreList(char* output_table)
{
  PoolMem query(PM_MESSAGE);
  if (CheckTemp(output_table)) {
    Mmsg(query, "DROP TABLE IF EXISTS %s", output_table);
    db->SqlQuery(query.c_str());
    return true;
  }
  return false;
}

bool Bvfs::compute_restore_list(char* fileid,
                                char* dirid,
                                char* hardlink,
                                char* output_table)
{
  PoolMem query(PM_MESSAGE);
  PoolMem tmp(PM_MESSAGE), tmp2(PM_MESSAGE);
  int64_t id, jobid, prev_jobid;
  bool init = false;
  bool retval = false;

  /* check args */
  if ((*fileid && !Is_a_number_list(fileid))
      || (*dirid && !Is_a_number_list(dirid))
      || (*hardlink && !Is_a_number_list(hardlink))
      || (!*hardlink && !*fileid && !*dirid && !*hardlink)) {
    return false;
  }
  if (!CheckTemp(output_table)) { return false; }

  DbLocker _{db};

  /* Cleanup old tables first */
  Mmsg(query, "DROP TABLE IF EXISTS btemp%s", output_table);
  db->SqlQuery(query.c_str());

  Mmsg(query, "DROP TABLE IF EXISTS %s", output_table);
  db->SqlQuery(query.c_str());

  Mmsg(query, "CREATE TABLE btemp%s AS ", output_table);

  if (*fileid) { /* Select files with their direct id */
    init = true;
    Mmsg(tmp,
         "SELECT Job.JobId, JobTDate, FileIndex, File.Name, "
         "PathId, FileId "
         "FROM File JOIN Job USING (JobId) WHERE FileId IN (%s) ",
         fileid);
    PmStrcat(query, tmp.c_str());
  }

  /* Add a directory content */
  while (GetNextIdFromList(&dirid, &id) == 1) {
    Mmsg(tmp, "SELECT Path FROM Path WHERE PathId=%" PRId64, id);

    if (!db->SqlQuery(tmp.c_str(), GetPathHandler, (void*)&tmp2)) {
      Dmsg0(dbglevel, "Can't search for path\n");
      /* print error */
      goto bail_out;
    }
    if (bstrcmp(tmp2.c_str(), "")) { /* path not found */
      Dmsg3(dbglevel, "Path not found %" PRId64 " q=%s s=%s\n", id, tmp.c_str(),
            tmp2.c_str());
      break;
    }
    /* escape % and _ for LIKE search */
    tmp.check_size((strlen(tmp2.c_str()) + 1) * 2);
    char* p = tmp.c_str();
    for (char* s = tmp2.c_str(); *s; s++) {
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
    tmp2.check_size((len + 1) * 2);
    db->EscapeString(jcr, tmp2.c_str(), tmp.c_str(), len);

    if (init) { query.strcat(" UNION "); }

    Mmsg(tmp,
         "SELECT Job.JobId, JobTDate, File.FileIndex, File.Name, "
         "File.PathId, FileId "
         "FROM Path JOIN File USING (PathId) JOIN Job USING (JobId) "
         "WHERE Path.Path LIKE '%s' AND File.JobId IN (%s) ",
         tmp2.c_str(), jobids);
    query.strcat(tmp.c_str());
    init = true;
  }

  /* expect jobid,fileindex */
  prev_jobid = 0;
  while (GetNextIdFromList(&hardlink, &jobid) == 1) {
    if (GetNextIdFromList(&hardlink, &id) != 1) {
      Dmsg0(dbglevel, "hardlink should be two by two\n");
      goto bail_out;
    }
    if (jobid != prev_jobid) { /* new job */
      if (prev_jobid == 0) {   /* first jobid */
        if (init) { query.strcat(" UNION "); }
      } else { /* end last job, start new one */
        tmp.strcat(") UNION ");
        query.strcat(tmp.c_str());
      }
      Mmsg(tmp,
           "SELECT Job.JobId, JobTDate, FileIndex, Name, PathId, FileId"
           " FROM File JOIN Job USING (JobId) WHERE JobId = %" PRId64
           " AND FileIndex IN (%" PRId64,
           jobid, id);
      prev_jobid = jobid;

    } else { /* same job, add new findex */
      Mmsg(tmp2, ", %" PRId64, id);
      tmp.strcat(tmp2.c_str());
    }
  }

  if (prev_jobid != 0) { /* end last job */
    tmp.strcat(") ");
    query.strcat(tmp.c_str());
    init = true;
  }

  Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());

  if (!db->SqlQuery(query.c_str())) {
    Dmsg0(dbglevel, "Can't execute q\n");
    goto bail_out;
  }

  db->FillQuery(query, BareosDb::SQL_QUERY::bvfs_select, output_table,
                output_table, output_table);

  /* TODO: handle jobid filter */
  Dmsg1(dbglevel_sql, "q=%s\n", query.c_str());
  if (!db->SqlQuery(query.c_str())) {
    Dmsg0(dbglevel, "Can't execute q\n");
    goto bail_out;
  }

  retval = true;

bail_out:
  Mmsg(query, "DROP TABLE IF EXISTS btemp%s", output_table);
  db->SqlQuery(query.c_str());
  return retval;
}

#endif /* HAVE_POSTGRESQL */
