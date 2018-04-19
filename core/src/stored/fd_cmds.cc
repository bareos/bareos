/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, MM
 */
/**
 * @file
 * This file handles commands from the File daemon.
 *
 * We get here because the Director has initiated a Job with
 * the Storage daemon, then done the same with the File daemon,
 * then when the Storage daemon receives a proper connection from
 * the File daemon, control is passed here to handle the
 * subsequent File daemon commands.
 */

#include "include/bareos.h"
#include "stored.h"
#include "stored/append.h"
#include "stored/authenticate.h"
#include "stored/fd_cmds.h"
#include "stored/read.h"
#include "stored/sd_stats.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "include/jcr.h"

/* Imported variables */

/* Static variables */
static char ferrmsg[] =
   "3900 Invalid command\n";

/* Imported functions */

/* Forward referenced FD commands */
static bool append_open_session(JobControlRecord *jcr);
static bool append_close_session(JobControlRecord *jcr);
static bool append_data_cmd(JobControlRecord *jcr);
static bool append_end_session(JobControlRecord *jcr);
static bool read_open_session(JobControlRecord *jcr);
static bool read_data_cmd(JobControlRecord *jcr);
static bool read_close_session(JobControlRecord *jcr);

/* Exported function */

struct s_cmds {
   const char *cmd;
   bool (*func)(JobControlRecord *jcr);
};

/**
 * The following are the recognized commands from the File daemon
 */
static struct s_cmds fd_cmds[] = {
   { "append open", append_open_session },
   { "append data", append_data_cmd },
   { "append end", append_end_session },
   { "append close", append_close_session },
   { "read open", read_open_session },
   { "read data", read_data_cmd },
   { "read close", read_close_session },
   { NULL, NULL } /* list terminator */
};

/* Commands from the File daemon that require additional scanning */
static char read_open[] =
   "read open session = %127s %ld %ld %ld %ld %ld %ld\n";

/* Responses sent to the File daemon */
static char NO_open[] =
   "3901 Error session already open\n";
static char NOT_opened[] =
   "3902 Error session not opened\n";
static char OK_end[] =
   "3000 OK end\n";
static char OK_close[] =
   "3000 OK close Status = %d\n";
static char OK_open[] =
   "3000 OK open ticket = %d\n";
static char ERROR_append[] =
   "3903 Error append data\n";

/* Responses sent to the Director */
static char Job_start[] =
   "3010 Job %s start\n";
static char Job_end[] =
   "3099 Job %s end JobStatus=%d JobFiles=%d JobBytes=%s JobErrors=%u\n";

/**
 * After receiving a connection (in dircmd.c) if it is
 * from the File daemon, this routine is called.
 */
void *handle_filed_connection(BareosSocket *fd, char *job_name)
{
   JobControlRecord *jcr;

/**
 * With the following bmicrosleep on, running the
 * SD under the debugger fails.
 */
// bmicrosleep(0, 50000);             /* wait 50 millisecs */
   if (!(jcr = get_jcr_by_full_name(job_name))) {
      Jmsg1(NULL, M_FATAL, 0, _("FD connect failed: Job name not found: %s\n"), job_name);
      Dmsg1(3, "**** Job \"%s\" not found.\n", job_name);
      fd->close();
      return NULL;
   }

   Dmsg1(50, "Found Job %s\n", job_name);

   if (jcr->authenticated) {
      Jmsg2(jcr, M_FATAL, 0, _("Hey!!!! JobId %u Job %s already authenticated.\n"),
            (uint32_t)jcr->JobId, jcr->Job);
      Dmsg2(50, "Hey!!!! JobId %u Job %s already authenticated.\n",
            (uint32_t)jcr->JobId, jcr->Job);
      fd->close();
      free_jcr(jcr);
      return NULL;
   }

   jcr->file_bsock = fd;
   jcr->file_bsock->set_jcr(jcr);

   /*
    * Authenticate the File daemon
    */
   if (!authenticate_filedaemon(jcr)) {
      Dmsg1(50, "Authentication failed Job %s\n", jcr->Job);
      Jmsg(jcr, M_FATAL, 0, _("Unable to authenticate File daemon\n"));
      jcr->setJobStatus(JS_ErrorTerminated);
   } else {
      utime_t now;

      jcr->authenticated = true;
      Dmsg2(50, "OK Authentication jid=%u Job %s\n", (uint32_t)jcr->JobId, jcr->Job);

      /*
       * Update the initial Job Statistics.
       */
      now = (utime_t)time(NULL);
      update_job_statistics(jcr, now);
   }

   pthread_cond_signal(&jcr->job_start_wait); /* wake waiting job */
   free_jcr(jcr);

   return NULL;
}

/**
 * Run a File daemon Job -- File daemon already authorized
 * Director sends us this command.
 *
 * Basic task here is:
 * - Read a command from the File daemon
 * - Execute it
 */
