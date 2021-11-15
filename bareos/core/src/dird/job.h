/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2020 Bareos GmbH & Co. KG

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

class JobControlRecord;

namespace directordaemon {

class UaContext;
class JobResource;
class UnifiedStorageResource;
class RunResource;

bool AllowDuplicateJob(JobControlRecord* jcr);
JobControlRecord* NewDirectorJcr();
void SetJcrDefaults(JobControlRecord* jcr, JobResource* job);
void CreateUniqueJobName(JobControlRecord* jcr, const char* base_name);
void UpdateJobEndRecord(JobControlRecord* jcr);
bool GetOrCreateClientRecord(JobControlRecord* jcr);
bool GetOrCreateFilesetRecord(JobControlRecord* jcr);
DBId_t GetOrCreatePoolRecord(JobControlRecord* jcr, char* pool_name);
bool GetLevelSinceTime(JobControlRecord* jcr);
void ApplyPoolOverrides(JobControlRecord* jcr, bool force = false);
JobId_t RunJob(JobControlRecord* jcr);
bool CancelJob(UaContext* ua, JobControlRecord* jcr);
void GetJobStorage(UnifiedStorageResource* store,
                   JobResource* job,
                   RunResource* run);
void InitJcrJobRecord(JobControlRecord* jcr);
void UpdateJobEnd(JobControlRecord* jcr, int TermCode);
bool SetupJob(JobControlRecord* jcr, bool suppress_output = false);
void ExecuteJob(JobControlRecord* jcr);
void CreateClones(JobControlRecord* jcr);
int CreateRestoreBootstrapFile(JobControlRecord* jcr);
void DirdFreeJcr(JobControlRecord* jcr);
void DirdFreeJcrPointers(JobControlRecord* jcr);
void CancelStorageDaemonJob(JobControlRecord* jcr);
bool RunConsoleCommand(JobControlRecord* jcr, const char* cmd);
void SdMsgThreadSendSignal(JobControlRecord* jcr, int sig);
void InitJobServer(int max_workers);
void TermJobServer();

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_JOB_H_
