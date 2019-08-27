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

void WriteStateFile(char* dir, const char* progname, int port);
void RegisterJobEndCallback(JobControlRecord* jcr,
                            void JobEndCb(JobControlRecord* jcr, void*),
                            void* ctx);
void LockJobs();
void UnlockJobs();

void LockJcrChain();
void UnlockJcrChain();

JobControlRecord* jcr_walk_start();
JobControlRecord* jcr_walk_next(JobControlRecord* prev_jcr);
void JcrWalkEnd(JobControlRecord* jcr);
int JobCount();
void InitJcrChain();
void CleanupJcrChain();

#endif  // BAREOS_LIB_JCR_H_
