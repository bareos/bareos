/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, October MMI
/**
 * @file
 * User Agent Prompt and Selection code
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/storage.h"
#include "dird/ua_input.h"
#include "dird/ua_select.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

#include <algorithm>
#include <string>
#include <vector>

#include "job_levels.h"

namespace directordaemon {

/* Imported variables */
extern struct s_jt jobtypes[];

// Confirm a retention period
bool ConfirmRetention(UaContext* ua, utime_t* ret, const char* msg)
{
  bool retval;
  char ed1[100];
  int yes_in_arg;

  yes_in_arg = FindArg(ua, NT_("yes"));
  for (;;) {
    ua->InfoMsg(T_("The current %s retention period is: %s\n"), msg,
                edit_utime(*ret, ed1, sizeof(ed1)));
    if (yes_in_arg != -1) { return true; }

    if (!GetCmd(ua, T_("Continue? (yes/mod/no): "))) { return false; }

    if (Bstrcasecmp(ua->cmd, T_("mod"))) {
      if (!GetCmd(ua, T_("Enter new retention period: "))) { return false; }
      if (!DurationToUtime(ua->cmd, ret)) {
        ua->ErrorMsg(T_("Invalid period.\n"));
        continue;
      }
      continue;
    }

    if (IsYesno(ua->cmd, &retval)) { return retval; }
  }

  return true;
}

/**
 * Given a list of keywords, find the first one that is in the argument list.
 *
 * Returns: -1 if not found
 *          index into list (base 0) on success
 */
int FindArgKeyword(UaContext* ua, const char** list)
{
  for (int i = 1; i < ua->argc; i++) {
    for (int j = 0; list[j]; j++) {
      if (Bstrcasecmp(list[j], ua->argk[i])) { return j; }
    }
  }

  return -1;
}

/**
 * Given one keyword, find the first one that is in the argument list.
 *
 * Returns: argk index (always gt 0)
 *          -1 if not found
 */
int FindArg(UaContext* ua, const char* keyword)
{
  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(keyword, ua->argk[i])) { return i; }
  }

  return -1;
}

/**
 * Given a single keyword, find it in the argument list, but it must have a
 * value
 *
 * Returns: -1 if not found or no value
 *           list index (base 0) on success
 */
int FindArgWithValue(UaContext* ua, const char* keyword)
{
  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(keyword, ua->argk[i])) {
      if (ua->argv[i]) {
        return i;
      } else {
        return -1;
      }
    }
  }

  return -1;
}

/**
 * Given a single keyword, find it in the argument list, check if it has a
 * value and return the pointer to the value.
 *
 * Returns: pointer to value
 *          nullptr if no such key or key has no value
 */
const char* GetArgValue(UaContext* ua, const char* keyword)
{
  if (int i = FindArgWithValue(ua, keyword); i >= 0) {
    return ua->argv[i];
  } else {
    return nullptr;
  }
}

/**
 * Tries to find client name, provided as command line argument in the Bareos
 * catalog. This differs from ua->GetClientResWithName as it also gets clients
 * no longer available in the configuration.
 *
 * @return -1: if invalid client parameter is given.
 * @return  0: no client parameter given.
 * @return >0: valid client parameter is given.
 */
int FindClientArgFromDatabase(UaContext* ua)
{
  ClientDbRecord cr;
  int i = FindArgWithValue(ua, NT_("client"));
  ASSERT(i != 0);
  if (i >= 0) {
    if (!IsNameValid(ua->argv[i], ua->errmsg)) {
      ua->ErrorMsg("%s argument: %s", ua->argk[i], ua->errmsg.c_str());
      return -1;
    }
    // this is checked by IsNameValid
    ASSERT(strlen(ua->argv[i]) < sizeof(cr.Name));
    bstrncpy(cr.Name, ua->argv[i], sizeof(cr.Name));
    if (!ua->db->GetClientRecord(ua->jcr, &cr)) {
      ua->ErrorMsg("invalid %s argument: %s\n", ua->argk[i], ua->argv[i]);
      return -1;
    }
    if (!ua->AclAccessOk(Client_ACL, cr.Name)) { return -1; }
    return i;
  }
  return 0;
}

/**
 * Given a list of keywords, prompt the user to choose one.
 *
 * Returns: -1 on failure
 *          index into list (base 0) on success
 */
int DoKeywordPrompt(UaContext* ua, const char* msg, const char** list)
{
  StartPrompt(ua, T_("You have the following choices:\n"));
  for (int i = 0; list[i]; i++) { AddPrompt(ua, list[i]); }

  return DoPrompt(ua, "", msg, NULL, 0);
}

// Select a Storage resource from prompt list
StorageResource* select_storage_resource(UaContext* ua, bool autochanger_only)
{
  StorageResource* store;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> storage_resource_names;

  if (autochanger_only) {
    StartPrompt(ua, T_("The defined Autochanger Storage resources are:\n"));
  } else {
    StartPrompt(ua, T_("The defined Storage resources are:\n"));
  }

  foreach_res (store, R_STORAGE) {
    if (ua->AclAccessOk(Storage_ACL, store->resource_name_)) {
      if (autochanger_only && !store->autochanger) {
        continue;
      } else {
        storage_resource_names.emplace_back(store->resource_name_);
      }
    }
  }

  SortCaseInsensitive(storage_resource_names);

  for (auto& resource_name : storage_resource_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("Storage"), T_("Select Storage resource"), name,
               sizeof(name))
      < 0) {
    return NULL;
  }
  store = ua->GetStoreResWithName(name);

  return store;
}

// Select a FileSet resource from prompt list
FilesetResource* select_fileset_resource(UaContext* ua)
{
  FilesetResource* fs;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> fileset_resource_names;

  StartPrompt(ua, T_("The defined FileSet resources are:\n"));

  foreach_res (fs, R_FILESET) {
    if (ua->AclAccessOk(FileSet_ACL, fs->resource_name_)) {
      fileset_resource_names.emplace_back(fs->resource_name_);
    }
  }


  SortCaseInsensitive(fileset_resource_names);

  for (auto& resource_name : fileset_resource_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("FileSet"), T_("Select FileSet resource"), name,
               sizeof(name))
      < 0) {
    return NULL;
  }

  fs = ua->GetFileSetResWithName(name);

  return fs;
}

// Get a catalog resource from prompt list
CatalogResource* get_catalog_resource(UaContext* ua)
{
  CatalogResource* catalog = NULL;
  char name[MAX_NAME_LENGTH];

  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("catalog")) && ua->argv[i]) {
      catalog = ua->GetCatalogResWithName(ua->argv[i]);
      if (catalog) { break; }
    }
  }

  if (ua->gui && !catalog) {
    {
      ResLocker _{my_config};
      catalog = (CatalogResource*)my_config->GetNextRes(R_CATALOG, NULL);
    }

    if (!catalog) {
      ua->ErrorMsg(T_("Could not find a Catalog resource\n"));
      return NULL;
    } else if (!ua->AclAccessOk(Catalog_ACL, catalog->resource_name_)) {
      ua->ErrorMsg(
          T_("You must specify a \"use <catalog-name>\" command before "
             "continuing.\n"));
      return NULL;
    }

    return catalog;
  }

  if (!catalog) {
    StartPrompt(ua, T_("The defined Catalog resources are:\n"));

    foreach_res (catalog, R_CATALOG) {
      if (ua->AclAccessOk(Catalog_ACL, catalog->resource_name_)) {
        AddPrompt(ua, catalog->resource_name_);
      }
    }

    if (DoPrompt(ua, T_("Catalog"), T_("Select Catalog resource"), name,
                 sizeof(name))
        < 0) {
      return NULL;
    }
    catalog = ua->GetCatalogResWithName(name);
  }

  return catalog;
}

// Select a job to enable or disable
JobResource* select_enable_disable_job_resource(UaContext* ua, bool enable)
{
  JobResource* job;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> job_resource_names;

  StartPrompt(ua, T_("The defined Job resources are:\n"));

  foreach_res (job, R_JOB) {
    if (!ua->AclAccessOk(Job_ACL, job->resource_name_)) { continue; }
    if (job->enabled == enable) { /* Already enabled/disabled? */
      continue;                   /* yes, skip */
    }
    job_resource_names.emplace_back(job->resource_name_);
  }

  SortCaseInsensitive(job_resource_names);

  for (auto& resource_name : job_resource_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("Job"), T_("Select Job resource"), name, sizeof(name))
      < 0) {
    return NULL;
  }

  job = ua->GetJobResWithName(name);

  return job;
}

// Select a Job resource from prompt list
JobResource* select_job_resource(UaContext* ua)
{
  JobResource* job;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> job_resource_names;

  StartPrompt(ua, T_("The defined Job resources are:\n"));

  foreach_res (job, R_JOB) {
    if (ua->AclAccessOk(Job_ACL, job->resource_name_)) {
      job_resource_names.emplace_back(job->resource_name_);
    }
  }

  SortCaseInsensitive(job_resource_names);

  for (auto& resource_name : job_resource_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("Job"), T_("Select Job resource"), name, sizeof(name))
      < 0) {
    return NULL;
  }

  job = ua->GetJobResWithName(name);

  return job;
}

// Select a Restore Job resource from argument or prompt
JobResource* get_restore_job(UaContext* ua)
{
  int i;
  JobResource* job;

  i = FindArgWithValue(ua, NT_("restorejob"));
  if (i >= 0) {
    job = ua->GetJobResWithName(ua->argv[i]);
    if (job && job->JobType == JT_RESTORE) { return job; }
    ua->ErrorMsg(T_("Error: Restore Job resource \"%s\" does not exist.\n"),
                 ua->argv[i]);
  }

  return select_restore_job_resource(ua);
}

// Select a Restore Job resource from prompt list
JobResource* select_restore_job_resource(UaContext* ua)
{
  JobResource* job;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> restore_job_names;

  StartPrompt(ua, T_("The defined Restore Job resources are:\n"));


  foreach_res (job, R_JOB) {
    if (job->JobType == JT_RESTORE
        && ua->AclAccessOk(Job_ACL, job->resource_name_)) {
      restore_job_names.emplace_back(job->resource_name_);
    }
  }

  SortCaseInsensitive(restore_job_names);

  for (auto& resource_name : restore_job_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("Job"), T_("Select Restore Job"), name, sizeof(name))
      < 0) {
    return NULL;
  }

  job = ua->GetJobResWithName(name);

  return job;
}

// Select a client resource from prompt list
ClientResource* select_client_resource(UaContext* ua)
{
  ClientResource* client;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> client_resource_names;

  StartPrompt(ua, T_("The defined Client resources are:\n"));

  foreach_res (client, R_CLIENT) {
    if (ua->AclAccessOk(Client_ACL, client->resource_name_)) {
      client_resource_names.emplace_back(client->resource_name_);
    }
  }

  SortCaseInsensitive(client_resource_names);

  for (auto& resource_name : client_resource_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("Client"), T_("Select Client (File daemon) resource"),
               name, sizeof(name))
      < 0) {
    return NULL;
  }

  client = ua->GetClientResWithName(name);

  return client;
}

// Select a client to enable or disable
ClientResource* select_enable_disable_client_resource(UaContext* ua,
                                                      bool enable)
{
  ClientResource* client;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> client_resource_names;

  StartPrompt(ua, T_("The defined Client resources are:\n"));

  foreach_res (client, R_CLIENT) {
    if (!ua->AclAccessOk(Client_ACL, client->resource_name_)) { continue; }
    if (client->enabled == enable) { /* Already enabled/disabled? */
      continue;                      /* yes, skip */
    }
    client_resource_names.emplace_back(client->resource_name_);
  }

  SortCaseInsensitive(client_resource_names);

  for (auto& resource_name : client_resource_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("Client"), T_("Select Client resource"), name,
               sizeof(name))
      < 0) {
    return NULL;
  }

  client = ua->GetClientResWithName(name);

  return client;
}

/**
 *  Get client resource, start by looking for
 *   client=<client-name>
 *  if we don't find the keyword, we prompt the user.
 */
ClientResource* get_client_resource(UaContext* ua)
{
  ClientResource* client = NULL;

  for (int i = 1; i < ua->argc; i++) {
    if ((Bstrcasecmp(ua->argk[i], NT_("client"))
         || Bstrcasecmp(ua->argk[i], NT_("fd")))
        && ua->argv[i]) {
      client = ua->GetClientResWithName(ua->argv[i]);
      if (client) { return client; }

      ua->ErrorMsg(T_("Error: Client resource %s does not exist.\n"),
                   ua->argv[i]);

      break;
    }
  }

  return select_client_resource(ua);
}

// Select a schedule to enable or disable
ScheduleResource* select_enable_disable_schedule_resource(UaContext* ua,
                                                          bool enable)
{
  ScheduleResource* sched;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> schedule_resource_names;

  StartPrompt(ua, T_("The defined Schedule resources are:\n"));

  foreach_res (sched, R_SCHEDULE) {
    if (!ua->AclAccessOk(Schedule_ACL, sched->resource_name_)) { continue; }
    if (sched->enabled == enable) { /* Already enabled/disabled? */
      continue;                     /* yes, skip */
    }
    schedule_resource_names.emplace_back(sched->resource_name_);
  }

  SortCaseInsensitive(schedule_resource_names);

  for (auto& resource_name : schedule_resource_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("Schedule"), T_("Select Schedule resource"), name,
               sizeof(name))
      < 0) {
    return NULL;
  }

  sched = ua->GetScheduleResWithName(name);

  return sched;
}

/**
 * Scan what the user has entered looking for:
 *
 * client=<client-name>
 *
 * If error or not found, put up a list of client DBRs to choose from.
 *
 * returns: false on error
 *          true on success and fills in ClientDbRecord
 */
bool GetClientDbr(UaContext* ua, ClientDbRecord* cr)
{
  if (cr->Name[0]) { /* If name already supplied */
    if (ua->db->GetClientRecord(ua->jcr, cr)) { return true; }
    ua->ErrorMsg(T_("Could not find Client %s: ERR=%s"), cr->Name,
                 ua->db->strerror());
  }

  for (int i = 1; i < ua->argc; i++) {
    if ((Bstrcasecmp(ua->argk[i], NT_("client"))
         || Bstrcasecmp(ua->argk[i], NT_("fd")))
        && ua->argv[i]) {
      if (!ua->AclAccessOk(Client_ACL, ua->argv[i])) { break; }
      bstrncpy(cr->Name, ua->argv[i], sizeof(cr->Name));
      if (!ua->db->GetClientRecord(ua->jcr, cr)) {
        ua->ErrorMsg(T_("Could not find Client \"%s\": ERR=%s"), ua->argv[i],
                     ua->db->strerror());
        cr->ClientId = 0;
        break;
      }
      return true;
    }
  }

  if (!SelectClientDbr(ua, cr)) { /* try once more by proposing a list */
    return false;
  }

  return true;
}

/**
 * Select a Client record from the catalog
 *
 * Returns true on success
 *         false on failure
 */
bool SelectClientDbr(UaContext* ua, ClientDbRecord* cr)
{
  DBId_t* ids = nullptr;
  ClientDbRecord ocr;
  int num_clients;
  char name[MAX_NAME_LENGTH];

  cr->ClientId = 0;
  if (!ua->db->GetClientIds(ua->jcr, &num_clients, &ids)) {
    ua->ErrorMsg(T_("Error obtaining client ids. ERR=%s\n"),
                 ua->db->strerror());
    if (ids) { free(ids); }
    return false;
  }

  if (num_clients <= 0) {
    ua->ErrorMsg(
        T_("No clients defined. You must run a job before using this "
           "command.\n"));
    if (ids) { free(ids); }
    return false;
  }

  StartPrompt(ua, T_("Defined Clients:\n"));
  for (int i = 0; i < num_clients; i++) {
    ocr.ClientId = ids[i];
    if (!ua->db->GetClientRecord(ua->jcr, &ocr)
        || !ua->AclAccessOk(Client_ACL, ocr.Name)) {
      continue;
    }
    AddPrompt(ua, ocr.Name);
  }
  if (ids) { free(ids); }

  if (DoPrompt(ua, T_("Client"), T_("Select the Client"), name, sizeof(name))
      < 0) {
    return false;
  }

  new (&ocr) ClientDbRecord();
  bstrncpy(ocr.Name, name, sizeof(ocr.Name));

  if (!ua->db->GetClientRecord(ua->jcr, &ocr)) {
    ua->ErrorMsg(T_("Could not find Client \"%s\": ERR=%s"), name,
                 ua->db->strerror());
    return false;
  }

  memcpy(cr, &ocr, sizeof(ocr));

  return true;
}

/**
 * Scan what the user has entered looking for:
 *
 * argk=<storage-name>
 *
 * where argk can be : storage
 *
 * If error or not found, put up a list of storage DBRs to choose from.
 *
 * returns: false on error
 *          true  on success and fills in StorageDbRecord
 */
bool GetStorageDbr(UaContext* ua, StorageDbRecord* sr, const char* argk)
{
  if (sr->Name[0]) { /* If name already supplied */
    if (ua->db->GetStorageRecord(ua->jcr, sr)
        && ua->AclAccessOk(Pool_ACL, sr->Name)) {
      return true;
    }
    ua->ErrorMsg(T_("Could not find Storage \"%s\": ERR=%s"), sr->Name,
                 ua->db->strerror());
  }

  if (!SelectStorageDbr(ua, sr, argk)) { /* try once more */
    return false;
  }

  return true;
}

/**
 * Scan what the user has entered looking for:
 *
 * argk=<pool-name>
 *
 * where argk can be : pool, recyclepool, scratchpool, nextpool etc..
 *
 * If error or not found, put up a list of pool DBRs to choose from.
 *
 * returns: false on error
 *          true  on success and fills in PoolDbRecord
 */
bool GetPoolDbr(UaContext* ua, PoolDbRecord* pr, const char* argk)
{
  if (pr->Name[0]) { /* If name already supplied */
    if (ua->db->GetPoolRecord(ua->jcr, pr)
        && ua->AclAccessOk(Pool_ACL, pr->Name)) {
      return true;
    }
    ua->ErrorMsg(T_("Could not find Pool \"%s\": ERR=%s"), pr->Name,
                 ua->db->strerror());
  }

  if (!SelectPoolDbr(ua, pr, argk)) { /* try once more */
    return false;
  }

  return true;
}

/**
 * Select a Pool record from catalog
 * argk can be pool, recyclepool, scratchpool etc..
 */
bool SelectPoolDbr(UaContext* ua, PoolDbRecord* pr, const char* argk)
{
  DBId_t* ids = nullptr;
  int num_pools;
  char name[MAX_NAME_LENGTH];

  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], argk) && ua->argv[i]
        && ua->AclAccessOk(Pool_ACL, ua->argv[i])) {
      bstrncpy(pr->Name, ua->argv[i], sizeof(pr->Name));
      if (DbLocker _{ua->db}; !ua->db->GetPoolRecord(ua->jcr, pr)) {
        ua->ErrorMsg(T_("Could not find Pool \"%s\": ERR=%s"), ua->argv[i],
                     ua->db->strerror());
        pr->PoolId = 0;
        break;
      }
      return true;
    }
  }

  pr->PoolId = 0;
  if (DbLocker _{ua->db}; !ua->db->GetPoolIds(ua->jcr, &num_pools, &ids)) {
    ua->ErrorMsg(T_("Error obtaining pool ids. ERR=%s\n"), ua->db->strerror());
    if (ids) { free(ids); }
    return 0;
  }

  if (num_pools <= 0) {
    ua->ErrorMsg(
        T_("No pools defined. Use the \"create\" command to create one.\n"));
    if (ids) { free(ids); }
    return false;
  }

  StartPrompt(ua, T_("Defined Pools:\n"));
  if (bstrcmp(argk, NT_("recyclepool"))) { AddPrompt(ua, T_("*None*")); }

  for (int i = 0; i < num_pools; i++) {
    PoolDbRecord candidate{};
    candidate.PoolId = ids[i];
    if (!ua->db->GetPoolRecord(ua->jcr, &candidate)
        || !ua->AclAccessOk(Pool_ACL, candidate.Name)) {
      continue;
    }
    AddPrompt(ua, candidate.Name);
  }
  if (ids) { free(ids); }

  if (DoPrompt(ua, T_("Pool"), T_("Select the Pool"), name, sizeof(name)) < 0) {
    return false;
  }

  PoolDbRecord selected{};

  /* *None* is only returned when selecting a recyclepool, and in that case
   * the calling code is only interested in opr.Name, so then we can leave
   * pr as all zero. */
  if (!bstrcmp(name, T_("*None*"))) {
    bstrncpy(selected.Name, name, sizeof(selected.Name));

    if (DbLocker _{ua->db}; !ua->db->GetPoolRecord(ua->jcr, &selected)) {
      ua->ErrorMsg(T_("Could not find Pool \"%s\": ERR=%s"), name,
                   ua->db->strerror());
      return false;
    }
  }

  *pr = std::move(selected);

  return true;
}

// Select a Pool and a Media (Volume) record from the database
bool SelectPoolForMediaDbr(UaContext* ua, PoolDbRecord* pr, MediaDbRecord* mr)
{
  if (!mr) { return false; }

  new (pr) PoolDbRecord();  // placement new instead of memset

  pr->PoolId = mr->PoolId;
  if (!ua->db->GetPoolRecord(ua->jcr, pr)) {
    ua->ErrorMsg("%s", ua->db->strerror());
    return false;
  }

  if (!ua->AclAccessOk(Pool_ACL, pr->Name, true)) {
    ua->ErrorMsg(T_("No access to Pool \"%s\"\n"), pr->Name);
    return false;
  }

  return true;
}

/**
 * Select a Storage record from catalog
 * argk can be storage
 */
