/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, December 2000
 */
/**
 * @file
 * BAREOS Catalog Database Delete record interface routines
 */

#include "include/bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "lib/edit.h"

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
  bool retval = false;
  SQL_ROW row;
  int num_rows;
  char esc[MAX_ESCAPE_NAME_LENGTH];

  DbLock(this);
  EscapeString(jcr, esc, pr->Name, strlen(pr->Name));
  Mmsg(cmd, "SELECT PoolId FROM Pool WHERE Name='%s'", esc);
  Dmsg1(10, "selectpool: %s\n", cmd);

  pr->PoolId = pr->NumVols = 0;

  if (QUERY_DB(jcr, cmd)) {
    num_rows = SqlNumRows();
    if (num_rows == 0) {
      Mmsg(errmsg, _("No pool record %s exists\n"), pr->Name);
      SqlFreeResult();
      goto bail_out;
    } else if (num_rows != 1) {
      Mmsg(errmsg, _("Expecting one pool record, got %d\n"), num_rows);
      SqlFreeResult();
      goto bail_out;
    }
    if ((row = SqlFetchRow()) == NULL) {
      Mmsg1(errmsg, _("Error fetching row %s\n"), sql_strerror());
      goto bail_out;
    }
    pr->PoolId = str_to_int64(row[0]);
    SqlFreeResult();
  }

  /* Delete Media owned by this pool */
  Mmsg(cmd, "DELETE FROM Media WHERE Media.PoolId = %d", pr->PoolId);

  pr->NumVols = DELETE_DB(jcr, cmd);
  Dmsg1(200, "Deleted %d Media records\n", pr->NumVols);

  /* Delete Pool */
  Mmsg(cmd, "DELETE FROM Pool WHERE Pool.PoolId = %d", pr->PoolId);
  pr->PoolId = DELETE_DB(jcr, cmd);
  Dmsg1(200, "Deleted %d Pool records\n", pr->PoolId);

  retval = true;

bail_out:
  DbUnlock(this);
  return retval;
}

#define MAX_DEL_LIST_LEN 1000000

struct s_del_ctx {
  JobId_t* JobId;
  int num_ids; /* ids stored */
  int max_ids; /* size of array */
  int num_del; /* number deleted */
  int tot_ids; /* total to process */
};

/**
 * Called here to make in memory list of JobIds to be
 *  deleted. The in memory list will then be transversed
 *  to issue the SQL DELETE commands.  Note, the list
 *  is allowed to get to MAX_DEL_LIST_LEN to limit the
 *  maximum malloc'ed memory.
 */
static int DeleteHandler(void* ctx, int num_fields, char** row)
{
  struct s_del_ctx* del = (struct s_del_ctx*)ctx;

  if (del->num_ids == MAX_DEL_LIST_LEN) { return 1; }
  if (del->num_ids == del->max_ids) {
    del->max_ids = (del->max_ids * 3) / 2;
    del->JobId = (JobId_t*)brealloc(del->JobId, sizeof(JobId_t) * del->max_ids);
  }
  del->JobId[del->num_ids++] = (JobId_t)str_to_int64(row[0]);
  return 0;
}


/**
 * This routine will purge (delete) all records
 * associated with a particular Volume. It will
 * not delete the media record itself.
 * TODO: This function is broken and it doesn't purge
 *       File, BaseFiles, Log, ...
 *       We call it from relabel and delete volume=, both ensure
 *       that the volume is properly purged.
 */
static int DoMediaPurge(BareosDb* mdb, MediaDbRecord* mr)
{
  int i;
  char ed1[50];
  struct s_del_ctx del;
  PoolMem query(PM_MESSAGE);

  del.num_ids = 0;
  del.tot_ids = 0;
  del.num_del = 0;
  del.max_ids = 0;

  Mmsg(query, "SELECT JobId from JobMedia WHERE MediaId=%d", mr->MediaId);

  del.max_ids = mr->VolJobs;
  if (del.max_ids < 100) {
    del.max_ids = 100;
  } else if (del.max_ids > MAX_DEL_LIST_LEN) {
    del.max_ids = MAX_DEL_LIST_LEN;
  }
  del.JobId = (JobId_t*)malloc(sizeof(JobId_t) * del.max_ids);

  mdb->SqlQuery(query.c_str(), DeleteHandler, (void*)&del);

  for (i = 0; i < del.num_ids; i++) {
    Dmsg1(400, "Delete JobId=%d\n", del.JobId[i]);
    Mmsg(query, "DELETE FROM Job WHERE JobId=%s",
         edit_int64(del.JobId[i], ed1));
    mdb->SqlQuery(query.c_str());

    Mmsg(query, "DELETE FROM File WHERE JobId=%s",
         edit_int64(del.JobId[i], ed1));
    mdb->SqlQuery(query.c_str());

    Mmsg(query, "DELETE FROM JobMedia WHERE JobId=%s",
         edit_int64(del.JobId[i], ed1));
    mdb->SqlQuery(query.c_str());
  }

  free(del.JobId);

  return 1;
}

/**
 * Delete Media record and all records that are associated with it.
 * Returns: false on error
 *          true on success
 */
bool BareosDb::DeleteMediaRecord(JobControlRecord* jcr, MediaDbRecord* mr)
{
  bool retval = false;

  DbLock(this);
  if (mr->MediaId == 0 && !GetMediaRecord(jcr, mr)) { goto bail_out; }
  /* Do purge if not already purged */
  if (!bstrcmp(mr->VolStatus, "Purged")) {
    /* Delete associated records */
    DoMediaPurge(this, mr);
  }

  Mmsg(cmd, "DELETE FROM Media WHERE MediaId=%d", mr->MediaId);
  SqlQuery(cmd);
  retval = true;

bail_out:
  DbUnlock(this);
  return retval;
}

/**
 * Purge all records associated with a
 * media record. This does not delete the
 * media record itself. But the media status
 * is changed to "Purged".
 */
bool BareosDb::PurgeMediaRecord(JobControlRecord* jcr, MediaDbRecord* mr)
{
  bool retval = false;

  DbLock(this);
  if (mr->MediaId == 0 && !GetMediaRecord(jcr, mr)) { goto bail_out; }

  /*
   * Delete associated records
   */
  DoMediaPurge(this, mr); /* Note, always purge */

  /*
   * Mark Volume as purged
   */
  strcpy(mr->VolStatus, "Purged");
  if (!UpdateMediaRecord(jcr, mr)) { goto bail_out; }

  retval = true;

bail_out:
  DbUnlock(this);
  return retval;
}
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES */
