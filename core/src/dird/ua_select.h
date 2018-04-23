/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

StoreResource *select_storage_resource(UaContext *ua, bool autochanger_only = false);
JobResource *select_job_resource(UaContext *ua);
JobResource *select_enable_disable_job_resource(UaContext *ua, bool enable);
JobResource *select_restore_job_resource(UaContext *ua);
ClientResource *select_client_resource(UaContext *ua);
ClientResource *select_enable_disable_client_resource(UaContext *ua, bool enable);
FilesetResource *select_fileset_resource(UaContext *ua);
ScheduleResource *select_enable_disable_schedule_resource(UaContext *ua, bool enable);
bool select_pool_and_media_dbr(UaContext *ua, PoolDbRecord *pr, MediaDbRecord *mr);
bool select_media_dbr(UaContext *ua, MediaDbRecord *mr);
bool select_pool_dbr(UaContext *ua, PoolDbRecord *pr, const char *argk = "pool");
bool select_storage_dbr(UaContext *ua, StorageDbRecord *pr, const char *argk = "storage");
bool select_client_dbr(UaContext *ua, ClientDbRecord *cr);

void start_prompt(UaContext *ua, const char *msg);
void add_prompt(UaContext *ua, const char *prompt);
int do_prompt(UaContext *ua, const char *automsg, const char *msg, char *prompt, int max_prompt);
CatalogResource *get_catalog_resource(UaContext *ua);
StoreResource *get_storage_resource(UaContext *ua, bool use_default = false,
                               bool autochanger_only = false);
drive_number_t get_storage_drive(UaContext *ua, StoreResource *store);
slot_number_t get_storage_slot(UaContext *ua, StoreResource *store);
int get_media_type(UaContext *ua, char *MediaType, int max_media);
bool get_pool_dbr(UaContext *ua, PoolDbRecord *pr, const char *argk = "pool");
bool get_storage_dbr(UaContext *ua, StorageDbRecord *sr, const char *argk = "storage");
bool get_client_dbr(UaContext *ua, ClientDbRecord *cr);
PoolResource *get_pool_resource(UaContext *ua);
JobResource *get_restore_job(UaContext *ua);
PoolResource *select_pool_resource(UaContext *ua);
alist *select_jobs(UaContext *ua, const char *reason);
ClientResource *get_client_resource(UaContext *ua);
int get_job_dbr(UaContext *ua, JobDbRecord *jr);
bool get_user_slot_list(UaContext *ua, char *slot_list, const char *argument, int num_slots);
bool get_user_job_type_selection(UaContext *ua, int *jobtype);
bool get_user_job_status_selection(UaContext *ua, int *jobstatus);
bool get_user_job_level_selection(UaContext *ua, int *joblevel);

int find_arg_keyword(UaContext *ua, const char **list);
int find_arg(UaContext *ua, const char *keyword);
int find_arg_with_value(UaContext *ua, const char *keyword);
int do_keyword_prompt(UaContext *ua, const char *msg, const char **list);
bool confirm_retention(UaContext *ua, utime_t *ret, const char *msg);
bool get_level_from_name(JobControlRecord *jcr, const char *level_name);


#endif // BAREOS_DIRD_UA_SELECT_H_
