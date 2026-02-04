/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, February MMII
/**
 * @file
 * User Agent Database prune Command
 *
 * Applies retention periods
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/ua_input.h"
#include "cats/sql.h"
#include "dird/ua_db.h"
#include "dird/ua_select.h"
#include "dird/ua_prune.h"
#include "dird/ua_purge.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"

#include <algorithm>

namespace directordaemon {

/* Forward referenced functions */
static bool PruneDirectory(UaContext* ua, ClientResource* client);
static bool PruneStats(UaContext* ua, utime_t retention);

// Called here to count entries to be deleted
int DelCountHandler(void* ctx, int, char** row)
{
  s_count_ctx* cnt = static_cast<s_count_ctx*>(ctx);

  if (row[0]) {
    cnt->count = str_to_int64(row[0]);
  } else {
    cnt->count = 0;
  }
  return 0;
}

/**
 * Called here to make in memory list of JobIds to be
 *  deleted and the associated PurgedFiles flag.
 *  The in memory list will then be traversed
 *  to issue the SQL DELETE commands.  Note, the list
 *  is allowed to get to MAX_DEL_LIST_LEN to limit the
 *  maximum malloc'ed memory.
 */
int JobDeleteHandler(void* ctx, int, char** row)
{
  std::vector<JobId_t>* jobs_todelete = static_cast<std::vector<JobId_t>*>(ctx);

  if (jobs_todelete->size() >= MAX_DEL_LIST_LEN) { return 1; }

  jobs_todelete->push_back(static_cast<JobId_t>(str_to_int64(row[0])));
  Dmsg2(60, "JobDeleteHandler row=%" PRIuz " val=%" PRIu32 "\n",
        jobs_todelete->size(), jobs_todelete->back());
  return 0;
}

int FileDeleteHandler(void* ctx, int, char** row)
{
  std::vector<JobId_t>* jobs_todelete = static_cast<std::vector<JobId_t>*>(ctx);
  if (jobs_todelete->size() >= MAX_DEL_LIST_LEN) { return 1; }
  jobs_todelete->push_back(static_cast<JobId_t>(str_to_int64(row[0])));
  return 0;
}

static bool PruneAllVolumes(UaContext* ua,
                            PoolResource* pool,
                            PoolDbRecord pr,
                            MediaDbRecord mr)
{
  bool result = true;
  db_list_ctx volumenames;
  if (!ua->db->GetAllVolumeNames(&volumenames)) { return false; }
  for (const auto& volume : volumenames) {
    if (!SelectMediaDbrByName(ua, &mr, volume.c_str())) { return false; }
    if (!SelectPoolForMediaDbr(ua, &pr, &mr)) { return false; }
    if (pool) {
      if (strcmp(pool->resource_name_, pr.Name)) { continue; }
    }

    if (mr.Enabled == VOL_ARCHIVED) {
      ua->ErrorMsg(T_("Cannot prune Volume \"%s\" because it is archived.\n"),
                   mr.VolumeName);
      result = false;
      continue;
    }

    std::string msg("Volume");
    msg.append(" (" + volume + ")");
    if (!ConfirmRetention(ua, &mr.VolRetention, msg.c_str())) {
      result = false;
      break;
    }

    result &= PruneVolume(ua, &mr);
  }

  return result;
}

