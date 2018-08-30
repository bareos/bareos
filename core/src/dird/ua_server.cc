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

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/authenticate.h"
#include "dird/job.h"
#include "dird/ua_cmds.h"
#include "dird/ua_db.h"
#include "dird/ua_input.h"
#include "dird/ua_output.h"
#include "dird/ua_server.h"
#include "lib/bnet.h"

namespace directordaemon {

/**
 * Create a Job Control Record for a control "job", filling in all the appropriate fields.
 */
JobControlRecord *new_control_jcr(const char *base_name, int job_type)
{
   JobControlRecord *jcr;

   jcr = new_jcr(sizeof(JobControlRecord), DirdFreeJcr);

   /*
    * The job and defaults are not really used, but we set them up to ensure that
    * everything is correctly initialized.
    */
   LockRes();
   jcr->res.job = (JobResource *)my_config->GetNextRes(R_JOB, NULL);
   SetJcrDefaults(jcr, jcr->res.job);
   UnlockRes();

   jcr->sd_auth_key = bstrdup("dummy"); /* dummy Storage daemon key */
   CreateUniqueJobName(jcr, base_name);
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
void *HandleUserAgentClientRequest(BareosSocket *user_agent_socket)
{
   int status;
   UaContext *ua;
   JobControlRecord *jcr;

   pthread_detach(pthread_self());

   jcr = new_control_jcr("-Console-", JT_CONSOLE);

   ua = new_ua_context(jcr);
   ua->UA_sock = user_agent_socket;
   SetJcrInTsd(INVALID_JCR);

   if (!AuthenticateUserAgent(ua)) {
      goto getout;
   }

   while (!ua->quit) {
      if (ua->api) {
         user_agent_socket->signal(BNET_MAIN_PROMPT);
      }

      status = user_agent_socket->recv();
      if (status >= 0) {
         PmStrcpy(ua->cmd, ua->UA_sock->msg);
         ParseUaArgs(ua);
         Do_a_command(ua);

         DequeueMessages(ua->jcr);

         if (!ua->quit) {
            if (console_msg_pending && ua->AclAccessOk(Command_ACL, "messages")) {
               if (ua->auto_display_messages) {
                  PmStrcpy(ua->cmd, "messages");
                  DotMessagesCmd(ua, ua->cmd);
                  ua->user_notified_msg_pending = false;
               } else if (!ua->gui && !ua->user_notified_msg_pending && console_msg_pending) {
                  if (ua->api) {
                     user_agent_socket->signal(BNET_MSGS_PENDING);
                  } else {
                     bsendmsg(ua, _("You have messages.\n"));
                  }
                  ua->user_notified_msg_pending = true;
               }
            }
            if (!ua->api) {
               user_agent_socket->signal(BNET_EOD); /* send end of command */
            }
         }
      } else if (IsBnetStop(user_agent_socket)) {
         ua->quit = true;
      } else { /* signal */
         user_agent_socket->signal(BNET_POLL);
      }
   }

getout:
   CloseDb(ua);
   FreeUaContext(ua);
   FreeJcr(jcr);
   user_agent_socket->close();
   delete user_agent_socket;

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
   ua->cmd = GetPoolMemory(PM_FNAME);
   ua->args = GetPoolMemory(PM_FNAME);
   ua->errmsg = GetPoolMemory(PM_FNAME);
   ua->verbose = true;
   ua->automount = true;
   ua->send = New(OutputFormatter(printit, ua, filterit, ua));

   return ua;
}

void FreeUaContext(UaContext *ua)
{
   if (ua->guid) {
      FreeGuidList(ua->guid);
   }
   if (ua->cmd) {
      FreePoolMemory(ua->cmd);
   }
   if (ua->args) {
      FreePoolMemory(ua->args);
   }
   if (ua->errmsg) {
      FreePoolMemory(ua->errmsg);
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
} /* namespace directordaemon */
