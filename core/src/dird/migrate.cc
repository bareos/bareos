/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
 * BAREOS Director -- migrate.c -- responsible for doing migration and copy
 * jobs. Also handles Copy jobs (March 2008) Kern Sibbald, September 2004 SD-SD
 * Migration by Marco van Wieringen, November 2012
 */
/**
 * @file
 * responsible for doing migration and copy jobs.
 *
 * Also handles Copy jobs
 *
 * Basic tasks done here:
 *    Open DB and create records for this job.
 *    Open Message Channel with Storage daemon to tell him a job will be
 * starting. Open connection with Storage daemon and pass him commands to do the
 * backup. When the Storage daemon finishes the job, update the DB.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/backup.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/migration.h"
#include "dird/msgchan.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "dird/ua_input.h"
#include "dird/ua_server.h"
#include "dird/ua_purge.h"
#include "dird/ua_run.h"
#include "include/auth_protocol_types.h"
#include "include/migration_selection_types.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include "cats/sql.h"

#ifndef HAVE_REGEX_H
#include "lib/bregex.h"
#else
#include <regex.h>
#endif

namespace directordaemon {

/* Commands sent to other storage daemon */
static char replicatecmd[] =
    "replicate JobId=%d Job=%s address=%s port=%d ssl=%d Authorization=%s\n";

/**
 * Get Job names in Pool
 */
static const char* sql_job =
    "SELECT DISTINCT Job.Name from Job,Pool"
    " WHERE Pool.Name='%s' AND Job.PoolId=Pool.PoolId";

/**
 * Get JobIds from regex'ed Job names
 */
static const char* sql_jobids_from_job =
    "SELECT DISTINCT Job.JobId,Job.StartTime FROM Job,Pool"
    " WHERE Job.Name='%s' AND Pool.Name='%s' AND Job.PoolId=Pool.PoolId"
    " ORDER by Job.StartTime";

/**
 * Get Client names in Pool
 */
static const char* sql_client =
    "SELECT DISTINCT Client.Name from Client,Pool,Job"
    " WHERE Pool.Name='%s' AND Job.ClientId=Client.ClientId AND"
    " Job.PoolId=Pool.PoolId";

/**
 * Get JobIds from regex'ed Client names
 */
static const char* sql_jobids_from_client =
    "SELECT DISTINCT Job.JobId,Job.StartTime FROM Job,Pool,Client"
    " WHERE Client.Name='%s' AND Pool.Name='%s' AND Job.PoolId=Pool.PoolId"
    " AND Job.ClientId=Client.ClientId AND Job.Type IN ('B','C')"
    " AND Job.JobStatus IN ('T','W')"
    " ORDER by Job.StartTime";

/**
 * Get Volume names in Pool
 */
static const char* sql_vol =
    "SELECT DISTINCT VolumeName FROM Media,Pool WHERE"
    " VolStatus in ('Full','Used','Error') AND Media.Enabled=1 AND"
    " Media.PoolId=Pool.PoolId AND Pool.Name='%s'";

/**
 * Get JobIds from regex'ed Volume names
 */
static const char* sql_jobids_from_vol =
    "SELECT DISTINCT Job.JobId,Job.StartTime FROM Media,JobMedia,Job"
    " WHERE Media.VolumeName='%s' AND Media.MediaId=JobMedia.MediaId"
    " AND JobMedia.JobId=Job.JobId AND Job.Type IN ('B','C')"
    " AND Job.JobStatus IN ('T','W') AND Media.Enabled=1"
    " ORDER by Job.StartTime";

/**
 * Get JobIds from the smallest volume
 */
static const char* sql_smallest_vol =
    "SELECT Media.MediaId FROM Media,Pool,JobMedia WHERE"
    " Media.MediaId in (SELECT DISTINCT MediaId from JobMedia) AND"
    " Media.VolStatus in ('Full','Used','Error') AND Media.Enabled=1 AND"
    " Media.PoolId=Pool.PoolId AND Pool.Name='%s'"
    " ORDER BY VolBytes ASC LIMIT 1";

/**
 * Get JobIds from the oldest volume
 */
static const char* sql_oldest_vol =
    "SELECT Media.MediaId FROM Media,Pool,JobMedia WHERE"
    " Media.MediaId in (SELECT DISTINCT MediaId from JobMedia) AND"
    " Media.VolStatus in ('Full','Used','Error') AND Media.Enabled=1 AND"
    " Media.PoolId=Pool.PoolId AND Pool.Name='%s'"
    " ORDER BY LastWritten ASC LIMIT 1";

/**
 * Get JobIds when we have selected MediaId
 */
static const char* sql_jobids_from_mediaid =
    "SELECT DISTINCT Job.JobId,Job.StartTime FROM JobMedia,Job"
    " WHERE JobMedia.JobId=Job.JobId AND JobMedia.MediaId IN (%s)"
    " AND Job.Type IN ('B','C') AND Job.JobStatus IN ('T','W')"
    " ORDER by Job.StartTime";

/**
 * Get the number of bytes in the pool
 */
static const char* sql_pool_bytes =
    "SELECT SUM(JobBytes) FROM Job WHERE JobId IN"
    " (SELECT DISTINCT Job.JobId from Pool,Job,Media,JobMedia WHERE"
    " Pool.Name='%s' AND Media.PoolId=Pool.PoolId AND"
    " VolStatus in ('Full','Used','Error','Append') AND Media.Enabled=1 AND"
    " Job.Type IN ('B','C') AND Job.JobStatus IN ('T','W') AND"
    " JobMedia.JobId=Job.JobId AND Job.PoolId=Media.PoolId)";

/**
 * Get the number of bytes in the Jobs
 */
static const char* sql_job_bytes =
    "SELECT SUM(JobBytes) FROM Job WHERE JobId IN (%s)";

/**
 * Get Media Ids in Pool
 */
static const char* sql_mediaids =
    "SELECT MediaId FROM Media,Pool WHERE"
    " VolStatus in ('Full','Used','Error') AND Media.Enabled=1 AND"
    " Media.PoolId=Pool.PoolId AND Pool.Name='%s' ORDER BY LastWritten ASC";

/**
 * Get JobIds in Pool longer than specified time
 */
static const char* sql_pool_time =
    "SELECT DISTINCT Job.JobId FROM Pool,Job,Media,JobMedia WHERE"
    " Pool.Name='%s' AND Media.PoolId=Pool.PoolId AND"
    " VolStatus IN ('Full','Used','Error') AND Media.Enabled=1 AND"
    " Job.Type IN ('B','C') AND Job.JobStatus IN ('T','W') AND"
    " JobMedia.JobId=Job.JobId AND Job.PoolId=Media.PoolId"
    " AND Job.RealEndTime<='%s'";

/**
 * Get JobIds from successfully completed backup jobs which have not been copied
 * before
 */
static const char* sql_jobids_of_pool_uncopied_jobs =
    "SELECT DISTINCT Job.JobId,Job.StartTime FROM Job,Pool"
    " WHERE Pool.Name = '%s' AND Pool.PoolId = Job.PoolId"
    " AND Job.Type = 'B' AND Job.JobStatus IN ('T','W')"
    " AND Job.jobBytes > 0"
    " AND Job.JobId NOT IN"
    " (SELECT PriorJobId FROM Job WHERE"
    " Type IN ('B','C') AND Job.JobStatus IN ('T','W')"
    " AND PriorJobId != 0)"
    " ORDER by Job.StartTime";

/**
 * Migrate NDMP Job MetaData.
 */
static const char* sql_migrate_ndmp_metadata =
    "UPDATE File SET JobId=%s "
    "WHERE JobId=%s "
    "AND Name NOT IN ("
    "SELECT Name "
    "FROM File "
    "WHERE JobId=%s)";

/**
 * Copy NDMP Job MetaData.
 */
static const char* sql_copy_ndmp_metadata =
    "INSERT INTO File (FileIndex, JobId, PathId, Name, DeltaSeq, MarkId, "
    "LStat, MD5) "
    "SELECT FileIndex, %s, PathId, Name, DeltaSeq, MarkId, LStat, MD5 "
    "FROM File "
    "WHERE JobId=%s "
    "AND Name NOT IN ("
    "SELECT Name "
    "FROM File "
    "WHERE JobId=%s)";

static const int dbglevel = 10;

struct idpkt {
  POOLMEM* list;
  uint32_t count;
};

/**
 * See if two storage definitions point to the same Storage Daemon.
 *
 * We compare:
 *  - address
 *  - SDport
 *  - password
 */
static inline bool IsSameStorageDaemon(StorageResource* read_storage,
                                       StorageResource* write_storage)
{
  return read_storage->SDport == write_storage->SDport &&
         Bstrcasecmp(read_storage->address, write_storage->address) &&
         Bstrcasecmp(read_storage->password_.value,
                     write_storage->password_.value);
}

bool SetMigrationWstorage(JobControlRecord* jcr,
                          PoolResource* pool,
                          PoolResource* next_pool,
                          const char* where)
{
  if (!next_pool) {
    Jmsg(jcr, M_FATAL, 0,
         _("No Next Pool specification found in Pool \"%s\".\n"),
         pool->resource_name_);
    return false;
  }

  if (!next_pool->storage || next_pool->storage->size() == 0) {
    Jmsg(jcr, M_FATAL, 0,
         _("No Storage specification found in Next Pool \"%s\".\n"),
         next_pool->resource_name_);
    return false;
  }

  CopyWstorage(jcr, next_pool->storage, where);

  return true;
}