// Prune records from database
bool PruneCmd(UaContext* ua, const char*)
{
  ClientResource* client;
  PoolResource* pool;
  PoolDbRecord pr;
  MediaDbRecord mr;
  utime_t retention;
  int kw;
  const char* permission_denied_message
      = T_("Permission denied: need full %s permission.\n");
  static const char* keywords[]
      = {NT_("Files"), NT_("Jobs"),      NT_("Volume"),
         NT_("Stats"), NT_("Directory"), NULL};

  /* All prune commands might target jobs that reside on different storages.
   * Instead of checking all of them,
   * we require full permission on jobs and storages.
   * Client and Pool permissions are checked at the individual subcommands. */
  if (ua->AclHasRestrictions(Job_ACL)) {
    ua->ErrorMsg(permission_denied_message, "job");
    return false;
  }
  if (ua->AclHasRestrictions(Storage_ACL)) {
    ua->ErrorMsg(permission_denied_message, "storage");
    return false;
  }

  if (!OpenClientDb(ua, true)) { return false; }

  // First search args
  kw = FindArgKeyword(ua, keywords);
  if (kw < 0 || kw > 4) {
    // No args, so check known equivalents
    for (int i = 1; i < ua->argc; i++) {
      if (bstrncasecmp(ua->argk[i], NT_("Volume"), 6)) {
        kw = 2;
        break;
      }
    }

    if (kw < 0 || kw > 4) {
      // still nothing? ask user
      kw = DoKeywordPrompt(ua, T_("Choose item to prune"), keywords);
    }
  }

  switch (kw) {
    case 0: /* prune files */

      if (!(client = get_client_resource(ua))) { return false; }

      if ((FindArgWithValue(ua, NT_("pool")) >= 0)
          || ua->AclHasRestrictions(Pool_ACL)) {
        pool = get_pool_resource(ua);
      } else {
        pool = NULL;
      }

      // Pool File Retention takes precedence over client File Retention
      if (pool && pool->FileRetention > 0) {
        if (!ConfirmRetention(ua, &pool->FileRetention, "File")) {
          return false;
        }
      } else if (!ConfirmRetention(ua, &client->FileRetention, "File")) {
        return false;
      }

      PruneFiles(ua, client, pool);

      return true;
    case 1: { /* prune jobs */
      if (!(client = get_client_resource(ua))) { return false; }

      if ((FindArgWithValue(ua, NT_("pool")) >= 0)
          || ua->AclHasRestrictions(Pool_ACL)) {
        pool = get_pool_resource(ua);
      } else {
        pool = NULL;
      }

      // Pool Job Retention takes precedence over client Job Retention
      if (pool && pool->JobRetention > 0) {
        if (!ConfirmRetention(ua, &pool->JobRetention, "Job")) { return false; }
      } else if (!ConfirmRetention(ua, &client->JobRetention, "Job")) {
        return false;
      }

      return PruneJobs(ua, client, pool);
    }
    case 2: /* prune volume */
      if (FindArg(ua, "all") >= 0) {
        if ((FindArgWithValue(ua, NT_("pool")) >= 0)
            || ua->AclHasRestrictions(Pool_ACL)) {
          pool = get_pool_resource(ua);
        } else {
          pool = nullptr;
        }
        return PruneAllVolumes(ua, pool, pr, mr);
      } else {
        if (ua->AclHasRestrictions(Client_ACL)) {
          ua->ErrorMsg(permission_denied_message, "client");
          return false;
        }
        if (!SelectMediaDbr(ua, &mr)) { return false; }
        if (!SelectPoolForMediaDbr(ua, &pr, &mr)) { return false; }

        if (mr.Enabled == VOL_ARCHIVED) {
          ua->ErrorMsg(
              T_("Cannot prune Volume \"%s\" because it is archived.\n"),
              mr.VolumeName);
          return false;
        }

        if (!ConfirmRetention(ua, &mr.VolRetention, "Volume")) { return false; }

        return PruneVolume(ua, &mr);
      }

    case 3: /* prune stats */
      if (!me->stats_retention) { return false; }

      retention = me->stats_retention;

      if (ua->AclHasRestrictions(Client_ACL)) {
        ua->ErrorMsg(permission_denied_message, "client");
        return false;
      }
      if (ua->AclHasRestrictions(Pool_ACL)) {
        ua->ErrorMsg(permission_denied_message, "pool");
        return false;
      }

      if (!ConfirmRetention(ua, &retention, "Statistics")) { return false; }

      return PruneStats(ua, retention);
    case 4: /* prune directory */

      if (ua->AclHasRestrictions(Pool_ACL)) {
        ua->ErrorMsg(permission_denied_message, "pool");
        return false;
      }

      if ((FindArgWithValue(ua, NT_("client")) >= 0)
          || ua->AclHasRestrictions(Client_ACL)) {
        if (!(client = get_client_resource(ua))) { return false; }
      } else {
        client = NULL;
      }

      return PruneDirectory(ua, client);

    default:
      break;
  }

  return true;
}

