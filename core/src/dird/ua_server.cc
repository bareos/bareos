/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
// Kern Sibbald, September MM
/**
 * @file
 * User Agent Server
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/authenticate.h"
#include "dird/authenticate_console.h"
#include "dird/director_jcr_impl.h"
#include "dird/job.h"
#include "dird/pthread_detach_if_not_detached.h"
#include "dird/ua_cmds.h"
#include "dird/ua_db.h"
#include "dird/ua_input.h"
#include "dird/ua_output.h"
#include "dird/ua_server.h"
#include "lib/bnet.h"
#include "lib/parse_conf.h"
#include "lib/thread_specific_data.h"
#include "dird/jcr_util.h"
#include "console_connection_lease.h"

namespace directordaemon {

/**
 * Create a Job Control Record for a control "job", filling in all the
 * appropriate fields.
 */
JobControlRecord* new_control_jcr(const char* base_name, int job_type)
{
  JobControlRecord* jcr;
  jcr = NewDirectorJcr(DirdFreeJcr);

  // exclude JT_SYSTEM job from shared config counting
  if (job_type == JT_SYSTEM) {
    jcr->dir_impl->job_config_resources_container_ = nullptr;
  }

  /* The job and defaults are not really used, but we set them up to ensure that
   * everything is correctly initialized. */
  {
    ResLocker _{my_config};
    jcr->dir_impl->res.job = (JobResource*)my_config->GetNextRes(R_JOB, NULL);
    SetJcrDefaults(jcr, jcr->dir_impl->res.job);
  }

  jcr->sd_auth_key = strdup("dummy"); /* dummy Storage daemon key */
  CreateUniqueJobName(jcr, base_name);
  jcr->sched_time = jcr->start_time;
  jcr->setJobType(job_type);
  jcr->setJobLevel(L_NONE);
  jcr->setJobStatusWithPriorityCheck(JS_Running);
  jcr->JobId = 0;

  return jcr;
}

// Handle Director User Agent commands
void* HandleUserAgentClientRequest(BareosSocket* user_agent_socket)
{
  DetachIfNotDetached(pthread_self());

  JobControlRecord* jcr = new_control_jcr("-Console-", JT_CONSOLE);

  UaContext* ua = new_ua_context(jcr);
  ua->UA_sock = user_agent_socket;
  SetJcrInThreadSpecificData(nullptr);

  ConsoleConnectionLease lease;  // obtain lease to count connections
  bool success = AuthenticateConsole(ua);

  if (!success) { ua->quit = true; }

  while (!ua->quit) {
    if (ua->api) { user_agent_socket->signal(BNET_MAIN_PROMPT); }

    int status = user_agent_socket->recv();
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
          } else if (!ua->gui && !ua->user_notified_msg_pending
                     && console_msg_pending) {
            if (ua->api) {
              user_agent_socket->signal(BNET_MSGS_PENDING);
            } else {
              bsendmsg(ua, T_("You have messages.\n"));
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
  } /* while (!ua->quit) */

  CloseDb(ua);
  FreeUaContext(ua);
  FreeJcr(jcr);
  delete user_agent_socket;

  return NULL;
}
} /* namespace directordaemon */
