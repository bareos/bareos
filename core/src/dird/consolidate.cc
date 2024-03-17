/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
// Philipp Storz, May 2016
/** @file
 * responsible for doing consolidation jobs
 *
 * Basic tasks done here:
 *   run a virtual full job for all jobs that are configured to be always
 * incremental based on admin.c
 *
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/consolidate.h"
#include "dird/director_jcr_impl.h"
#include "dird/job.h"
#include "dird/storage.h"
#include "dird/ua_input.h"
#include "dird/ua_server.h"
#include "dird/ua_run.h"
#include "dird/dird_globals.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/version.h"

namespace directordaemon {

static const int debuglevel = 100;

bool DoConsolidateInit(JobControlRecord* jcr)
{
  FreeRstorage(jcr);
  if (!AllowDuplicateJob(jcr)) { return false; }
  return true;
}

/**
 * Start a Virtual(Full) Job that creates a new virtual backup
 * containing the jobids given in jcr->dir_impl_->vf_jobids
 */
static inline void StartNewConsolidationJob(const JobResource* consolidate_job,
                                            JobControlRecord* jcr,
                                            char* jobname)
{
  JobId_t jobid;
  UaContext* ua;
  PoolMem cmd(PM_MESSAGE);

  ua = new_ua_context(jcr);
  ua->batch = true;
  Mmsg(ua->cmd,
       "run job=\"%s\" jobid=%s level=VirtualFull priority=%d accurate=%s "
       "spooldata=%s ignoreduplicatecheck=yes consolidatejob=%s",
       jobname, jcr->dir_impl->vf_jobids, jcr->JobPriority,
       jcr->accurate ? "yes" : "no", jcr->dir_impl->spool_data ? "yes" : "no",
       consolidate_job->resource_name_);

  Dmsg1(debuglevel, "=============== consolidate cmd=%s\n", ua->cmd);
  ParseUaArgs(ua); /* parse command */

  jobid = DoRunCmd(ua, ua->cmd);
  if (jobid == 0) {
    Jmsg(jcr, M_ERROR, 0, T_("Could not start %s job.\n"),
         jcr->get_OperationName());
  } else {
    Jmsg(jcr, M_INFO, 0, T_("%s JobId %d started.\n"), jcr->get_OperationName(),
         (int)jobid);
  }

  FreeUaContext(ua);
}

