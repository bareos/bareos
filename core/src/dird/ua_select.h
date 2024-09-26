/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_UA_SELECT_H_
#define BAREOS_DIRD_UA_SELECT_H_

#include "dird/ua.h"
template <typename T> class alist;

struct PoolDbRecord;
struct MediaDbRecord;
struct StorageDbRecord;
struct ClientDbRecord;
struct JobDbRecord;

namespace directordaemon {

StorageResource* select_storage_resource(UaContext* ua,
                                         bool autochanger_only = false);
JobResource* select_job_resource(UaContext* ua);
JobResource* select_enable_disable_job_resource(UaContext* ua, bool enable);
JobResource* select_restore_job_resource(UaContext* ua);
ClientResource* select_client_resource(UaContext* ua);
ClientResource* select_enable_disable_client_resource(UaContext* ua,
                                                      bool enable);
FilesetResource* select_fileset_resource(UaContext* ua);
ScheduleResource* select_enable_disable_schedule_resource(UaContext* ua,
                                                          bool enable);

bool SelectPoolForMediaDbr(UaContext* ua, PoolDbRecord* pr, MediaDbRecord* mr);
bool SelectMediaDbr(UaContext* ua, MediaDbRecord* mr);
bool SelectMediaDbrByName(UaContext* ua,
                          MediaDbRecord* mr,
                          const char* volumename);
bool SelectPoolDbr(UaContext* ua, PoolDbRecord* pr, const char* argk = "pool");
bool SelectStorageDbr(UaContext* ua,
                      StorageDbRecord* pr,
                      const char* argk = "storage");
bool SelectClientDbr(UaContext* ua, ClientDbRecord* cr);

void StartPrompt(UaContext* ua, const char* msg);
void AddPrompt(UaContext* ua, const char* prompt);
void AddPrompt(UaContext* ua, std::string&& prompt);
std::string FormatPrompts(const UaContext* ua,
                          int window_width,
                          int min_lines_threshold);
int DoPrompt(UaContext* ua,
             const char* automsg,
             const char* msg,
             char* prompt,
             int max_prompt);
CatalogResource* get_catalog_resource(UaContext* ua);
StorageResource* get_storage_resource(UaContext* ua,
                                      bool use_default = false,
                                      bool autochanger_only = false);
drive_number_t GetStorageDrive(UaContext* ua, StorageResource* store);
slot_number_t GetStorageSlot(UaContext* ua, StorageResource* store);
int GetMediaType(UaContext* ua, char* MediaType, int max_media);
bool GetPoolDbr(UaContext* ua, PoolDbRecord* pr, const char* argk = "pool");
bool GetStorageDbr(UaContext* ua,
                   StorageDbRecord* sr,
                   const char* argk = "storage");
bool GetClientDbr(UaContext* ua, ClientDbRecord* cr);
PoolResource* get_pool_resource(UaContext* ua);
JobResource* get_restore_job(UaContext* ua);
PoolResource* select_pool_resource(UaContext* ua);
alist<JobId_t*>* select_jobs(UaContext* ua, const char* reason);
ClientResource* get_client_resource(UaContext* ua);
int GetJobDbr(UaContext* ua, JobDbRecord* jr);
bool GetUserSlotList(UaContext* ua,
                     char* slot_list,
                     const char* argument,
                     int num_slots);
bool GetUserJobTypeListSelection(UaContext* ua,
                                 std::vector<char>& passed_jobtypes,
                                 bool ask_user);
bool GetUserJobStatusSelection(UaContext* ua, std::vector<char>& jobstatus);
bool GetUserJobLevelSelection(UaContext* ua, std::vector<char>& joblevel_list);

int FindArgKeyword(UaContext* ua, const char** list);
int FindArg(UaContext* ua, const char* keyword);
int FindArgWithValue(UaContext* ua, const char* keyword);
const char* GetArgValue(UaContext* ua, const char* keyword);
int FindClientArgFromDatabase(UaContext* ua);
int DoKeywordPrompt(UaContext* ua, const char* msg, const char** list);
bool ConfirmRetention(UaContext* ua, utime_t* ret, const char* msg);
bool GetLevelFromName(JobControlRecord* jcr, const char* level_name);


} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_SELECT_H_