// Prune Directory meta data records from the database.
static bool PruneDirectory(UaContext* ua, ClientResource* client)
{
  int i, len;
  ClientDbRecord cr;
  char* prune_topdir = NULL;
  PoolMem query(PM_MESSAGE), temp(PM_MESSAGE);
  bool recursive = false;

  // See if a client was selected.
  if (!client) {
    if (!GetYesno(ua, T_("No client restriction given really remove "
                         "directory for all clients (yes/no): "))
        || !ua->pint32_val) {
      if (!(client = get_client_resource(ua))) { return false; }
    }
  }

  // See if we need to recursively remove all directories under a certain
  // path.
  recursive = FindArg(ua, NT_("recursive")) >= 0;

  // Get the directory to prune.
  i = FindArgWithValue(ua, NT_("directory"));
  if (i >= 0) {
    PmStrcpy(temp, ua->argv[i]);
  } else {
    if (recursive) {
      if (!GetCmd(ua, T_("Please enter the full path prefix to remove: "),
                  false)) {
        return false;
      }
    } else {
      if (!GetCmd(ua, T_("Please enter the full path to remove: "), false)) {
        return false;
      }
    }
    PmStrcpy(temp, ua->cmd);
  }

  /* See if the directory ends in a / and escape it for usage in a database
   * query. */
  len = strlen(temp.c_str());
  if (*(temp.c_str() + len - 1) != '/') {
    PmStrcat(temp, "/");
    len++;
  }
  prune_topdir = (char*)malloc(len * 2 + 1);
  ua->db->EscapeString(ua->jcr, prune_topdir, temp.c_str(), len);

  // Remove all files in particular directory.
  if (recursive) {
    Mmsg(query,
         "DELETE FROM file WHERE pathid IN ("
         "SELECT pathid FROM path "
         "WHERE path LIKE '%s%%'"
         ")",
         prune_topdir);
  } else {
    Mmsg(query,
         "DELETE FROM file WHERE pathid IN ("
         "SELECT pathid FROM path "
         "WHERE path LIKE '%s'"
         ")",
         prune_topdir);
  }

  if (client) {
    char ed1[50];
    cr = ClientDbRecord{};
    bstrncpy(cr.Name, client->resource_name_, sizeof(cr.Name));
    if (!ua->db->CreateClientRecord(ua->jcr, &cr)) {
      if (prune_topdir) { free(prune_topdir); }
      return false;
    }

    Mmsg(temp,
         " AND JobId IN ("
         "SELECT JobId FROM Job "
         "WHERE ClientId=%s"
         ")",
         edit_int64(cr.ClientId, ed1));

    PmStrcat(query, temp.c_str());
  }
  {
    DbLocker _{ua->db};
    ua->db->SqlExec(query.c_str());
  }

  /* If we removed the entries from the file table without limiting it to a
   * certain client we created orphaned path entries as no one is referencing
   * them anymore. */
  if (!client) {
    if (!GetYesno(ua, T_("Cleanup orphaned path records (yes/no):"))
        || !ua->pint32_val) {
      if (prune_topdir) { free(prune_topdir); }
      return true;
    }

    if (recursive) {
      Mmsg(query,
           "DELETE FROM path "
           "WHERE path LIKE '%s%%'",
           prune_topdir);
    } else {
      Mmsg(query,
           "DELETE FROM path "
           "WHERE path LIKE '%s'",
           prune_topdir);
    }
    {
      DbLocker _{ua->db};
      ua->db->SqlExec(query.c_str());
    }
  }

  if (prune_topdir) { free(prune_topdir); }
  return true;
}

// Prune Job stat records from the database.
static bool PruneStats(UaContext* ua, utime_t retention)
{
  char ed1[50];
  char dt[MAX_TIME_LENGTH];
  PoolMem query(PM_MESSAGE);
  utime_t now = (utime_t)time(NULL);

  {
    DbLocker _{ua->db};
    Mmsg(query, "DELETE FROM JobHisto WHERE JobTDate < %s",
         edit_int64(now - retention, ed1));
    ua->db->SqlExec(query.c_str());
  }

  ua->InfoMsg(T_("Pruned Jobs from JobHisto in catalog.\n"));

  bstrutime(dt, sizeof(dt), now - retention);
  {
    DbLocker _{ua->db};
    Mmsg(query, "DELETE FROM DeviceStats WHERE SampleTime < '%s'", dt);
    ua->db->SqlExec(query.c_str());
  }

  ua->InfoMsg(T_("Pruned Statistics from DeviceStats in catalog.\n"));
  {
    DbLocker _{ua->db};
    Mmsg(query, "DELETE FROM JobStats WHERE SampleTime < '%s'", dt);
    ua->db->SqlExec(query.c_str());
  }

  ua->InfoMsg(T_("Pruned Statistics from JobStats in catalog.\n"));

  return true;
}

