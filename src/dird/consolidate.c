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
 * BAREOS Director -- consolidate.c -- responsible for doing consolidation jobs
 *
 * based on admin.c
 * Philipp Storz, May 2016
 *
 * Basic tasks done here:
 *   run a virtual full job for all jobs that are configured to be always incremental
 */

#include "bareos.h"
#include "dird.h"

static const int dbglvl = 100;

bool do_consolidate_init(JCR *jcr)
{
   free_rstorage(jcr);
   if (!allow_duplicate_job(jcr)) {
      return false;
   }
   return true;
}

static inline void start_new_consolidation_job(JCR *jcr, char *jobname)
{
   JobId_t jobid;
   UAContext *ua;
   POOL_MEM cmd(PM_MESSAGE);

   ua = new_ua_context(jcr);
   ua->batch = true;

   Mmsg(ua->cmd, "run job=\"%s\" level=VirtualFull", jobname);

   Dmsg1(dbglvl, "=============== consolidate cmd=%s\n", ua->cmd);
   parse_ua_args(ua);                 /* parse command */

   jobid = do_run_cmd(ua, ua->cmd);
   if (jobid == 0) {
      Jmsg(jcr, M_ERROR, 0, _("Could not start %s job.\n"), jcr->get_OperationName());
   } else {
      Jmsg(jcr, M_INFO, 0, _("%s JobId %d started.\n"), jcr->get_OperationName(), (int)jobid);
   }

   free_ua_context(ua);
}

/*
 * Returns: false on failure
 *          true  on success
 */
bool do_consolidate(JCR *jcr)
{
   JOBRES *job;

   jcr->jr.JobId = jcr->JobId;

   jcr->fname = (char *)get_pool_memory(PM_FNAME);

   /*
    * Print Job Start message
    */
   Jmsg(jcr, M_INFO, 0, _("Start Consolidate JobId %d, Job=%s\n"), jcr->JobId, jcr->Job);

   jcr->setJobStatus(JS_Running);

   Jmsg(jcr, M_INFO, 0, _("Jobs with Always Incremental set:\n"));

   foreach_res(job, R_JOB) {
      if (job->AlwaysIncremental) {

         Jmsg(jcr, M_INFO, 0, _("%s -> AlwaysIncrementalInterval: %d, AlwaysIncrementalNumber : %d\n"),
              job->name(), job->AlwaysIncrementalInterval, job->AlwaysIncrementalNumber);
         start_new_consolidation_job(jcr, job->name());
      }
   }

   jcr->setJobStatus(JS_Terminated);

   consolidate_cleanup(jcr, JS_Terminated);
   return true;
}

/*
 * Release resources allocated during backup.
 */
void consolidate_cleanup(JCR *jcr, int TermCode)
{
   char sdt[50], edt[50], schedt[50];
   char term_code[100];
   const char *term_msg;
   int msg_type;

   Dmsg0(dbglvl, "Enter backup_cleanup()\n");

   update_job_end(jcr, TermCode);

   if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Job record for Job report: ERR=%s"),
         db_strerror(jcr->db));
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   msg_type = M_INFO;                 /* by default INFO message */
   switch (jcr->JobStatus) {
   case JS_Terminated:
      term_msg = _("Consolidate OK");
      break;
   case JS_FatalError:
   case JS_ErrorTerminated:
      term_msg = _("*** Consolidate Error ***");
      msg_type = M_ERROR;          /* Generate error message */
      break;
   case JS_Canceled:
      term_msg = _("Consolidate Canceled");
      break;
   default:
      term_msg = term_code;
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
        term_msg);

   Dmsg0(dbglvl, "Leave consolidate_cleanup()\n");
}
