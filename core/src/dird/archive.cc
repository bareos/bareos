/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2016-2019 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, June 2016
 */

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
#include "dird/jcr_private.h"
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
  jcr->impl->jr.JobId = jcr->JobId;

  jcr->impl->fname = (char*)GetPoolMemory(PM_FNAME);

  /*
   * Print Job Start message
   */
  Jmsg(jcr, M_INFO, 0, _("Start Archive JobId %d, Job=%s\n"), jcr->JobId,
       jcr->Job);

  jcr->setJobStatus(JS_Running);
  ArchiveCleanup(jcr, JS_Terminated);

  return true;
}

/**
 * Release resources allocated during archive.
 */
void ArchiveCleanup(JobControlRecord* jcr, int TermCode)
{
  char sdt[50], edt[50], schedt[50];
  char term_code[100];
  const char* TermMsg;
  int msg_type;

  Dmsg0(debuglevel, "Enter ArchiveCleanup()\n");

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
      sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
      break;
  }

  bstrftimes(schedt, sizeof(schedt), jcr->impl->jr.SchedTime);
  bstrftimes(sdt, sizeof(sdt), jcr->impl->jr.StartTime);
  bstrftimes(edt, sizeof(edt), jcr->impl->jr.EndTime);

  Jmsg(jcr, msg_type, 0,
       _("BAREOS " VERSION " (" LSMDATE "): %s\n"
         "  JobId:                  %d\n"
         "  Job:                    %s\n"
         "  Scheduled time:         %s\n"
         "  Start time:             %s\n"
         "  End time:               %s\n"
         "  Bareos binary info:     %s\n"
         "  Termination:            %s\n\n"),
       edt, jcr->impl->jr.JobId, jcr->impl->jr.Job, schedt, sdt, edt,
       BAREOS_JOBLOG_MESSAGE, TermMsg);

  Dmsg0(debuglevel, "Leave ArchiveCleanup()\n");
}
} /* namespace directordaemon */