/**
 * Use pool and client specified by user to select jobs to prune
 * returns add_from string to add in FROM clause
 *         add_where string to add in WHERE clause
 */
static bool prune_set_filter(UaContext* ua,
                             ClientResource* client,
                             PoolResource* pool,
                             utime_t period,
                             PoolMem* add_from,
                             PoolMem* add_where)
{
  utime_t now;
  char ed1[50], ed2[MAX_ESCAPE_NAME_LENGTH];
  PoolMem tmp(PM_MESSAGE);

  now = (utime_t)time(NULL);
  edit_int64(now - period, ed1);
  Dmsg3(150, "now=%" PRId64 " period=%" PRId64 " JobTDate=%s\n", now, period,
        ed1);
  Mmsg(tmp, " AND JobTDate < %s ", ed1);
  PmStrcat(*add_where, tmp.c_str());

  DbLocker _{ua->db};
  if (client) {
    ua->db->EscapeString(ua->jcr, ed2, client->resource_name_,
                         strlen(client->resource_name_));
    Mmsg(tmp, " AND Client.Name = '%s' ", ed2);
    PmStrcat(*add_where, tmp.c_str());
    PmStrcat(*add_from, " JOIN Client USING (ClientId) ");
  }

  if (pool) {
    ua->db->EscapeString(ua->jcr, ed2, pool->resource_name_,
                         strlen(pool->resource_name_));
    Mmsg(tmp, " AND Pool.Name = '%s' ", ed2);
    PmStrcat(*add_where, tmp.c_str());
    PmStrcat(*add_from, " JOIN Pool USING(PoolId) ");
  }
  Dmsg2(150, "f=%s w=%s\n", add_from->c_str(), add_where->c_str());
  return true;
}

/**
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
bool PruneFiles(UaContext* ua, ClientResource* client, PoolResource* pool)
{
  char ed1[50];

  utime_t period;
  if (pool && pool->FileRetention > 0) {
    period = pool->FileRetention;

  } else if (client) {
    period = client->FileRetention;

  } else { /* should specify at least pool or client */
    return false;
  }

  DbLocker _{ua->db};
  /* Specify JobTDate and Pool.Name= and/or Client.Name= in the query */
  PoolMem sql_where(PM_MESSAGE);
  PoolMem sql_from(PM_MESSAGE);
  if (!prune_set_filter(ua, client, pool, period, &sql_from, &sql_where)) {
    return true;
  }

  ua->SendMsg(T_("Begin pruning Files.\n"));
  /* Select Jobs -- for counting */
  PoolMem query(PM_MESSAGE);
  Mmsg(query, "SELECT COUNT(1) FROM Job %s WHERE PurgedFiles=0 %s",
       sql_from.c_str(), sql_where.c_str());
  Dmsg1(050, "select sql=%s\n", query.c_str());

  struct s_count_ctx cnt;
  cnt.count = 0;
  if (!ua->db->SqlQuery(query.c_str(), DelCountHandler,
                        static_cast<void*>(&cnt))) {
    ua->ErrorMsg("%s", ua->db->strerror());
    Dmsg0(050, "Count failed\n");
    return true;
  }

  if (cnt.count == 0) {
    if (ua->verbose) { ua->WarningMsg(T_("No Files found to prune.\n")); }
    return true;
  }

  std::vector<JobId_t> prune_list;

  /* Now process same set but making a delete list */
  Mmsg(query, "SELECT JobId FROM Job %s WHERE PurgedFiles=0 %s ORDER BY JobId",
       sql_from.c_str(), sql_where.c_str());
  Dmsg1(050, "select sql=%s\n", query.c_str());
  ua->db->SqlQuery(query.c_str(), FileDeleteHandler,
                   static_cast<void*>(&prune_list));

  PurgeFilesFromJobList(ua, prune_list);

  edit_uint64_with_commas(prune_list.size(), ed1);
  ua->InfoMsg(T_("Pruned Files from %s Jobs for client %s from catalog.\n"),
              ed1, client->resource_name_);

  return true;
}

