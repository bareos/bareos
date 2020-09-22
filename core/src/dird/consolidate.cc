/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2020 Bareos GmbH & Co. KG

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
/*
 * Philipp Storz, May 2016
 */
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
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/storage.h"
#include "dird/ua_input.h"
#include "dird/ua_server.h"
#include "dird/ua_run.h"
#include "dird/dird_globals.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"

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
 * containing the jobids given in jcr->impl_->vf_jobids
 */
static inline void StartNewConsolidationJob(JobControlRecord* jcr,
                                            char* jobname)
{
  JobId_t jobid;
  UaContext* ua;
  PoolMem cmd(PM_MESSAGE);

  ua = new_ua_context(jcr);
  ua->batch = true;
  Mmsg(ua->cmd, "run job=\"%s\" jobid=%s level=VirtualFull %s", jobname,
       jcr->impl->vf_jobids, jcr->accurate ? "accurate=yes" : "accurate=no");

  Dmsg1(debuglevel, "=============== consolidate cmd=%s\n", ua->cmd);
  ParseUaArgs(ua); /* parse command */

  jobid = DoRunCmd(ua, ua->cmd);
  if (jobid == 0) {
    Jmsg(jcr, M_ERROR, 0, _("Could not start %s job.\n"),
         jcr->get_OperationName());
  } else {
    Jmsg(jcr, M_INFO, 0, _("%s JobId %d started.\n"), jcr->get_OperationName(),
         (int)jobid);
  }

  FreeUaContext(ua);
}

/**
 * The actual consolidation worker
 *
 * Returns: false on failure
 *          true  on success
 */
