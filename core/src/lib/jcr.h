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

DLL_IMP_EXP void InitLastJobsList();
DLL_IMP_EXP void TermLastJobsList();
DLL_IMP_EXP void LockLastJobsList();
DLL_IMP_EXP void UnlockLastJobsList();
DLL_IMP_EXP bool ReadLastJobsList(int fd, uint64_t addr);
DLL_IMP_EXP uint64_t WriteLastJobsList(int fd, uint64_t addr);
DLL_IMP_EXP void WriteStateFile(char *dir, const char *progname, int port);
DLL_IMP_EXP void RegisterJobEndCallback(JobControlRecord *jcr, void JobEndCb(JobControlRecord *jcr,void *), void *ctx);
DLL_IMP_EXP void LockJobs();
DLL_IMP_EXP void UnlockJobs();
DLL_IMP_EXP JobControlRecord *jcr_walk_start();
DLL_IMP_EXP JobControlRecord *jcr_walk_next(JobControlRecord *prev_jcr);
DLL_IMP_EXP void JcrWalkEnd(JobControlRecord *jcr);
DLL_IMP_EXP int JobCount();
DLL_IMP_EXP JobControlRecord *get_jcr_from_tsd();
DLL_IMP_EXP void SetJcrInTsd(JobControlRecord *jcr);
DLL_IMP_EXP void RemoveJcrFromTsd(JobControlRecord *jcr);
DLL_IMP_EXP uint32_t GetJobidFromTsd();
DLL_IMP_EXP uint32_t GetJobidFromTid(pthread_t tid);

#endif // BAREOS_LIB_JCR_H_