/**
 * SetMigrationNextPool() called by DoMigrationInit()
 * at different stages.
 *
 * The idea here is to make a common subroutine for the
 * NextPool's search code and to permit DoMigrationInit()
 * to return with NextPool set in jcr struct.
 */
static inline bool SetMigrationNextPool(JobControlRecord* jcr,
                                        PoolResource** retpool)
{
  PoolDbRecord pr;
  char ed1[100];
  PoolResource* pool;
  const char* storage_source;

  /*
   * Get the PoolId used with the original job. Then
   * find the pool name from the database record.
   */
  pr.PoolId = jcr->impl->jr.PoolId;
  if (!jcr->db->GetPoolRecord(jcr, &pr)) {
    Jmsg(jcr, M_FATAL, 0, _("Pool for JobId %s not in database. ERR=%s\n"),
         edit_int64(pr.PoolId, ed1), jcr->db->strerror());
    return false;
  }

  /*
   * Get the pool resource corresponding to the original job
   */
  pool = (PoolResource*)my_config->GetResWithName(R_POOL, pr.Name);
  *retpool = pool;
  if (!pool) {
    Jmsg(jcr, M_FATAL, 0, _("Pool resource \"%s\" not found.\n"), pr.Name);
    return false;
  }

  /*
   * See if there is a next pool override.
   */
  if (jcr->impl->res.run_next_pool_override) {
    PmStrcpy(jcr->impl->res.npool_source, _("Run NextPool override"));
    PmStrcpy(jcr->impl->res.pool_source, _("Run NextPool override"));
    storage_source = _("Storage from Run NextPool override");
  } else {
    /*
     * See if there is a next pool override in the Job definition.
     */
    if (jcr->impl->res.job->next_pool) {
      jcr->impl->res.next_pool = jcr->impl->res.job->next_pool;
      PmStrcpy(jcr->impl->res.npool_source, _("Job's NextPool resource"));
      PmStrcpy(jcr->impl->res.pool_source, _("Job's NextPool resource"));
      storage_source = _("Storage from Job's NextPool resource");
    } else {
      /*
       * Fall back to the pool's NextPool definition.
       */
      jcr->impl->res.next_pool = pool->NextPool;
      PmStrcpy(jcr->impl->res.npool_source, _("Job Pool's NextPool resource"));
      PmStrcpy(jcr->impl->res.pool_source, _("Job Pool's NextPool resource"));
      storage_source = _("Storage from Pool's NextPool resource");
    }
  }

  /*
   * If the original backup pool has a NextPool, make sure a
   * record exists in the database. Note, in this case, we
   * will be migrating from pool to pool->NextPool.
   */
  if (jcr->impl->res.next_pool) {
    jcr->impl->jr.PoolId =
        GetOrCreatePoolRecord(jcr, jcr->impl->res.next_pool->resource_name_);
    if (jcr->impl->jr.PoolId == 0) { return false; }
  }

  if (!SetMigrationWstorage(jcr, pool, jcr->impl->res.next_pool,
                            storage_source)) {
    return false;
  }

  jcr->impl->res.pool = jcr->impl->res.next_pool;
  Dmsg2(dbglevel, "Write pool=%s read rpool=%s\n",
        jcr->impl->res.pool->resource_name_,
        jcr->impl->res.rpool->resource_name_);

  return true;
}

/**
 * Sanity check that we are not using the same storage for reading and writing.
 */
static inline bool SameStorage(JobControlRecord* jcr)
{
  StorageResource *read_store, *write_store;

  read_store = (StorageResource*)jcr->impl->res.read_storage_list->first();
  write_store = (StorageResource*)jcr->impl->res.write_storage_list->first();

  if (!read_store->autochanger && !write_store->autochanger &&
      bstrcmp(read_store->resource_name_, write_store->resource_name_)) {
    return true;
  }

  return false;
}

static inline void StartNewMigrationJob(JobControlRecord* jcr)
{
  char ed1[50];
  JobId_t jobid;
  UaContext* ua;
  PoolMem cmd(PM_MESSAGE);

  ua = new_ua_context(jcr);
  ua->batch = true;
  Mmsg(ua->cmd, "run job=\"%s\" jobid=%s ignoreduplicatecheck=yes",
       jcr->impl->res.job->resource_name_,
       edit_uint64(jcr->impl->MigrateJobId, ed1));

  /*
   * Make sure we have something to compare against.
   */
  if (jcr->impl->res.pool) {
    /*
     * See if there was actually a pool override.
     */
    if (jcr->impl->res.pool != jcr->impl->res.job->pool) {
      Mmsg(cmd, " pool=\"%s\"", jcr->impl->res.pool->resource_name_);
      PmStrcat(ua->cmd, cmd.c_str());
    }

    /*
     * See if there was actually a next pool override.
     */
    if (jcr->impl->res.next_pool &&
        jcr->impl->res.next_pool != jcr->impl->res.pool->NextPool) {
      Mmsg(cmd, " nextpool=\"%s\"", jcr->impl->res.next_pool->resource_name_);
      PmStrcat(ua->cmd, cmd.c_str());
    }
  }

  Dmsg2(dbglevel, "=============== %s cmd=%s\n", jcr->get_OperationName(),
        ua->cmd);
  ParseUaArgs(ua); /* parse command */

  jobid = DoRunCmd(ua, ua->cmd);
  if (jobid == 0) {
    Jmsg(jcr, M_ERROR, 0, _("Could not start migration job.\n"));
  } else {
    Jmsg(jcr, M_INFO, 0, _("%s JobId %d started.\n"), jcr->get_OperationName(),
         (int)jobid);
  }

  FreeUaContext(ua);
}

/**
 * Return next DBId from comma separated list
 *
 * Returns:
 *   1 if next DBId returned
 *   0 if no more DBIds are in list
 *  -1 there is an error
 */
static inline int GetNextDbidFromList(const char** p, DBId_t* DBId)
{
  int i;
  const int maxlen = 30;
  char id[maxlen + 1];
  const char* q = *p;

  id[0] = 0;
  for (i = 0; i < maxlen; i++) {
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
  *DBId = str_to_int64(id);

  return 1;
}

/**
 * Add an item to the list if it is unique
 */
static void AddUniqueId(idpkt* ids, char* item)
{
  const int maxlen = 30;
  char id[maxlen + 1];
  char* q = ids->list;

  /*
   * Walk through current list to see if each item is the same as item
   */
  while (*q) {
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
    if (bstrcmp(item, id)) { return; }
  }

  /*
   * Did not find item, so add it to list
   */
  if (ids->count == 0) {
    ids->list[0] = 0;
  } else {
    PmStrcat(ids->list, ",");
  }

  PmStrcat(ids->list, item);
  ids->count++;

  return;
}

/**
 * Callback handler make list of DB Ids
 */
static int UniqueDbidHandler(void* ctx, int num_fields, char** row)
{
  idpkt* ids = (idpkt*)ctx;

  /*
   * Sanity check
   */
  if (!row || !row[0]) {
    Dmsg0(dbglevel, "dbid_hdlr error empty row\n");
    return 1; /* stop calling us */
  }

  AddUniqueId(ids, row[0]);
  Dmsg3(dbglevel, "dbid_hdlr count=%d Ids=%p %s\n", ids->count, ids->list,
        ids->list);
  return 0;
}

struct uitem {
  dlink link;
  char* item;
};

static int ItemCompare(void* item1, void* item2)
{
  uitem* i1 = (uitem*)item1;
  uitem* i2 = (uitem*)item2;

  return strcmp(i1->item, i2->item);
}

static int UniqueNameHandler(void* ctx, int num_fields, char** row)
{
  dlist* list = (dlist*)ctx;

  uitem* new_item = (uitem*)malloc(sizeof(uitem));
  new (new_item) uitem();

  uitem* item;
  new_item->item = strdup(row[0]);
  Dmsg1(dbglevel, "Unique_name_hdlr Item=%s\n", row[0]);

  item = (uitem*)list->binary_insert((void*)new_item, ItemCompare);
  if (item != new_item) { /* already in list */
    free(new_item->item);
    free((char*)new_item);
    return 0;
  }

  return 0;
}

/**
 * This routine returns:
 *    false       if an error occurred
 *    true        otherwise
 *    ids.count   number of jobids found (may be zero)
 */
static bool FindJobidsFromMediaidList(JobControlRecord* jcr,
                                      idpkt* ids,
                                      const char* type)
{
  bool ok = false;
  PoolMem query(PM_MESSAGE);

  Mmsg(query, sql_jobids_from_mediaid, ids->list);
  ids->count = 0;
  if (!jcr->db->SqlQuery(query.c_str(), UniqueDbidHandler, (void*)ids)) {
    Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), jcr->db->strerror());
    goto bail_out;
  }
  if (ids->count == 0) {
    Jmsg(jcr, M_INFO, 0, _("No %ss found to %s.\n"), type,
         jcr->get_ActionName());
  }
  ok = true;

bail_out:
  return ok;
}