static bool ConsolidateJobs(JobControlRecord* jcr)
{
  const int32_t max_full_consolidations
      = jcr->dir_impl->res.job->MaxFullConsolidations;
  JobResource* const consolidate_job = jcr->dir_impl->res.job;
  int32_t fullconsolidations_started = 0;
  JobResource* job;
  time_t now = time(NULL);
  foreach_res (job, R_JOB) {
    if (job->AlwaysIncremental) {
      Jmsg(jcr, M_INFO, 0, T_("Looking at always incremental job %s\n"),
           job->resource_name_);

      // Fake always incremental job as job of current jcr.
      jcr->dir_impl->res.job = job;
      jcr->dir_impl->res.fileset = job->fileset;
      jcr->dir_impl->res.client = job->client;
      jcr->dir_impl->jr.JobLevel = L_INCREMENTAL;
      jcr->dir_impl->jr.limit = 0;
      jcr->dir_impl->jr.StartTime = 0;

      if (!GetOrCreateFilesetRecord(jcr)) {
        Jmsg(jcr, M_FATAL, 0, T_("JobId=%d no FileSet\n"), (int)jcr->JobId);
        return false;
      }

      if (!GetOrCreateClientRecord(jcr)) {
        Jmsg(jcr, M_FATAL, 0, T_("JobId=%d no ClientId\n"), (int)jcr->JobId);
        return false;
      }

      // First determine the number of total incrementals
      db_list_ctx all_jobids_ctx;
      jcr->db->AccurateGetJobids(jcr, &jcr->dir_impl->jr, &all_jobids_ctx);
      Dmsg1(10, "unlimited jobids list:  %s.\n",
            all_jobids_ctx.GetAsString().c_str());

      /* If we are doing always incremental, we need to limit the search to
       * only include incrementals that are older than (now -
       * AlwaysIncrementalJobRetention) */
      if (job->AlwaysIncrementalJobRetention) {
        char sdt[50];

        jcr->dir_impl->jr.StartTime = now - job->AlwaysIncrementalJobRetention;
        bstrftimes(sdt, sizeof(sdt), jcr->dir_impl->jr.StartTime);
        Jmsg(jcr, M_INFO, 0,
             T_("%s: considering jobs older than %s for consolidation.\n"),
             job->resource_name_, sdt);
        Dmsg4(10,
              T_("%s: considering jobs with ClientId %d and FilesetId %d older "
                 "than %s for consolidation.\n"),
              job->resource_name_, jcr->dir_impl->jr.ClientId,
              jcr->dir_impl->jr.FileSetId, sdt);
      }

      db_list_ctx jobids_ctx;
      jcr->db->AccurateGetJobids(jcr, &jcr->dir_impl->jr, &jobids_ctx);
      Dmsg1(10, "consolidate candidates:  %s.\n",
            jobids_ctx.GetAsString().c_str());

      const db_list_ctx zero_file_jobs
          = jcr->db->FilterZeroFileJobs(jobids_ctx);
      if (zero_file_jobs.size() > 0) {
        Jmsg(jcr, M_INFO, 0, "%s: purging empty jobids %s\n",
             job->resource_name_, zero_file_jobs.Join(", ").c_str());
        jcr->db->PurgeJobs(zero_file_jobs.GetAsString().c_str());
      }

      // all jobs - any empty jobs - the full backup
      const int32_t incrementals_total
          = all_jobids_ctx.size() - zero_file_jobs.size() - 1;
      /* Consolidation of zero or one job does not make sense, we leave it
       * like it is */
      if (incrementals_total < 1) {
        Jmsg(jcr, M_INFO, 0,
             T_("%s: less than two jobs to consolidate found, doing "
                "nothing.\n"),
             job->resource_name_);
        continue;
      }

      /* Calculate limit for query. We specify how many incrementals should be
       * left. the limit is total number of incrementals - number required - 1
       */
      Dmsg2(10, "Incrementals found/required. (%d/%d).\n", incrementals_total,
            job->AlwaysIncrementalKeepNumber);
      if (incrementals_total <= job->AlwaysIncrementalKeepNumber) {
        Jmsg(jcr, M_INFO, 0,
             T_("%s: less incrementals than required, not consolidating\n"),
             job->resource_name_);
        continue;
      }

      const int32_t max_incrementals_to_consolidate
          = incrementals_total - job->AlwaysIncrementalKeepNumber;

      jcr->dir_impl->jr.limit = max_incrementals_to_consolidate + 1;
      Dmsg3(10, "total: %d, to_consolidate: %d, limit: %d.\n",
            incrementals_total, max_incrementals_to_consolidate,
            jcr->dir_impl->jr.limit);
      jobids_ctx.clear();
      jcr->db->AccurateGetJobids(jcr, &jcr->dir_impl->jr, &jobids_ctx);
      const int32_t incrementals_to_consolidate = jobids_ctx.size() - 1;
      Dmsg2(10, "%d consolidate ids after limit: %s.\n", jobids_ctx.size(),
            jobids_ctx.GetAsString().c_str());
      if (incrementals_to_consolidate < 1) {
        Jmsg(jcr, M_INFO, 0,
             T_("%s: After limited query: less incrementals than required, "
                "not consolidating\n"),
             job->resource_name_);
        continue;
      }

      // Check if we need to skip the first (full) job from consolidation
      if (job->AlwaysIncrementalMaxFullAge) {
        char sdt_allowed[50];
        char sdt_starttime[50];
        time_t starttime, oldest_allowed_starttime;

        if (incrementals_to_consolidate < 2) {
          Jmsg(jcr, M_INFO, 0,
               T_("%s: less incrementals than required to consolidate without "
                  "full, not consolidating\n"),
               job->resource_name_);
          continue;
        }
        Jmsg(jcr, M_INFO, 0, T_("before ConsolidateFull: jobids: %s\n"),
             jobids_ctx.GetAsString().c_str());

        std::string oldestjobid = jobids_ctx.front();

        // Get db record of oldest jobid and check its age
        jcr->dir_impl->previous_jr = JobDbRecord{};
        jcr->dir_impl->previous_jr.JobId = std::stoul(oldestjobid);
        Dmsg1(10, "Previous JobId=%s\n", oldestjobid.c_str());

        if (!jcr->db->GetJobRecord(jcr, &jcr->dir_impl->previous_jr)) {
          Jmsg(jcr, M_FATAL, 0,
               T_("Error getting Job record for first Job: ERR=%s\n"),
               jcr->db->strerror());
          return true;
        }

        starttime = jcr->dir_impl->previous_jr.JobTDate;
        oldest_allowed_starttime = now - job->AlwaysIncrementalMaxFullAge;
        bstrftimes(sdt_allowed, sizeof(sdt_allowed), oldest_allowed_starttime);
        bstrftimes(sdt_starttime, sizeof(sdt_starttime), starttime);

        // Check if job is older than AlwaysIncrementalMaxFullAge
        Jmsg(jcr, M_INFO, 0, T_("check full age: full is %s, allowed is %s\n"),
             sdt_starttime, sdt_allowed);
        if (starttime > oldest_allowed_starttime) {
          Jmsg(jcr, M_INFO, 0,
               T_("Full is newer than AlwaysIncrementalMaxFullAge -> skipping "
                  "first jobid %s because of age\n"),
               oldestjobid.c_str());

          jobids_ctx.PopFront();

        } else if (max_full_consolidations
                   && fullconsolidations_started >= max_full_consolidations) {
          Jmsg(jcr, M_INFO, 0,
               T_("%d AlwaysIncrementalFullConsolidations reached -> skipping "
                  "first jobid %s independent of age\n"),
               max_full_consolidations, oldestjobid.c_str());

          jobids_ctx.PopFront();

        } else {
          Jmsg(jcr, M_INFO, 0,
               T_("Full is older than AlwaysIncrementalMaxFullAge -> also "
                  "consolidating Full jobid %s\n"),
               oldestjobid.c_str());
          fullconsolidations_started++;
        }
        Jmsg(jcr, M_INFO, 0, T_("after ConsolidateFull: jobids: %s\n"),
             jobids_ctx.GetAsString().c_str());
      }

      // Set the virtualfull jobids to be consolidated
      if (!jcr->dir_impl->vf_jobids) {
        jcr->dir_impl->vf_jobids = GetPoolMemory(PM_MESSAGE);
      }
      PmStrcpy(jcr->dir_impl->vf_jobids, jobids_ctx.GetAsString().c_str());

      Jmsg(jcr, M_INFO, 0, T_("%s: Start new consolidation\n"),
           job->resource_name_);
      StartNewConsolidationJob(consolidate_job, jcr, job->resource_name_);
    }
  }
  return true;
}