bool SelectStorageDbr(UaContext* ua, StorageDbRecord* sr, const char* argk)
{
  StorageDbRecord osr;
  DBId_t* ids = nullptr;
  int num_storages;
  char name[MAX_NAME_LENGTH];

  for (int i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], argk) && ua->argv[i]
        && ua->AclAccessOk(Storage_ACL, ua->argv[i])) {
      bstrncpy(sr->Name, ua->argv[i], sizeof(sr->Name));
      if (!ua->db->GetStorageRecord(ua->jcr, sr)) {
        ua->ErrorMsg(T_("Could not find Storage \"%s\": ERR=%s"), ua->argv[i],
                     ua->db->strerror());
        sr->StorageId = 0;
        break;
      }
      return true;
    }
  }

  sr->StorageId = 0;
  if (!ua->db->GetStorageIds(ua->jcr, &num_storages, &ids)) {
    ua->ErrorMsg(T_("Error obtaining storage ids. ERR=%s\n"),
                 ua->db->strerror());
    if (ids) { free(ids); }
    return 0;
  }

  if (num_storages <= 0) {
    ua->ErrorMsg(T_("No storages defined.\n"));
    if (ids) { free(ids); }
    return false;
  }

  StartPrompt(ua, T_("Defined Storages:\n"));
  if (bstrcmp(argk, NT_("recyclestorage"))) { AddPrompt(ua, T_("*None*")); }

  for (int i = 0; i < num_storages; i++) {
    osr.StorageId = ids[i];
    if (!ua->db->GetStorageRecord(ua->jcr, &osr)
        || !ua->AclAccessOk(Storage_ACL, osr.Name)) {
      continue;
    }
    AddPrompt(ua, osr.Name);
  }
  if (ids) { free(ids); }

  if (DoPrompt(ua, T_("Storage"), T_("Select the Storage"), name, sizeof(name))
      < 0) {
    return false;
  }

  new (&osr) StorageDbRecord();  // placement new instead of memset

  /* *None* is only returned when selecting a recyclestorage, and in that case
   * the calling code is only interested in osr.Name, so then we can leave
   * sr as all zero. */
  if (!bstrcmp(name, T_("*None*"))) {
    bstrncpy(osr.Name, name, sizeof(osr.Name));

    if (!ua->db->GetStorageRecord(ua->jcr, &osr)) {
      ua->ErrorMsg(T_("Could not find Storage \"%s\": ERR=%s"), name,
                   ua->db->strerror());
      return false;
    }
  }

  memcpy(sr, &osr, sizeof(osr));

  return true;
}

bool SelectMediaDbrByName(UaContext* ua,
                          MediaDbRecord* mr,
                          const char* volumename)
{
  new (mr) MediaDbRecord();
  std::string err{};
  if (IsNameValid(volumename, err)) {
    bstrncpy(mr->VolumeName, volumename, sizeof(mr->VolumeName));
  } else {
    err = ua->db->strerror();
    return false;
  }

  bstrncpy(mr->VolumeName, volumename, sizeof(mr->VolumeName));
  if (!ua->db->GetMediaRecord(ua->jcr, mr)) {
    ua->ErrorMsg("%s", ua->db->strerror());
    return false;
  }
  return true;
}

// Select a Media (Volume) record from the database
bool SelectMediaDbr(UaContext* ua, MediaDbRecord* mr)
{
  int i;
  int retval = false;
  std::string err{};

  *mr = {};

  i = FindArgWithValue(ua, NT_("volume"));
  if (i >= 0) {
    if (IsNameValid(ua->argv[i], err)) {
      bstrncpy(mr->VolumeName, ua->argv[i], sizeof(mr->VolumeName));
    } else {
      goto bail_out;
    }
  }

  if (mr->VolumeName[0] == 0) {
    PoolDbRecord pr;
    // Get the pool from pool=<pool-name>
    if (DbLocker _{ua->db}; !GetPoolDbr(ua, &pr)) {
      err = ua->db->strerror();
      goto bail_out;
    }

    mr->PoolId = pr.PoolId;
    ua->db->ListMediaRecords(ua->jcr, mr, NULL, false, ua->send, HORZ_LIST);

    ua->SendMsg(
        T_("Enter the volume name or MediaId of the volume prefixed with an "
           "asterisk (*).\n"));
    if (!GetCmd(ua, T_("E.g. \"full-0001\" or \"*42\": "))) { goto bail_out; }

    if (ua->cmd[0] == '*' && Is_a_number(ua->cmd + 1)) {
      mr->MediaId = str_to_int64(ua->cmd + 1);
    } else if (IsNameValid(ua->cmd, err)) {
      bstrncpy(mr->VolumeName, ua->cmd, sizeof(mr->VolumeName));
    } else {
      goto bail_out;
    }
  }

  if (DbLocker _{ua->db}; !ua->db->GetMediaRecord(ua->jcr, mr)) {
    err = ua->db->strerror();
    goto bail_out;
  }
  retval = true;

bail_out:
  if (!retval && !err.empty()) { ua->ErrorMsg("%s", err.c_str()); }
  return retval;
}

// Select a pool resource from prompt list
PoolResource* select_pool_resource(UaContext* ua)
{
  PoolResource* pool;
  char name[MAX_NAME_LENGTH];
  std::vector<std::string> pool_resource_names;

  StartPrompt(ua, T_("The defined Pool resources are:\n"));

  foreach_res (pool, R_POOL) {
    if (ua->AclAccessOk(Pool_ACL, pool->resource_name_)) {
      pool_resource_names.emplace_back(pool->resource_name_);
    }
  }


  SortCaseInsensitive(pool_resource_names);

  for (auto& resource_name : pool_resource_names) {
    AddPrompt(ua, std::move(resource_name));
  }

  if (DoPrompt(ua, T_("Pool"), T_("Select Pool resource"), name, sizeof(name))
      < 0) {
    return NULL;
  }

  pool = ua->GetPoolResWithName(name);

  return pool;
}

/**
 *  If you are thinking about using it, you
 *  probably want to use SelectPoolDbr()
 *  or GetPoolDbr() above.
 */
PoolResource* get_pool_resource(UaContext* ua)
{
  int i;
  PoolResource* pool = NULL;

  i = FindArgWithValue(ua, NT_("pool"));
  if (i >= 0 && ua->AclAccessOk(Pool_ACL, ua->argv[i])) {
    pool = ua->GetPoolResWithName(ua->argv[i]);
    if (pool) { return pool; }
    ua->ErrorMsg(T_("Error: Pool resource \"%s\" does not exist.\n"),
                 ua->argv[i]);
  }

  return select_pool_resource(ua);
}

// List all jobs and ask user to select one
int SelectJobDbr(UaContext* ua, JobDbRecord* jr)
{
  ua->db->ListJobRecords(ua->jcr, jr, "", NULL, {}, {}, {}, nullptr, nullptr, 0,
                         0, 0, ua->send, HORZ_LIST);
  if (!GetPint(ua, T_("Enter the JobId to select: "))) { return 0; }

  jr->JobId = ua->int64_val;
  if (!ua->db->GetJobRecord(ua->jcr, jr)) {
    ua->ErrorMsg("%s", ua->db->strerror());
    return 0;
  }

  return jr->JobId;
}

/**
 * Scan what the user has entered looking for:
 *
 * jobid=nn
 *
 * if error or not found, put up a list of Jobs
 * to choose from.
 *
 * returns: 0 on error
 *          JobId on success and fills in JobDbRecord
 */
int GetJobDbr(UaContext* ua, JobDbRecord* jr)
{
  int i;

  for (i = 1; i < ua->argc; i++) {
    if (Bstrcasecmp(ua->argk[i], NT_("ujobid")) && ua->argv[i]) {
      jr->JobId = 0;
      bstrncpy(jr->Job, ua->argv[i], sizeof(jr->Job));
    } else if (Bstrcasecmp(ua->argk[i], NT_("jobid")) && ua->argv[i]) {
      jr->JobId = str_to_int64(ua->argv[i]);
      jr->Job[0] = 0;
    } else {
      continue;
    }
    if (!ua->db->GetJobRecord(ua->jcr, jr)) {
      ua->ErrorMsg(T_("Could not find Job \"%s\": ERR=%s"), ua->argv[i],
                   ua->db->strerror());
      jr->JobId = 0;
      break;
    }
    return jr->JobId;
  }

  jr->JobId = 0;
  jr->Job[0] = 0;

  for (i = 1; i < ua->argc; i++) {
    if ((Bstrcasecmp(ua->argk[i], NT_("jobname"))
         || Bstrcasecmp(ua->argk[i], NT_("job")))
        && ua->argv[i]) {
      jr->JobId = 0;
      bstrncpy(jr->Name, ua->argv[i], sizeof(jr->Name));
      break;
    }
  }

  if (!SelectJobDbr(ua, jr)) { /* try once more */
    return 0;
  }

  return jr->JobId;
}