static void DropTempTables(UaContext* ua)
{
  ua->db->SqlQuery(BareosDb::SQL_QUERY::drop_deltabs);
}

static bool CreateTempTables(UaContext* ua)
{
  /* Create temp tables and indicies */
  if (!ua->db->SqlQuery(BareosDb::SQL_QUERY::create_deltabs)) {
    ua->ErrorMsg("%s", ua->db->strerror());
    Dmsg0(050, "create DelTables table failed\n");
    return false;
  }

  if (!ua->db->SqlQuery(BareosDb::SQL_QUERY::create_delindex)) {
    ua->ErrorMsg("%s", ua->db->strerror());
    Dmsg0(050, "create DelInx1 index failed\n");
    return false;
  }

  return true;
}

struct accurate_check_ctx {
  DBId_t ClientId;  /* Id of client */
  DBId_t FileSetId; /* Id of FileSet */
};

// row: Job.Name, FileSet, Client.Name, FileSetId, ClientId, Type
static int JobSelectHandler(void* ctx, int num_fields, char** row)
{
  ASSERT(num_fields == 6);

  // If this job doesn't exist anymore in the configuration, delete it.
  if (my_config->GetResWithName(R_JOB, row[0], false) == NULL) { return 0; }

  // If this fileset doesn't exist anymore in the configuration, delete it.
  if (my_config->GetResWithName(R_FILESET, row[1], false) == NULL) { return 0; }

  // If this client doesn't exist anymore in the configuration, delete it.
  if (my_config->GetResWithName(R_CLIENT, row[2], false) == NULL) { return 0; }

  // Don't compute accurate things for Verify jobs
  if (*row[5] == 'V') { return 0; }

  std::vector<accurate_check_ctx>* lst
      = static_cast<std::vector<accurate_check_ctx>*>(ctx);
  accurate_check_ctx res{};
  res.FileSetId = str_to_int64(row[3]);
  res.ClientId = str_to_int64(row[4]);
  lst->emplace_back(res);

  return 0;
}