/**
 * The actual consolidation worker
 *
 * Returns: false on failure
 *          true  on success
 */
bool DoConsolidate(JobControlRecord* jcr)
{
  jcr->dir_impl->jr.JobId = jcr->JobId;
  jcr->dir_impl->fname = (char*)GetPoolMemory(PM_FNAME);

  // do not cancel virtual fulls started by consolidation
  jcr->dir_impl->IgnoreDuplicateJobChecking = true;

  // Print Job Start message
  Jmsg(jcr, M_INFO, 0, T_("Start Consolidate JobId %d, Job=%s\n"), jcr->JobId,
       jcr->Job);

  jcr->setJobStatusWithPriorityCheck(JS_Running);

  Resources tmpres = jcr->dir_impl->res;
  JobDbRecord tmpjr = jcr->dir_impl->jr;

  bool returnval = ConsolidateJobs(jcr);

  jcr->dir_impl->res = tmpres;
  jcr->dir_impl->jr = tmpjr;
  return returnval;
}

// Release resources allocated during backup.
void ConsolidateCleanup(JobControlRecord* jcr, int TermCode)
{
  int msg_type;
  char term_code[100];
  const char* TermMsg;
  char sdt[50], edt[50], schedt[50];

  Dmsg0(debuglevel, "Enter backup_cleanup()\n");

  UpdateJobEnd(jcr, TermCode);

  if (!jcr->db->GetJobRecord(jcr, &jcr->dir_impl->jr)) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Error getting Job record for Job report: ERR=%s\n"),
         jcr->db->strerror());
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
  }

  msg_type = M_INFO; /* by default INFO message */
  switch (jcr->getJobStatus()) {
    case JS_Terminated:
      TermMsg = T_("Consolidate OK");
      break;
    case JS_FatalError:
    case JS_ErrorTerminated:
      TermMsg = T_("*** Consolidate Error ***");
      msg_type = M_ERROR; /* Generate error message */
      break;
    case JS_Canceled:
      TermMsg = T_("Consolidate Canceled");
      break;
    default:
      TermMsg = term_code;
      sprintf(term_code, T_("Inappropriate term code: %c\n"),
              jcr->getJobStatus());
      break;
  }
  bstrftimes(schedt, sizeof(schedt), jcr->dir_impl->jr.SchedTime);
  bstrftimes(sdt, sizeof(sdt), jcr->dir_impl->jr.StartTime);
  bstrftimes(edt, sizeof(edt), jcr->dir_impl->jr.EndTime);

  Jmsg(jcr, msg_type, 0,
       T_("BAREOS %s (%s): %s\n"
          "  JobId:                  %d\n"
          "  Job:                    %s\n"
          "  Scheduled time:         %s\n"
          "  Start time:             %s\n"
          "  End time:               %s\n"
          "  Bareos binary info:     %s\n"
          "  Job triggered by:       %s\n"
          "  Termination:            %s\n\n"),
       kBareosVersionStrings.Full, kBareosVersionStrings.ShortDate, edt,
       jcr->dir_impl->jr.JobId, jcr->dir_impl->jr.Job, schedt, sdt, edt,
       kBareosVersionStrings.JoblogMessage,
       JobTriggerToString(jcr->dir_impl->job_trigger).c_str(), TermMsg);

  Dmsg0(debuglevel, "Leave ConsolidateCleanup()\n");
}
} /* namespace directordaemon */
