/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

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
 *   Bacula Director -- User Agent Server
 *
 *     Kern Sibbald, September MM
 *
 *    Version $Id$
 */

#include "bacula.h"
#include "dird.h"

/* Imported variables */


/* Forward referenced functions */
extern "C" void *connect_thread(void *arg);
static void *handle_UA_client_request(void *arg);


/* Global variables */
static int started = FALSE;
static workq_t ua_workq;

struct s_addr_port {
   char *addr;
   char *port;
};

/* Called here by Director daemon to start UA (user agent)
 * command thread. This routine creates the thread and then
 * returns.
 */
void start_UA_server(dlist *addrs)
{
   pthread_t thid;
   int status;
   static dlist *myaddrs = addrs;

   if ((status=pthread_create(&thid, NULL, connect_thread, (void *)myaddrs)) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Cannot create UA thread: %s\n"), be.bstrerror(status));
   }
   started = TRUE;
   return;
}

extern "C"
void *connect_thread(void *arg)
{
   pthread_detach(pthread_self());
   set_jcr_in_tsd(INVALID_JCR);

   /* Permit MaxConsoleConnect console connections */
   bnet_thread_server((dlist*)arg, director->MaxConsoleConnect, &ua_workq, handle_UA_client_request);
   return NULL;
}

/*
 * Create a Job Control Record for a control "job",
 *   filling in all the appropriate fields.
 */
JCR *new_control_jcr(const char *base_name, int job_type)
{
   JCR *jcr;
   jcr = new_jcr(sizeof(JCR), dird_free_jcr);
   /*
    * The job and defaults are not really used, but
    *  we set them up to ensure that everything is correctly
    *  initialized.
    */
   LockRes();
   jcr->job = (JOB *)GetNextRes(R_JOB, NULL);
   set_jcr_defaults(jcr, jcr->job);
   UnlockRes();
   jcr->sd_auth_key = bstrdup("dummy"); /* dummy Storage daemon key */
   create_unique_job_name(jcr, base_name);
   jcr->sched_time = jcr->start_time;
   jcr->setJobType(job_type);
   jcr->setJobLevel(L_NONE);
   jcr->setJobStatus(JS_Running);
   jcr->JobId = 0;
   return jcr;
}

/*
 * Handle Director User Agent commands
 *
 */
static void *handle_UA_client_request(void *arg)
{
   int stat;
   UAContext *ua;
   JCR *jcr;
   BSOCK *user = (BSOCK *)arg;

   pthread_detach(pthread_self());

   jcr = new_control_jcr("-Console-", JT_CONSOLE);

   ua = new_ua_context(jcr);
   ua->UA_sock = user;
   set_jcr_in_tsd(INVALID_JCR);

   user->recv();             /* Get first message */
   if (!authenticate_user_agent(ua)) {
      goto getout;
   }

   while (!ua->quit) {
      if (ua->api) user->signal(BNET_MAIN_PROMPT);
      stat = user->recv();
      if (stat >= 0) {
         pm_strcpy(ua->cmd, ua->UA_sock->msg);
         parse_ua_args(ua);
         if (ua->argc > 0 && ua->argk[0][0] == '.') {
            do_a_dot_command(ua);
         } else {
            do_a_command(ua);
         }
         dequeue_messages(ua->jcr);
         if (!ua->quit) {
            if (console_msg_pending && acl_access_ok(ua, Command_ACL, "messages", 8)) {
               if (ua->auto_display_messages) {
                  pm_strcpy(ua->cmd, "messages");
                  qmessagescmd(ua, ua->cmd);
                  ua->user_notified_msg_pending = false;
               } else if (!ua->gui && !ua->user_notified_msg_pending && console_msg_pending) {
                  if (ua->api) {
                     user->signal(BNET_MSGS_PENDING);
                  } else {
                     bsendmsg(ua, _("You have messages.\n"));
                  }
                  ua->user_notified_msg_pending = true;
               }
            }
            if (!ua->api) user->signal(BNET_EOD);     /* send end of command */
         }
      } else if (is_bnet_stop(user)) {
         ua->quit = true;
      } else { /* signal */
         user->signal(BNET_POLL);
      }
   }

getout:
   close_db(ua);
   free_ua_context(ua);
   free_jcr(jcr);

   return NULL;
}

/*
 * Create a UAContext for a Job that is running so that
 *   it can the User Agent routines and
 *   to ensure that the Job gets the proper output.
 *   This is a sort of mini-kludge, and should be
 *   unified at some point.
 */
UAContext *new_ua_context(JCR *jcr)
{
   UAContext *ua;

   ua = (UAContext *)malloc(sizeof(UAContext));
   memset(ua, 0, sizeof(UAContext));
   ua->jcr = jcr;
   ua->db = jcr->db;
   ua->cmd = get_pool_memory(PM_FNAME);
   ua->args = get_pool_memory(PM_FNAME);
   ua->errmsg = get_pool_memory(PM_FNAME);
   ua->verbose = true;
   ua->automount = true;
   return ua;
}

void free_ua_context(UAContext *ua)
{
   if (ua->cmd) {
      free_pool_memory(ua->cmd);
   }
   if (ua->args) {
      free_pool_memory(ua->args);
   }
   if (ua->errmsg) {
      free_pool_memory(ua->errmsg);
   }
   if (ua->prompt) {
      free(ua->prompt);
   }
   if (ua->UA_sock) {
      bnet_close(ua->UA_sock);
      ua->UA_sock = NULL;
   }
   free(ua);
}


/*
 * Called from main Bacula thread
 */
void term_ua_server()
{
   if (!started) {
      return;
   }
}
