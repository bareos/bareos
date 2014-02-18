/*
   BAREOS - Backup Archiving REcovery Open Sourced

   Copyright (C) 2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
*/
/*
 * This file handles commands from an other Storage daemon.
 *
 * Marco van Wieringen, November 2012
 *
 * We get here because the Director has initiated a Job with
 * an other Storage daemon, then done the same with this
 * Storage daemon. When the Storage daemon receives a proper
 * connection from the other Storage daemon, control is
 * passed here to handle the subsequent Storage daemon commands.
 */

#include "bareos.h"
#include "stored.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Imported variables */

/* Static variables */
static char serrmsg[] =
   "3900 Invalid command\n";

/* Imported functions */

/* Forward referenced SD commands */
static bool start_replication_session(JCR *jcr);
static bool replicate_data(JCR *jcr);
static bool end_replication_session(JCR *jcr);

struct s_cmds {
   const char *cmd;
   bool (*func)(JCR *jcr);
};

/*
 * The following are the recognized commands from the Remote Storage daemon
 */
static struct s_cmds sd_cmds[] = {
   { "start replicate", start_replication_session },
   { "replicate data", replicate_data },
   { "end replicate", end_replication_session },
   { NULL, NULL } /* list terminator */
};

/*
 * Responses sent to the Remote Storage daemon
 */
static char NO_open[] =
   "3901 Error replicate session already open\n";
static char NOT_opened[] =
   "3902 Error replicate session not opened\n";
static char ERROR_replicate[] =
   "3903 Error replicate data\n";
static char OK_end_replicate[] =
   "3000 OK end replicate\n";
static char OK_start_replicate[] =
   "3000 OK start replicate ticket = %d\n";

/*
 * Responses sent to the Director
 */
static char Job_start[] =
   "3010 Job %s start\n";
static char Job_end[] =
   "3099 Job %s end JobStatus=%d JobFiles=%d JobBytes=%s JobErrors=%u\n";

/*
 * After receiving a connection (in dircmd.c) if it is
 * from the Storage daemon, this routine is called.
 */
void *handle_stored_connection(BSOCK *sd, char *job_name)
{
   JCR *jcr;

/*
 * With the following bmicrosleep on, running the
 * SD under the debugger fails.
 */
// bmicrosleep(0, 50000);             /* wait 50 millisecs */
   if (!(jcr = get_jcr_by_full_name(job_name))) {
      Jmsg1(NULL, M_FATAL, 0, _("SD connect failed: Job name not found: %s\n"), job_name);
      Dmsg1(3, "**** Job \"%s\" not found.\n", job_name);
      sd->close();
      delete sd;
      return NULL;
   }

   Dmsg1(50, "Found Job %s\n", job_name);

   if (jcr->authenticated) {
      Jmsg2(jcr, M_FATAL, 0, _("Hey!!!! JobId %u Job %s already authenticated.\n"),
         (uint32_t)jcr->JobId, jcr->Job);
      Dmsg2(50, "Hey!!!! JobId %u Job %s already authenticated.\n",
         (uint32_t)jcr->JobId, jcr->Job);
      sd->close();
      delete sd;
      free_jcr(jcr);
      return NULL;
   }

   jcr->store_bsock = sd;
   jcr->store_bsock->set_jcr(jcr);

   /*
    * Authenticate the Storage daemon
    */
   if (jcr->authenticated || !authenticate_storagedaemon(jcr)) {
      Dmsg1(50, "Authentication failed Job %s\n", jcr->Job);
      Jmsg(jcr, M_FATAL, 0, _("Unable to authenticate Storage daemon\n"));
   } else {
      jcr->authenticated = true;
      Dmsg2(50, "OK Authentication jid=%u Job %s\n", (uint32_t)jcr->JobId, jcr->Job);
   }

   if (!jcr->authenticated) {
      jcr->setJobStatus(JS_ErrorTerminated);
   }

   pthread_cond_signal(&jcr->job_start_wait); /* wake waiting job */
   free_jcr(jcr);

   return NULL;
}

/*
 * Now talk to the SD and do what he says
 */
