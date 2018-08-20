/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 *   run a virtual full job for all jobs that are configured to be always incremental
 * based on admin.c
 *
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/consolidate.h"
#include "dird/job.h"
#include "dird/storage.h"
#include "dird/ua_input.h"
#include "dird/ua_server.h"
#include "dird/ua_run.h"
#include "lib/edit.h"

namespace directordaemon {

static const int debuglevel = 100;

bool DoConsolidateInit(JobControlRecord *jcr)
{
   FreeRstorage(jcr);
   if (!AllowDuplicateJob(jcr)) {
      return false;
   }
   return true;
}

/**
 * Start a Virtual(Full) Job that creates a new virtual backup
 * containing the jobids given in jcr->vf_jobids
 */
static inline void StartNewConsolidationJob(JobControlRecord *jcr, char *jobname)
{
   JobId_t jobid;
   UaContext *ua;
   PoolMem cmd(PM_MESSAGE);

   ua = new_ua_context(jcr);
   ua->batch = true;
   Mmsg(ua->cmd, "run job=\"%s\" jobid=%s level=VirtualFull %s", jobname, jcr->vf_jobids, jcr->accurate ? "accurate=yes" : "accurate=no");

   Dmsg1(debuglevel, "=============== consolidate cmd=%s\n", ua->cmd);
   ParseUaArgs(ua);                 /* parse command */

   jobid = DoRunCmd(ua, ua->cmd);
   if (jobid == 0) {
      Jmsg(jcr, M_ERROR, 0, _("Could not start %s job.\n"), jcr->get_OperationName());
   } else {
      Jmsg(jcr, M_INFO, 0, _("%s JobId %d started.\n"), jcr->get_OperationName(), (int)jobid);
   }

   FreeUaContext(ua);
}

/**
 * The actual consolidation worker
 *
 * Returns: false on failure
 *          true  on success
 */
bool DoConsolidate(JobControlRecord *jcr)
{
   char *p;
   JobResource *job;
   JobResource *tmpjob;
   bool retval = true;
   char *jobids = NULL;
   time_t now = time(NULL);
   int32_t fullconsolidations_started = 0;
   int32_t max_full_consolidations = 0;

   tmpjob = jcr->res.job; /* Memorize job */

   /*
    * Get Value for MaxFullConsolidations from Consolidation job
    */
   max_full_consolidations = jcr->res.job->MaxFullConsolidations;

   jcr->jr.JobId = jcr->JobId;
   jcr->fname = (char *)GetPoolMemory(PM_FNAME);

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start Consolidate JobId %d, Job=%s\n"), jcr->JobId, jcr->Job);

   jcr->setJobStatus(JS_Running);

   foreach_res(job, R_JOB) {
      if (job->AlwaysIncremental) {
         db_list_ctx jobids_ctx;
         int32_t incrementals_total;
         int32_t incrementals_to_consolidate;
         int32_t max_incrementals_to_consolidate;

         Jmsg(jcr, M_INFO, 0, _("Looking at always incremental job %s\n"), job->name());

         /*
          * Fake always incremental job as job of current jcr.
          */
         jcr->res.job = job;
         jcr->res.fileset = job->fileset;
         jcr->res.client = job->client;
         jcr->jr.JobLevel = L_INCREMENTAL;
         jcr->jr.limit = 0;
         jcr->jr.StartTime = 0;

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
         jcr->db->AccurateGetJobids(jcr, &jcr->jr, &jobids_ctx);
         incrementals_total = jobids_ctx.count - 1;
         Dmsg1(10, "unlimited jobids list:  %s.\n", jobids_ctx.list);

         /*
          * If we are doing always incremental, we need to limit the search to
          * only include incrementals that are older than (now - AlwaysIncrementalJobRetention)
          */
         if (job->AlwaysIncrementalJobRetention) {
            char sdt[50];

            jcr->jr.StartTime = now - job->AlwaysIncrementalJobRetention;
            bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
            Jmsg(jcr, M_INFO, 0, _("%s: considering jobs older than %s for consolidation.\n"), job->name(), sdt);
            Dmsg4(10, _("%s: considering jobs with ClientId %d and FilesetId %d older than %s for consolidation.\n"),
                  job->name(), jcr->jr.ClientId, jcr->jr.FileSetId, sdt);
         }

         jcr->db->AccurateGetJobids(jcr, &jcr->jr, &jobids_ctx);
         Dmsg1(10, "consolidate candidates:  %s.\n", jobids_ctx.list);

         /**
          * Consolidation of zero or one job does not make sense, we leave it like it is
          */
         if (incrementals_total < 1) {
            Jmsg(jcr, M_INFO, 0, _("%s: less than two jobs to consolidate found, doing nothing.\n"), job->name());
            continue;
         }

         /**
          * Calculate limit for query. We specify how many incrementals should be left.
          * the limit is total number of incrementals - number required - 1
          */
         max_incrementals_to_consolidate = incrementals_total - job->AlwaysIncrementalKeepNumber;

         Dmsg2(10, "Incrementals found/required. (%d/%d).\n", incrementals_total, job->AlwaysIncrementalKeepNumber);
         if ((max_incrementals_to_consolidate + 1 ) > 1) {
            jcr->jr.limit = max_incrementals_to_consolidate + 1;
            Dmsg3(10, "total: %d, to_consolidate: %d, limit: %d.\n", incrementals_total, max_incrementals_to_consolidate, jcr->jr.limit);
            jobids_ctx.reset();
            jcr->db->AccurateGetJobids(jcr, &jcr->jr, &jobids_ctx);
            incrementals_to_consolidate = jobids_ctx.count - 1;
            Dmsg2(10, "%d consolidate ids after limit: %s.\n", jobids_ctx.count, jobids_ctx.list);
            if (incrementals_to_consolidate < 1) {
               Jmsg(jcr, M_INFO, 0, _("%s: After limited query: less incrementals than required, not consolidating\n"), job->name());
               continue;
            }
         } else {
            Jmsg(jcr, M_INFO, 0, _("%s: less incrementals than required, not consolidating\n"), job->name());
            continue;
         }

         if (jobids) {
            free(jobids);
            jobids = NULL;
         }

         jobids = bstrdup(jobids_ctx.list);
         p = jobids;

         /**
          * Check if we need to skip the first (full) job from consolidation
          */
         if (job->AlwaysIncrementalMaxFullAge) {
            char sdt_allowed[50];
            char sdt_starttime[50];
            time_t starttime,
                   oldest_allowed_starttime;

            if (incrementals_to_consolidate < 2) {
               Jmsg(jcr, M_INFO, 0, _("%s: less incrementals than required to consolidate without full, not consolidating\n"), job->name());
               continue;
            }
            Jmsg(jcr, M_INFO, 0, _("before ConsolidateFull: jobids: %s\n"), jobids);

            p = strchr(jobids, ',');                /* find oldest jobid and optionally skip it */
            if (p) {
               *p = '\0';
            }

            /**
             * Get db record of oldest jobid and check its age
             */
            memset(&jcr->previous_jr, 0, sizeof(jcr->previous_jr));
            jcr->previous_jr.JobId = str_to_int64(jobids);
            Dmsg1(10, "Previous JobId=%s\n", jobids);

            if (!jcr->db->GetJobRecord(jcr, &jcr->previous_jr)) {
               Jmsg(jcr, M_FATAL, 0, _("Error getting Job record for first Job: ERR=%s"), jcr->db->strerror());
               goto bail_out;
            }

            starttime = jcr->previous_jr.JobTDate;
            oldest_allowed_starttime = now - job->AlwaysIncrementalMaxFullAge;
            bstrftimes(sdt_allowed, sizeof(sdt_allowed), oldest_allowed_starttime);
            bstrftimes(sdt_starttime, sizeof(sdt_starttime), starttime);

            /**
             * Check if job is older than AlwaysIncrementalMaxFullAge
             */
            Jmsg(jcr, M_INFO, 0,  _("check full age: full is %s, allowed is %s\n"), sdt_starttime, sdt_allowed);
            if (starttime > oldest_allowed_starttime) {
               Jmsg(jcr, M_INFO, 0, _("Full is newer than AlwaysIncrementalMaxFullAge -> skipping first jobid %s because of age\n"), jobids);
               if (p) {
                  *p++ = ','; /* Restore , and point to rest of list */
               }

            } else if (max_full_consolidations &&
                       fullconsolidations_started >= max_full_consolidations) {
               Jmsg(jcr, M_INFO, 0, _("%d AlwaysIncrementalFullConsolidations reached -> skipping first jobid %s independent of age\n"),
                       max_full_consolidations, jobids);
               if (p) {
                  *p++ = ','; /* Restore , and point to rest of list */
               }

            } else {
               Jmsg(jcr, M_INFO, 0, _("Full is older than AlwaysIncrementalMaxFullAge -> also consolidating Full jobid %s\n"), jobids);
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
         if (!jcr->vf_jobids) {
            jcr->vf_jobids = GetPoolMemory(PM_MESSAGE);
         }
         PmStrcpy(jcr->vf_jobids, p);

         Jmsg(jcr, M_INFO, 0, _("%s: Start new consolidation\n"), job->name());
         StartNewConsolidationJob(jcr, job->name());
      }
   }

bail_out:
   /**
    * Restore original job back to jcr.
    */
   jcr->res.job = tmpjob;
   jcr->setJobStatus(JS_Terminated);
   ConsolidateCleanup(jcr, JS_Terminated);

   if (jobids) {
      free(jobids);
   }

   return retval;
}

/**
 * Release resources allocated during backup.
 */
void ConsolidateCleanup(JobControlRecord *jcr, int TermCode)
{
   int msg_type;
   char term_code[100];
   const char *TermMsg;
   char sdt[50], edt[50], schedt[50];

   Dmsg0(debuglevel, "Enter backup_cleanup()\n");

   UpdateJobEnd(jcr, TermCode);

   if (!jcr->db->GetJobRecord(jcr, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Job record for Job report: ERR=%s"), jcr->db->strerror());
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   msg_type = M_INFO;                 /* by default INFO message */
   switch (jcr->JobStatus) {
   case JS_Terminated:
      TermMsg = _("Consolidate OK");
      break;
   case JS_FatalError:
   case JS_ErrorTerminated:
      TermMsg = _("*** Consolidate Error ***");
      msg_type = M_ERROR;          /* Generate error message */
      break;
   case JS_Canceled:
      TermMsg = _("Consolidate Canceled");
      break;
   default:
      TermMsg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
      break;
   }
   bstrftimes(schedt, sizeof(schedt), jcr->jr.SchedTime);
   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);

   Jmsg(jcr, msg_type, 0, _("BAREOS " VERSION " (" LSMDATE "): %s\n"
        "  JobId:                  %d\n"
        "  Job:                    %s\n"
        "  Scheduled time:         %s\n"
        "  Start time:             %s\n"
        "  End time:               %s\n"
        "  Termination:            %s\n\n"),
        edt,
        jcr->jr.JobId,
        jcr->jr.Job,
        schedt,
        sdt,
        edt,
        TermMsg);

   Dmsg0(debuglevel, "Leave ConsolidateCleanup()\n");
}
} /* namespace directordaemon */
