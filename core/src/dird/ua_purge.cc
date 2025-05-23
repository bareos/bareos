/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.
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
 * BAREOS Director -- User Agent Database Purge Command
 *
 * Purges Files from specific JobIds
 * or
 * Purges Jobs from Volumes
 */

#include "include/bareos.h"
#include "cats/sql.h"
#include "dird.h"
#include "dird/director_jcr_impl.h"
#include "dird/next_vol.h"
#include "dird/sd_cmds.h"
#include "dird/ua_db.h"
#include "dird/ua_update.h"
#include "dird/ua_input.h"
#include "dird/ua_select.h"
#include "dird/ua_prune.h"
#include "dird/ua_purge.h"
#include "include/auth_protocol_types.h"
#include "lib/bstringlist.h"
#include "lib/edit.h"
#include "lib/util.h"

#include <algorithm>

namespace directordaemon {

/* Forward referenced functions */
static bool PurgeFilesFromClient(UaContext* ua, ClientResource* client);
static bool PurgeJobsFromClient(UaContext* ua, ClientResource* client);
static bool PurgeJobsFromPool(UaContext* ua, PoolDbRecord* pool_record);
static bool PurgeQuotaFromClient(UaContext* ua, ClientResource* client);
static bool ActionOnPurgeCmd(UaContext* ua, const char* cmd);

/**
 * Purge records from database
 *
 */
bool PurgeCmd(UaContext* ua, const char*)
{
  int i;
  ClientResource* client;
  MediaDbRecord mr;
  JobDbRecord jr;
  PoolMem cmd_holder(PM_MESSAGE);
  const char* permission_denied_message
      = T_("Permission denied: need full %s permission.\n");

  static const char* keywords[]
      = {NT_("files"), NT_("jobs"), NT_("volume"), NT_("quota"), NULL};

  static const char* files_keywords[]
      = {NT_("Job"), NT_("JobId"), NT_("Client"), NT_("Volume"), NULL};

  static const char* quota_keywords[] = {NT_("Client"), NULL};

  static const char* jobs_keywords[]
      = {NT_("Client"), NT_("Volume"), NT_("Pool"), NULL};

  ua->WarningMsg(
      T_("\nThis command can be DANGEROUS!!!\n\n"
         "It purges (deletes) all Files from a Job,\n"
         "JobId, Client or Volume; or it purges (deletes)\n"
         "all Jobs from a Client or Volume without regard\n"
         "to retention periods. Normally you should use the\n"
         "PRUNE command, which respects retention periods.\n"
         "This command requires full access to all resources.\n"));

  /* Check for console ACL permissions.
   * These permission might be harder than required.
   * However, otherwise it gets hard to figure out the correct permission.
   * E.g. when purging a volume, this volume can contain
   * different jobs from different client, stored in different pools and
   * storages. Instead of checking all of this, we require full permissions to
   * all of these resources. */
  if (ua->AclHasRestrictions(Client_ACL)) {
    ua->ErrorMsg(permission_denied_message, "client");
    return false;
  }
  if (ua->AclHasRestrictions(Job_ACL)) {
    ua->ErrorMsg(permission_denied_message, "job");
    return false;
  }
  if (ua->AclHasRestrictions(Pool_ACL)) {
    ua->ErrorMsg(permission_denied_message, "pool");
    return false;
  }
  if (ua->AclHasRestrictions(Storage_ACL)) {
    ua->ErrorMsg(permission_denied_message, "storage");
    return false;
  }

  if (!OpenClientDb(ua, true)) { return true; }

  switch (FindArgKeyword(ua, keywords)) {
    /* Files */
    case 0:
      switch (FindArgKeyword(ua, files_keywords)) {
        case 0: /* Job */
        case 1: /* JobId */
          if (GetJobDbr(ua, &jr)) {
            char jobid[50];
            edit_int64(jr.JobId, jobid);
            PurgeFilesFromJobs(ua, jobid);
          }
          return true;
        case 2: /* client */
          client = get_client_resource(ua);
          if (client) { PurgeFilesFromClient(ua, client); }
          return true;
        case 3: /* Volume */
          // not implemented
          return true;
      }
      break;
    /* Jobs */
    case 1:
      switch (FindArgKeyword(ua, jobs_keywords)) {
        case 0: /* client */
        {
          client = get_client_resource(ua);
          if (client) { PurgeJobsFromClient(ua, client); }
          return true;
        }
        case 1: /* Volume */
          if (SelectMediaDbr(ua, &mr)) {
            PurgeJobsFromVolume(ua, &mr, /*force*/ true);
          }
          return true;
        case 2: /* pool */
        {
          PoolDbRecord pool_record;
          if (SelectPoolDbr(ua, &pool_record)) {
            PurgeJobsFromPool(ua, &pool_record);
          }
          return true;
        }
      }
      break;
    /* Volume */
    case 2:
      // Store cmd for later reuse.
      PmStrcpy(cmd_holder, ua->cmd);
      while ((i = FindArg(ua, NT_("volume"))) >= 0) {
        if (SelectMediaDbr(ua, &mr)) {
          PurgeJobsFromVolume(ua, &mr, /*force*/ true);
        }
        *ua->argk[i] = 0; /* zap keyword already seen */
        ua->SendMsg("\n");

        /* Add volume=mr.VolumeName to cmd_holder if we have a new volume name
         * from interactive selection. In certain cases this can produce
         * duplicates, which we don't prevent as there are no side effects. */
        if (!bstrcmp(ua->cmd, cmd_holder.c_str())) {
          PmStrcat(cmd_holder, " volume=");
          PmStrcat(cmd_holder, mr.VolumeName);
        }
      }

      // Restore ua args based on cmd_holder
      PmStrcpy(ua->cmd, cmd_holder);
      ParseArgs(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);

      // Perform ActionOnPurge (action=truncate)
      if (FindArg(ua, "action") >= 0) { return ActionOnPurgeCmd(ua, ua->cmd); }
      return true;
    /* Quota */
    case 3:
      switch (FindArgKeyword(ua, quota_keywords)) {
        case 0: /* client */
          client = get_client_resource(ua);
          if (client) { PurgeQuotaFromClient(ua, client); }
          return true;
      }
      break;
    default:
      break;
  }
  switch (DoKeywordPrompt(ua, T_("Choose item to purge"), keywords)) {
    case 0: /* files */
      client = get_client_resource(ua);
      if (client) { PurgeFilesFromClient(ua, client); }
      break;
    case 1: /* jobs */
    {
      client = get_client_resource(ua);
      if (client) { PurgeJobsFromClient(ua, client); }
      break;
    }
    case 2: /* Volume */
      if (SelectMediaDbr(ua, &mr)) {
        PurgeJobsFromVolume(ua, &mr, /*force*/ true);
      }
      break;
    case 3: /* Quota */
      client = get_client_resource(ua);
      if (client) { PurgeQuotaFromClient(ua, client); }
      break;
  }
  return true;
}

/**
 * Purge File records from the database.
 * We unconditionally delete all File records for that Job.
 * This is simple enough that no
 * temporary tables are needed. We simply make an in memory list of
 * the JobIds meeting the prune conditions, then delete all File records
 * pointing to each of those JobIds.
 */
static bool PurgeFilesFromClient(UaContext* ua, ClientResource* client)
{
  std::vector<JobId_t> del;
  PoolMem query(PM_MESSAGE);
  ClientDbRecord cr;
  char ed1[50];

  bstrncpy(cr.Name, client->resource_name_, sizeof(cr.Name));
  if (!ua->db->CreateClientRecord(ua->jcr, &cr)) { return false; }

  ua->InfoMsg(T_("Begin purging files for Client \"%s\"\n"), cr.Name);

  const std::string select_jobIds_from_client
      = "SELECT JobId FROM Job "
        "WHERE ClientId=%s "
        "AND PurgedFiles=0 "
        "ORDER BY JobId";

  Mmsg(query, select_jobIds_from_client.c_str(), edit_int64(cr.ClientId, ed1));
  Dmsg1(050, "select sql=%s\n", query.c_str());
  ua->db->SqlQuery(query.c_str(), FileDeleteHandler, static_cast<void*>(&del));

  if (del.empty()) {
    ua->WarningMsg(
        T_("No Files found for client %s to purge from %s catalog.\n"),
        client->resource_name_, client->catalog->resource_name_);
  } else {
    ua->InfoMsg(
        T_("Found Files in %d Jobs for client \"%s\" in catalog \"%s\".\n"),
        del.size(), client->resource_name_, client->catalog->resource_name_);
    if (!GetConfirmation(ua, "Purge (yes/no)? ")) {
      ua->InfoMsg(T_("Purge canceled.\n"));
    } else {
      PurgeFilesFromJobList(ua, del);
    }
  }

  return true;
}

static bool PurgeJobsFromPool(UaContext* ua, PoolDbRecord* pool_record)
{
  std::vector<char> jobstatuslist;
  if (!GetUserJobStatusSelection(ua, jobstatuslist)) {
    ua->ErrorMsg(T_("invalid jobstatus parameter\n"));
    return false;
  }

  std::string select_jobs_from_pool
      = "SELECT DISTINCT JobMedia.JobId FROM JobMedia, Job "
        "WHERE Job.JobId = JobMedia.JobId "
        "AND Jobmedia.MediaId IN "
        "(SELECT MediaId FROM Media WHERE PoolId="
        + std::to_string(pool_record->PoolId) + ") ";

  if (!jobstatuslist.empty()) {
    std::string jobStatuses
        = CreateDelimitedStringForSqlQueries(jobstatuslist, ',');
    select_jobs_from_pool += "AND Job.JobStatus IN (" + jobStatuses + ") ";
  }
  select_jobs_from_pool.append("ORDER BY JobId");


  std::vector<JobId_t> delete_list;
  ua->db->SqlQuery(select_jobs_from_pool.c_str(), JobDeleteHandler,
                   &delete_list);

  if (delete_list.empty()) {
    ua->WarningMsg(T_("No Jobs found for pool %s to purge from catalog.\n"),
                   pool_record->Name);
  } else {
    ua->InfoMsg(T_("Found %d Jobs for pool \"%s\" in catalog.\n"),
                delete_list.size(), pool_record->Name);
    if (!GetConfirmation(ua, "Purge (yes/no)? ")) {
      ua->InfoMsg(T_("Purge canceled.\n"));
    } else {
      PurgeJobListFromCatalog(ua, delete_list);
      ua->InfoMsg(T_("%d Job%s on Pool \"%s\" purged from catalog.\n"),
                  delete_list.size(), delete_list.size() <= 1 ? "" : "s",
                  pool_record->Name);
    }
  }

  std::vector<DBId_t> media_ids_in_pool;
  if (ua->db->GetMediaIdsInPool(pool_record, &media_ids_in_pool)) {
    ua->WarningMsg(T_("No Media found for pool %s.\n"), pool_record->Name);
  }
  for (auto mediaid : media_ids_in_pool) {
    MediaDbRecord mr;
    mr.MediaId = mediaid;
    if (!ua->db->GetMediaRecord(ua->jcr, &mr)) {
      ua->ErrorMsg("%s", ua->db->strerror());
    }
    IsVolumePurged(ua, &mr, false);
  }

  return true;
}

/**
 * Purge Job records from the database.
 * We unconditionally delete the job and all File records for that Job.
 * This is simple enough that no temporary tables are needed.
 * We simply make an in memory list of the JobIds then delete the Job, Files,
 * and JobMedia records in that list.
 */
static bool PurgeJobsFromClient(UaContext* ua, ClientResource* client)
{
  ClientDbRecord cr;
  bstrncpy(cr.Name, client->resource_name_, sizeof(cr.Name));
  if (!ua->db->CreateClientRecord(ua->jcr, &cr)) { return false; }

  ua->InfoMsg(T_("Begin purging jobs from Client \"%s\"\n"), cr.Name);

  std::vector<char> jobstatuslist;
  if (!GetUserJobStatusSelection(ua, jobstatuslist)) {
    ua->ErrorMsg(T_("invalid jobstatus parameter\n"));
    return false;
  }

  std::string select_jobs_from_client
      = "SELECT JobId, PurgedFiles FROM Job "
        "WHERE ClientId=%lu ";
  if (!jobstatuslist.empty()) {
    std::string jobStatuses
        = CreateDelimitedStringForSqlQueries(jobstatuslist, ',');
    select_jobs_from_client += "AND JobStatus IN (" + jobStatuses + ") ";
  }

  select_jobs_from_client.append("ORDER BY JobId");

  PoolMem query(PM_MESSAGE);
  Mmsg(query, select_jobs_from_client.c_str(), cr.ClientId);
  Dmsg1(150, "select sql=%s\n", query.c_str());

  std::vector<JobId_t> delete_list;
  ua->db->SqlQuery(query.c_str(), JobDeleteHandler,
                   static_cast<void*>(&delete_list));

  if (delete_list.empty()) {
    ua->WarningMsg(
        T_("No Jobs found for client %s to purge from %s catalog.\n"),
        client->resource_name_, client->catalog->resource_name_);
  } else {
    ua->InfoMsg(T_("Found %d Jobs for client \"%s\" in catalog \"%s\".\n"),
                delete_list.size(), client->resource_name_,
                client->catalog->resource_name_);
    if (!GetConfirmation(ua, "Purge (yes/no)? ")) {
      ua->InfoMsg(T_("Purge canceled.\n"));
    } else {
      PurgeJobListFromCatalog(ua, delete_list);
      ua->InfoMsg(T_("%d Job%s on client \"%s\" purged from catalog.\n"),
                  delete_list.size(), delete_list.size() <= 1 ? "" : "s",
                  client->resource_name_);
    }
  }

  return true;
}


// Remove File records from a list of JobIds
void PurgeFilesFromJobs(UaContext* ua, const char* jobs)
{
  ua->db->PurgeFiles(jobs);
}

std::string PrepareJobidsTobedeleted(UaContext* ua,
                                     std::vector<JobId_t>& deletion_list)
{
  deletion_list.erase(std::remove_if(deletion_list.begin(), deletion_list.end(),
                                     [&ua](const JobId_t& jobid) {
                                       return ua->jcr->JobId == jobid
                                              || jobid == 0;
                                     }),
                      deletion_list.end());

  BStringList jobids{};
  std::transform(deletion_list.begin(), deletion_list.end(),
                 std::back_inserter(jobids),
                 [](JobId_t jobid) { return std::to_string(jobid); });

  return jobids.Join(',');
}

// Delete given jobids (all records) from the catalog.
void PurgeJobListFromCatalog(UaContext* ua, std::vector<JobId_t>& deletion_list)
{
  Dmsg1(150, "num_ids=%d\n", deletion_list.size());

  std::string jobids_to_delete_string
      = PrepareJobidsTobedeleted(ua, deletion_list);
  if (deletion_list.empty()) {
    ua->SendMsg(T_("No jobids found to be purged\n"), deletion_list.size(),
                jobids_to_delete_string.c_str());
    return;
  }
  ua->SendMsg(T_("Purging the following %d JobIds: %s\n"), deletion_list.size(),
              jobids_to_delete_string.c_str());
  PurgeJobsFromCatalog(ua, jobids_to_delete_string.c_str());
}

// Delete files of given list of jobids
void PurgeFilesFromJobList(UaContext* ua, std::vector<JobId_t>& del)
{
  std::string jobids_to_delete = PrepareJobidsTobedeleted(ua, del);

  PurgeFilesFromJobs(ua, jobids_to_delete.c_str());
}

/**
 * This resets quotas in the database table Quota for the matching client
 * It is necessary to purge this if you want to reset the quota and let it count
 * from scratch.
 *
 * This function does not actually delete records, rather it updates them to nil
 * It should also be noted that if a quota record does not exist it will
 * actually end up creating it!
 */

static bool PurgeQuotaFromClient(UaContext* ua, ClientResource* client)
{
  ClientDbRecord cr;

  bstrncpy(cr.Name, client->resource_name_, sizeof(cr.Name));
  if (!ua->db->CreateClientRecord(ua->jcr, &cr)) { return false; }
  if (!ua->db->CreateQuotaRecord(ua->jcr, &cr)) { return false; }
  if (!ua->db->ResetQuotaRecord(ua->jcr, &cr)) { return false; }
  ua->InfoMsg(T_("Purged quota for Client \"%s\"\n"), cr.Name);

  return true;
}

// Remove all records from catalog for a list of JobIds
void PurgeJobsFromCatalog(UaContext* ua, const char* jobs)
{
  ua->db->PurgeJobs(jobs);
}

/**
 * Returns: 1 if Volume purged
 *          0 if Volume not purged
 */
bool PurgeJobsFromVolume(UaContext* ua, MediaDbRecord* mr, bool force)
{
  bool status
      = bstrcmp(mr->VolStatus, "Append") || bstrcmp(mr->VolStatus, "Full")
        || bstrcmp(mr->VolStatus, "Used") || bstrcmp(mr->VolStatus, "Error");
  if (!status) {
    ua->ErrorMsg(
        T_("\nVolume \"%s\" has VolStatus \"%s\" and cannot be purged.\n"
           "The VolStatus must be: Append, Full, Used or Error to be "
           "purged.\n"),
        mr->VolumeName, mr->VolStatus);
    return false;
  }

  std::vector<char> jobstatuslist;
  if (!GetUserJobStatusSelection(ua, jobstatuslist)) {
    ua->ErrorMsg(T_("invalid jobstatus parameter\n"));
    return false;
  }

  // Check if he wants to purge a single jobid
  int i = FindArgWithValue(ua, "jobid");
  db_list_ctx lst;
  std::string jobids;
  if (i >= 0 && Is_a_number_list(ua->argv[i])) {
    jobids = std::string(ua->argv[i]);
  } else if (!jobstatuslist.empty()) {
    std::string jobStatuses
        = CreateDelimitedStringForSqlQueries(jobstatuslist, ',');
    PoolMem query(PM_MESSAGE);

    Mmsg(query,
         "SELECT DISTINCT JobMedia.JobId FROM JobMedia, Job "
         "WHERE JobMedia.MediaId=%lu "
         "AND Job.JobId = JobMedia.JobId "
         "AND Job.JobStatus in (%s)",
         mr->MediaId, jobStatuses.c_str());

    ua->db->SqlQuery(query.c_str(), DbListHandler, &lst);
    jobids = lst.GetAsString();

  } else {
    // Purge ALL JobIds
    if (!ua->db->GetVolumeJobids(mr, &lst)) {
      ua->ErrorMsg("%s", ua->db->strerror());
      Dmsg0(050, "Count failed\n");
      return false;
    }
    jobids = lst.GetAsString();
  }

  if (lst.empty()) {
    ua->WarningMsg(T_("No Jobs found for media %s to purge from catalog.\n"),
                   mr->VolumeName);
  } else {
    ua->InfoMsg(T_("Found %d Jobs for media \"%s\" in catalog.\n"), lst.size(),
                mr->VolumeName);
    if (!GetConfirmation(ua, "Purge (yes/no)? ", true)) {
      ua->InfoMsg(T_("Purge canceled.\n"));
    } else {
      PurgeJobsFromCatalog(ua, jobids.c_str());
      ua->InfoMsg(T_("%d Jobs%s on Volume \"%s\" purged from catalog.\n"),
                  lst.size(), lst.size() <= 1 ? "" : "s", mr->VolumeName);
    }
  }

  return IsVolumePurged(ua, mr, force);
}

/**
 * This routine will check the JobMedia records to see if the
 *   Volume has been purged. If so, it marks it as such and
 *
 * Returns: true if volume purged
 *          false if not
 *
 * Note, we normally will not purge a volume that has First or LastWritten
 *   zero, because it means the volume is most likely being written
 *   however, if the user manually purges using the purge command in
 *   the console, he has been warned, and we go ahead and purge
 *   the volume anyway, if possible).
 */
bool IsVolumePurged(UaContext* ua, MediaDbRecord* mr, bool force)
{
  if (!force && (mr->FirstWritten == 0 || mr->LastWritten == 0)) {
    return false; /* not written cannot purge */
  }

  if (bstrcmp(mr->VolStatus, "Purged")) { return true; }

  /* If purged, mark it so */
  PoolMem query(PM_MESSAGE);
  struct s_count_ctx cnt;
  cnt.count = 0;
  Mmsg(query, "SELECT 1 FROM JobMedia WHERE MediaId=%lu LIMIT 1", mr->MediaId);
  if (!ua->db->SqlQuery(query.c_str(), DelCountHandler,
                        static_cast<void*>(&cnt))) {
    ua->ErrorMsg("%s", ua->db->strerror());
    Dmsg0(050, "Count failed\n");
    return false;
  }

  bool purged = false;
  if (cnt.count == 0) {
    ua->WarningMsg(T_("There are no more Jobs associated with Volume \"%s\". "
                      "Marking it purged.\n"),
                   mr->VolumeName);
    if (!(purged = MarkMediaPurged(ua, mr))) {
      ua->ErrorMsg("%s", ua->db->strerror());
    }
  }

  return purged;
}

/**
 * Called here to send the appropriate commands to the SD
 *  to do truncate on purge.
 */
static void do_truncate_on_purge(UaContext* ua,
                                 MediaDbRecord* mr,
                                 char* pool,
                                 char* storage,
                                 drive_number_t drive,
                                 BareosSocket* sd)
{
  bool ok = false;
  uint64_t VolBytes = 0;

  // TODO: Return if not mr->Recyle ?
  if (!mr->Recycle) { return; }

  // Do it only if action on purge = truncate is set
  if (!(mr->ActionOnPurge & ON_PURGE_TRUNCATE)) {
    ua->ErrorMsg(T_("\nThe option \"Action On Purge = Truncate\" was not "
                    "defined in the Pool resource.\n"
                    "Unable to truncate volume \"%s\"\n"),
                 mr->VolumeName);
    return;
  }

  /* Send the command to truncate the volume after purge. If this feature
   * is disabled for the specific device, this will be a no-op. */

  // Protect us from spaces
  BashSpaces(mr->VolumeName);
  BashSpaces(mr->MediaType);
  BashSpaces(pool);
  BashSpaces(storage);

  // Do it by relabeling the Volume, which truncates it
  sd->fsend(
      "relabel %s OldName=%s NewName=%s PoolName=%s "
      "MediaType=%s Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d\n",
      storage, mr->VolumeName, mr->VolumeName, pool, mr->MediaType, mr->Slot,
      drive,
      // If relabeling, keep blocksize settings
      mr->MinBlocksize, mr->MaxBlocksize);

  UnbashSpaces(mr->VolumeName);
  UnbashSpaces(mr->MediaType);
  UnbashSpaces(pool);
  UnbashSpaces(storage);

  // Send relabel command, and check for valid response
  while (sd->recv() >= 0) {
    ua->SendMsg("%s", sd->msg);
    if (sscanf(sd->msg, "3000 OK label. VolBytes=%llu ", &VolBytes) == 1) {
      ok = true;
    }
  }

  if (ok) {
    mr->VolBytes = VolBytes;
    mr->VolFiles = 0;
    SetStorageidInMr(NULL, mr);
    if (!ua->db->UpdateMediaRecord(ua->jcr, mr)) {
      ua->ErrorMsg(T_("Can't update volume size in the catalog\n"));
    }
    ua->SendMsg(T_("The volume \"%s\" has been truncated\n"), mr->VolumeName);
  } else {
    ua->WarningMsg(T_("Unable to truncate volume \"%s\"\n"), mr->VolumeName);
  }
}

/**
 * Implement Bareos bconsole command  purge action
 * purge action= pool= volume= storage= devicetype=
 */
static bool ActionOnPurgeCmd(UaContext* ua, const char*)
{
  bool allpools = false;
  drive_number_t drive = kInvalidSlotNumber;
  int nb = 0;
  uint32_t* results = NULL;
  const char* action = "all";
  StorageResource* store = NULL;
  PoolResource* pool = NULL;
  MediaDbRecord mr;
  PoolDbRecord pr;
  BareosSocket* sd = NULL;
  char esc[MAX_NAME_LENGTH * 2 + 1];
  PoolMem buf(PM_MESSAGE), volumes(PM_MESSAGE);

  PmStrcpy(volumes, "");

  // Look at arguments
  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("allpools"))) {
      allpools = true;
    } else if (Bstrcasecmp(ua->argk[i], NT_("volume"))
               && IsNameValid(ua->argv[i])) {
      ua->db->EscapeString(ua->jcr, esc, ua->argv[i], strlen(ua->argv[i]));
      if (!*volumes.c_str()) {
        Mmsg(buf, "'%s'", esc);
      } else {
        Mmsg(buf, ",'%s'", esc);
      }
      PmStrcat(volumes, buf.c_str());
    } else if (Bstrcasecmp(ua->argk[i], NT_("devicetype")) && ua->argv[i]) {
      bstrncpy(mr.MediaType, ua->argv[i], sizeof(mr.MediaType));

    } else if (Bstrcasecmp(ua->argk[i], NT_("drive")) && ua->argv[i]) {
      drive = static_cast<drive_number_t>(atoi(ua->argv[i]));

    } else if (Bstrcasecmp(ua->argk[i], NT_("action"))
               && IsNameValid(ua->argv[i])) {
      action = ua->argv[i];
    }
  }

  // Choose storage
  ua->jcr->dir_impl->res.write_storage = store = get_storage_resource(ua);
  if (!store) { goto bail_out; }

  switch (store->Protocol) {
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      ua->WarningMsg(T_("Storage has non-native protocol.\n"));
      goto bail_out;
    default:
      break;
  }

  if (!OpenDb(ua)) {
    Dmsg0(100, "Can't open db\n");
    goto bail_out;
  }

  if (!allpools) {
    // Force pool selection
    pool = get_pool_resource(ua);
    if (!pool) {
      Dmsg0(100, "Can't get pool resource\n");
      goto bail_out;
    }
    bstrncpy(pr.Name, pool->resource_name_, sizeof(pr.Name));
    if (!ua->db->GetPoolRecord(ua->jcr, &pr)) {
      Dmsg0(100, "Can't get pool record\n");
      goto bail_out;
    }
    mr.PoolId = pr.PoolId;
  }

  /* Look for all Purged volumes that can be recycled, are enabled and have
   * more the 10,000 bytes. */
  mr.Recycle = 1;
  mr.Enabled = VOL_ENABLED;
  mr.VolBytes = 10000;
  SetStorageidInMr(store, &mr);
  bstrncpy(mr.VolStatus, "Purged", sizeof(mr.VolStatus));
  if (!ua->db->GetMediaIds(ua->jcr, &mr, volumes, &nb, &results)) {
    Dmsg0(100, "No results from GetMediaIds\n");
    goto bail_out;
  }

  if (!nb) {
    ua->SendMsg(T_("No Volumes found to perform %s action.\n"), action);
    goto bail_out;
  }

  if ((sd = open_sd_bsock(ua)) == NULL) {
    Dmsg0(100, "Can't open connection to sd\n");
    goto bail_out;
  }

  // Loop over the candidate Volumes and actually truncate them
  for (int i = 0; i < nb; i++) {
    MediaDbRecord mr_temp;
    mr_temp.MediaId = results[i];
    if (ua->db->GetMediaRecord(ua->jcr, &mr_temp)) {
      /* TODO: ask for drive and change Pool */
      if (Bstrcasecmp("truncate", action) || Bstrcasecmp("all", action)) {
        do_truncate_on_purge(ua, &mr_temp, pr.Name, store->dev_name(), drive,
                             sd);
      }
    } else {
      Dmsg1(0, "Can't find MediaId=%lld\n", (uint64_t)mr_temp.MediaId);
    }
  }