static void do_sd_commands(JCR *jcr)
{
   int i, status;
   bool found, quit;
   BSOCK *sd = jcr->store_bsock;

   sd->set_jcr(jcr);
   quit = false;
   while (!quit) {
      /*
       * Read command coming from the Storage daemon
       */
      status = sd->recv();
      if (is_bnet_stop(sd)) {         /* hardeof or error */
         break;                       /* connection terminated */
      }
      if (status <= 0) {
         continue;                    /* ignore signals and zero length msgs */
      }

      Dmsg1(110, "<stored: %s", sd->msg);
      found = false;
      for (i = 0; sd_cmds[i].cmd; i++) {
         if (bstrncmp(sd_cmds[i].cmd, sd->msg, strlen(sd_cmds[i].cmd))) {
            found = true;               /* indicate command found */
            jcr->errmsg[0] = 0;
            if (!sd_cmds[i].func(jcr)) {    /* do command */
               /*
                * Note sd->msg command may be destroyed by comm activity
                */
               if (!job_canceled(jcr)) {
                  if (jcr->errmsg[0]) {
                     Jmsg1(jcr, M_FATAL, 0, _("Command error with SD, hanging up. %s\n"),
                           jcr->errmsg);
                  } else {
                     Jmsg0(jcr, M_FATAL, 0, _("Command error with SD, hanging up.\n"));
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
            Jmsg1(jcr, M_FATAL, 0, _("SD command not found: %s\n"), sd->msg);
            Dmsg1(110, "<stored: Command not found: %s\n", sd->msg);
         }
         sd->fsend(serrmsg);
         break;
      }
   }
   sd->signal(BNET_TERMINATE);        /* signal to SD job is done */
}

/*
 * Run a Storage daemon replicate Job -- Wait for remote Storage daemon
 * to connect and authenticate it we then will get a wakeup sign using
 * the job_start_wait conditional
 *
 * Director sends us this command.
 *
 * Basic task here is:
 * - Read a command from the Storage daemon
 * - Execute it
 */
bool do_listen_run(JCR *jcr)
{
   char ec1[30];
   int errstat = 0;
   BSOCK *dir = jcr->dir_bsock;

   jcr->sendJobStatus(JS_WaitSD);          /* wait for SD to connect */

   Dmsg2(50, "%s waiting for SD to contact SD key=%s\n", jcr->Job, jcr->sd_auth_key);
   Dmsg2(800, "Wait SD for jid=%d %p\n", jcr->JobId, jcr);

   /*
    * Wait for the Storage daemon to contact us to start the Job, when he does, we will be released.
    */
   P(mutex);
   while (!jcr->authenticated && !job_canceled(jcr)) {
      errstat = pthread_cond_wait(&jcr->job_start_wait, &mutex);
      if (errstat == EINVAL || errstat == EPERM) {
         break;
      }
      Dmsg1(800, "=== Auth cond errstat=%d\n", errstat);
   }
   Dmsg3(50, "Auth=%d canceled=%d errstat=%d\n", jcr->authenticated, job_canceled(jcr), errstat);
   V(mutex);

   if (!jcr->authenticated || !jcr->store_bsock) {
      Dmsg2(800, "Auth fail or cancel for jid=%d %p\n", jcr->JobId, jcr);
      dequeue_messages(jcr);          /* send any queued messages */

      goto cleanup;
   }

   Dmsg1(120, "Start run Job=%s\n", jcr->Job);

   dir->fsend(Job_start, jcr->Job);
   jcr->start_time = time(NULL);
   jcr->run_time = jcr->start_time;
   jcr->sendJobStatus(JS_Running);

   /*
    * See if we need to limit the inbound bandwidth.
    */
   if (me->max_bandwidth_per_job && jcr->store_bsock) {
      jcr->store_bsock->set_bwlimit(me->max_bandwidth_per_job);
      if (me->allow_bw_bursting) {
         jcr->store_bsock->set_bwlimit_bursting();
      }
   }

   do_sd_commands(jcr);

   jcr->end_time = time(NULL);

   dequeue_messages(jcr);             /* send any queued messages */
   jcr->setJobStatus(JS_Terminated);

cleanup:
   generate_plugin_event(jcr, bsdEventJobEnd);

   dir->fsend(Job_end, jcr->Job, jcr->JobStatus, jcr->JobFiles,
              edit_uint64(jcr->JobBytes, ec1), jcr->JobErrors);

   dir->signal(BNET_EOD);             /* send EOD to Director daemon */

   free_plugins(jcr);                 /* release instantiated plugins */

   /*
    * After a listen cmd we are done e.g. return false.
    */
   return false;
}

/*
 * Start of replication.
 */
static bool start_replication_session(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   Dmsg1(120, "Start replication session: %s", sd->msg);
   if (jcr->session_opened) {
      pm_strcpy(jcr->errmsg, _("Attempt to open already open session.\n"));
      sd->fsend(NO_open);
      return false;
   }

   jcr->session_opened = true;

   /*
    * Send "Ticket" to Storage Daemon
    */
   sd->fsend(OK_start_replicate, jcr->VolSessionId);
   Dmsg1(110, ">stored: %s", sd->msg);

   return true;
}

/*
 * Replicate data.
 *    Open Data Channel and receive Data for archiving
 *    Write the Data to the archive device
 */
static bool replicate_data(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   Dmsg1(120, "Replicate data: %s", sd->msg);
   if (jcr->session_opened) {
      Dmsg1(110, "<stored: %s", sd->msg);
      if (do_append_data(jcr, sd, "SD")) {
         return true;
      } else {
         pm_strcpy(jcr->errmsg, _("Replicate data error.\n"));
         bnet_suppress_error_messages(sd, 1); /* ignore errors at this point */
         sd->fsend(ERROR_replicate);
      }
   } else {
      pm_strcpy(jcr->errmsg, _("Attempt to replicate on non-open session.\n"));
      sd->fsend(NOT_opened);
   }

   return false;
}

/*
 * End a replication session.
 */
static bool end_replication_session(JCR *jcr)
{
   BSOCK *sd = jcr->store_bsock;

   Dmsg1(120, "stored<stored: %s", sd->msg);
   if (!jcr->session_opened) {
      pm_strcpy(jcr->errmsg, _("Attempt to close non-open session.\n"));
      sd->fsend(NOT_opened);
      return false;
   }
   return sd->fsend(OK_end_replicate);
}
