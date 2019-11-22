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
#include "dird/dird_conf.h"
#include "dird/get_database_connection.h"
#include "dird/job.h"
#include "dird/run_on_incoming_connect_interval.h"
#include "dird/scheduler.h"

#include <utility>

namespace directordaemon {

RunOnIncomingConnectInterval::RunOnIncomingConnectInterval(
    std::string client_name)
    : client_name_(std::move(client_name))
    , scheduler_(Scheduler::GetMainScheduler())
{
  // initialize database by job settings
}

RunOnIncomingConnectInterval::RunOnIncomingConnectInterval(
    std::string client_name,
    Scheduler& scheduler,
    BareosDb* db)
    : client_name_(std::move(client_name)), scheduler_(scheduler), db_(db)
{
  // constructor used for tests to inject mocked scheduler and database
}

time_t RunOnIncomingConnectInterval::FindLastJobStart(JobResource* job)
{
  JobControlRecord* jcr = NewDirectorJcr();
  SetJcrDefaults(jcr, job);
  auto db = db_ != nullptr ? db_ : GetDatabaseConnection(jcr);
  if (db == nullptr) {
    Dmsg0(200, "Could not retrieve a database connection.");
    return 0;
  }
  jcr->db = db;

  std::vector<char> stime(MAX_NAME_LENGTH);

  BareosDb::SqlFindResult result = jcr->db->FindLastJobStartTimeForJobAndClient(
      jcr, job->resource_name_, client_name_, stime.data());

  time_t time{-1};

  switch (result) {
    case BareosDb::SqlFindResult::kSuccess: {
      time_t time_converted = static_cast<time_t>(StrToUtime(stime.data()));
      time = time_converted == 0 ? -1 : time_converted;
      break;
    }
    case BareosDb::SqlFindResult::kEmptyResultSet:
      time = 0;
      break;
    default:
    case BareosDb::SqlFindResult::kError:
      time = -1;
      break;
  }

  FreeJcr(jcr);

  return time;
}

void RunOnIncomingConnectInterval::RunJobIfIntervalExceeded(
    JobResource* job,
    time_t last_start_time)
{
  using std::chrono::duration_cast;
  using std::chrono::hours;
  using std::chrono::seconds;
  using std::chrono::system_clock;

  bool interval_time_exceeded = false;
  bool job_ran_before = last_start_time != 0;

  if (job_ran_before) {
    auto timepoint_now = system_clock::now();
    auto timepoint_last_start = system_clock::from_time_t(last_start_time);

    auto diff_seconds =
        duration_cast<seconds>(timepoint_now - timepoint_last_start).count();

    auto maximum_interval_seconds =
        seconds(job->RunOnIncomingConnectInterval).count();

    interval_time_exceeded = diff_seconds > maximum_interval_seconds;
  }

  if (!job_ran_before || interval_time_exceeded) {
    Dmsg1(800, "Add job %s to scheduler queue.", job->resource_name_);
    scheduler_.AddJobWithNoRunResourceToQueue(job);
  }
}

void RunOnIncomingConnectInterval::Run()
{
  std::vector<JobResource*> job_resources =
      GetAllJobResourcesByClientName(client_name_);

  if (job_resources.empty()) { return; }

  for (auto job : job_resources) {
    if (job->RunOnIncomingConnectInterval != 0) {
      time_t last_start_time = FindLastJobStart(job);
      if (last_start_time != -1) {
        Dmsg2(800, "Try RunOnIncomingConnectInterval job %s for client %s.",
              job->resource_name_, client_name_.c_str());
        RunJobIfIntervalExceeded(job, last_start_time);
      }
    }
  }
}

}  // namespace directordaemon
