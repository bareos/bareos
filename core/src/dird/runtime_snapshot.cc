/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "dird/runtime_snapshot.h"

#include "lib/util.h"

namespace {
constexpr int kRuntimeFieldSize = 1024;
constexpr int kRuntimePathSize = 4096;
}  // namespace

namespace directordaemon {

bool ParseStorageRuntimeSnapshotRecord(const char* record,
                                       JobId_t* jobid,
                                       StorageRuntimeSnapshot* snapshot)
{
  if (!record || !jobid || !snapshot) { return false; }

  unsigned int parsed_jobid = 0;
  char jobstatus = 0;
  long long sample_time = 0;
  unsigned int job_files = 0;
  unsigned long long job_bytes = 0;
  unsigned int average_rate = 0;
  unsigned int last_rate = 0;
  int spooling = 0;
  int despooling = 0;
  int despool_wait = 0;
  char read_device[kRuntimeFieldSize];
  char write_device[kRuntimeFieldSize];
  char read_volume[kRuntimeFieldSize];
  char write_volume[kRuntimeFieldSize];
  char pool_name[kRuntimeFieldSize];
  char current_file[kRuntimePathSize];

  read_device[0] = write_device[0] = read_volume[0] = write_volume[0] = '\0';
  pool_name[0] = current_file[0] = '\0';

  if (sscanf(record,
             "JobId=%u JobStatus=%c SampleTime=%lld JobFiles=%u "
             "JobBytes=%llu AverageRate=%u LastRate=%u Spooling=%d "
             "Despooling=%d DespoolWait=%d ReadDevice=%1023s "
             "WriteDevice=%1023s ReadVolume=%1023s WriteVolume=%1023s "
             "Pool=%1023s CurrentFile=%4095s",
             &parsed_jobid, &jobstatus, &sample_time, &job_files, &job_bytes,
             &average_rate, &last_rate, &spooling, &despooling, &despool_wait,
             read_device, write_device, read_volume, write_volume, pool_name,
             current_file)
      != 16) {
    return false;
  }

  UnbashSpaces(read_device);
  UnbashSpaces(write_device);
  UnbashSpaces(read_volume);
  UnbashSpaces(write_volume);
  UnbashSpaces(pool_name);
  UnbashSpaces(current_file);

  *jobid = parsed_jobid;
  snapshot->sample_time = sample_time;
  snapshot->job_files = job_files;
  snapshot->job_bytes = job_bytes;
  snapshot->average_rate = average_rate;
  snapshot->last_rate = last_rate;
  snapshot->job_status = jobstatus;
  snapshot->spooling = spooling;
  snapshot->despooling = despooling;
  snapshot->despool_wait = despool_wait;
  snapshot->read_device = read_device;
  snapshot->write_device = write_device;
  snapshot->read_volume = read_volume;
  snapshot->write_volume = write_volume;
  snapshot->pool_name = pool_name;
  snapshot->current_file = current_file;
  snapshot->valid = true;
  return true;
}

}  // namespace directordaemon