// Implement unique set of prompts
void StartPrompt(UaContext* ua, const char* msg)
{
  if (ua->max_prompts == 0) {
    ua->max_prompts = 10;
    ua->prompt = (char**)malloc(sizeof(char*) * ua->max_prompts);
  }
  ua->num_prompts = 1;
  ua->prompt[0] = strdup(msg);
}

// Add to prompts -- keeping them unique
void AddPrompt(UaContext* ua, const char* prompt)
{
  if (ua->num_prompts == ua->max_prompts) {
    ua->max_prompts *= 2;
    ua->prompt = (char**)realloc(ua->prompt, sizeof(char*) * ua->max_prompts);
  }

  for (int i = 1; i < ua->num_prompts; i++) {
    if (bstrcmp(ua->prompt[i], prompt)) { return; }
  }

  ua->prompt[ua->num_prompts++] = strdup(prompt);
}

void AddPrompt(UaContext* ua, std::string&& prompt)
{
  std::string p{prompt};
  AddPrompt(ua, p.c_str());
}

/**
 * Formats the prompts of a UaContext to be displayed in a multicolumn output
 * when possible
 */
std::string FormatPrompts(const UaContext* ua,
                          const int window_width,
                          const int min_lines_threshold)
{
  unsigned int max_prompt_length = 1;

  const int max_prompt_index_length = std::to_string(ua->num_prompts).length();

  for (int i = 1; i < ua->num_prompts; i++) {
    if (strlen(ua->prompt[i]) > max_prompt_length) {
      max_prompt_length = strlen(ua->prompt[i]);
    }
  }

  const short int colon_after_index = 1;
  const short int space_between_colon_and_prompt = 1;
  const short int space_after_prompt = 1;
  const short int extra_character_room_for_snprintf = 1;

  constexpr const short int extra_formatting_characters
      = colon_after_index + space_between_colon_and_prompt + space_after_prompt
        + extra_character_room_for_snprintf;

  const int max_formatted_prompt_length = max_prompt_length
                                          + max_prompt_index_length
                                          + extra_formatting_characters;
  const int prompts_perline
      = std::max(1, window_width / (max_formatted_prompt_length - 1));

  const int number_output_lines
      = (ua->num_prompts + prompts_perline - 1) / prompts_perline;

  std::vector<char> formatted_prompt(max_formatted_prompt_length);
  std::vector<std::vector<char>> formatted_prompts_container;

  std::string output{};

  if (ua->num_prompts > min_lines_threshold
      && window_width > max_formatted_prompt_length * 2) {
    for (int i = 1; i < ua->num_prompts; i++) {
      {
        snprintf(formatted_prompt.data(), max_formatted_prompt_length,
                 "%*d: %-*s ", max_prompt_index_length, i, max_prompt_length,
                 ua->prompt[i]);
      }

      formatted_prompts_container.push_back(formatted_prompt);
    }

    int index = 0;
    for (int i = 0; i < number_output_lines; i++) {
      index = i;
      while (static_cast<size_t>(index) < formatted_prompts_container.size()) {
        output.append(formatted_prompts_container[index].data());
        index += number_output_lines;
      }

      output.append("\n");
    }
  } else {
    for (int i = 1; i < ua->num_prompts; i++) {
      {
        snprintf(formatted_prompt.data(), max_formatted_prompt_length,
                 "%*d: %s\n", max_prompt_index_length, i, ua->prompt[i]);
        output.append(formatted_prompt.data());
      }
    }
  }

  return output;
}


/**
 * Display prompts and get user's choice
 *
 * Returns: -1 on error
 *           index base 0 on success, and choice is copied to prompt if not
 * NULL prompt is set to the chosen prompt item string
 */
int DoPrompt(UaContext* ua,
             const char* automsg,
             const char* msg,
             char* prompt,
             int max_prompt)
{
  int item;
  PoolMem pmsg(PM_MESSAGE);
  BareosSocket* user = ua->UA_sock;

  int window_width = 80;
  int min_lines_threshold = 20;

  if (prompt) { *prompt = 0; }
  if (ua->num_prompts == 2) {
    item = 1;
    if (prompt) { bstrncpy(prompt, ua->prompt[1], max_prompt); }
    if (!ua->api && !ua->runscript) {
      ua->SendMsg(T_("Automatically selected %s: %s\n"), automsg,
                  ua->prompt[1]);
    }
    goto done;
  }

  // If running non-interactive, bail out
  if (ua->batch) {
    // First print the choices he wanted to make
    ua->SendMsg("%s", ua->prompt[0]);
    ua->SendMsg("%s",
                FormatPrompts(ua, window_width, min_lines_threshold).c_str());

    // Now print error message
    ua->SendMsg(T_("Your request has multiple choices for \"%s\". Selection is "
                   "not possible in batch mode.\n"),
                automsg);
    item = -1;
    goto done;
  }

  if (ua->api) { user->signal(BNET_START_SELECT); }

  ua->SendMsg("%s", ua->prompt[0]);


  if (ua->api) {
    for (int i = 1; i < ua->num_prompts; i++) {
      ua->SendMsg("%s", ua->prompt[i]);
    }
  } else {
    ua->SendMsg("%s",
                FormatPrompts(ua, window_width, min_lines_threshold).c_str());
  }


  if (ua->api) { user->signal(BNET_END_SELECT); }

  while (1) {
    // First item is the prompt string, not the items
    if (ua->num_prompts == 1) {
      ua->ErrorMsg(T_("Selection list for \"%s\" is empty!\n"), automsg);
      item = -1; /* list is empty ! */
      break;
    }
    if (ua->num_prompts == 2) {
      item = 1;
      ua->SendMsg(T_("Automatically selected: %s\n"), ua->prompt[1]);
      if (prompt) { bstrncpy(prompt, ua->prompt[1], max_prompt); }
      break;
    } else {
      Mmsg(pmsg, "%s (1-%d): ", msg, ua->num_prompts - 1);
    }

    // Either a . or an @ will get you out of the loop
    if (ua->api) { user->signal(BNET_SELECT_INPUT); }

    if (!GetPint(ua, pmsg.c_str())) {
      item = -1; /* error */
      ua->InfoMsg(T_("Selection aborted, nothing done.\n"));
      break;
    }
    item = ua->pint32_val;
    if (item < 1 || item >= ua->num_prompts) {
      ua->WarningMsg(T_("Please enter a number between 1 and %d\n"),
                     ua->num_prompts - 1);
      continue;
    }
    if (prompt) { bstrncpy(prompt, ua->prompt[item], max_prompt); }
    break;
  }

done:
  for (int i = 0; i < ua->num_prompts; i++) { free(ua->prompt[i]); }
  ua->num_prompts = 0;

  return (item > 0) ? (item - 1) : item;
}

/**
 * We scan what the user has entered looking for :
 * - storage=<storage-resource>
 * - job=<job_name>
 * - jobid=<jobid>
 * - ?              (prompt him with storage list)
 * - <some-error>   (prompt him with storage list)
 *
 * If use_default is set, we assume that any keyword without a value
 * is the name of the Storage resource wanted.
 *
 * If autochangers_only is given, we limit the output to autochangers only.
 */