bool DoConsolidate(JobControlRecord* jcr)
{
  char* p;
  JobResource* job;
  JobResource* tmpjob;
  bool retval = true;
  char* jobids = NULL;
  time_t now = time(NULL);
  int32_t fullconsolidations_started = 0;
  int32_t max_full_consolidations = 0;

  tmpjob = jcr->impl->res.job; /* Memorize job */

  /*
   * Get Value for MaxFullConsolidations from Consolidation job
   */
  max_full_consolidations = jcr->impl->res.job->MaxFullConsolidations;

  jcr->impl->jr.JobId = jcr->JobId;
  jcr->impl->fname = (char*)GetPoolMemory(PM_FNAME);

  /*
   * Print Job Start message
   */
  Jmsg(jcr, M_INFO, 0, _("Start Consolidate JobId %d, Job=%s\n"), jcr->JobId,
       jcr->Job);

  jcr->setJobStatus(JS_Running);

  foreach_res (job, R_JOB) {
    if (job->AlwaysIncremental) {
      db_list_ctx jobids_ctx;
      int32_t incrementals_total;
      int32_t incrementals_to_consolidate;
      int32_t max_incrementals_to_consolidate;

      Jmsg(jcr, M_INFO, 0, _("Looking at always incremental job %s\n"),
           job->resource_name_);

      /*
       * Fake always incremental job as job of current jcr.
       */
      jcr->impl->res.job = job;
      jcr->impl->res.fileset = job->fileset;
      jcr->impl->res.client = job->client;
      jcr->impl->jr.JobLevel = L_INCREMENTAL;
      jcr->impl->jr.limit = 0;
      jcr->impl->jr.StartTime = 0;

      if (!GetOrCreateFilesetRecord(jcr)) {
        Jmsg(jcr, M_FATAL, 0, _("JobId=%d no FileSet\n"), (int)jcr->JobId);
        retval = false;
        goto bail_out;
      }

      if (!GetOrCreateClientRecord(jcr)) {
        Jmsg(jcr, M_FATAL, 0, _("JobId=%d no ClientId\n"), (int)jcr->JobId);
        retval = false;
        goto bail_out;
      }

      /*
       * First determine the number of total incrementals
       */
      jcr->db->AccurateGetJobids(jcr, &jcr->impl->jr, &jobids_ctx);
      incrementals_total = jobids_ctx.size() - 1;
      Dmsg1(10, "unlimited jobids list:  %s.\n",
            jobids_ctx.GetAsString().c_str());

      /*
       * If we are doing always incremental, we need to limit the search to
       * only include incrementals that are older than (now -
       * AlwaysIncrementalJobRetention)
       */
      if (job->AlwaysIncrementalJobRetention) {
        char sdt[50];

        jcr->impl->jr.StartTime = now - job->AlwaysIncrementalJobRetention;
        bstrftimes(sdt, sizeof(sdt), jcr->impl->jr.StartTime);
        Jmsg(jcr, M_INFO, 0,
             _("%s: considering jobs older than %s for consolidation.\n"),
             job->resource_name_, sdt);
        Dmsg4(10,
              _("%s: considering jobs with ClientId %d and FilesetId %d older "
                "than %s for consolidation.\n"),
              job->resource_name_, jcr->impl->jr.ClientId,
              jcr->impl->jr.FileSetId, sdt);
      }

      jcr->db->AccurateGetJobids(jcr, &jcr->impl->jr, &jobids_ctx);
      Dmsg1(10, "consolidate candidates:  %s.\n",
            jobids_ctx.GetAsString().c_str());

      /**
       * Consolidation of zero or one job does not make sense, we leave it like
       * it is
       */
      if (incrementals_total < 1) {
        Jmsg(jcr, M_INFO, 0,
             _("%s: less than two jobs to consolidate found, doing nothing.\n"),
             job->resource_name_);
        continue;
      }

      /**
       * Calculate limit for query. We specify how many incrementals should be
       * left. the limit is total number of incrementals - number required - 1
       */
      max_incrementals_to_consolidate =
          incrementals_total - job->AlwaysIncrementalKeepNumber;

      Dmsg2(10, "Incrementals found/required. (%d/%d).\n", incrementals_total,
            job->AlwaysIncrementalKeepNumber);
      if ((max_incrementals_to_consolidate + 1) > 1) {
        jcr->impl->jr.limit = max_incrementals_to_consolidate + 1;
        Dmsg3(10, "total: %d, to_consolidate: %d, limit: %d.\n",
              incrementals_total, max_incrementals_to_consolidate,
              jcr->impl->jr.limit);
        jobids_ctx.clear();
        jcr->db->AccurateGetJobids(jcr, &jcr->impl->jr, &jobids_ctx);
        incrementals_to_consolidate = jobids_ctx.size() - 1;
        Dmsg2(10, "%d consolidate ids after limit: %s.\n", jobids_ctx.size(),
              jobids_ctx.GetAsString().c_str());
        if (incrementals_to_consolidate < 1) {
          Jmsg(jcr, M_INFO, 0,
               _("%s: After limited query: less incrementals than required, "
                 "not consolidating\n"),
               job->resource_name_);
          continue;
        }
      } else {
        Jmsg(jcr, M_INFO, 0,
             _("%s: less incrementals than required, not consolidating\n"),
             job->resource_name_);
        continue;
      }

      if (jobids) {
        free(jobids);
        jobids = NULL;
      }

      jobids = strdup(jobids_ctx.GetAsString().c_str());
      p = jobids;

      /**
       * Check if we need to skip the first (full) job from consolidation
       */
      if (job->AlwaysIncrementalMaxFullAge) {
        char sdt_allowed[50];
        char sdt_starttime[50];
        time_t starttime, oldest_allowed_starttime;

        if (incrementals_to_consolidate < 2) {
          Jmsg(jcr, M_INFO, 0,
               _("%s: less incrementals than required to consolidate without "
                 "full, not consolidating\n"),
               job->resource_name_);
          continue;
        }
        Jmsg(jcr, M_INFO, 0, _("before ConsolidateFull: jobids: %s\n"), jobids);

        p = strchr(jobids, ','); /* find oldest jobid and optionally skip it */
        if (p) { *p = '\0'; }

        /**
         * Get db record of oldest jobid and check its age
         */
        jcr->impl->previous_jr = JobDbRecord{};
        jcr->impl->previous_jr.JobId = str_to_int64(jobids);
        Dmsg1(10, "Previous JobId=%s\n", jobids);

        if (!jcr->db->GetJobRecord(jcr, &jcr->impl->previous_jr)) {
          Jmsg(jcr, M_FATAL, 0,
               _("Error getting Job record for first Job: ERR=%s"),
               jcr->db->strerror());
          goto bail_out;
        }

        starttime = jcr->impl->previous_jr.JobTDate;
        oldest_allowed_starttime = now - job->AlwaysIncrementalMaxFullAge;
        bstrftimes(sdt_allowed, sizeof(sdt_allowed), oldest_allowed_starttime);
        bstrftimes(sdt_starttime, sizeof(sdt_starttime), starttime);

        /**
         * Check if job is older than AlwaysIncrementalMaxFullAge
         */
        Jmsg(jcr, M_INFO, 0, _("check full age: full is %s, allowed is %s\n"),
             sdt_starttime, sdt_allowed);
        if (starttime > oldest_allowed_starttime) {
          Jmsg(jcr, M_INFO, 0,
               _("Full is newer than AlwaysIncrementalMaxFullAge -> skipping "
                 "first jobid %s because of age\n"),
               jobids);
          if (p) { *p++ = ','; /* Restore , and point to rest of list */ }

        } else if (max_full_consolidations &&
                   fullconsolidations_started >= max_full_consolidations) {
          Jmsg(jcr, M_INFO, 0,
               _("%d AlwaysIncrementalFullConsolidations reached -> skipping "
                 "first jobid %s independent of age\n"),
               max_full_consolidations, jobids);
          if (p) { *p++ = ','; /* Restore , and point to rest of list */ }

        } else {
          Jmsg(jcr, M_INFO, 0,
               _("Full is older than AlwaysIncrementalMaxFullAge -> also "
                 "consolidating Full jobid %s\n"),
               jobids);
          if (p) {
            *p = ',';   /* Restore ,*/
            p = jobids; /* Point to full list */
          }
          fullconsolidations_started++;
        }
        Jmsg(jcr, M_INFO, 0, _("after ConsolidateFull: jobids: %s\n"), p);
      }

      /**
       * Set the virtualfull jobids to be consolidated
       */
      if (!jcr->impl->vf_jobids) {
        jcr->impl->vf_jobids = GetPoolMemory(PM_MESSAGE);
      }
      PmStrcpy(jcr->impl->vf_jobids, p);

      Jmsg(jcr, M_INFO, 0, _("%s: Start new consolidation\n"),
           job->resource_name_);
      StartNewConsolidationJob(jcr, job->resource_name_);
    }
  }

bail_out:
  /**
   * Restore original job back to jcr.
   */
  jcr->impl->res.job = tmpjob;
  jcr->setJobStatus(JS_Terminated);
  ConsolidateCleanup(jcr, JS_Terminated);

  if (jobids) { free(jobids); }

  return retval;
}