void run_job(JobControlRecord *jcr)
{
   BareosSocket *dir = jcr->dir_bsock;
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

   generate_plugin_event(jcr, bsdEventJobEnd);

   dir->fsend(Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles,
              edit_uint64(jcr->JobBytes, ec1), jcr->JobErrors);
   dir->signal(BNET_EOD);             /* send EOD to Director daemon */

   free_plugins(jcr);                 /* release instantiated plugins */
}

/**
 * Now talk to the FD and do what he says
 */
void do_fd_commands(JobControlRecord *jcr)
{
   int i, status;
   bool found, quit;
   BareosSocket *fd = jcr->file_bsock;

   fd->set_jcr(jcr);
   quit = false;
   while (!quit) {
      /*
       * Read command coming from the File daemon
       */
      status = fd->recv();
      if (is_bnet_stop(fd)) {         /* hardeof or error */
         break;                       /* connection terminated */
      }
      if (status <= 0) {
         continue;                    /* ignore signals and zero length msgs */
      }

      Dmsg1(110, "<filed: %s", fd->msg);
      found = false;
      for (i = 0; fd_cmds[i].cmd; i++) {
         if (bstrncmp(fd_cmds[i].cmd, fd->msg, strlen(fd_cmds[i].cmd))) {
            found = true;               /* indicate command found */
            jcr->errmsg[0] = 0;
            if (!fd_cmds[i].func(jcr)) {    /* do command */
               /*
                * Note fd->msg command may be destroyed by comm activity
                */
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
            Dmsg1(110, "<filed: Command not found: %s", fd->msg);
         }
         fd->fsend(ferrmsg);
         break;
      }
   }
   fd->signal(BNET_TERMINATE);        /* signal to FD job is done */
}

/**
 * Append Data command
 *    Open Data Channel and receive Data for archiving
 *    Write the Data to the archive device
 */
static bool append_data_cmd(JobControlRecord *jcr)
{
   BareosSocket *fd = jcr->file_bsock;

   Dmsg1(120, "Append data: %s", fd->msg);
   if (jcr->session_opened) {
      Dmsg1(110, "<filed: %s", fd->msg);
      jcr->setJobType(JT_BACKUP);
      if (do_append_data(jcr, fd, "FD")) {
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

static bool append_end_session(JobControlRecord *jcr)
{
   BareosSocket *fd = jcr->file_bsock;

   Dmsg1(120, "stored<filed: %s", fd->msg);
   if (!jcr->session_opened) {
      pm_strcpy(jcr->errmsg, _("Attempt to close non-open session.\n"));
      fd->fsend(NOT_opened);
      return false;
   }
   return fd->fsend(OK_end);
}

/**
 * Append Open session command
 */
static bool append_open_session(JobControlRecord *jcr)
{
   BareosSocket *fd = jcr->file_bsock;

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

/**
 * Append Close session command
 *    Close the append session and send back Statistics
 *    (need to fix statistics)
 */
static bool append_close_session(JobControlRecord *jcr)
{
   BareosSocket *fd = jcr->file_bsock;

   Dmsg1(120, "<filed: %s", fd->msg);
   if (!jcr->session_opened) {
      pm_strcpy(jcr->errmsg, _("Attempt to close non-open session.\n"));
      fd->fsend(NOT_opened);
      return false;
   }

   /*
    * Send final statistics to File daemon
    */
   fd->fsend(OK_close, jcr->JobStatus);
   Dmsg1(120, ">filed: %s", fd->msg);

   fd->signal(BNET_EOD);              /* send EOD to File daemon */

   jcr->session_opened = false;
   return true;
}

/**
 * Read Data command
 *    Open Data Channel, read the data from
 *    the archive device and send to File
 *    daemon.
 */
static bool read_data_cmd(JobControlRecord *jcr)
{
   BareosSocket *fd = jcr->file_bsock;

   Dmsg1(120, "Read data: %s", fd->msg);
   if (jcr->session_opened) {
      Dmsg1(120, "<filed: %s", fd->msg);
      return do_read_data(jcr);
   } else {
      pm_strcpy(jcr->errmsg, _("Attempt to read on non-open session.\n"));
      fd->fsend(NOT_opened);
      return false;
   }
}

/**
 * Read Open session command
 *    We need to scan for the parameters of the job
 *    to be restored.
 */
static bool read_open_session(JobControlRecord *jcr)
{
   BareosSocket *fd = jcr->file_bsock;

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

   /*
    * Send "Ticket" to File Daemon
    */
   fd->fsend(OK_open, jcr->VolSessionId);
   Dmsg1(110, ">filed: %s", fd->msg);

   return true;
}

/**
 * Read Close session command
 *    Close the read session
 */
static bool read_close_session(JobControlRecord *jcr)
{
   BareosSocket *fd = jcr->file_bsock;

   Dmsg1(120, "Read close session: %s\n", fd->msg);
   if (!jcr->session_opened) {
      fd->fsend(NOT_opened);
      return false;
   }

   /*
    * Send final close msg to File daemon
    */
   fd->fsend(OK_close, jcr->JobStatus);
   Dmsg1(160, ">filed: %s", fd->msg);

   fd->signal(BNET_EOD);            /* send EOD to File daemon */

   jcr->session_opened = false;
   return true;
}