static bool find_mediaid_then_jobids(JobControlRecord* jcr,
                                     idpkt* ids,
                                     const char* query1,
                                     const char* type)
{
  bool ok = false;
  PoolMem query(PM_MESSAGE);

  ids->count = 0;

  /*
   * Basic query for MediaId
   */
  Mmsg(query, query1, jcr->impl->res.rpool->resource_name_);
  if (!jcr->db->SqlQuery(query.c_str(), UniqueDbidHandler, (void*)ids)) {
    Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), jcr->db->strerror());
    goto bail_out;
  }

  if (ids->count == 0) {
    Jmsg(jcr, M_INFO, 0, _("No %s found to %s.\n"), type,
         jcr->get_ActionName());
    ok = true; /* Not an error */
    goto bail_out;
  } else if (ids->count != 1) {
    Jmsg(jcr, M_FATAL, 0, _("SQL error. Expected 1 MediaId got %d\n"),
         ids->count);
    goto bail_out;
  }

  Dmsg2(dbglevel, "%s MediaIds=%s\n", type, ids->list);

  ok = FindJobidsFromMediaidList(jcr, ids, type);

bail_out:
  return ok;
}

/**
 * This routine returns:
 *    false       if an error occurred
 *    true        otherwise
 *    ids.count   number of jobids found (may be zero)
 */
static inline bool FindJobidsOfPoolUncopiedJobs(JobControlRecord* jcr,
                                                idpkt* ids)
{
  bool ok = false;
  PoolMem query(PM_MESSAGE);

  /*
   * Only a copy job is allowed
   */
  if (!jcr->is_JobType(JT_COPY)) {
    Jmsg(jcr, M_FATAL, 0,
         _("Selection Type 'pooluncopiedjobs' only applies to Copy Jobs"));
    goto bail_out;
  }

  Dmsg1(dbglevel, "copy selection pattern=%s\n",
        jcr->impl->res.rpool->resource_name_);
  Mmsg(query, sql_jobids_of_pool_uncopied_jobs,
       jcr->impl->res.rpool->resource_name_);
  Dmsg1(dbglevel, "get uncopied jobs query=%s\n", query.c_str());
  if (!jcr->db->SqlQuery(query.c_str(), UniqueDbidHandler, (void*)ids)) {
    Jmsg(jcr, M_FATAL, 0, _("SQL to get uncopied jobs failed. ERR=%s\n"),
         jcr->db->strerror());
    goto bail_out;
  }
  ok = true;

bail_out:
  return ok;
}

static bool regex_find_jobids(JobControlRecord* jcr,
                              idpkt* ids,
                              const char* query1,
                              const char* query2,
                              const char* type)
{
  dlist* item_chain;
  uitem* item = NULL;
  uitem* last_item = NULL;
  regex_t preg{};
  char prbuf[500];
  int rc;
  bool ok = false;
  PoolMem query(PM_MESSAGE);

  item_chain = new dlist(item, &item->link);
  if (!jcr->impl->res.job->selection_pattern) {
    Jmsg(jcr, M_FATAL, 0, _("No %s %s selection pattern specified.\n"),
         jcr->get_OperationName(), type);
    goto bail_out;
  }
  Dmsg1(dbglevel, "regex-sel-pattern=%s\n",
        jcr->impl->res.job->selection_pattern);

  /*
   * Basic query for names
   */
  Mmsg(query, query1, jcr->impl->res.rpool->resource_name_);
  Dmsg1(dbglevel, "get name query1=%s\n", query.c_str());
  if (!jcr->db->SqlQuery(query.c_str(), UniqueNameHandler, (void*)item_chain)) {
    Jmsg(jcr, M_FATAL, 0, _("SQL to get %s failed. ERR=%s\n"), type,
         jcr->db->strerror());
    goto bail_out;
  }
  Dmsg1(dbglevel, "query1 returned %d names\n", item_chain->size());
  if (item_chain->size() == 0) {
    Jmsg(jcr, M_INFO, 0, _("Query of Pool \"%s\" returned no Jobs to %s.\n"),
         jcr->impl->res.rpool->resource_name_, jcr->get_ActionName());
    ok = true;
    goto bail_out; /* skip regex match */
  } else {
    /*
     * Compile regex expression
     */
    rc = regcomp(&preg, jcr->impl->res.job->selection_pattern, REG_EXTENDED);
    if (rc != 0) {
      regerror(rc, &preg, prbuf, sizeof(prbuf));
      Jmsg(jcr, M_FATAL, 0,
           _("Could not compile regex pattern \"%s\" ERR=%s\n"),
           jcr->impl->res.job->selection_pattern, prbuf);
      goto bail_out;
    }

    /*
     * Now apply the regex to the names and remove any item not matched
     */
    foreach_dlist (item, item_chain) {
      if (last_item) {
        Dmsg1(dbglevel, "Remove item %s\n", last_item->item);
        free(last_item->item);
        item_chain->remove(last_item);
      }
      Dmsg1(dbglevel, "get name Item=%s\n", item->item);
      rc = regexec(&preg, item->item, 0, NULL, 0);
      if (rc == 0) {
        last_item = NULL; /* keep this one */
      } else {
        last_item = item;
      }
    }

    if (last_item) {
      free(last_item->item);
      Dmsg1(dbglevel, "Remove item %s\n", last_item->item);
      item_chain->remove(last_item);
    }

    regfree(&preg);
  }

  if (item_chain->size() == 0) {
    Jmsg(jcr, M_INFO, 0, _("Regex pattern matched no Jobs to %s.\n"),
         jcr->get_ActionName());
    ok = true;
    goto bail_out; /* skip regex match */
  }

  /*
   * At this point, we have a list of items in item_chain
   * that have been matched by the regex, so now we need
   * to look up their jobids.
   */
  ids->count = 0;
  foreach_dlist (item, item_chain) {
    Dmsg2(dbglevel, "Got %s: %s\n", type, item->item);
    Mmsg(query, query2, item->item, jcr->impl->res.rpool->resource_name_);
    Dmsg1(dbglevel, "get id from name query2=%s\n", query.c_str());
    if (!jcr->db->SqlQuery(query.c_str(), UniqueDbidHandler, (void*)ids)) {
      Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), jcr->db->strerror());
      goto bail_out;
    }
  }

  if (ids->count == 0) {
    Jmsg(jcr, M_INFO, 0, _("No %ss found to %s.\n"), type,
         jcr->get_ActionName());
  }

  ok = true;

bail_out:
  Dmsg2(dbglevel, "Count=%d Jobids=%s\n", ids->count, ids->list);
  foreach_dlist (item, item_chain) {
    free(item->item);
  }
  delete item_chain;
  return ok;
}

/**
 * This is the central piece of code that finds jobs actually JobIds to migrate.
 * It examines the Selection Type to see what kind of migration we are doing
 * (Volume, Job, Client, ...) and applies any Selection Pattern if appropriate
 * to obtain a list of JobIds.
 *
 * Finally, it will loop over all the JobIds found, starting a new job with
 * MigrationJobId set to that JobId.
 *
 * Returns: false - On error
 *          true - If OK
 */