StorageResource* get_storage_resource(UaContext* ua,
                                      bool use_default,
                                      bool autochangers_only)
{
  int i;
  JobControlRecord* jcr;
  int jobid;
  char ed1[50];
  char* StoreName = NULL;
  StorageResource* store = NULL;

  Dmsg1(100, "get_storage_resource: autochangers_only is %d\n",
        autochangers_only);

  for (i = 1; i < ua->argc; i++) {
    // Ignore any zapped keyword.
    if (*ua->argk[i] == 0) { continue; }
    if (use_default && !ua->argv[i]) {
      // Ignore alldrives, barcode, barcodes, encrypt, scan and slots keywords.
      if (Bstrcasecmp("barcode", ua->argk[i])
          || Bstrcasecmp("barcodes", ua->argk[i])
          || Bstrcasecmp("encrypt", ua->argk[i])
          || Bstrcasecmp("scan", ua->argk[i])
          || Bstrcasecmp("slots", ua->argk[i])
          || Bstrcasecmp("alldrives", ua->argk[i])) {
        continue;
      }
      // Default argument is storage
      if (StoreName) {
        ua->ErrorMsg(T_("Storage name given twice.\n"));
        return NULL;
      }
      StoreName = ua->argk[i];
      if (*StoreName == '?') {
        *StoreName = 0;
        break;
      }
    } else {
      if (Bstrcasecmp(ua->argk[i], NT_("storage"))
          || Bstrcasecmp(ua->argk[i], NT_("sd"))) {
        StoreName = ua->argv[i];
        break;
      } else if (Bstrcasecmp(ua->argk[i], NT_("jobid"))) {
        jobid = str_to_int64(ua->argv[i]);
        if (jobid <= 0) {
          ua->ErrorMsg(T_("Expecting jobid=nn command, got: %s\n"),
                       ua->argk[i]);
          return NULL;
        }
        if (!(jcr = get_jcr_by_id(jobid))) {
          ua->ErrorMsg(T_("JobId %s is not running.\n"),
                       edit_int64(jobid, ed1));
          return NULL;
        }
        store = jcr->dir_impl->res.write_storage;
        FreeJcr(jcr);
        break;
      } else if (Bstrcasecmp(ua->argk[i], NT_("job"))
                 || Bstrcasecmp(ua->argk[i], NT_("jobname"))) {
        if (!ua->argv[i]) {
          ua->ErrorMsg(T_("Expecting job=xxx, got: %s.\n"), ua->argk[i]);
          return NULL;
        }
        if (!(jcr = get_jcr_by_partial_name(ua->argv[i]))) {
          ua->ErrorMsg(T_("Job \"%s\" is not running.\n"), ua->argv[i]);
          return NULL;
        }
        store = jcr->dir_impl->res.write_storage;
        FreeJcr(jcr);
        break;
      } else if (Bstrcasecmp(ua->argk[i], NT_("ujobid"))) {
        if (!ua->argv[i]) {
          ua->ErrorMsg(T_("Expecting ujobid=xxx, got: %s.\n"), ua->argk[i]);
          return NULL;
        }
        if (!(jcr = get_jcr_by_full_name(ua->argv[i]))) {
          ua->ErrorMsg(T_("Job \"%s\" is not running.\n"), ua->argv[i]);
          return NULL;
        }
        store = jcr->dir_impl->res.write_storage;
        FreeJcr(jcr);
        break;
      }
    }
  }

  if (store && !ua->AclAccessOk(Storage_ACL, store->resource_name_)) {
    store = NULL;
  }

  if (!store && StoreName && StoreName[0] != 0) {
    store = ua->GetStoreResWithName(StoreName);

    if (!store) {
      ua->ErrorMsg(T_("Storage resource \"%s\": not found\n"), StoreName);
    }
  }

  if (store && !ua->AclAccessOk(Storage_ACL, store->resource_name_)) {
    store = NULL;
  }

  // No keywords found, so present a selection list
  if (!store) { store = select_storage_resource(ua, autochangers_only); }

  return store;
}

// Get drive that we are working with for this storage
drive_number_t GetStorageDrive(UaContext* ua, StorageResource* store)
{
  int i;
  char drivename[10];
  drive_number_t drive = kInvalidDriveNumber;

  // Get drive for autochanger if possible
  i = FindArgWithValue(ua, NT_("drive"));
  if (i >= 0) {
    drive = static_cast<drive_number_t>(atoi(ua->argv[i]));
  } else if (store && store->autochanger) {
    drive_number_t drives;

    drives = GetNumDrives(ua, store);

    // If only one drive, default = 0
    if (drives == 1) {
      drive = 0;
    } else {
      // Ask user to enter drive number
      StartPrompt(ua, T_("Select Drive:\n"));
      for (drive_number_t cnt = 0; cnt < drives; cnt++) {
        Bsnprintf(drivename, sizeof(drivename), "Drive %hd", cnt);
        AddPrompt(ua, drivename);
      }
      if (DoPrompt(ua, T_("Drive"), T_("Select drive"), drivename,
                   sizeof(drivename))
          < 0) {
        drive = kInvalidDriveNumber; /* None */
      } else {
        sscanf(drivename, "Drive %hd", &drive);
      }
    }
  } else {
    // If only one drive, default = 0
    drive = 0;
  }

  return drive;
}

// Get slot that we are working with for this storage
slot_number_t GetStorageSlot(UaContext* ua, StorageResource* store)
{
  int i;
  slot_number_t slot = -1;

  // Get slot for autochanger if possible
  i = FindArgWithValue(ua, NT_("slot"));
  if (i >= 0) {
    slot = atoi(ua->argv[i]);
  } else if (store && store->autochanger) {
    // Ask user to enter slot number
    ua->cmd[0] = 0;
    if (!GetCmd(ua, T_("Enter autochanger slot: "))) {
      slot = -1; /* None */
    } else {
      slot = atoi(ua->cmd);
    }
  }

  return slot;
}

/**
 * Scan looking for mediatype=
 *
 *  if not found or error, put up selection list
 *
 *  Returns: 0 on error
 *           1 on success, MediaType is set
 */
int GetMediaType(UaContext* ua, char* MediaType, int max_media)
{
  StorageResource* store;
  int i;

  i = FindArgWithValue(ua, NT_("mediatype"));
  if (i >= 0) {
    bstrncpy(MediaType, ua->argv[i], max_media);
    return 1;
  }

  StartPrompt(ua, T_("Media Types defined in conf file:\n"));

  foreach_res (store, R_STORAGE) {
    if (ua->AclAccessOk(Storage_ACL, store->resource_name_)) {
      AddPrompt(ua, store->media_type);
    }
  }

  return (DoPrompt(ua, T_("Media Type"), T_("Select the Media Type"), MediaType,
                   max_media)
          < 0)
             ? 0
             : 1;
}

bool GetLevelFromName(JobControlRecord* jcr, const char* level_name)
{
  bool found = false;

  // Look up level name and pull code
  for (int i = 0; joblevels[i].name; i++) {
    if (Bstrcasecmp(level_name, joblevels[i].name)) {
      jcr->setJobLevel(joblevels[i].level);
      found = true;
      break;
    }
  }

  return found;
}

// Insert an JobId into the list of selected JobIds when its a unique new id.
static inline bool InsertSelectedJobid(alist<JobId_t*>* selected_jobids,
                                       JobId_t JobId)
{
  if (!selected_jobids) { return false; }

  for (auto* jobid : selected_jobids) {
    if (*jobid == JobId) { return false; }
  }

  auto* selected_jobid = (JobId_t*)malloc(sizeof(JobId_t));
  *selected_jobid = JobId;
  selected_jobids->append(selected_jobid);
  return true;
}

/**
 * Get a job selection, "reason" is used in user messages and can be: cancel,
 * limit, ...
 *
 * Returns: NULL on error
 *          alist on success with the selected jobids.
 */
