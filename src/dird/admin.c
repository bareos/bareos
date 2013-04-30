/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 *   Bacula Director -- admin.c -- responsible for doing admin jobs
 *
 *     Kern Sibbald, May MMIII
 *
 *  Basic tasks done here:
 *     Display the job report.
 *
 */

#include "bacula.h"
#include "dird.h"
#include "ua.h"


bool do_admin_init(JCR *jcr)
{
   free_rstorage(jcr);
   if (!allow_duplicate_job(jcr)) {
      return false;
   }
   return true;
}

/*
 *  Returns:  false on failure
 *            true  on success
 */
bool do_admin(JCR *jcr)
{

   jcr->jr.JobId = jcr->JobId;

   jcr->fname = (char *)get_pool_memory(PM_FNAME);

   /* Print Job Start message */
   Jmsg(jcr, M_INFO, 0, _("Start Admin JobId %d, Job=%s\n"),
        jcr->JobId, jcr->Job);

   jcr->setJobStatus(JS_Running);
   admin_cleanup(jcr, JS_Terminated);
   return true;
}


/*
 * Release resources allocated during backup.
 */
void admin_cleanup(JCR *jcr, int TermCode)
{
   char sdt[50], edt[50], schedt[50];
   char term_code[100];
   const char *term_msg;
   int msg_type;
   MEDIA_DBR mr;

   Dmsg0(100, "Enter backup_cleanup()\n");

   update_job_end(jcr, TermCode);

   if (!db_get_job_record(jcr, jcr->db, &jcr->jr)) {
      Jmsg(jcr, M_WARNING, 0, _("Error getting Job record for Job report: ERR=%s"),
         db_strerror(jcr->db));
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   msg_type = M_INFO;                 /* by default INFO message */
   switch (jcr->JobStatus) {
   case JS_Terminated:
      term_msg = _("Admin OK");
      break;
   case JS_FatalError:
   case JS_ErrorTerminated:
      term_msg = _("*** Admin Error ***");
      msg_type = M_ERROR;          /* Generate error message */
      break;
   case JS_Canceled:
      term_msg = _("Admin Canceled");
      break;
   default:
      term_msg = term_code;
      sprintf(term_code, _("Inappropriate term code: %c\n"), jcr->JobStatus);
      break;
   }
   bstrftimes(schedt, sizeof(schedt), jcr->jr.SchedTime);
   bstrftimes(sdt, sizeof(sdt), jcr->jr.StartTime);
   bstrftimes(edt, sizeof(edt), jcr->jr.EndTime);


   Jmsg(jcr, msg_type, 0, _("Bacula " VERSION " (" LSMDATE "): %s\n"
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

   Dmsg0(100, "Leave admin_cleanup()\n");
}
