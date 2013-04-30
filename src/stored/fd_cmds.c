/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.

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
 * This file handles commands from the File daemon.
 *
 *  Kern Sibbald, MM
 *
 * We get here because the Director has initiated a Job with
 *  the Storage daemon, then done the same with the File daemon,
 *  then when the Storage daemon receives a proper connection from
 *  the File daemon, control is passed here to handle the
 *  subsequent File daemon commands.
 *
 *
 */

#include "bacula.h"
#include "stored.h"

/* Imported variables */
extern STORES *me;

/* Static variables */
static char ferrmsg[]      = "3900 Invalid command\n";

/* Imported functions */
extern bool do_append_data(JCR *jcr);
extern bool do_read_data(JCR *jcr);

/* Forward referenced FD commands */
static bool append_open_session(JCR *jcr);
static bool append_close_session(JCR *jcr);
static bool append_data_cmd(JCR *jcr);
static bool append_end_session(JCR *jcr);
static bool read_open_session(JCR *jcr);
static bool read_data_cmd(JCR *jcr);
static bool read_close_session(JCR *jcr);

/* Exported function */
bool get_bootstrap_file(JCR *jcr, BSOCK *bs);

struct s_cmds {
   const char *cmd;
   bool (*func)(JCR *jcr);
};

/*
 * The following are the recognized commands from the File daemon
 */
static struct s_cmds fd_cmds[] = {
   {"append open",  append_open_session},
   {"append data",  append_data_cmd},
   {"append end",   append_end_session},
   {"append close", append_close_session},
   {"read open",    read_open_session},
   {"read data",    read_data_cmd},
   {"read close",   read_close_session},
   {NULL,           NULL}                  /* list terminator */
};

/* Commands from the File daemon that require additional scanning */
static char read_open[]       = "read open session = %127s %ld %ld %ld %ld %ld %ld\n";

/* Responses sent to the File daemon */
static char NO_open[]         = "3901 Error session already open\n";
static char NOT_opened[]      = "3902 Error session not opened\n";
static char OK_end[]          = "3000 OK end\n";
static char OK_close[]        = "3000 OK close Status = %d\n";
static char OK_open[]         = "3000 OK open ticket = %d\n";
static char ERROR_append[]    = "3903 Error append data\n";

/* Information sent to the Director */
static char Job_start[] = "3010 Job %s start\n";
char Job_end[]   =
   "3099 Job %s end JobStatus=%d JobFiles=%d JobBytes=%s JobErrors=%u\n";

/*
 * Run a File daemon Job -- File daemon already authorized
 *  Director sends us this command.
 *
 * Basic task here is:
 * - Read a command from the File daemon
 * - Execute it
 *
 */
void run_job(JCR *jcr)
{
   BSOCK *dir = jcr->dir_bsock;
   char ec1[30];

   dir->set_jcr(jcr);
   Dmsg1(120, "Start run Job=%s\n", jcr->Job);
   dir->fsend(Job_start, jcr->Job);
   jcr->start_time = time(NULL);
   jcr->run_time = jcr->start_time;
   jcr->sendJobStatus(JS_Running);
   do_fd_commands(jcr);
   jcr->end_time = time(NULL);
   dequeue_messages(jcr);             /* send any queued messages */
   jcr->setJobStatus(JS_Terminated);
   generate_daemon_event(jcr, "JobEnd");
   generate_plugin_event(jcr, bsdEventJobEnd);
   dir->fsend(Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles,
      edit_uint64(jcr->JobBytes, ec1), jcr->JobErrors);
   dir->signal(BNET_EOD);             /* send EOD to Director daemon */
   free_plugins(jcr);                 /* release instantiated plugins */
   return;
}

/*
 * Now talk to the FD and do what he says
 */
void do_fd_commands(JCR *jcr)
{
   int i;
   bool found, quit;
   BSOCK *fd = jcr->file_bsock;

   fd->set_jcr(jcr);
   for (quit=false; !quit;) {
      int stat;

      /* Read command coming from the File daemon */
      stat = fd->recv();
      if (is_bnet_stop(fd)) {         /* hardeof or error */
         break;                       /* connection terminated */
      }
      if (stat <= 0) {
         continue;                    /* ignore signals and zero length msgs */
      }
      Dmsg1(110, "<filed: %s", fd->msg);
      found = false;
      for (i=0; fd_cmds[i].cmd; i++) {
         if (strncmp(fd_cmds[i].cmd, fd->msg, strlen(fd_cmds[i].cmd)) == 0) {
            found = true;               /* indicate command found */
            jcr->errmsg[0] = 0;
            if (!fd_cmds[i].func(jcr)) {    /* do command */
               /* Note fd->msg command may be destroyed by comm activity */
               if (!job_canceled(jcr)) {
                  if (jcr->errmsg[0]) {
                     Jmsg1(jcr, M_FATAL, 0, _("Command error with FD, hanging up. %s\n"),
                           jcr->errmsg);
                  } else {
                     Jmsg0(jcr, M_FATAL, 0, _("Command error with FD, hanging up.\n"));
                  }
                  jcr->setJobStatus(JS_ErrorTerminated);
               }
               quit = true;
            }
            break;
         }
      }
      if (!found) {                   /* command not found */
         if (!job_canceled(jcr)) {
            Jmsg1(jcr, M_FATAL, 0, _("FD command not found: %s\n"), fd->msg);
            Dmsg1(110, "<filed: Command not found: %s\n", fd->msg);
         }
         fd->fsend(ferrmsg);
         break;
      }
   }
   fd->signal(BNET_TERMINATE);        /* signal to FD job is done */
}

