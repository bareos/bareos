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
#ifndef BAREOS_LIB_JCR_H_
#define BAREOS_LIB_JCR_H_
class JobControlRecord;

DLL_IMP_EXP void init_last_jobs_list();
DLL_IMP_EXP void term_last_jobs_list();
DLL_IMP_EXP void lock_last_jobs_list();
DLL_IMP_EXP void unlock_last_jobs_list();
DLL_IMP_EXP bool read_last_jobs_list(int fd, uint64_t addr);
DLL_IMP_EXP uint64_t write_last_jobs_list(int fd, uint64_t addr);
DLL_IMP_EXP void write_state_file(char *dir, const char *progname, int port);
DLL_IMP_EXP void register_job_end_callback(JobControlRecord *jcr, void job_end_cb(JobControlRecord *jcr,void *), void *ctx);
DLL_IMP_EXP void lock_jobs();
DLL_IMP_EXP void unlock_jobs();
DLL_IMP_EXP JobControlRecord *jcr_walk_start();
DLL_IMP_EXP JobControlRecord *jcr_walk_next(JobControlRecord *prev_jcr);
DLL_IMP_EXP void jcr_walk_end(JobControlRecord *jcr);
DLL_IMP_EXP int job_count();
DLL_IMP_EXP JobControlRecord *get_jcr_from_tsd();
DLL_IMP_EXP void set_jcr_in_tsd(JobControlRecord *jcr);
DLL_IMP_EXP void remove_jcr_from_tsd(JobControlRecord *jcr);
DLL_IMP_EXP uint32_t get_jobid_from_tsd();
DLL_IMP_EXP uint32_t get_jobid_from_tid(pthread_t tid);

#endif // BAREOS_LIB_JCR_H_
