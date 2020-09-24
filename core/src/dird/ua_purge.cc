/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, February MMII
 */
/**
 * @file
 * BAREOS Director -- User Agent Database Purge Command
 *
 * Purges Files from specific JobIds
 * or
 * Purges Jobs from Volumes
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/jcr_private.h"
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
static bool PurgeQuotaFromClient(UaContext* ua, ClientResource* client);
static bool ActionOnPurgeCmd(UaContext* ua, const char* cmd);

static const char* select_jobsfiles_from_client =
    "SELECT JobId FROM Job "
    "WHERE ClientId=%s "
    "AND PurgedFiles=0";

static const char* select_jobs_from_client =
    "SELECT JobId, PurgedFiles FROM Job "
    "WHERE ClientId=%s";

/**
 * Purge records from database
 *
 */
bool PurgeCmd(UaContext* ua, const char* cmd)
{
  int i;
  ClientResource* client;
  MediaDbRecord mr;
  JobDbRecord jr;
  PoolMem cmd_holder(PM_MESSAGE);
  const char* permission_denied_message =
      _("Permission denied: need full %s permission.\n");

  static const char* keywords[] = {NT_("files"), NT_("jobs"), NT_("volume"),
                                   NT_("quota"), NULL};

  static const char* files_keywords[] = {NT_("Job"), NT_("JobId"),
                                         NT_("Client"), NT_("Volume"), NULL};

  static const char* quota_keywords[] = {NT_("Client"), NULL};

  static const char* jobs_keywords[] = {NT_("Client"), NT_("Volume"), NULL};

  ua->WarningMsg(
      _("\nThis command can be DANGEROUS!!!\n\n"
        "It purges (deletes) all Files from a Job,\n"
        "JobId, Client or Volume; or it purges (deletes)\n"
        "all Jobs from a Client or Volume without regard\n"
        "to retention periods. Normally you should use the\n"
        "PRUNE command, which respects retention periods.\n"
        "This command requires full access to all resources.\n"));

  /*
   * Check for console ACL permissions.
   * These permission might be harder than required.
   * However, otherwise it gets hard to figure out the correct permission.
   * E.g. when purging a volume, this volume can contain
   * different jobs from different client, stored in different pools and
   * storages. Instead of checking all of this, we require full permissions to
   * all of these resources.
   */
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
          if (SelectMediaDbr(ua, &mr)) { PurgeFilesFromVolume(ua, &mr); }
          return true;
      }
      break;
    /* Jobs */
    case 1:
      switch (FindArgKeyword(ua, jobs_keywords)) {
        case 0: /* client */
          client = get_client_resource(ua);
          if (client) { PurgeJobsFromClient(ua, client); }
          return true;
        case 1: /* Volume */
          if (SelectMediaDbr(ua, &mr)) {
            PurgeJobsFromVolume(ua, &mr, /*force*/ true);
          }
          return true;
      }
      break;
    /* Volume */
    case 2:
      /*
       * Store cmd for later reuse.
       */
      PmStrcpy(cmd_holder, ua->cmd);
      while ((i = FindArg(ua, NT_("volume"))) >= 0) {
        if (SelectMediaDbr(ua, &mr)) {
          PurgeJobsFromVolume(ua, &mr, /*force*/ true);
        }
        *ua->argk[i] = 0; /* zap keyword already seen */
        ua->SendMsg("\n");

        /*
         * Add volume=mr.VolumeName to cmd_holder if we have a new volume name
         * from interactive selection. In certain cases this can produce
         * duplicates, which we don't prevent as there are no side effects.
         */
        if (!bstrcmp(ua->cmd, cmd_holder.c_str())) {
          PmStrcat(cmd_holder, " volume=");
          PmStrcat(cmd_holder, mr.VolumeName);
        }
      }

      /*
       * Restore ua args based on cmd_holder
       */
      PmStrcpy(ua->cmd, cmd_holder);
      ParseArgs(ua->cmd, ua->args, &ua->argc, ua->argk, ua->argv, MAX_CMD_ARGS);

      /*
       * Perform ActionOnPurge (action=truncate)
       */
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
  switch (DoKeywordPrompt(ua, _("Choose item to purge"), keywords)) {
    case 0: /* files */
      client = get_client_resource(ua);
      if (client) { PurgeFilesFromClient(ua, client); }
      break;
    case 1: /* jobs */
      client = get_client_resource(ua);
      if (client) { PurgeJobsFromClient(ua, client); }
      break;
    case 2: /* Volume */
      if (SelectMediaDbr(ua, &mr)) {
        PurgeJobsFromVolume(ua, &mr, /*force*/ true);
      }
      break;
    case 3:
      client = get_client_resource(ua); /* Quota */
      if (client) { PurgeQuotaFromClient(ua, client); }
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
  del_ctx del;
  PoolMem query(PM_MESSAGE);
  ClientDbRecord cr;
  char ed1[50];

  bstrncpy(cr.Name, client->resource_name_, sizeof(cr.Name));
  if (!ua->db->CreateClientRecord(ua->jcr, &cr)) { return false; }

  del.max_ids = 1000;
  del.JobId = (JobId_t*)malloc(sizeof(JobId_t) * del.max_ids);

  ua->InfoMsg(_("Begin purging files for Client \"%s\"\n"), cr.Name);

  Mmsg(query, select_jobsfiles_from_client, edit_int64(cr.ClientId, ed1));
  Dmsg1(050, "select sql=%s\n", query.c_str());
  ua->db->SqlQuery(query.c_str(), FileDeleteHandler, (void*)&del);

  if (del.num_ids == 0) {
    ua->WarningMsg(
        _("No Files found for client %s to purge from %s catalog.\n"),
        client->resource_name_, client->catalog->resource_name_);
  } else {
    ua->InfoMsg(
        _("Found Files in %d Jobs for client \"%s\" in catalog \"%s\".\n"),
        del.num_ids, client->resource_name_, client->catalog->resource_name_);
    if (!GetConfirmation(ua, "Purge (yes/no)? ")) {
      ua->InfoMsg(_("Purge canceled.\n"));
    } else {
      PurgeFilesFromJobList(ua, del);
    }
  }

  if (del.JobId) { free(del.JobId); }
  return true;
}