/*
 *   Append Data command
 *     Open Data Channel and receive Data for archiving
 *     Write the Data to the archive device
 */
static bool append_data_cmd(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "Append data: %s", fd->msg);
   if (jcr->session_opened) {
      Dmsg1(110, "<bfiled: %s", fd->msg);
      jcr->setJobType(JT_BACKUP);
      if (do_append_data(jcr)) {
         return true;
      } else {
         pm_strcpy(jcr->errmsg, _("Append data error.\n"));
         bnet_suppress_error_messages(fd, 1); /* ignore errors at this point */
         fd->fsend(ERROR_append);
      }
   } else {
      pm_strcpy(jcr->errmsg, _("Attempt to append on non-open session.\n"));
      fd->fsend(NOT_opened);
   }
   return false;
}

static bool append_end_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "store<file: %s", fd->msg);
   if (!jcr->session_opened) {
      pm_strcpy(jcr->errmsg, _("Attempt to close non-open session.\n"));
      fd->fsend(NOT_opened);
      return false;
   }
   return fd->fsend(OK_end);
}


/*
 * Append Open session command
 *
 */
static bool append_open_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "Append open session: %s", fd->msg);
   if (jcr->session_opened) {
      pm_strcpy(jcr->errmsg, _("Attempt to open already open session.\n"));
      fd->fsend(NO_open);
      return false;
   }

   jcr->session_opened = true;

   /* Send "Ticket" to File Daemon */
   fd->fsend(OK_open, jcr->VolSessionId);
   Dmsg1(110, ">filed: %s", fd->msg);

   return true;
}

/*
 *   Append Close session command
 *      Close the append session and send back Statistics
 *         (need to fix statistics)
 */
static bool append_close_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "<filed: %s", fd->msg);
   if (!jcr->session_opened) {
      pm_strcpy(jcr->errmsg, _("Attempt to close non-open session.\n"));
      fd->fsend(NOT_opened);
      return false;
   }
   /* Send final statistics to File daemon */
   fd->fsend(OK_close, jcr->JobStatus);
   Dmsg1(120, ">filed: %s", fd->msg);

   fd->signal(BNET_EOD);              /* send EOD to File daemon */

   jcr->session_opened = false;
   return true;
}

/*
 *   Read Data command
 *     Open Data Channel, read the data from
 *     the archive device and send to File
 *     daemon.
 */
static bool read_data_cmd(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "Read data: %s", fd->msg);
   if (jcr->session_opened) {
      Dmsg1(120, "<bfiled: %s", fd->msg);
      return do_read_data(jcr);
   } else {
      pm_strcpy(jcr->errmsg, _("Attempt to read on non-open session.\n"));
      fd->fsend(NOT_opened);
      return false;
   }
}

/*
 * Read Open session command
 *
 *  We need to scan for the parameters of the job
 *    to be restored.
 */
static bool read_open_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "%s\n", fd->msg);
   if (jcr->session_opened) {
      pm_strcpy(jcr->errmsg, _("Attempt to open read on non-open session.\n"));
      fd->fsend(NO_open);
      return false;
   }

   if (sscanf(fd->msg, read_open, jcr->read_dcr->VolumeName, &jcr->read_VolSessionId,
         &jcr->read_VolSessionTime, &jcr->read_StartFile, &jcr->read_EndFile,
         &jcr->read_StartBlock, &jcr->read_EndBlock) == 7) {
      if (jcr->session_opened) {
         pm_strcpy(jcr->errmsg, _("Attempt to open read on non-open session.\n"));
         fd->fsend(NOT_opened);
         return false;
      }
      Dmsg4(100, "read_open_session got: JobId=%d Vol=%s VolSessId=%ld VolSessT=%ld\n",
         jcr->JobId, jcr->read_dcr->VolumeName, jcr->read_VolSessionId,
         jcr->read_VolSessionTime);
      Dmsg4(100, "  StartF=%ld EndF=%ld StartB=%ld EndB=%ld\n",
         jcr->read_StartFile, jcr->read_EndFile, jcr->read_StartBlock,
         jcr->read_EndBlock);
   }

   jcr->session_opened = true;
   jcr->setJobType(JT_RESTORE);

   /* Send "Ticket" to File Daemon */
   fd->fsend(OK_open, jcr->VolSessionId);
   Dmsg1(110, ">filed: %s", fd->msg);

   return true;
}

/*
 *   Read Close session command
 *      Close the read session
 */
static bool read_close_session(JCR *jcr)
{
   BSOCK *fd = jcr->file_bsock;

   Dmsg1(120, "Read close session: %s\n", fd->msg);
   if (!jcr->session_opened) {
      fd->fsend(NOT_opened);
      return false;
   }
   /* Send final close msg to File daemon */
   fd->fsend(OK_close, jcr->JobStatus);
   Dmsg1(160, ">filed: %s\n", fd->msg);

   fd->signal(BNET_EOD);            /* send EOD to File daemon */

   jcr->session_opened = false;
   return true;
}