/**
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
bool PruneJobs(UaContext* ua, ClientResource* client, PoolResource* pool)
{
  utime_t period;
  if (pool && pool->JobRetention > 0) {
    period = pool->JobRetention;
  } else if (client) {
    period = client->JobRetention;
  } else {  // should specify at least pool or client
    return false;
  }

  PoolMem sql_where(PM_MESSAGE);
  PoolMem sql_from(PM_MESSAGE);
  DbLocker _{ua->db};
  if (!prune_set_filter(ua, client, pool, period, &sql_from, &sql_where)) {
    return true;
  }

  // Drop any previous temporary tables still there
  DropTempTables(ua);

  // Create temp tables and indicies
  if (!CreateTempTables(ua)) {
    DropTempTables(ua);
    return true;
  }

  char ed1[50];
  edit_utime(period, ed1, sizeof(ed1));
  ua->SendMsg(T_("Begin pruning Jobs older than %s.\n"), ed1);

  /* Select all files that are older than the JobRetention period
   * and add them into the "DeletionCandidates" table. */

  PoolMem query(PM_MESSAGE);
  Mmsg(query,
       "INSERT INTO DelCandidates "
       "SELECT JobId,PurgedFiles,FileSetId,JobFiles,JobStatus "
       "FROM Job %s " /* JOIN Pool/Client */
       "WHERE Type NOT IN ('A') "
       " %s ", /* Pool/Client + JobTDate */
       sql_from.c_str(), sql_where.c_str());

  Dmsg1(050, "select sql=%s\n", query.c_str());
  if (!ua->db->SqlExec(query.c_str())) {
    if (ua->verbose) { ua->ErrorMsg("%s", ua->db->strerror()); }
    DropTempTables(ua);
    return true;
  }

  /* Now, for the selection, we discard some of them in order to be always
   * able to restore files. (ie, last full, last diff, last incrs)
   * Note: The DISTINCT could be more useful if we don't get FileSetId */

  std::vector<accurate_check_ctx> accurate_job_check;
  Mmsg(query,
       "SELECT DISTINCT Job.Name, FileSet, Client.Name, Job.FileSetId, "
       "Job.ClientId, Job.Type "
       "FROM DelCandidates "
       "JOIN Job USING (JobId) "
       "JOIN Client USING (ClientId) "
       "JOIN FileSet ON (Job.FileSetId = FileSet.FileSetId) "
       "WHERE Job.Type IN ('B') "         /* Look only Backup jobs */
       "AND Job.JobStatus IN ('T', 'W') " /* Look only useful jobs */
  );

  /* The JobSelectHandler will skip jobs or filesets that are no longer
   * in the configuration file. Interesting ClientId/FileSetId will be
   * added to jobids_check. */
  if (!ua->db->SqlQuery(query.c_str(), JobSelectHandler, &accurate_job_check)) {
    ua->ErrorMsg("%s", ua->db->strerror());
  }

  /* For this selection, we exclude current jobs used for restore or
   * accurate. This will prevent to prune the last full backup used for
   * current backup & restore */

  // To find useful jobs, we do like an incremental
  JobDbRecord jr;
  jr.JobLevel = L_INCREMENTAL;

  db_list_ctx jobids;
  db_list_ctx tempids;
  for (auto elt : accurate_job_check) {
    jr.ClientId = elt.ClientId; /* should be always the same */
    jr.FileSetId = elt.FileSetId;
    ua->db->AccurateGetJobids(ua->jcr, &jr, &tempids);
    jobids.add(tempids);
  }

  /* Discard latest Verify level=InitCatalog job
   * TODO: can have multiple fileset */
  Mmsg(query,
       "SELECT JobId, JobTDate "
       "FROM Job %s " /* JOIN Client/Pool */
       "WHERE Type='V'    AND Level='V' "
       " %s " /* Pool, JobTDate, Client */
       "ORDER BY JobTDate DESC LIMIT 1",
       sql_from.c_str(), sql_where.c_str());

  if (!ua->db->SqlQuery(query.c_str(), DbListHandler, &jobids)) {
    ua->ErrorMsg("%s", ua->db->strerror());
  }

  // We use DISTINCT because we can have two times the same job
  Mmsg(query,
       "SELECT DISTINCT DelCandidates.JobId "
       "FROM DelCandidates ORDER BY JobId");

  std::vector<JobId_t> prune_list;
  if (!ua->db->SqlQuery(query.c_str(), JobDeleteHandler,
                        static_cast<void*>(&prune_list))) {
    ua->ErrorMsg("%s", ua->db->strerror());
  }

  PurgeJobListFromCatalog(ua, prune_list);

  if (prune_list.size() > 0) {
    ua->InfoMsg(T_("Pruned %" PRIuz " %s for client %s from catalog.\n"),
                prune_list.size(),
                prune_list.size() == 1 ? T_("Job") : T_("Jobs"),
                client->resource_name_);
  }

  DropTempTables(ua);
  return true;
}

// Prune a given Volume
bool PruneVolume(UaContext* ua, MediaDbRecord* mr)
{
  bool VolumeIsNowEmtpy = false;

  if (mr->Enabled == VOL_ARCHIVED) {
    return false; /* Cannot prune archived volumes */
  }

  DbLocker _{ua->db};

  /* Prune only Volumes with status "Full", or "Used" */
  if (bstrcmp(mr->VolStatus, "Full") || bstrcmp(mr->VolStatus, "Used")) {
    Dmsg2(050, "get prune list MediaId=%d Volume %s\n", (int)mr->MediaId,
          mr->VolumeName);

    std::vector<JobId_t> prune_list;
    int NumJobsToBePruned = GetPruneListForVolume(ua, mr, prune_list);
    ua->SendMsg(
        T_("Pruning volume %s: %d Jobs have expired and can be pruned.\n"),
        mr->VolumeName, NumJobsToBePruned);
    Dmsg1(050, "Num pruned = %d\n", NumJobsToBePruned);
    if (NumJobsToBePruned != 0) { PurgeJobListFromCatalog(ua, prune_list); }
    VolumeIsNowEmtpy = IsVolumePurged(ua, mr);

    if (!VolumeIsNowEmtpy) {
      ua->SendMsg(T_("Volume \"%s\" still contains jobs after pruning.\n"),
                  mr->VolumeName);
    } else {
      ua->SendMsg(T_("Volume \"%s\" contains no jobs after pruning.\n"),
                  mr->VolumeName);
    }
  } else {
    ua->SendMsg(
        T_("Pruning volume %s: cannot prune as Volstatus is %s but needs to "
           "be "
           "Full or Used.\n"),
        mr->VolumeName, mr->VolStatus);
  }

  return VolumeIsNowEmtpy;
}

