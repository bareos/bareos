/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2022 Bareos GmbH & Co. KG

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
// Marco van Wieringen, June 2016

/**
 * @file
 * responsible for doing archive jobs
 *
 * based on admin.c
 */
#include "include/bareos.h"
#include "dird.h"
#include "dird/archive.h"
#include "dird/job.h"
#include "dird/director_jcr_impl.h"
#include "dird/storage.h"

namespace directordaemon {

static const int debuglevel = 100;

bool DoArchiveInit(JobControlRecord* jcr)
{
  FreeRstorage(jcr);
  if (!AllowDuplicateJob(jcr)) { return false; }

  return true;
}

/**
 * Returns: false on failure
 *          true  on success
 */
bool DoArchive(JobControlRecord* jcr)
{
  jcr->dir_impl->jr.JobId = jcr->JobId;

  jcr->dir_impl->fname = (char*)GetPoolMemory(PM_FNAME);

  Jmsg(jcr, M_INFO, 0, _("Start Archive JobId %d, Job=%s\n"), jcr->JobId,
       jcr->Job);

  jcr->setJobStatusWithPriorityCheck(JS_Running);
  ArchiveCleanup(jcr, JS_Terminated);

  return true;
}

void ArchiveCleanup(JobControlRecord* jcr, int TermCode)
{
  char sdt[50], edt[50], schedt[50];
  char term_code[100];
  const char* TermMsg;
  int msg_type;

  Dmsg0(debuglevel, "Enter ArchiveCleanup()\n");

  UpdateJobEnd(jcr, TermCode);

  if (!jcr->db->GetJobRecord(jcr, &jcr->dir_impl->jr)) {
    Jmsg(jcr, M_WARNING, 0,
         _("Error getting Job record for Job report: ERR=%s\n"),
         jcr->db->strerror());
    jcr->setJobStatusWithPriorityCheck(JS_ErrorTerminated);
  }

  msg_type = M_INFO; /* by default INFO message */
  switch (jcr->getJobStatus()) {
    case JS_Terminated:
      TermMsg = _("Archive OK");
      break;
    case JS_FatalError:
    case JS_ErrorTerminated:
      TermMsg = _("*** Archive Error ***");
      msg_type = M_ERROR; /* Generate error message */
      break;
    case JS_Canceled:
      TermMsg = _("Archive Canceled");
      break;
    default:
      TermMsg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"),
              jcr->getJobStatus());
      break;
  }

  bstrftimes(schedt, sizeof(schedt), jcr->dir_impl->jr.SchedTime);
  bstrftimes(sdt, sizeof(sdt), jcr->dir_impl->jr.StartTime);
  bstrftimes(edt, sizeof(edt), jcr->dir_impl->jr.EndTime);

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
       jcr->dir_impl->jr.JobId, jcr->dir_impl->jr.Job, schedt, sdt, edt,
       kBareosVersionStrings.JoblogMessage,
       JobTriggerToString(jcr->dir_impl->job_trigger).c_str(), TermMsg);

  Dmsg0(debuglevel, "Leave ArchiveCleanup()\n");
}
} /* namespace directordaemon */