bail_out:
  CloseDb(ua);
  if (sd) { CloseSdBsock(ua); }
  ua->jcr->dir_impl->res.write_storage = NULL;
  if (results) { free(results); }

  return true;
}

/**
 * If volume status is Append, Full, Used, or Error, mark it Purged
 * Purged volumes can then be recycled (if enabled).
 */
bool MarkMediaPurged(UaContext* ua, MediaDbRecord* mr)
{
  JobControlRecord* jcr = ua->jcr;
  bool status;

  status = bstrcmp(mr->VolStatus, "Append") || bstrcmp(mr->VolStatus, "Full")
           || bstrcmp(mr->VolStatus, "Used") || bstrcmp(mr->VolStatus, "Error");

  if (status) {
    bstrncpy(mr->VolStatus, "Purged", sizeof(mr->VolStatus));
    SetStorageidInMr(NULL, mr);

    if (!ua->db->UpdateMediaRecord(jcr, mr)) { return false; }

    PmStrcpy(jcr->VolumeName, mr->VolumeName);
    GeneratePluginEvent(jcr, bDirEventVolumePurged);

    // If the RecyclePool is defined, move the volume there
    if (mr->RecyclePoolId && mr->RecyclePoolId != mr->PoolId) {
      PoolDbRecord oldpr, newpr;
      newpr.PoolId = mr->RecyclePoolId;
      oldpr.PoolId = mr->PoolId;
      if (ua->db->GetPoolRecord(jcr, &oldpr)
          && ua->db->GetPoolRecord(jcr, &newpr)) {
        // Check if destination pool size is ok
        if (newpr.MaxVols > 0 && newpr.NumVols >= newpr.MaxVols) {
          ua->ErrorMsg(T_("Unable move recycled Volume in full "
                          "Pool \"%s\" MaxVols=%d\n"),
                       newpr.Name, newpr.MaxVols);

        } else { /* move media */
          UpdateVolPool(ua, newpr.Name, mr, &oldpr);
        }
      } else {
        ua->ErrorMsg("%s", ua->db->strerror());
      }
    }

    // job
    if (jcr->JobId > 0) {
      ua->SendMsg(
          T_("All records pruned from Volume \"%s\"; marking it \"Purged\"\n"),
          mr->VolumeName);
    }

    return true;
  } else {
    ua->ErrorMsg(T_("Cannot purge Volume with VolStatus=%s\n"), mr->VolStatus);
  }

  return bstrcmp(mr->VolStatus, "Purged");
}
} /* namespace directordaemon */
