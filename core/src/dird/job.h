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

#ifndef BAREOS_DIRD_JOB_H_
#define BAREOS_DIRD_JOB_H_

bool allow_duplicate_job(JobControlRecord *jcr);
void set_jcr_defaults(JobControlRecord *jcr, JobResource *job);
void create_unique_job_name(JobControlRecord *jcr, const char *base_name);
void update_job_end_record(JobControlRecord *jcr);
bool get_or_create_client_record(JobControlRecord *jcr);
bool get_or_create_fileset_record(JobControlRecord *jcr);
DBId_t get_or_create_pool_record(JobControlRecord *jcr, char *pool_name);
bool get_level_since_time(JobControlRecord *jcr);
void apply_pool_overrides(JobControlRecord *jcr, bool force = false);
JobId_t run_job(JobControlRecord *jcr);
bool cancel_job(UaContext *ua, JobControlRecord *jcr);
void get_job_storage(UnifiedStoreResource *store, JobResource *job, RunResource *run);
void init_jcr_job_record(JobControlRecord *jcr);
void update_job_end(JobControlRecord *jcr, int TermCode);
bool setup_job(JobControlRecord *jcr, bool suppress_output = false);
void create_clones(JobControlRecord *jcr);
int create_restore_bootstrap_file(JobControlRecord *jcr);
void dird_free_jcr(JobControlRecord *jcr);
void dird_free_jcr_pointers(JobControlRecord *jcr);
void cancel_storage_daemon_job(JobControlRecord *jcr);
bool run_console_command(JobControlRecord *jcr, const char *cmd);
void sd_msg_thread_send_signal(JobControlRecord *jcr, int sig);

#endif // BAREOS_DIRD_JOB_H_