/**
 * Release resources allocated during backup.
 */
void ConsolidateCleanup(JobControlRecord* jcr, int TermCode)
{
  int msg_type;
  char term_code[100];
  const char* TermMsg;
  char sdt[50], edt[50], schedt[50];

  Dmsg0(debuglevel, "Enter backup_cleanup()\n");

  UpdateJobEnd(jcr, TermCode);

  if (!jcr->db->GetJobRecord(jcr, &jcr->impl->jr)) {
    Jmsg(jcr, M_WARNING, 0,
         _("Error getting Job record for Job report: ERR=%s"),
         jcr->db->strerror());
    jcr->setJobStatus(JS_ErrorTerminated);
  }

  msg_type = M_INFO; /* by default INFO message */
  switch (jcr->JobStatus) {
    case JS_Terminated:
      TermMsg = _("Consolidate OK");
      break;
    case JS_FatalError:
    case JS_ErrorTerminated:
      TermMsg = _("*** Consolidate Error ***");
      msg_type = M_ERROR; /* Generate error message */
      break;
    case JS_Canceled:
      TermMsg = _("Consolidate Canceled");
      break;
    default:
      TermMsg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
      break;
  }
  bstrftimes(schedt, sizeof(schedt), jcr->impl->jr.SchedTime);
  bstrftimes(sdt, sizeof(sdt), jcr->impl->jr.StartTime);
  bstrftimes(edt, sizeof(edt), jcr->impl->jr.EndTime);

  Jmsg(jcr, msg_type, 0,
       _("BAREOS %s (%s): %s\n"
         "  JobId:                  %d\n"
         "  Job:                    %s\n"
         "  Scheduled time:         %s\n"
         "  Start time:             %s\n"
         "  End time:               %s\n"
         "  Bareos binary info:     %s\n"
         "  Job triggered by:       %s\n"
         "  Termination:            %s\n\n"),
       kBareosVersionStrings.Full, kBareosVersionStrings.ShortDate, edt,
       jcr->impl->jr.JobId, jcr->impl->jr.Job, schedt, sdt, edt,
       kBareosVersionStrings.JoblogMessage,
       JobTriggerToString(jcr->impl->job_trigger).c_str(), TermMsg);

  Dmsg0(debuglevel, "Leave ConsolidateCleanup()\n");
}
} /* namespace directordaemon */