static inline bool getJobs_to_migrate(JobControlRecord* jcr)
{
  const char* p;
  int status;
  int limit = -1;
  bool apply_limit = false;
  bool retval = false;
  JobId_t JobId;
  db_int64_ctx ctx;
  idpkt ids, mid, jids;
  char ed1[30], ed2[30];
  PoolMem query(PM_MESSAGE);

  ids.list = GetPoolMemory(PM_MESSAGE);
  ids.list[0] = 0;
  ids.count = 0;
  mid.list = NULL;
  jids.list = NULL;

  switch (jcr->impl->res.job->selection_type) {
    case MT_JOB:
      if (!regex_find_jobids(jcr, &ids, sql_job, sql_jobids_from_job, "Job")) {
        goto bail_out;
      }
      break;
    case MT_CLIENT:
      if (!regex_find_jobids(jcr, &ids, sql_client, sql_jobids_from_client,
                             "Client")) {
        goto bail_out;
      }
      break;
    case MT_VOLUME:
      if (!regex_find_jobids(jcr, &ids, sql_vol, sql_jobids_from_vol,
                             "Volume")) {
        goto bail_out;
      }
      break;
    case MT_SQLQUERY:
      if (!jcr->impl->res.job->selection_pattern) {
        Jmsg(jcr, M_FATAL, 0, _("No %s SQL selection pattern specified.\n"),
             jcr->get_OperationName());
        goto bail_out;
      }
      Dmsg1(dbglevel, "SQL=%s\n", jcr->impl->res.job->selection_pattern);
      if (!jcr->db->SqlQuery(jcr->impl->res.job->selection_pattern,
                             UniqueDbidHandler, (void*)&ids)) {
        Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), jcr->db->strerror());
        goto bail_out;
      }
      break;
    case MT_SMALLEST_VOL:
      if (!find_mediaid_then_jobids(jcr, &ids, sql_smallest_vol,
                                    "Smallest Volume")) {
        goto bail_out;
      }
      break;
    case MT_OLDEST_VOL:
      if (!find_mediaid_then_jobids(jcr, &ids, sql_oldest_vol,
                                    "Oldest Volume")) {
        goto bail_out;
      }
      break;
    case MT_POOL_OCCUPANCY: {
      int64_t pool_bytes;
      DBId_t DBId = 0;

      mid.list = GetPoolMemory(PM_MESSAGE);
      mid.list[0] = 0;
      mid.count = 0;
      jids.list = GetPoolMemory(PM_MESSAGE);
      jids.list[0] = 0;
      jids.count = 0;
      ctx.count = 0;

      /*
       * Find count of bytes in pool
       */
      Mmsg(query, sql_pool_bytes, jcr->impl->res.rpool->resource_name_);

      if (!jcr->db->SqlQuery(query.c_str(), db_int64_handler, (void*)&ctx)) {
        Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), jcr->db->strerror());
        goto bail_out;
      }

      if (ctx.count == 0) {
        Jmsg(jcr, M_INFO, 0, _("No Volumes found to %s.\n"),
             jcr->get_ActionName());
        retval = true;
        goto bail_out;
      }

      pool_bytes = ctx.value;
      Dmsg2(dbglevel, "highbytes=%lld pool=%lld\n",
            jcr->impl->res.rpool->MigrationHighBytes, pool_bytes);

      if (pool_bytes < (int64_t)jcr->impl->res.rpool->MigrationHighBytes) {
        Jmsg(jcr, M_INFO, 0, _("No Volumes found to %s.\n"),
             jcr->get_ActionName());
        retval = true;
        goto bail_out;
      }

      Dmsg0(dbglevel, "We should do Occupation migration.\n");

      ids.count = 0;
      /*
       * Find a list of MediaIds that could be migrated
       */
      Mmsg(query, sql_mediaids, jcr->impl->res.rpool->resource_name_);
      Dmsg1(dbglevel, "query=%s\n", query.c_str());

      if (!jcr->db->SqlQuery(query.c_str(), UniqueDbidHandler, (void*)&ids)) {
        Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), jcr->db->strerror());
        goto bail_out;
      }

      if (ids.count == 0) {
        Jmsg(jcr, M_INFO, 0, _("No Volumes found to %s.\n"),
             jcr->get_ActionName());
        retval = true;
        goto bail_out;
      }

      Dmsg2(dbglevel, "Pool Occupancy ids=%d MediaIds=%s\n", ids.count,
            ids.list);

      if (!FindJobidsFromMediaidList(jcr, &ids, "Volume")) { goto bail_out; }

      /*
       * ids == list of jobs
       */
      p = ids.list;
      for (int i = 0; i < (int)ids.count; i++) {
        status = GetNextDbidFromList(&p, &DBId);
        Dmsg2(dbglevel, "get_next_dbid status=%d JobId=%u\n", status,
              (uint32_t)DBId);
        if (status < 0) {
          Jmsg(jcr, M_FATAL, 0, _("Invalid JobId found.\n"));
          goto bail_out;
        } else if (status == 0) {
          break;
        }

        mid.count = 1;
        Mmsg(mid.list, "%s", edit_int64(DBId, ed1));
        if (jids.count > 0) { PmStrcat(jids.list, ","); }
        PmStrcat(jids.list, mid.list);
        jids.count += mid.count;

        /*
         * Find count of bytes from Jobs
         */
        Mmsg(query, sql_job_bytes, mid.list);
        Dmsg1(dbglevel, "Jobbytes query: %s\n", query.c_str());
        if (!jcr->db->SqlQuery(query.c_str(), db_int64_handler, (void*)&ctx)) {
          Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), jcr->db->strerror());
          goto bail_out;
        }
        pool_bytes -= ctx.value;
        Dmsg2(dbglevel, "Total %s Job bytes=%s\n", jcr->get_ActionName(),
              edit_int64_with_commas(ctx.value, ed1));
        Dmsg2(dbglevel, "lowbytes=%s poolafter=%s\n",
              edit_int64_with_commas(jcr->impl->res.rpool->MigrationLowBytes,
                                     ed1),
              edit_int64_with_commas(pool_bytes, ed2));
        if (pool_bytes <= (int64_t)jcr->impl->res.rpool->MigrationLowBytes) {
          Dmsg0(dbglevel, "We should be done.\n");
          break;
        }
      }

      /*
       * Transfer jids to ids, where the jobs list is expected
       */
      ids.count = jids.count;
      PmStrcpy(ids.list, jids.list);
      Dmsg2(dbglevel, "Pool Occupancy ids=%d JobIds=%s\n", ids.count, ids.list);
      break;
    }
    case MT_POOL_TIME: {
      time_t ttime;
      char dt[MAX_TIME_LENGTH];

      ttime = time(NULL) - (time_t)jcr->impl->res.rpool->MigrationTime;
      bstrutime(dt, sizeof(dt), ttime);

      ids.count = 0;
      Mmsg(query, sql_pool_time, jcr->impl->res.rpool->resource_name_, dt);
      Dmsg1(dbglevel, "query=%s\n", query.c_str());

      if (!jcr->db->SqlQuery(query.c_str(), UniqueDbidHandler, (void*)&ids)) {
        Jmsg(jcr, M_FATAL, 0, _("SQL failed. ERR=%s\n"), jcr->db->strerror());
        goto bail_out;
      }

      if (ids.count == 0) {
        Jmsg(jcr, M_INFO, 0, _("No Volumes found to %s.\n"),
             jcr->get_ActionName());
        retval = true;
        goto bail_out;
      }

      Dmsg2(dbglevel, "PoolTime ids=%d JobIds=%s\n", ids.count, ids.list);
      break;
    }
    case MT_POOL_UNCOPIED_JOBS:
      if (!FindJobidsOfPoolUncopiedJobs(jcr, &ids)) { goto bail_out; }
      break;
    default:
      Jmsg(jcr, M_FATAL, 0, _("Unknown %s Selection Type.\n"),
           jcr->get_OperationName());
      goto bail_out;
  }

  if (ids.count == 0) {
    Jmsg(jcr, M_INFO, 0, _("No JobIds found to %s.\n"), jcr->get_ActionName());
    retval = true;
    goto bail_out;
  }

  Jmsg(jcr, M_INFO, 0, _("The following %u JobId%s chosen to be %s: %s\n"),
       ids.count, (ids.count < 2) ? _(" was") : _("s were"),
       jcr->get_ActionName(true), ids.list);

  Dmsg2(dbglevel, "Before loop count=%d ids=%s\n", ids.count, ids.list);

  /*
   * Note: to not over load the system, limit the number of new jobs started.
   */
  if (jcr->impl->res.job->MaxConcurrentCopies) {
    limit = jcr->impl->res.job->MaxConcurrentCopies;
    apply_limit = true;
  }

  p = ids.list;
  for (int i = 0; i < (int)ids.count; i++) {
    JobId = 0;
    status = GetNextJobidFromList(&p, &JobId);
    Dmsg3(dbglevel, "getJobid_no=%d status=%d JobId=%u\n", i, status, JobId);
    if (status < 0) {
      Jmsg(jcr, M_FATAL, 0, _("Invalid JobId found.\n"));
      goto bail_out;
    } else if (status == 0) {
      Jmsg(jcr, M_INFO, 0, _("No JobIds found to %s.\n"),
           jcr->get_ActionName());
      retval = true;
      goto bail_out;
    }
    jcr->impl->MigrateJobId = JobId;

    if (apply_limit) {
      /*
       * Don't start any more when limit reaches zero
       */
      limit--;
      if (limit < 0) { continue; }
    }

    StartNewMigrationJob(jcr);
    Dmsg0(dbglevel, "Back from StartNewMigrationJob\n");
  }

  jcr->impl->HasSelectedJobs = true;
  retval = true;

bail_out:
  FreePoolMemory(ids.list);

  if (mid.list) { FreePoolMemory(mid.list); }

  if (jids.list) { FreePoolMemory(jids.list); }

  return retval;
}

/**
 * Called here before the job is run to do the job
 * specific setup. Note, one of the important things to
 * complete in this init code is to make the definitive
 * choice of input and output storage devices.  This is
 * because immediately after the init, the job is queued
 * in the jobq.c code, and it checks that all the resources
 * (storage resources in particular) are available, so these
 * must all be properly defined.
 */