/**
 * Purge Job records from the database.
 * We unconditionally delete the job and all File records for that Job.
 * This is simple enough that no
 * temporary tables are needed. We simply make an in memory list of
 * the JobIds then delete the Job, Files, and JobMedia records in that list.
 */
static bool PurgeJobsFromClient(UaContext* ua, ClientResource* client)
{
  del_ctx del;
  PoolMem query(PM_MESSAGE);
  ClientDbRecord cr;
  char ed1[50];


  bstrncpy(cr.Name, client->resource_name_, sizeof(cr.Name));
  if (!ua->db->CreateClientRecord(ua->jcr, &cr)) { return false; }

  del.max_ids = 1000;
  del.JobId = (JobId_t*)malloc(sizeof(JobId_t) * del.max_ids);
  del.PurgedFiles = (char*)malloc(del.max_ids);

  ua->InfoMsg(_("Begin purging jobs from Client \"%s\"\n"), cr.Name);

  Mmsg(query, select_jobs_from_client, edit_int64(cr.ClientId, ed1));
  Dmsg1(150, "select sql=%s\n", query.c_str());
  ua->db->SqlQuery(query.c_str(), JobDeleteHandler, (void*)&del);

  if (del.num_ids == 0) {
    ua->WarningMsg(_("No Jobs found for client %s to purge from %s catalog.\n"),
                   client->resource_name_, client->catalog->resource_name_);
  } else {
    ua->InfoMsg(_("Found %d Jobs for client \"%s\" in catalog \"%s\".\n"),
                del.num_ids, client->resource_name_,
                client->catalog->resource_name_);
    if (!GetConfirmation(ua, "Purge (yes/no)? ")) {
      ua->InfoMsg(_("Purge canceled.\n"));
    } else {
      PurgeJobListFromCatalog(ua, del);
    }
  }

  if (del.JobId) { free(del.JobId); }
  if (del.PurgedFiles) { free(del.PurgedFiles); }
  return true;
}


/**
 * Remove File records from a list of JobIds
 */
void PurgeFilesFromJobs(UaContext* ua, const char* jobs)
{
  PoolMem query(PM_MESSAGE);

  Mmsg(query, "DELETE FROM File WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete File sql=%s\n", query.c_str());

  Mmsg(query, "DELETE FROM BaseFiles WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete BaseFiles sql=%s\n", query.c_str());

  /*
   * Now mark Job as having files purged. This is necessary to
   * avoid having too many Jobs to process in future prunings. If
   * we don't do this, the number of JobId's in our in memory list
   * could grow very large.
   */
  Mmsg(query, "UPDATE Job SET PurgedFiles=1 WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Mark purged sql=%s\n", query.c_str());
}

/**
 * Delete jobs (all records) from the catalog in groups of 1000
 *  at a time.
 */
