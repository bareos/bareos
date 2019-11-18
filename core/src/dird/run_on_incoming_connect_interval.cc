/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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

#include "include/bareos.h"
#include "cats/cats.h"
#include "dird/run_on_incoming_connect_interval.h"
#include "dird/dird_conf.h"
#include "dird/get_database_connection.h"
#include "dird/job.h"
#include "dird/scheduler.h"

namespace directordaemon {

static time_t FindLastJobStart(JobResource* job, const std::string& client_name)
{
  JobControlRecord* jcr = NewDirectorJcr();
  SetJcrDefaults(jcr, job);
  jcr->db = GetDatabaseConnection(jcr);

  POOLMEM* stime = GetPoolMemory(PM_MESSAGE);

  bool success = jcr->db->FindLastStartTimeForJobAndClient(
      jcr, job->resource_name_, client_name, stime);

  time_t time = 0;
  if (success) { time = static_cast<time_t>(StrToUtime(stime)); }

  FreeJcr(jcr);

  return time;
}

static void RunJobIfIntervalExceeded(JobResource* job,
                                     time_t last_start_time,
                                     Scheduler& scheduler)
{
  using std::chrono::duration_cast;
  using std::chrono::hours;
  using std::chrono::seconds;
  using std::chrono::system_clock;

  bool interval_time_exceeded = false;
  bool job_run_before = last_start_time != 0;

  if (job_run_before) {
    auto timepoint_now = system_clock::now();
    auto timepoint_last_start = system_clock::from_time_t(last_start_time);

    auto diff_seconds =
        duration_cast<seconds>(timepoint_now - timepoint_last_start).count();

    auto maximum_interval_seconds =
        seconds(job->RunOnIncomingConnectInterval).count();

    interval_time_exceeded = diff_seconds > maximum_interval_seconds;
  }

  if (job_run_before == false || interval_time_exceeded) {
    scheduler.AddJobWithNoRunResourceToQueue(job);
  }
}

void RunOnIncomingConnectInterval(std::string client_name, Scheduler& scheduler)
{
  std::vector<JobResource*> job_resources =
      GetAllJobResourcesByClientName(client_name.c_str());

  if (job_resources.empty()) { return; }

  for (auto job : job_resources) {
    if (job->RunOnIncomingConnectInterval != 0) {
      time_t last_start_time = FindLastJobStart(job, client_name);
      RunJobIfIntervalExceeded(job, last_start_time, scheduler);
    }
  }
}

}  // namespace directordaemon