// Get prune list for a volume
int GetPruneListForVolume(UaContext* ua,
                          MediaDbRecord* mr,
                          std::vector<JobId_t>& prune_list)
{
  PoolMem query(PM_MESSAGE);
  utime_t now;
  char ed1[50], ed2[50];

  if (mr->Enabled == VOL_ARCHIVED) {
    return 0; /* cannot prune Archived volumes */
  }

  // Now add to the  list of JobIds for Jobs written to this Volume
  utime_t VolRetention = mr->VolRetention;
  now = (utime_t)time(NULL);
  ua->db->FillQuery(query, BareosDb::SQL_QUERY::sel_JobMedia,
                    edit_int64(mr->MediaId, ed1),
                    edit_int64(now - VolRetention, ed2));

  Dmsg3(250, "Now=%d VolRetention=%d now-VolRetention=%s\n", (int)now,
        (int)VolRetention, ed2);
  Dmsg1(050, "Query=%s\n", query.c_str());


  if (!ua->db->SqlQuery(query.c_str(), FileDeleteHandler,
                        static_cast<void*>(&prune_list))) {
    if (ua->verbose) { ua->ErrorMsg("%s", ua->db->strerror()); }
    Dmsg0(050, "Adding eligible jobs for pruning failed\n");
    return 0;
  }

  // in case job was the result of a copy or migration, delete the related
  // migration or copy job
  if (prune_list.size() > 0) {
    BStringList copy_migrate_jobs_list;
    for (auto jobid : prune_list) { copy_migrate_jobs_list << jobid; }
    Mmsg(query,
         "SELECT JobId from Job "
         "WHERE type in ('%c','%c') AND priorjobid in (%s)",
         JT_COPY, JT_MIGRATE, copy_migrate_jobs_list.Join(',').c_str());

    if (!ua->db->SqlQuery(query.c_str(), FileDeleteHandler,
                          static_cast<void*>(&prune_list))) {
      if (ua->verbose) { ua->ErrorMsg("%s", ua->db->strerror()); }
      Dmsg0(050,
            "Adding migration and copy child jobs eligible for pruning "
            "failed\n");
      return 0;
    }
  }

  int NumJobsToBePruned = ExcludeRunningJobsFromList(prune_list);
  if (NumJobsToBePruned > 0) {
    ua->SendMsg(T_("Volume \"%s\" has Volume Retention of %" PRId64
                   " sec. and has %d jobs that will be pruned\n"),
                mr->VolumeName, VolRetention, NumJobsToBePruned);
  }
  return NumJobsToBePruned;
}

/**
 * We have a list of jobs to prune or purge. If any of them is
 *   currently running, we set its JobId to zero which effectively
 *   excludes it.
 *
 * Returns the number of jobs that can be pruned or purged.
 */
int ExcludeRunningJobsFromList(std::vector<JobId_t>& prune_list)
{
  JobControlRecord* jcr;

  /* Do not prune any job currently running */
  foreach_jcr (jcr) {
    prune_list.erase(std::remove_if(prune_list.begin(), prune_list.end(),
                                    [&jcr](const JobId_t& jobid) {
                                      return jcr->JobId == jobid || jobid == 0;
                                    }),
                     prune_list.end());
  }
  endeach_jcr(jcr);
  return prune_list.size();
}

} /* namespace directordaemon */