void PurgeJobListFromCatalog(UaContext* ua, del_ctx& del)
{
  for (int i = 0; del.num_ids;) {
    std::vector<JobId_t> jobid_list{};
    Dmsg1(150, "num_ids=%d\n", del.num_ids);
    for (int j = 0; j < 1000 && del.num_ids > 0; j++) {
      del.num_ids--;
      if (del.JobId[i] == 0 || ua->jcr->JobId == del.JobId[i]) {
        Dmsg2(150, "skip JobId[%d]=%llu\n", i, del.JobId[i]);
        ++i;
        continue;
      }
      jobid_list.push_back(del.JobId[i]);
      Dmsg1(150, "Add id=%llu\n", del.JobId[i]);
      ++i;
      ++del.num_del;
    }
    Dmsg1(150, "num_ids=%d\n", del.num_ids);
    std::sort(jobid_list.begin(), jobid_list.end());
    BStringList jobids{};
    std::transform(jobid_list.begin(), jobid_list.end(),
                   std::back_inserter(jobids),
                   [](JobId_t jobid) { return std::to_string(jobid); });

    Jmsg(ua->jcr, M_INFO, 0, _("Purging the following JobIds: %s\n"),
         jobids.Join(',').c_str());
    PurgeJobsFromCatalog(ua, jobids.Join(',').c_str());
  }
}

/**
 * Delete files from a list of jobs in groups of 1000
 *  at a time.
 */
void PurgeFilesFromJobList(UaContext* ua, del_ctx& del)
{
  PoolMem jobids(PM_MESSAGE);
  char ed1[50];
  /*
   * OK, now we have the list of JobId's to be pruned, send them
   *   off to be deleted batched 1000 at a time.
   */
  for (int i = 0; del.num_ids;) {
    PmStrcat(jobids, "");
    for (int j = 0; j < 1000 && del.num_ids > 0; j++) {
      del.num_ids--;
      if (del.JobId[i] == 0 || ua->jcr->JobId == del.JobId[i]) {
        Dmsg2(150, "skip JobId[%d]=%d\n", i, (int)del.JobId[i]);
        i++;
        continue;
      }
      if (*jobids.c_str() != 0) { PmStrcat(jobids, ","); }
      PmStrcat(jobids, edit_int64(del.JobId[i++], ed1));
      Dmsg1(150, "Add id=%s\n", ed1);
      del.num_del++;
    }
    PurgeFilesFromJobs(ua, jobids.c_str());
  }
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
  ua->InfoMsg(_("Purged quota for Client \"%s\"\n"), cr.Name);

  return true;
}

/**
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
void UpgradeCopies(UaContext* ua, const char* jobs)
{
  PoolMem query(PM_MESSAGE);

  DbLock(ua->db);

  /* Do it in two times for mysql */
  ua->db->FillQuery(query, BareosDb::SQL_QUERY::uap_upgrade_copies_oldest_job,
                    JT_JOB_COPY, jobs, jobs);

  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Upgrade copies Log sql=%s\n", query.c_str());

  /* Now upgrade first copy to Backup */
  Mmsg(query,
       "UPDATE Job SET Type='B' " /* JT_JOB_COPY => JT_BACKUP  */
       "WHERE JobId IN ( SELECT JobId FROM cpy_tmp )");

  ua->db->SqlQuery(query.c_str());

  Mmsg(query, "DROP TABLE cpy_tmp");
  ua->db->SqlQuery(query.c_str());

  DbUnlock(ua->db);
}

/**
 * Remove all records from catalog for a list of JobIds
 */
void PurgeJobsFromCatalog(UaContext* ua, const char* jobs)
{
  PoolMem query(PM_MESSAGE);

  /* Delete (or purge) records associated with the job */
  PurgeFilesFromJobs(ua, jobs);

  Mmsg(query, "DELETE FROM JobMedia WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete JobMedia sql=%s\n", query.c_str());

  Mmsg(query, "DELETE FROM Log WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete Log sql=%s\n", query.c_str());

  Mmsg(query, "DELETE FROM RestoreObject WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete RestoreObject sql=%s\n", query.c_str());

  Mmsg(query, "DELETE FROM PathVisibility WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete PathVisibility sql=%s\n", query.c_str());

  Mmsg(query, "DELETE FROM NDMPJobEnvironment WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete NDMPJobEnvironment sql=%s\n", query.c_str());

  Mmsg(query, "DELETE FROM JobStats WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete JobStats sql=%s\n", query.c_str());

  UpgradeCopies(ua, jobs);

  /* Now remove the Job record itself */
  Mmsg(query, "DELETE FROM Job WHERE JobId IN (%s)", jobs);
  ua->db->SqlQuery(query.c_str());
  Dmsg1(050, "Delete Job sql=%s\n", query.c_str());
}