alist<JobId_t*>* select_jobs(UaContext* ua, const char* reason)
{
  int i;
  int cnt = 0;
  int njobs = 0;
  JobControlRecord* jcr = NULL;
  bool select_all = false;
  bool select_by_state = false;
  alist<JobId_t*>* selected_jobids;
  const char* lst[] = {"job", "jobid", "ujobid", NULL};
  enum
  {
    none,
    all_jobs,
    created_jobs,
    blocked_jobs,
    waiting_jobs,
    running_jobs
  } selection_criterium;

  // Allocate a list for holding the selected JobIds.
  selected_jobids = new alist<JobId_t*>(10, owned_by_alist);

  // See if "all" is given.
  if (FindArg(ua, NT_("all")) > 0) { select_all = true; }

  // See if "state=" is given.
  if (FindArgWithValue(ua, NT_("state")) > 0) { select_by_state = true; }

  // See if there are any jobid, job or ujobid keywords.
  if (FindArgKeyword(ua, lst) > 0) {
    for (i = 1; i < ua->argc; i++) {
      if (Bstrcasecmp(ua->argk[i], NT_("jobid"))) {
        JobId_t JobId = str_to_int64(ua->argv[i]);
        if (!JobId) { continue; }
        if (!(jcr = get_jcr_by_id(JobId))) {
          ua->ErrorMsg(T_("JobId %s is not running. Use Job name to %s "
                          "inactive jobs.\n"),
                       ua->argv[i], T_(reason));
          continue;
        }
      } else if (Bstrcasecmp(ua->argk[i], NT_("job"))) {
        if (!ua->argv[i]) { continue; }
        if (!(jcr = get_jcr_by_partial_name(ua->argv[i]))) {
          ua->WarningMsg(
              T_("Warning Job %s is not running. Continuing anyway ...\n"),
              ua->argv[i]);
          continue;
        }
      } else if (Bstrcasecmp(ua->argk[i], NT_("ujobid"))) {
        if (!ua->argv[i]) { continue; }
        if (!(jcr = get_jcr_by_full_name(ua->argv[i]))) {
          ua->WarningMsg(
              T_("Warning Job %s is not running. Continuing anyway ...\n"),
              ua->argv[i]);
          continue;
        }
      }

      if (jcr) {
        if (jcr->dir_impl->res.job
            && !ua->AclAccessOk(Job_ACL, jcr->dir_impl->res.job->resource_name_,
                                true)) {
          ua->ErrorMsg(T_("Unauthorized command from this console.\n"));
          goto bail_out;
        }

        if (InsertSelectedJobid(selected_jobids, jcr->JobId)) { cnt++; }

        FreeJcr(jcr);
        jcr = NULL;
      }
    }
  }

  /* If we didn't select any Jobs using jobid, job or ujobid keywords try
   * other selections. */
  if (cnt == 0) {
    char buf[1000];
    int tjobs = 0; /* Total # number jobs */

    // Count Jobs running
    foreach_jcr (jcr) {
      if (jcr->JobId == 0) { /* This is us */
        continue;
      }
      tjobs++; /* Count of all jobs */
      if (!ua->AclAccessOk(Job_ACL, jcr->dir_impl->res.job->resource_name_)) {
        continue; /* Skip not authorized */
      }
      njobs++; /* Count of authorized jobs */
    }
    endeach_jcr(jcr);

    if (njobs == 0) { /* No authorized */
      if (tjobs == 0) {
        ua->SendMsg(T_("No Jobs running.\n"));
      } else {
        ua->SendMsg(T_("None of your jobs are running.\n"));
      }
      goto bail_out;
    }

    if (select_all || select_by_state) {
      // Set selection criterium.
      selection_criterium = none;
      if (select_all) {
        selection_criterium = all_jobs;
      } else {
        i = FindArgWithValue(ua, NT_("state"));
        if (i > 0) {
          if (Bstrcasecmp(ua->argv[i], NT_("created"))) {
            selection_criterium = created_jobs;
          }

          if (Bstrcasecmp(ua->argv[i], NT_("blocked"))) {
            selection_criterium = blocked_jobs;
          }

          if (Bstrcasecmp(ua->argv[i], NT_("waiting"))) {
            selection_criterium = waiting_jobs;
          }

          if (Bstrcasecmp(ua->argv[i], NT_("running"))) {
            selection_criterium = running_jobs;
          }

          if (selection_criterium == none) {
            ua->ErrorMsg(
                T_("Illegal state either created, blocked, waiting or "
                   "running\n"));
            goto bail_out;
          }
        }
      }

      /* Select from all available Jobs the Jobs matching the selection
       * criterium. */
      foreach_jcr (jcr) {
        if (jcr->JobId == 0) { /* This is us */
          continue;
        }

        if (!ua->AclAccessOk(Job_ACL, jcr->dir_impl->res.job->resource_name_)) {
          continue; /* Skip not authorized */
        }

        // See if we need to select this JobId.
        switch (selection_criterium) {
          case all_jobs:
            break;
          case created_jobs:
            if (jcr->getJobStatus() != JS_Created) { continue; }
            break;
          case blocked_jobs:
            if (!jcr->job_started || jcr->getJobStatus() != JS_Blocked) {
              continue;
            }
            break;
          case waiting_jobs:
            if (!JobWaiting(jcr)) { continue; }
            break;
          case running_jobs:
            if (!jcr->job_started || jcr->getJobStatus() != JS_Running) {
              continue;
            }
            break;
          default:
            break;
        }

        InsertSelectedJobid(selected_jobids, jcr->JobId);
        ua->SendMsg(T_("Selected Job %d for cancelling\n"), jcr->JobId);
      }
      endeach_jcr(jcr);

      if (selected_jobids->empty()) {
        ua->SendMsg(T_("No Jobs selected.\n"));
        goto bail_out;
      }

      /* Only ask for confirmation when not in batch mode and there is no yes
       * on the cmdline. */
      if (!ua->batch && FindArg(ua, NT_("yes")) == -1) {
        if (!GetYesno(ua, T_("Confirm cancel (yes/no): ")) || !ua->pint32_val) {
          goto bail_out;
        }
      }
    } else {
      char temp[256];
      char JobName[MAX_NAME_LENGTH];

      // Interactivly select a Job.
      StartPrompt(ua, T_("Select Job:\n"));
      foreach_jcr (jcr) {
        char ed1[50];
        if (jcr->JobId == 0) { /* This is us */
          continue;
        }
        if (!ua->AclAccessOk(Job_ACL, jcr->dir_impl->res.job->resource_name_)) {
          continue; /* Skip not authorized */
        }
        Bsnprintf(buf, sizeof(buf), T_("JobId=%s Job=%s"),
                  edit_int64(jcr->JobId, ed1), jcr->Job);
        AddPrompt(ua, buf);
      }
      endeach_jcr(jcr);

      Bsnprintf(temp, sizeof(temp), T_("Choose Job to %s"), T_(reason));
      if (DoPrompt(ua, T_("Job"), temp, buf, sizeof(buf)) < 0) {
        goto bail_out;
      }

      if (bstrcmp(reason, "cancel")) {
        if (ua->api && njobs == 1) {
          char nbuf[1000];

          Bsnprintf(nbuf, sizeof(nbuf), T_("Cancel: %s\n\n%s"), buf,
                    T_("Confirm cancel?"));
          if (!GetYesno(ua, nbuf) || !ua->pint32_val) { goto bail_out; }
        } else {
          if (njobs == 1) {
            if (!GetYesno(ua, T_("Confirm cancel (yes/no): "))
                || !ua->pint32_val) {
              goto bail_out;
            }
          }
        }
      }

      sscanf(buf, "JobId=%d Job=%127s", &njobs, JobName);
      jcr = get_jcr_by_full_name(JobName);
      if (!jcr) {
        ua->WarningMsg(T_("Job \"%s\" not found.\n"), JobName);
        goto bail_out;
      }

      InsertSelectedJobid(selected_jobids, jcr->JobId);
      FreeJcr(jcr);
    }
  }

  return selected_jobids;

bail_out:
  delete selected_jobids;

  return NULL;
}

