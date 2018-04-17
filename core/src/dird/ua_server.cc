/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, September MM
 */
/**
 * @file
 * User Agent Server
 */

#include "bareos.h"
#include "dird.h"

/* Imported variables */

/* Forward referenced functions */

/**
 * Create a Job Control Record for a control "job", filling in all the appropriate fields.
 */
JobControlRecord *new_control_jcr(const char *base_name, int job_type)
{
   JobControlRecord *jcr;

   jcr = new_jcr(sizeof(JobControlRecord), dird_free_jcr);

   /*
    * The job and defaults are not really used, but we set them up to ensure that
    * everything is correctly initialized.
    */
   LockRes();
   jcr->res.job = (JobResource *)GetNextRes(R_JOB, NULL);
   set_jcr_defaults(jcr, jcr->res.job);
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

/**
 * Handle Director User Agent commands
 */
void *handle_UA_client_request(BareosSocket *user)
{
   int status;
   UaContext *ua;
   JobControlRecord *jcr;

   pthread_detach(pthread_self());

   jcr = new_control_jcr("-Console-", JT_CONSOLE);

   ua = new_ua_context(jcr);
   ua->UA_sock = user;
   set_jcr_in_tsd(INVALID_JCR);

   if (!authenticate_user_agent(ua)) {
      goto getout;
   }

   while (!ua->quit) {
      if (ua->api) {
         user->signal(BNET_MAIN_PROMPT);
      }

      status = user->recv();
      if (status >= 0) {
         pm_strcpy(ua->cmd, ua->UA_sock->msg);
         parse_ua_args(ua);
         do_a_command(ua);

         dequeue_messages(ua->jcr);

         if (!ua->quit) {
            if (console_msg_pending && ua->acl_access_ok(Command_ACL, "messages")) {
               if (ua->auto_display_messages) {
                  pm_strcpy(ua->cmd, "messages");
                  dot_messages_cmd(ua, ua->cmd);
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
            if (!ua->api) {
               user->signal(BNET_EOD); /* send end of command */
            }
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
   user->close();
   delete user;

   return NULL;
}

/**
 * Create a UaContext for a Job that is running so that
 *   it can the User Agent routines and
 *   to ensure that the Job gets the proper output.
 *   This is a sort of mini-kludge, and should be
 *   unified at some point.
 */
UaContext *new_ua_context(JobControlRecord *jcr)
{
   UaContext *ua;

   ua = (UaContext *)malloc(sizeof(UaContext));
   memset(ua, 0, sizeof(UaContext));
   ua->jcr = jcr;
   ua->db = jcr->db;
   ua->cmd = get_pool_memory(PM_FNAME);
   ua->args = get_pool_memory(PM_FNAME);
   ua->errmsg = get_pool_memory(PM_FNAME);
   ua->verbose = true;
   ua->automount = true;
   ua->send = New(OutputFormatter(printit, ua, filterit, ua));

   return ua;
}

void free_ua_context(UaContext *ua)
{
   if (ua->guid) {
      free_guid_list(ua->guid);
   }
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
   if (ua->send) {
      delete ua->send;
   }
   if (ua->UA_sock) {
      ua->UA_sock->close();
      ua->UA_sock = NULL;
   }
   free(ua);
}