void PurgeFilesFromVolume(UaContext* ua, MediaDbRecord* mr) {
} /* ***FIXME*** implement */

/**
 * Returns: 1 if Volume purged
 *          0 if Volume not purged
 */
bool PurgeJobsFromVolume(UaContext* ua, MediaDbRecord* mr, bool force)
{
  PoolMem query(PM_MESSAGE);
  db_list_ctx lst;
  std::string jobids;
  int i;
  bool purged = false;
  bool status;

  status = bstrcmp(mr->VolStatus, "Append") || bstrcmp(mr->VolStatus, "Full") ||
           bstrcmp(mr->VolStatus, "Used") || bstrcmp(mr->VolStatus, "Error");
  if (!status) {
    ua->ErrorMsg(
        _("\nVolume \"%s\" has VolStatus \"%s\" and cannot be purged.\n"
          "The VolStatus must be: Append, Full, Used or Error to be purged.\n"),
        mr->VolumeName, mr->VolStatus);
    return false;
  }

  /*
   * Check if he wants to purge a single jobid
   */
  i = FindArgWithValue(ua, "jobid");
  if (i >= 0 && Is_a_number_list(ua->argv[i])) {
    jobids = std::string(ua->argv[i]);
  } else {
    /*
     * Purge ALL JobIds
     */
    if (!ua->db->GetVolumeJobids(ua->jcr, mr, &lst)) {
      ua->ErrorMsg("%s", ua->db->strerror());
      Dmsg0(050, "Count failed\n");
      goto bail_out;
    }
    jobids = lst.GetAsString();
  }

  if (jobids.size() > 0) { PurgeJobsFromCatalog(ua, jobids.c_str()); }

  ua->InfoMsg(_("%d File%s on Volume \"%s\" purged from catalog.\n"),
              lst.size(), lst.size() <= 1 ? "" : "s", mr->VolumeName);

  purged = IsVolumePurged(ua, mr, force);

bail_out:
  return purged;
}

/**
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
bool IsVolumePurged(UaContext* ua, MediaDbRecord* mr, bool force)
{
  PoolMem query(PM_MESSAGE);
  struct s_count_ctx cnt;
  bool purged = false;
  char ed1[50];

  if (!force && (mr->FirstWritten == 0 || mr->LastWritten == 0)) {
    goto bail_out; /* not written cannot purge */
  }

  if (bstrcmp(mr->VolStatus, "Purged")) {
    purged = true;
    goto bail_out;
  }

  /* If purged, mark it so */
  cnt.count = 0;
  Mmsg(query, "SELECT 1 FROM JobMedia WHERE MediaId=%s LIMIT 1",
       edit_int64(mr->MediaId, ed1));
  if (!ua->db->SqlQuery(query.c_str(), DelCountHandler, (void*)&cnt)) {
    ua->ErrorMsg("%s", ua->db->strerror());
    Dmsg0(050, "Count failed\n");
    goto bail_out;
  }

  if (cnt.count == 0) {
    ua->WarningMsg(_("There are no more Jobs associated with Volume \"%s\". "
                     "Marking it purged.\n"),
                   mr->VolumeName);
    if (!(purged = MarkMediaPurged(ua, mr))) {
      ua->ErrorMsg("%s", ua->db->strerror());
    }
  }
bail_out:
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

  /*
   * TODO: Return if not mr->Recyle ?
   */
  if (!mr->Recycle) { return; }

  /*
   * Do it only if action on purge = truncate is set
   */
  if (!(mr->ActionOnPurge & ON_PURGE_TRUNCATE)) {
    ua->ErrorMsg(_("\nThe option \"Action On Purge = Truncate\" was not "
                   "defined in the Pool resource.\n"
                   "Unable to truncate volume \"%s\"\n"),
                 mr->VolumeName);
    return;
  }

  /*
   * Send the command to truncate the volume after purge. If this feature
   * is disabled for the specific device, this will be a no-op.
   */

  /*
   * Protect us from spaces
   */
  BashSpaces(mr->VolumeName);
  BashSpaces(mr->MediaType);
  BashSpaces(pool);
  BashSpaces(storage);

  /*
   * Do it by relabeling the Volume, which truncates it
   */
  sd->fsend(
      "relabel %s OldName=%s NewName=%s PoolName=%s "
      "MediaType=%s Slot=%hd drive=%hd MinBlocksize=%d MaxBlocksize=%d\n",
      storage, mr->VolumeName, mr->VolumeName, pool, mr->MediaType, mr->Slot,
      drive,
      /*
       * If relabeling, keep blocksize settings
       */
      mr->MinBlocksize, mr->MaxBlocksize);

  UnbashSpaces(mr->VolumeName);
  UnbashSpaces(mr->MediaType);
  UnbashSpaces(pool);
  UnbashSpaces(storage);

  /*
   * Send relabel command, and check for valid response
   */
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
      ua->ErrorMsg(_("Can't update volume size in the catalog\n"));
    }
    ua->SendMsg(_("The volume \"%s\" has been truncated\n"), mr->VolumeName);
  } else {
    ua->WarningMsg(_("Unable to truncate volume \"%s\"\n"), mr->VolumeName);
  }
}

