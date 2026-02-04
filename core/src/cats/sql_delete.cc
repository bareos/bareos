/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, December 2000
/**
 * @file
 * BAREOS Catalog Database Delete record interface routines
 */

#include "include/bareos.h"

#if HAVE_POSTGRESQL

#  include "cats.h"
#  include "lib/edit.h"

/* -----------------------------------------------------------------------
 *
 *   Generic Routines (or almost generic)
 *
 * -----------------------------------------------------------------------
 */

/**
 * Delete Pool record, must also delete all associated
 *  Media records.
 *
 *  Returns: false on error
 *           true on success
 *           PoolId = number of Pools deleted (should be 1)
 *           NumVols = number of Media records deleted
 */
bool BareosDb::DeletePoolRecord(JobControlRecord* jcr, PoolDbRecord* pr)
{
  SQL_ROW row;
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLocker _{this};
  EscapeString(jcr, esc, pr->Name, strlen(pr->Name));
  Mmsg(cmd, "SELECT PoolId FROM Pool WHERE Name='%s'", esc);
  Dmsg1(10, "selectpool: %s\n", cmd);

  pr->PoolId = pr->NumVols = 0;

  if (QueryDb(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows == 0) {
      Mmsg(errmsg, T_("No pool record %s exists\n"), pr->Name);
      SqlFreeResult();
      return false;
    } else if (num_rows != 1) {
      Mmsg(errmsg, T_("Expecting one pool record, got %d\n"), num_rows);
      SqlFreeResult();
      return false;
    }
    if ((row = SqlFetchRow()) == NULL) {
      Mmsg1(errmsg, T_("Error fetching row %s\n"), sql_strerror());
      return false;
    }
    pr->PoolId = str_to_int64(row[0]);
    SqlFreeResult();
  }

  /* Delete Media owned by this pool */
  Mmsg(cmd, "DELETE FROM Media WHERE Media.PoolId = %d", pr->PoolId);

  pr->NumVols = DeleteDb(jcr, cmd);
  Dmsg1(200, "Deleted %d Media records\n", pr->NumVols);

  /* Delete Pool */
  Mmsg(cmd, "DELETE FROM Pool WHERE Pool.PoolId = %d", pr->PoolId);
  pr->PoolId = DeleteDb(jcr, cmd);
  Dmsg1(200, "Deleted %d Pool records\n", pr->PoolId);

  return true;
}

#  define MAX_DEL_LIST_LEN 1000000

struct s_del_ctx {
  std::vector<JobId_t> ids;
};

/**
 * Called here to make in memory list of JobIds to be
 *  deleted. The in memory list will then be transversed
 *  to issue the SQL DELETE commands.  Note, the list
 *  is allowed to get to MAX_DEL_LIST_LEN to limit the
 *  maximum malloc'ed memory.
 */
static int DeleteHandler(void* ctx, int, char** row)
{
  auto* del = static_cast<s_del_ctx*>(ctx);

  if (del->ids.size() == MAX_DEL_LIST_LEN) { return 1; }

  del->ids.push_back(str_to_int64(row[0]));

  return 0;
}

/**
 * Delete all NullJobMedia records for this job
 * Returns: -1 on failure
 *          n>=0 on success,
 *               where n is the number of rows affected
 */
int BareosDb::DeleteNullJobmediaRecords(JobControlRecord* jcr,
                                        std::uint32_t jobid)
{
  DbLocker _{this};

  Mmsg(cmd,
       "DELETE FROM jobmedia WHERE jobid=%u AND firstindex=0 AND lastindex=0",
       jobid);
  Dmsg1(200, "DeleteNullJobmediaRecords: %s\n", cmd);

  int numrows = DeleteDb(jcr, cmd);

  return numrows;
}

/**
 * This routine will purge (delete) all records
 * associated with a particular Volume. It will
 * not delete the media record itself.
 * TODO: This function is broken and it doesn't purge
 *       File, Log, ...
 *       We call it from relabel and delete volume=, both ensure
 *       that the volume is properly purged.
 */
static int DoMediaPurge(BareosDb* mdb, MediaDbRecord* mr)
{
  char ed1[50];
  struct s_del_ctx del{};
  PoolMem query(PM_MESSAGE);

  Mmsg(query, "SELECT JobId from JobMedia WHERE MediaId=%d", mr->MediaId);

  if (mr->VolJobs) {
    del.ids.reserve(100);
  } else if (mr->VolJobs > MAX_DEL_LIST_LEN) {
    del.ids.reserve(MAX_DEL_LIST_LEN);
  } else {
    del.ids.reserve(mr->VolJobs);
  }

  mdb->SqlQuery(query.c_str(), DeleteHandler, (void*)&del);

  for (auto jobid : del.ids) {
    Dmsg1(400, "Delete JobId=%d\n", jobid);
    Mmsg(query, "DELETE FROM Job WHERE JobId=%s", edit_int64(jobid, ed1));
    mdb->SqlQuery(query.c_str());

    Mmsg(query, "DELETE FROM File WHERE JobId=%s", edit_int64(jobid, ed1));
    mdb->SqlQuery(query.c_str());

    Mmsg(query, "DELETE FROM JobMedia WHERE JobId=%s", edit_int64(jobid, ed1));
    mdb->SqlQuery(query.c_str());
  }

  return 1;
}

/**
 * Delete Media record and all records that are associated with it.
 * Returns: false on error
 *          true on success
 */
bool BareosDb::DeleteMediaRecord(JobControlRecord* jcr, MediaDbRecord* mr)
{
  DbLocker _{this};
  if (mr->MediaId == 0 && !GetMediaRecord(jcr, mr)) { return false; }
  /* Do purge if not already purged */
  if (!bstrcmp(mr->VolStatus, "Purged")) {
    /* Delete associated records */
    DoMediaPurge(this, mr);
  }

  Mmsg(cmd, "DELETE FROM Media WHERE MediaId=%d", mr->MediaId);
  return DeleteDb(jcr, cmd) != -1;
}

void BareosDb::PurgeFiles(const char* jobids)
{
  if (strcmp(jobids, "") == 0) {
    Dmsg0(100, "No jobids to use for purging files\n");
    return;
  }

  PoolMem query(PM_MESSAGE);

  Mmsg(query, "DELETE FROM File WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());

  Mmsg(query, "UPDATE Job SET PurgedFiles=1 WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());
}

void BareosDb::PurgeJobs(const char* jobids)
{
  PoolMem query(PM_MESSAGE);

  if (strcmp(jobids, "") == 0) {
    Dmsg0(100, "No jobids to purge\n");
    return;
  }

  /* Delete (or purge) records associated with the job */
  PurgeFiles(jobids);

  Mmsg(query, "DELETE FROM JobMedia WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());

  Mmsg(query, "DELETE FROM Log WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());

  Mmsg(query, "DELETE FROM RestoreObject WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());

  Mmsg(query, "DELETE FROM PathVisibility WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());

  Mmsg(query, "DELETE FROM NDMPJobEnvironment WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());

  Mmsg(query, "DELETE FROM JobStats WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());

  UpgradeCopies(jobids);

  /* Now remove the Job record itself */
  Mmsg(query, "DELETE FROM Job WHERE JobId IN (%s)", jobids);
  SqlQuery(query.c_str());
}
#endif /* HAVE_POSTGRESQL */