bool DoMigrationInit(JobControlRecord* jcr)
{
  char ed1[100];
  PoolResource* pool = NULL;
  JobResource *job, *prev_job;
  JobControlRecord* mig_jcr = NULL; /* newly migrated job */

  ApplyPoolOverrides(jcr);

  if (!AllowDuplicateJob(jcr)) { return false; }

  jcr->impl->jr.PoolId =
      GetOrCreatePoolRecord(jcr, jcr->impl->res.pool->resource_name_);
  if (jcr->impl->jr.PoolId == 0) {
    Dmsg1(dbglevel, "JobId=%d no PoolId\n", (int)jcr->JobId);
    Jmsg(jcr, M_FATAL, 0, _("Could not get or create a Pool record.\n"));
    return false;
  }

  /*
   * Note, at this point, pool is the pool for this job.
   * We transfer it to rpool (read pool), and a bit later,
   * pool will be changed to point to the write pool,
   * which comes from pool->NextPool.
   */
  jcr->impl->res.rpool = jcr->impl->res.pool; /* save read pool */
  PmStrcpy(jcr->impl->res.rpool_source, jcr->impl->res.pool_source);
  Dmsg2(dbglevel, "Read pool=%s (From %s)\n",
        jcr->impl->res.rpool->resource_name_, jcr->impl->res.rpool_source);

  /*
   * See if this is a control job e.g. the one that selects the Jobs to Migrate
   * or Copy or one of the worker Jobs that do the actual Migration or Copy. If
   * jcr->impl_->MigrateJobId is set we know that its an actual Migration or
   * Copy Job.
   */
  if (jcr->impl->MigrateJobId != 0) {
    Dmsg1(dbglevel, "At Job start previous jobid=%u\n",
          jcr->impl->MigrateJobId);

    jcr->impl->previous_jr.JobId = jcr->impl->MigrateJobId;
    Dmsg1(dbglevel, "Previous jobid=%d\n", (int)jcr->impl->previous_jr.JobId);

    if (!jcr->db->GetJobRecord(jcr, &jcr->impl->previous_jr)) {
      Jmsg(jcr, M_FATAL, 0,
           _("Could not get job record for JobId %s to %s. ERR=%s"),
           edit_int64(jcr->impl->previous_jr.JobId, ed1), jcr->get_ActionName(),
           jcr->db->strerror());
      return false;
    }

    Jmsg(jcr, M_INFO, 0, _("%s using JobId=%s Job=%s\n"),
         jcr->get_OperationName(),
         edit_int64(jcr->impl->previous_jr.JobId, ed1),
         jcr->impl->previous_jr.Job);
    Dmsg4(dbglevel, "%s JobId=%d  using JobId=%s Job=%s\n",
          jcr->get_OperationName(), jcr->JobId,
          edit_int64(jcr->impl->previous_jr.JobId, ed1),
          jcr->impl->previous_jr.Job);

    if (CreateRestoreBootstrapFile(jcr) < 0) {
      Jmsg(jcr, M_FATAL, 0, _("Create bootstrap file failed.\n"));
      return false;
    }

    if (jcr->impl->previous_jr.JobId == 0 || jcr->impl->ExpectedFiles == 0) {
      jcr->setJobStatus(JS_Terminated);
      Dmsg1(dbglevel, "JobId=%d expected files == 0\n", (int)jcr->JobId);
      if (jcr->impl->previous_jr.JobId == 0) {
        Jmsg(jcr, M_INFO, 0, _("No previous Job found to %s.\n"),
             jcr->get_ActionName());
      } else {
        Jmsg(jcr, M_INFO, 0, _("Previous Job has no data to %s.\n"),
             jcr->get_ActionName());
      }
      SetMigrationNextPool(jcr, &pool);
      return true; /* no work */
    }

    Dmsg5(dbglevel, "JobId=%d: Current: Name=%s JobId=%d Type=%c Level=%c\n",
          (int)jcr->JobId, jcr->impl->jr.Name, (int)jcr->impl->jr.JobId,
          jcr->impl->jr.JobType, jcr->impl->jr.JobLevel);

    job = (JobResource*)my_config->GetResWithName(R_JOB, jcr->impl->jr.Name);
    prev_job = (JobResource*)my_config->GetResWithName(
        R_JOB, jcr->impl->previous_jr.Name);

    if (!job) {
      Jmsg(jcr, M_FATAL, 0, _("Job resource not found for \"%s\".\n"),
           jcr->impl->jr.Name);
      return false;
    }

    if (!prev_job) {
      Jmsg(jcr, M_FATAL, 0, _("Previous Job resource not found for \"%s\".\n"),
           jcr->impl->previous_jr.Name);
      return false;
    }

    /*
     * Copy the actual level setting of the previous Job to this Job.
     * This overrides the dummy backup level given to the migrate/copy Job and
     * replaces it with the actual level the backup run at.
     */
    jcr->setJobLevel(prev_job->JobLevel);

    /*
     * If the current Job has no explicit client set use the client setting of
     * the previous Job.
     */
    if (!jcr->impl->res.client && prev_job->client) {
      jcr->impl->res.client = prev_job->client;
      if (!jcr->client_name) { jcr->client_name = GetPoolMemory(PM_NAME); }
      PmStrcpy(jcr->client_name, jcr->impl->res.client->resource_name_);
    }

    /*
     * If the current Job has no explicit fileset set use the client setting of
     * the previous Job.
     */
    if (!jcr->impl->res.fileset) { jcr->impl->res.fileset = prev_job->fileset; }

    /*
     * See if spooling data is not enabled yet. If so turn on spooling if
     * requested in job
     */
    if (!jcr->impl->spool_data) { jcr->impl->spool_data = job->spool_data; }

    /*
     * Create a migration jcr
     */
    mig_jcr = NewDirectorJcr();
    jcr->impl->mig_jcr = mig_jcr;
    memcpy(&mig_jcr->impl->previous_jr, &jcr->impl->previous_jr,
           sizeof(mig_jcr->impl->previous_jr));

    /*
     * Turn the mig_jcr into a "real" job that takes on the aspects of
     * the previous backup job "prev_job". We only don't want it to
     * ever send any messages to the database or mail messages when
     * we are doing a migrate or copy to a remote storage daemon. When
     * doing such operations the mig_jcr is used for tracking some of
     * the remote state and it might want to send some captured state
     * info on tear down of the mig_jcr so we call SetupJob with the
     * suppress_output argument set to true (e.g. don't init messages
     * and set the jcr suppress_output boolean to true).
     */
    SetJcrDefaults(mig_jcr, prev_job);

    /*
     * Don't let Watchdog checks Max*Time value on this Job
     */
    mig_jcr->impl->no_maxtime = true;

    /*
     * Don't check for duplicates on migration and copy jobs
     */
    mig_jcr->impl->IgnoreDuplicateJobChecking = true;

    /*
     * Copy some overwrites back from the Control Job to the migration and copy
     * job.
     */
    mig_jcr->impl->spool_data = jcr->impl->spool_data;
    mig_jcr->impl->spool_size = jcr->impl->spool_size;


    if (!SetupJob(mig_jcr, true)) {
      Jmsg(jcr, M_FATAL, 0, _("setup job failed.\n"));
      return false;
    }

    /*
     * Keep track that the mig_jcr has a controlling JobControlRecord.
     */
    mig_jcr->cjcr = jcr;

    /*
     * Now reset the job record from the previous job
     */
    memcpy(&mig_jcr->impl->jr, &jcr->impl->previous_jr,
           sizeof(mig_jcr->impl->jr));

    /*
     * Update the jr to reflect the new values of PoolId and JobId.
     */
    mig_jcr->impl->jr.PoolId = jcr->impl->jr.PoolId;
    mig_jcr->impl->jr.JobId = mig_jcr->JobId;

    if (SetMigrationNextPool(jcr, &pool)) {
      /*
       * If pool storage specified, use it as source
       */
      CopyRstorage(mig_jcr, pool->storage, _("Pool resource"));
      CopyRstorage(jcr, pool->storage, _("Pool resource"));

      mig_jcr->impl->res.pool = jcr->impl->res.pool;
      mig_jcr->impl->res.next_pool = jcr->impl->res.next_pool;
      mig_jcr->impl->jr.PoolId = jcr->impl->jr.PoolId;
    }

    /*
     * Get the storage that was used for the original Job.
     * This only happens when the original pool used doesn't have an explicit
     * storage.
     */
    if (!jcr->impl->res.read_storage_list) {
      CopyRstorage(jcr, prev_job->storage, _("previous Job"));
    }

    /*
     * See if the read and write storage is the same.
     * When they are we do the migrate/copy over one SD connection
     * otherwise we open a connection to the reading SD and a second
     * one to the writing SD.
     */
    jcr->impl->remote_replicate = !IsSameStorageDaemon(
        jcr->impl->res.read_storage, jcr->impl->res.write_storage);

    /*
     * set the JobLevel to what the original job was
     */
    mig_jcr->setJobLevel(mig_jcr->impl->previous_jr.JobLevel);


    Dmsg4(dbglevel, "mig_jcr: Name=%s JobId=%d Type=%c Level=%c\n",
          mig_jcr->impl->jr.Name, (int)mig_jcr->impl->jr.JobId,
          mig_jcr->impl->jr.JobType, mig_jcr->impl->jr.JobLevel);
  }

  return true;
}

/**
 * Do a Migration of a previous job
 *
 * - previous_jr refers to the job DB record of the Job that is
 *   going to be migrated.
 * - prev_job refers to the job resource of the Job that is
 *   going to be migrated.
 * - jcr is the jcr for the current "migration" job. It is a
 *   control job that is put in the DB as a migration job, which
 *   means that this job migrated a previous job to a new job.
 *   No Volume or File data is associated with this control
 *   job.
 * - mig_jcr refers to the newly migrated job that is run by
 *   the current jcr. It is a backup job that moves (migrates) the
 *   data written for the previous_jr into the new pool. This
 *   job (mig_jcr) becomes the new backup job that replaces
 *   the original backup job. Note, when this is a migration
 *   on a single storage daemon this jcr is not really run. It
 *   is simply attached to the current jcr. It will show up in
 *   the Director's status output, but not in the SD or FD, both of
 *   which deal only with the current migration job (i.e. jcr).
 *   When this is is a migration between two storage daemon this
 *   mig_jcr is used to control the second connection to the
 *   remote storage daemon.
 *
 * Returns:  false on failure
 *           true  on success
 */