/**
 * Implement Bareos bconsole command  purge action
 * purge action= pool= volume= storage= devicetype=
 */
static bool ActionOnPurgeCmd(UaContext* ua, const char* cmd)
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

  /*
   * Look at arguments
   */
  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("allpools"))) {
      allpools = true;
    } else if (Bstrcasecmp(ua->argk[i], NT_("volume")) &&
               IsNameValid(ua->argv[i])) {
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

    } else if (Bstrcasecmp(ua->argk[i], NT_("action")) &&
               IsNameValid(ua->argv[i])) {
      action = ua->argv[i];
    }
  }

  /*
   * Choose storage
   */
  ua->jcr->impl->res.write_storage = store = get_storage_resource(ua);
  if (!store) { goto bail_out; }

  switch (store->Protocol) {
    case APT_NDMPV2:
    case APT_NDMPV3:
    case APT_NDMPV4:
      ua->WarningMsg(_("Storage has non-native protocol.\n"));
      goto bail_out;
    default:
      break;
  }

  if (!OpenDb(ua)) {
    Dmsg0(100, "Can't open db\n");
    goto bail_out;
  }

  if (!allpools) {
    /*
     * Force pool selection
     */
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

  /*
   * Look for all Purged volumes that can be recycled, are enabled and have more
   * the 10,000 bytes.
   */
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
    ua->SendMsg(_("No Volumes found to perform %s action.\n"), action);
    goto bail_out;
  }

  if ((sd = open_sd_bsock(ua)) == NULL) {
    Dmsg0(100, "Can't open connection to sd\n");
    goto bail_out;
  }

  /*
   * Loop over the candidate Volumes and actually truncate them
   */
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
  ua->jcr->impl->res.write_storage = NULL;
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

  status = bstrcmp(mr->VolStatus, "Append") || bstrcmp(mr->VolStatus, "Full") ||
           bstrcmp(mr->VolStatus, "Used") || bstrcmp(mr->VolStatus, "Error");

  if (status) {
    bstrncpy(mr->VolStatus, "Purged", sizeof(mr->VolStatus));
    SetStorageidInMr(NULL, mr);

    if (!ua->db->UpdateMediaRecord(jcr, mr)) { return false; }

    PmStrcpy(jcr->VolumeName, mr->VolumeName);
    GeneratePluginEvent(jcr, bDirEventVolumePurged);

    /*
     * If the RecyclePool is defined, move the volume there
     */
    if (mr->RecyclePoolId && mr->RecyclePoolId != mr->PoolId) {
      PoolDbRecord oldpr, newpr;
      newpr.PoolId = mr->RecyclePoolId;
      oldpr.PoolId = mr->PoolId;
      if (ua->db->GetPoolRecord(jcr, &oldpr) &&
          ua->db->GetPoolRecord(jcr, &newpr)) {
        /*
         * Check if destination pool size is ok
         */
        if (newpr.MaxVols > 0 && newpr.NumVols >= newpr.MaxVols) {
          ua->ErrorMsg(_("Unable move recycled Volume in full "
                         "Pool \"%s\" MaxVols=%d\n"),
                       newpr.Name, newpr.MaxVols);

        } else { /* move media */
          UpdateVolPool(ua, newpr.Name, mr, &oldpr);
        }
      } else {
        ua->ErrorMsg("%s", ua->db->strerror());
      }
    }

    /*
     * Send message to Job report, if it is a *real* job
     */
    if (jcr->JobId > 0) {
      Jmsg(jcr, M_INFO, 0,
           _("All records pruned from Volume \"%s\"; marking it \"Purged\"\n"),
           mr->VolumeName);
    }

    return true;
  } else {
    ua->ErrorMsg(_("Cannot purge Volume with VolStatus=%s\n"), mr->VolStatus);
  }

  return bstrcmp(mr->VolStatus, "Purged");
}
} /* namespace directordaemon */