/**
 * Get a slot selection.
 *
 * Returns: false on error
 *          true on success with the selected slots set in the slot_list.
 */
bool GetUserSlotList(UaContext* ua,
                     char* slot_list,
                     const char* argument,
                     int num_slots)
{
  int i, len, beg, end;
  const char* msg;
  char search_argument[20];
  char *p, *e, *h;

  // See if the argument given is found on the cmdline.
  bstrncpy(search_argument, argument, sizeof(search_argument));
  i = FindArgWithValue(ua, search_argument);
  if (i == -1) { /* not found */
    /* See if the last letter of search_argument is a 's'
     * When it is strip it and try if that argument is given. */
    len = strlen(search_argument);
    if (len > 0 && search_argument[len - 1] == 's') {
      search_argument[len - 1] = '\0';
      i = FindArgWithValue(ua, search_argument);
    }
  }

  if (i > 0) {
    // Scan slot list in ua->argv[i]
    StripTrailingJunk(ua->argv[i]);
    for (p = ua->argv[i]; p && *p; p = e) {
      // Check for list
      e = strchr(p, ',');
      if (e) { *e++ = 0; }
      // Check for range
      h = strchr(p, '-'); /* range? */
      if (h == p) {
        msg = T_("Negative numbers not permitted\n");
        goto bail_out;
      }
      if (h) {
        *h++ = 0;
        if (!IsAnInteger(h)) {
          msg = T_("Range end is not integer.\n");
          goto bail_out;
        }
        SkipSpaces(&p);
        if (!IsAnInteger(p)) {
          msg = T_("Range start is not an integer.\n");
          goto bail_out;
        }
        beg = atoi(p);
        end = atoi(h);
        if (end < beg) {
          msg = T_("Range end not bigger than start.\n");
          goto bail_out;
        }
      } else {
        SkipSpaces(&p);
        if (!IsAnInteger(p)) {
          msg = T_("Input value is not an integer.\n");
          goto bail_out;
        }
        beg = end = atoi(p);
      }
      if (beg <= 0 || end <= 0) {
        msg = T_("Values must be be greater than zero.\n");
        goto bail_out;
      }
      if (end > num_slots) {
        msg = T_("Slot too large.\n");
        goto bail_out;
      }

      // Turn on specified range
      for (i = beg; i <= end; i++) { SetBit(i - 1, slot_list); }
    }
  } else {
    // Turn everything on
    for (i = 1; i <= num_slots; i++) { SetBit(i - 1, slot_list); }
  }

  if (debug_level >= 100) {
    Dmsg0(100, "Slots turned on:\n");
    for (i = 1; i <= num_slots; i++) {
      if (BitIsSet(i - 1, slot_list)) { Dmsg1(100, "%d\n", i); }
    }
  }

  return true;

bail_out:
  Dmsg1(100, "Problem with user selection ERR=%s\n", msg);

  return false;
}

static int GetParsedJobType(std::string jobtype_argument)
{
  int i;
  int return_jobtype = -1;
  if (jobtype_argument.size() == 1) {
    for (i = 0; jobtypes[i].job_type; i++) {
      if (jobtype_argument[0] == static_cast<char>(jobtypes[i].job_type)) {
        break;
      }
    }
  } else {
    for (i = 0; jobtypes[i].name; i++) {
      if (jobtypes[i].name == jobtype_argument) { break; }
    }
  }
  if (jobtypes[i].name) { return_jobtype = jobtypes[i].job_type; }

  return return_jobtype;
}

bool GetUserJobTypeListSelection(UaContext* ua,
                                 std::vector<char>& passed_jobtypes,
                                 bool ask_user)
{
  char jobtype_argument[MAX_NAME_LENGTH];
  int argument;

  if ((argument = FindArgWithValue(ua, NT_("jobtype"))) >= 0) {
    bstrncpy(jobtype_argument, ua->argv[argument], sizeof(jobtype_argument));
  } else if (ask_user) {
    StartPrompt(ua, T_("Jobtype:\n"));
    for (int i = 0; jobtypes[i].name; i++) { AddPrompt(ua, jobtypes[i].name); }

    if (DoPrompt(ua, T_("JobType"), T_("Select Job Type"), jobtype_argument,
                 sizeof(jobtype_argument))
        < 0) {
      return false;
    }
  } else {
    return true;
  }

  char delimiter = ',';
  std::vector<std::string> split_jobtypes;
  if (strchr(jobtype_argument, delimiter) != nullptr) {
    split_jobtypes = split_string(jobtype_argument, delimiter);
  } else {
    split_jobtypes.push_back(jobtype_argument);
  }

  for (auto& jobtype : split_jobtypes) {
    int type = GetParsedJobType(jobtype);
    if (type == -1) {
      ua->WarningMsg(T_("Illegal jobtype %s\n"), jobtype.c_str());
      return false;
    }
    passed_jobtypes.push_back(type);
  }

  return true;
}

bool GetUserJobStatusSelection(UaContext* ua, std::vector<char>& jobstatuslist)
{
  int i;

  if ((i = FindArgWithValue(ua, NT_("jobstatus"))) >= 0) {
    if (strlen(ua->argv[i]) > 0) {
      std::vector<std::string> jobstatusinput_list;
      jobstatusinput_list = split_string(ua->argv[i], ',');

      for (auto& jobstatus : jobstatusinput_list) {
        if (strlen(jobstatus.c_str()) == 1 && jobstatus.c_str()[0] >= 'A'
            && jobstatus.c_str()[0] <= 'z') {
          jobstatuslist.push_back(jobstatus[0]);
        } else if (Bstrcasecmp(jobstatus.c_str(), "terminated")) {
          jobstatuslist.push_back(JS_Terminated);
        } else if (Bstrcasecmp(jobstatus.c_str(), "warnings")) {
          jobstatuslist.push_back(JS_Warnings);
        } else if (Bstrcasecmp(jobstatus.c_str(), "canceled")) {
          jobstatuslist.push_back(JS_Canceled);
        } else if (Bstrcasecmp(jobstatus.c_str(), "running")) {
          jobstatuslist.push_back(JS_Running);
        } else if (Bstrcasecmp(jobstatus.c_str(), "error")) {
          jobstatuslist.push_back(JS_ErrorTerminated);
        } else if (Bstrcasecmp(jobstatus.c_str(), "fatal")) {
          jobstatuslist.push_back(JS_FatalError);
        } else {
          /* invalid jobstatus */
          return false;
        }
      }
    } else {
      return false;
    }
  }
  return true;
}

bool GetUserJobLevelSelection(UaContext* ua, std::vector<char>& joblevel_list)
{
  int i;

  if (((i = FindArgWithValue(ua, NT_("joblevel"))) >= 0)
      || ((i = FindArgWithValue(ua, NT_("level"))) >= 0)) {
    std::vector<std::string> joblevelinput_list
        = split_string(ua->argv[i], ',');

    for (const auto& level : joblevelinput_list) {
      if (level.size() == 1 && level[0] >= 'A' && level[0] <= 'z') {
        joblevel_list.push_back(level[0]);
      } else if (Bstrcasecmp(level.c_str(), "Full")) {
        joblevel_list.push_back('F');
      } else if (Bstrcasecmp(level.c_str(), "Differential")) {
        joblevel_list.push_back('D');
      } else if (Bstrcasecmp(level.c_str(), "Incremental")) {
        joblevel_list.push_back('I');
      } else {
        /* invalid joblevel */
        return false;
      }
    }
  }
  return true;
}
} /* namespace directordaemon */