static inline bool DoActualMigration(JobControlRecord* jcr)
{
  char ed1[100];
  bool retval = false;
  JobControlRecord* mig_jcr = jcr->impl->mig_jcr;

  ASSERT(mig_jcr);

  /*
   * Make sure this job was not already migrated
   */
  if (jcr->impl->previous_jr.JobType != JT_BACKUP &&
      jcr->impl->previous_jr.JobType != JT_JOB_COPY) {
    Jmsg(jcr, M_INFO, 0,
         _("JobId %s already %s probably by another Job. %s stopped.\n"),
         edit_int64(jcr->impl->previous_jr.JobId, ed1),
         jcr->get_ActionName(true), jcr->get_OperationName());
    jcr->setJobStatus(JS_Terminated);
    MigrationCleanup(jcr, jcr->JobStatus);
    return true;
  }

  if (SameStorage(jcr)) {
    Jmsg(jcr, M_FATAL, 0,
         _("JobId %s cannot %s using the same read and write storage.\n"),
         edit_int64(jcr->impl->previous_jr.JobId, ed1),
         jcr->get_OperationName());
    jcr->setJobStatus(JS_Terminated);
    MigrationCleanup(jcr, jcr->JobStatus);
    return true;
  }

  /*
   * Print Job Start message
   */
  Jmsg(jcr, M_INFO, 0, _("Start %s JobId %s, Job=%s\n"),
       jcr->get_OperationName(), edit_uint64(jcr->JobId, ed1), jcr->Job);

  /*
   * See if the read storage is paired NDMP storage, if so setup
   * the Job to use the native storage instead.
   */
  if (HasPairedStorage(jcr)) { SetPairedStorage(jcr); }

  Dmsg2(dbglevel, "Read store=%s, write store=%s\n",
        ((StorageResource*)jcr->impl->res.read_storage_list->first())
            ->resource_name_,
        ((StorageResource*)jcr->impl->res.write_storage_list->first())
            ->resource_name_);

  if (jcr->impl->remote_replicate) {
    alist* write_storage_list;

    /*
     * See if we need to apply any bandwidth limiting.
     * We search the bandwidth limiting in the following way:
     * - Job bandwidth limiting
     * - Writing Storage Daemon bandwidth limiting
     * - Reading Storage Daemon bandwidth limiting
     */
    if (jcr->impl->res.job->max_bandwidth > 0) {
      jcr->max_bandwidth = jcr->impl->res.job->max_bandwidth;
    } else if (jcr->impl->res.write_storage->max_bandwidth > 0) {
      jcr->max_bandwidth = jcr->impl->res.write_storage->max_bandwidth;
    } else if (jcr->impl->res.read_storage->max_bandwidth > 0) {
      jcr->max_bandwidth = jcr->impl->res.read_storage->max_bandwidth;
    }

    /*
     * Open a message channel connection to the Reading Storage daemon.
     */
    Dmsg0(110, "Open connection with reading storage daemon\n");

    /*
     * Clear the write_storage of the jcr and assign it to the mig_jcr so
     * the jcr is connected to the reading storage daemon and the
     * mig_jcr to the writing storage daemon.
     */
    mig_jcr->impl->res.write_storage = jcr->impl->res.write_storage;
    jcr->impl->res.write_storage = NULL;

    /*
     * Swap the write_storage_list between the jcr and the mig_jcr.
     */
    write_storage_list = mig_jcr->impl->res.write_storage_list;
    mig_jcr->impl->res.write_storage_list = jcr->impl->res.write_storage_list;
    jcr->impl->res.write_storage_list = write_storage_list;

    /*
     * Start conversation with Reading Storage daemon
     */
    jcr->setJobStatus(JS_WaitSD);
    if (!ConnectToStorageDaemon(jcr, 10, me->SDConnectTimeout, true)) {
      goto bail_out;
    }

    /*
     * Open a message channel connection with the Writing Storage daemon.
     */
    Dmsg0(110, "Open connection with writing storage daemon\n");

    /*
     * Start conversation with Writing Storage daemon
     */
    mig_jcr->setJobStatus(JS_WaitSD);
    if (!ConnectToStorageDaemon(mig_jcr, 10, me->SDConnectTimeout, true)) {
      goto bail_out;
    }

    /*
     * Now start a job with the Reading Storage daemon
     */
    if (!StartStorageDaemonJob(jcr, jcr->impl->res.read_storage_list, NULL,
                               /* send_bsr */ true)) {
      goto bail_out;
    }

    Dmsg0(150, "Reading Storage daemon connection OK\n");

    /*
     * Now start a job with the Writing Storage daemon
     */

    if (!StartStorageDaemonJob(mig_jcr, NULL,
                               mig_jcr->impl->res.write_storage_list,
                               /* send_bsr */ false)) {
      goto bail_out;
    }

    Dmsg0(150, "Writing Storage daemon connection OK\n");

  } else { /* local replicate */

    /*
     * Open a message channel connection with the Storage daemon.
     */
    Dmsg0(110, "Open connection with storage daemon\n");
    jcr->setJobStatus(JS_WaitSD);
    mig_jcr->setJobStatus(JS_WaitSD);

    /*
     * Start conversation with Storage daemon
     */
    if (!ConnectToStorageDaemon(jcr, 10, me->SDConnectTimeout, true)) {
      FreePairedStorage(jcr);
      return false;
    }

    /*
     * Now start a job with the Storage daemon
     */
    if (!StartStorageDaemonJob(jcr, jcr->impl->res.read_storage_list,
                               jcr->impl->res.write_storage_list,
                               /* send_bsr */ true)) {
      FreePairedStorage(jcr);
      return false;
    }

    Dmsg0(150, "Storage daemon connection OK\n");
  }

  /*
   * We re-update the job start record so that the start
   * time is set after the run before job.  This avoids
   * that any files created by the run before job will
   * be saved twice.  They will be backed up in the current
   * job, but not in the next one unless they are changed.
   * Without this, they will be backed up in this job and
   * in the next job run because in that case, their date
   * is after the start of this run.
   */
  jcr->start_time = time(NULL);
  jcr->impl->jr.StartTime = jcr->start_time;
  jcr->impl->jr.JobTDate = jcr->start_time;
  jcr->setJobStatus(JS_Running);

  /*
   * Update job start record for this migration control job
   */
  if (!jcr->db->UpdateJobStartRecord(jcr, &jcr->impl->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", jcr->db->strerror());
    goto bail_out;
  }

  /*
   * Declare the job started to start the MaxRunTime check
   */
  jcr->setJobStarted();

  mig_jcr->start_time = time(NULL);
  mig_jcr->impl->jr.StartTime = mig_jcr->start_time;
  mig_jcr->impl->jr.JobTDate = mig_jcr->start_time;
  mig_jcr->setJobStatus(JS_Running);

  /*
   * Update job start record for the real migration backup job
   */
  if (!mig_jcr->db->UpdateJobStartRecord(mig_jcr, &mig_jcr->impl->jr)) {
    Jmsg(jcr, M_FATAL, 0, "%s", mig_jcr->db->strerror());
    goto bail_out;
  }

  Dmsg4(dbglevel, "mig_jcr: Name=%s JobId=%d Type=%c Level=%c\n",
        mig_jcr->impl->jr.Name, (int)mig_jcr->impl->jr.JobId,
        mig_jcr->impl->jr.JobType, mig_jcr->impl->jr.JobLevel);

  /*
   * If we are connected to two different SDs tell the writing one
   * to be ready to receive the data and tell the reading one
   * to replicate to the other.
   */
  if (jcr->impl->remote_replicate) {
    StorageResource* write_storage = mig_jcr->impl->res.write_storage;
    StorageResource* read_storage = jcr->impl->res.read_storage;
    PoolMem command(PM_MESSAGE);
    uint32_t tls_need = 0;

    if (jcr->max_bandwidth > 0) { SendBwlimitToSd(jcr, jcr->Job); }

    /*
     * Start the job prior to starting the message thread below
     * to avoid two threads from using the BareosSocket structure at
     * the same time.
     */
    if (!mig_jcr->store_bsock->fsend("listen")) { goto bail_out; }

    if (!StartStorageDaemonMessageThread(mig_jcr)) { goto bail_out; }

    /*
     * Send Storage daemon address to the other Storage daemon
     */
    if (write_storage->SDDport == 0) {
      write_storage->SDDport = write_storage->SDport;
    }

    /*
     * TLS Requirement
     */
    tls_need = write_storage->IsTlsConfigured() ? TlsPolicy::kBnetTlsAuto
                                                : TlsPolicy::kBnetTlsNone;

    char* connection_target_address =
        StorageAddressToContact(read_storage, write_storage);

    Mmsg(command, replicatecmd, mig_jcr->JobId, mig_jcr->Job,
         connection_target_address, write_storage->SDDport, tls_need,
         mig_jcr->sd_auth_key);

    if (!jcr->store_bsock->fsend(command.c_str())) { goto bail_out; }

    if (jcr->store_bsock->recv() <= 0) { goto bail_out; }

    std::string OK_replicate{"3000 OK replicate\n"};
    std::string received = jcr->store_bsock->msg;
    if (received != OK_replicate) { goto bail_out; }
  }

  /*
   * Start the job prior to starting the message thread below
   * to avoid two threads from using the BareosSocket structure at
   * the same time.
   */
  if (!jcr->store_bsock->fsend("run")) { goto bail_out; }

  /*
   * Now start a Storage daemon message thread
   */
  if (!StartStorageDaemonMessageThread(jcr)) { goto bail_out; }

  jcr->setJobStatus(JS_Running);
  mig_jcr->setJobStatus(JS_Running);

  /*
   * Pickup Job termination data
   * Note, the SD stores in jcr->JobFiles/ReadBytes/JobBytes/JobErrors or
   * mig_jcr->JobFiles/ReadBytes/JobBytes/JobErrors when replicating to
   * a remote storage daemon.
   */
  if (jcr->impl->remote_replicate) {
    WaitForStorageDaemonTermination(jcr);
    WaitForStorageDaemonTermination(mig_jcr);
    jcr->setJobStatus(jcr->impl->SDJobStatus);
    mig_jcr->db_batch->WriteBatchFileRecords(mig_jcr);
  } else {
    WaitForStorageDaemonTermination(jcr);
    jcr->setJobStatus(jcr->impl->SDJobStatus);
    jcr->db_batch->WriteBatchFileRecords(jcr);
  }

bail_out:
  if (jcr->impl->remote_replicate && mig_jcr) {
    alist* write_storage_list;

    /*
     * Swap the write_storage_list between the jcr and the mig_jcr.
     */
    write_storage_list = mig_jcr->impl->res.write_storage_list;
    mig_jcr->impl->res.write_storage_list = jcr->impl->res.write_storage_list;
    jcr->impl->res.write_storage_list = write_storage_list;

    /*
     * Undo the clear of the write_storage in the jcr and assign the mig_jcr
     * write_storage back to the jcr. This is an undo of the clearing we did
     * earlier as we want the jcr connected to the reading storage daemon and
     * the mig_jcr to the writing jcr. By clearing the write_storage of the jcr
     * the ConnectToStorageDaemon function will do the right thing e.g. connect
     * the jcrs in the way we want them to.
     */
    jcr->impl->res.write_storage = mig_jcr->impl->res.write_storage;
    mig_jcr->impl->res.write_storage = NULL;
  }

  FreePairedStorage(jcr);

  if (jcr->is_JobStatus(JS_Terminated)) {
    MigrationCleanup(jcr, jcr->JobStatus);
    retval = true;
  }

  return retval;
}

/**
 * Select the Jobs to Migrate/Copy using the getJobs_to_migrate function and
 * then exit.
 */
static inline bool DoMigrationSelection(JobControlRecord* jcr)
{
  bool retval;

  retval = getJobs_to_migrate(jcr);
  if (retval) {
    jcr->setJobStatus(JS_Terminated);
    MigrationCleanup(jcr, jcr->JobStatus);
  } else {
    jcr->setJobStatus(JS_ErrorTerminated);
  }

  return retval;
}

bool DoMigration(JobControlRecord* jcr)
{
  /*
   * See if this is a control job e.g. the one that selects the Jobs to Migrate
   * or Copy or one of the worker Jobs that do the actual Migration or Copy. If
   * jcr->impl_->MigrateJobId is unset we know that its the control job.
   */
  if (jcr->impl->MigrateJobId == 0) {
    return DoMigrationSelection(jcr);
  } else {
    return DoActualMigration(jcr);
  }
}

static inline void GenerateMigrateSummary(JobControlRecord* jcr,
                                          MediaDbRecord* mr,
                                          int msg_type,
                                          const char* TermMsg)
{
  double kbps;
  utime_t RunTime;
  JobControlRecord* mig_jcr = jcr->impl->mig_jcr;
  char term_code[100], sd_term_msg[100];
  char sdt[MAX_TIME_LENGTH], edt[MAX_TIME_LENGTH];
  char ec1[30], ec2[30], ec3[30], ec4[30], ec5[30], elapsed[50];
  char ec6[50], ec7[50], ec8[50];

  Bsnprintf(term_code, sizeof(term_code), TermMsg, jcr->get_OperationName(),
            jcr->get_ActionName());
  bstrftimes(sdt, sizeof(sdt), jcr->impl->jr.StartTime);
  bstrftimes(edt, sizeof(edt), jcr->impl->jr.EndTime);
  RunTime = jcr->impl->jr.EndTime - jcr->impl->jr.StartTime;

  JobstatusToAscii(jcr->impl->SDJobStatus, sd_term_msg, sizeof(sd_term_msg));
  if (jcr->impl->previous_jr.JobId != 0) {
    /*
     * Copy/Migrate worker Job.
     */
    if (RunTime <= 0) {
      kbps = 0;
    } else {
      kbps = (double)jcr->impl->SDJobBytes / (1000 * RunTime);
    }

    Jmsg(jcr, msg_type, 0,
         _("%s %s %s (%s):\n"
           "  Build OS:               %s\n"
           "  Prev Backup JobId:      %s\n"
           "  Prev Backup Job:        %s\n"
           "  New Backup JobId:       %s\n"
           "  Current JobId:          %s\n"
           "  Current Job:            %s\n"
           "  Backup Level:           %s\n"
           "  Client:                 %s\n"
           "  FileSet:                \"%s\"\n"
           "  Read Pool:              \"%s\" (From %s)\n"
           "  Read Storage:           \"%s\" (From %s)\n"
           "  Write Pool:             \"%s\" (From %s)\n"
           "  Write Storage:          \"%s\" (From %s)\n"
           "  Next Pool:              \"%s\" (From %s)\n"
           "  Catalog:                \"%s\" (From %s)\n"
           "  Start time:             %s\n"
           "  End time:               %s\n"
           "  Elapsed time:           %s\n"
           "  Priority:               %d\n"
           "  SD Files Written:       %s\n"
           "  SD Bytes Written:       %s (%sB)\n"
           "  Rate:                   %.1f KB/s\n"
           "  Volume name(s):         %s\n"
           "  Volume Session Id:      %d\n"
           "  Volume Session Time:    %d\n"
           "  Last Volume Bytes:      %s (%sB)\n"
           "  SD Errors:              %d\n"
           "  SD termination status:  %s\n"
           "  Bareos binary info:     %s\n"
           "  Job triggered by:       %s\n"
           "  Termination:            %s\n\n"),
         BAREOS, my_name, kBareosVersionStrings.Full,
         kBareosVersionStrings.ShortDate, kBareosVersionStrings.GetOsInfo(),
         edit_uint64(jcr->impl->previous_jr.JobId, ec6),
         jcr->impl->previous_jr.Job,
         mig_jcr ? edit_uint64(mig_jcr->impl->jr.JobId, ec7) : _("*None*"),
         edit_uint64(jcr->impl->jr.JobId, ec8), jcr->impl->jr.Job,
         JobLevelToString(jcr->getJobLevel()),
         jcr->impl->res.client ? jcr->impl->res.client->resource_name_
                               : _("*None*"),
         jcr->impl->res.fileset ? jcr->impl->res.fileset->resource_name_
                                : _("*None*"),
         jcr->impl->res.rpool->resource_name_, jcr->impl->res.rpool_source,
         jcr->impl->res.read_storage
             ? jcr->impl->res.read_storage->resource_name_
             : _("*None*"),
         NPRT(jcr->impl->res.rstore_source),
         jcr->impl->res.pool->resource_name_, jcr->impl->res.pool_source,
         jcr->impl->res.write_storage
             ? jcr->impl->res.write_storage->resource_name_
             : _("*None*"),
         NPRT(jcr->impl->res.wstore_source),
         jcr->impl->res.next_pool ? jcr->impl->res.next_pool->resource_name_
                                  : _("*None*"),
         NPRT(jcr->impl->res.npool_source),
         jcr->impl->res.catalog->resource_name_, jcr->impl->res.catalog_source,
         sdt, edt, edit_utime(RunTime, elapsed, sizeof(elapsed)),
         jcr->JobPriority, edit_uint64_with_commas(jcr->impl->SDJobFiles, ec1),
         edit_uint64_with_commas(jcr->impl->SDJobBytes, ec2),
         edit_uint64_with_suffix(jcr->impl->SDJobBytes, ec3), (float)kbps,
         mig_jcr ? mig_jcr->VolumeName : _("*None*"), jcr->VolSessionId,
         jcr->VolSessionTime, edit_uint64_with_commas(mr->VolBytes, ec4),
         edit_uint64_with_suffix(mr->VolBytes, ec5), jcr->impl->SDErrors,
         sd_term_msg, kBareosVersionStrings.JoblogMessage,
         JobTriggerToString(jcr->impl->job_trigger).c_str(), term_code);
  } else {
    /*
     * Copy/Migrate selection only Job.
     */
    Jmsg(jcr, msg_type, 0,
         _("%s %s %s (%s):\n"
           "  Build OS:               %s\n"
           "  Current JobId:          %s\n"
           "  Current Job:            %s\n"
           "  Catalog:                \"%s\" (From %s)\n"
           "  Start time:             %s\n"
           "  End time:               %s\n"
           "  Elapsed time:           %s\n"
           "  Priority:               %d\n"
           "  Bareos binary info:     %s\n"
           "  Job triggered by:       %s\n"
           "  Termination:            %s\n\n"),
         BAREOS, my_name, kBareosVersionStrings.Full,
         kBareosVersionStrings.ShortDate, kBareosVersionStrings.GetOsInfo(),
         edit_uint64(jcr->impl->jr.JobId, ec8), jcr->impl->jr.Job,
         jcr->impl->res.catalog->resource_name_, jcr->impl->res.catalog_source,
         sdt, edt, edit_utime(RunTime, elapsed, sizeof(elapsed)),
         jcr->JobPriority, kBareosVersionStrings.JoblogMessage,
         JobTriggerToString(jcr->impl->job_trigger).c_str(), term_code);
  }
}

/**
 * Release resources allocated during backup.
 */
void MigrationCleanup(JobControlRecord* jcr, int TermCode)
{
  char ec1[30];
  const char* TermMsg;
  int msg_type = M_INFO;
  MediaDbRecord mr;
  JobControlRecord* mig_jcr = jcr->impl->mig_jcr;
  PoolMem query(PM_MESSAGE);

  Dmsg2(100, "Enter migrate_cleanup %d %c\n", TermCode, TermCode);
  UpdateJobEnd(jcr, TermCode);

  /*
   * Check if we actually did something.
   * mig_jcr is jcr of the newly migrated job.
   */
  if (mig_jcr) {
    char old_jobid[50], new_jobid[50];

    edit_uint64(jcr->impl->previous_jr.JobId, old_jobid);
    edit_uint64(mig_jcr->impl->jr.JobId, new_jobid);

    /*
     * See if we used a remote SD if so the mig_jcr contains
     * the jobfiles and jobbytes and the new volsessionid
     * and volsessiontime as the writing SD generates this info.
     */
    if (jcr->impl->remote_replicate) {
      mig_jcr->JobFiles = jcr->JobFiles = mig_jcr->impl->SDJobFiles;
      mig_jcr->JobBytes = jcr->JobBytes = mig_jcr->impl->SDJobBytes;
    } else {
      mig_jcr->JobFiles = jcr->JobFiles = jcr->impl->SDJobFiles;
      mig_jcr->JobBytes = jcr->JobBytes = jcr->impl->SDJobBytes;
      mig_jcr->VolSessionId = jcr->VolSessionId;
      mig_jcr->VolSessionTime = jcr->VolSessionTime;
    }
    mig_jcr->impl->jr.RealEndTime = 0;
    mig_jcr->impl->jr.PriorJobId = jcr->impl->previous_jr.JobId;

    if (jcr->is_JobStatus(JS_Terminated) &&
        (jcr->JobErrors || jcr->impl->SDErrors)) {
      TermCode = JS_Warnings;
    }

    UpdateJobEnd(mig_jcr, TermCode);

    /*
     * Update final items to set them to the previous job's values
     */
    Mmsg(query,
         "UPDATE Job SET StartTime='%s',EndTime='%s',"
         "JobTDate=%s WHERE JobId=%s",
         jcr->impl->previous_jr.cStartTime, jcr->impl->previous_jr.cEndTime,
         edit_uint64(jcr->impl->previous_jr.JobTDate, ec1), new_jobid);
    jcr->db->SqlQuery(query.c_str());

    if (jcr->IsTerminatedOk()) {
      UaContext* ua;

      switch (jcr->getJobType()) {
        case JT_MIGRATE:
          /*
           * If we terminated a Migration Job successfully we should:
           * - Mark the previous job as migrated
           * - Move any Log records to the new JobId
           * - Move any MetaData of a NDMP backup
           * - Purge the File records from the previous job
           */
          Mmsg(query, "UPDATE Job SET Type='%c' WHERE JobId=%s",
               (char)JT_MIGRATED_JOB, old_jobid);
          mig_jcr->db->SqlQuery(query.c_str());

          /*
           * Move JobLog to new JobId
           */
          Mmsg(query, "UPDATE Log SET JobId=%s WHERE JobId=%s", new_jobid,
               old_jobid);
          mig_jcr->db->SqlQuery(query.c_str());

          /*
           * If we just migrated a NDMP job, we need to move the file MetaData
           * to the new job. The file MetaData is stored as hardlinks to the
           * NDMP archive itself. And as we only clone the actual data in the
           * storage daemon we need to add data normally send to the director
           * via the FHDB interface here.
           */
          switch (jcr->impl->res.client->Protocol) {
            case APT_NDMPV2:
            case APT_NDMPV3:
            case APT_NDMPV4:
              Mmsg(query, sql_migrate_ndmp_metadata, new_jobid, old_jobid,
                   new_jobid);
              mig_jcr->db->SqlQuery(query.c_str());
              break;
            default:
              break;
          }

          ua = new_ua_context(jcr);
          if (jcr->impl->res.job->PurgeMigrateJob) {
            /*
             * Purge old Job record
             */
            PurgeJobsFromCatalog(ua, old_jobid);
          } else {
            /*
             * Purge all old file records, but leave Job record
             */
            PurgeFilesFromJobs(ua, old_jobid);
          }

          FreeUaContext(ua);
          break;
        case JT_COPY:
          /*
           * If we terminated a Copy Job successfully we should:
           * - Copy any Log records to the new JobId
           * - Copy any MetaData of a NDMP backup
           * - Set type="Job Copy" for the new job
           */
          Mmsg(query,
               "INSERT INTO Log (JobId, Time, LogText ) "
               "SELECT %s, Time, LogText FROM Log WHERE JobId=%s",
               new_jobid, old_jobid);
          mig_jcr->db->SqlQuery(query.c_str());

          /*
           * If we just copied a NDMP job, we need to copy the file MetaData
           * to the new job. The file MetaData is stored as hardlinks to the
           * NDMP archive itself. And as we only clone the actual data in the
           * storage daemon we need to add data normally send to the director
           * via the FHDB interface here.
           */
          switch (jcr->impl->res.client->Protocol) {
            case APT_NDMPV2:
            case APT_NDMPV3:
            case APT_NDMPV4:
              Mmsg(query, sql_copy_ndmp_metadata, new_jobid, old_jobid,
                   new_jobid);
              mig_jcr->db->SqlQuery(query.c_str());
              break;
            default:
              break;
          }

          Mmsg(query, "UPDATE Job SET Type='%c' WHERE JobId=%s",
               (char)JT_JOB_COPY, new_jobid);
          mig_jcr->db->SqlQuery(query.c_str());
          break;
        default:
          break;
      }
    }

    if (!jcr->db->GetJobRecord(jcr, &jcr->impl->jr)) {
      Jmsg(jcr, M_WARNING, 0,
           _("Error getting Job record for Job report: ERR=%s"),
           jcr->db->strerror());
      jcr->setJobStatus(JS_ErrorTerminated);
    }

    UpdateBootstrapFile(mig_jcr);

    if (!mig_jcr->db->GetJobVolumeNames(mig_jcr, mig_jcr->impl->jr.JobId,
                                        mig_jcr->VolumeName)) {
      /*
       * Note, if the job has failed, most likely it did not write any
       * tape, so suppress this "error" message since in that case
       * it is normal. Or look at it the other way, only for a
       * normal exit should we complain about this error.
       */
      if (jcr->IsTerminatedOk() && jcr->impl->jr.JobBytes) {
        Jmsg(jcr, M_ERROR, 0, "%s", mig_jcr->db->strerror());
      }
      mig_jcr->VolumeName[0] = 0; /* none */
    }

    if (mig_jcr->VolumeName[0]) {
      /*
       * Find last volume name. Multiple vols are separated by |
       */
      char* p = strrchr(mig_jcr->VolumeName, '|');
      if (p) {
        p++; /* skip | */
      } else {
        p = mig_jcr->VolumeName; /* no |, take full name */
      }
      bstrncpy(mr.VolumeName, p, sizeof(mr.VolumeName));
      if (!jcr->db->GetMediaRecord(jcr, &mr)) {
        Jmsg(jcr, M_WARNING, 0,
             _("Error getting Media record for Volume \"%s\": ERR=%s"),
             mr.VolumeName, jcr->db->strerror());
      }
    }

    switch (jcr->JobStatus) {
      case JS_Terminated:
        TermMsg = _("%s OK");
        break;
      case JS_Warnings:
        TermMsg = _("%s OK -- with warnings");
        break;
      case JS_FatalError:
      case JS_ErrorTerminated:
      case JS_Canceled:
        /*
         * We catch any error here as the close of the SD sessions is mandatory
         * for each failure path. The termination message and the message type
         * can be different so that is why we do a second switch inside the
         * switch on the JobStatus.
         */
        switch (jcr->JobStatus) {
          case JS_Canceled:
            TermMsg = _("%s Canceled");
            break;
          default:
            TermMsg = _("*** %s Error ***");
            msg_type = M_ERROR; /* Generate error message */
            break;
        }

        /*
         * Close connection to Reading SD.
         */
        if (jcr->store_bsock) {
          jcr->store_bsock->signal(BNET_TERMINATE);
          if (jcr->impl->SD_msg_chan_started) {
            pthread_cancel(jcr->impl->SD_msg_chan);
          }
        }

        /*
         * Close connection to Writing SD (if SD-SD replication)
         */
        if (mig_jcr->store_bsock) {
          mig_jcr->store_bsock->signal(BNET_TERMINATE);
          if (mig_jcr->impl->SD_msg_chan_started) {
            pthread_cancel(mig_jcr->impl->SD_msg_chan);
          }
        }
        break;
      default:
        TermMsg = _("Inappropriate %s term code");
        break;
    }
  } else if (jcr->impl->HasSelectedJobs) {
    switch (jcr->JobStatus) {
      case JS_Terminated:
        TermMsg = _("%s OK");
        break;
      case JS_Warnings:
        TermMsg = _("%s OK -- with warnings");
        break;
      case JS_FatalError:
      case JS_ErrorTerminated:
        TermMsg = _("*** %s Error ***");
        msg_type = M_ERROR; /* Generate error message */
        break;
      case JS_Canceled:
        TermMsg = _("%s Canceled");
        break;
      default:
        TermMsg = _("Inappropriate %s term code");
        break;
    }
  } else {
    TermMsg = _("%s -- no files to %s");
  }

  GenerateMigrateSummary(jcr, &mr, msg_type, TermMsg);

  Dmsg0(100, "Leave migrate_cleanup()\n");
}
} /* namespace directordaemon */
