/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_LAST_JOBS_LIST_H
#define BAREOS_LIB_LAST_JOBS_LIST_H

struct s_last_job {
  int32_t Errors = 0; /** FD/SD errors */
  int32_t JobType = 0;
  int32_t JobStatus = 0;
  int32_t JobLevel = 0;
  uint32_t JobId = 0;
  uint32_t VolSessionId = 0;
  uint32_t VolSessionTime = 0;
  uint32_t JobFiles = 0;
  uint64_t JobBytes = 0;
  utime_t start_time = 0;
  utime_t end_time = 0;
  char Job[MAX_NAME_LENGTH]{0};
};

void TermLastJobsList();
void LockLastJobsList();
void UnlockLastJobsList();

bool ReadLastJobsList(int fd, uint64_t addr);
uint64_t WriteLastJobsList(int fd, uint64_t addr);

void SetLastJobsStatistics(JobControlRecord* jcr);

std::size_t LastJobsCount();
std::size_t LastJobsEmpty();
std::vector<s_last_job*> GetLastJobsList();
s_last_job GetLastJob();

#endif  // BAREOS_LIB_LAST_JOBS_LIST_H
