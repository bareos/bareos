/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#include "dird/ua.h"

#include "include/jcr.h"
#include "dird/ua_output.h"
#include "dird/dird_conf.h"

namespace directordaemon {

UaContext::UaContext()
    : UA_sock(nullptr)
    , sd(nullptr)
    , jcr(nullptr)
    , db(nullptr)
    , shared_db(nullptr)
    , private_db(nullptr)
    , catalog(nullptr)
    , user_acl(nullptr)
    , cmd(nullptr)
    , args(nullptr)
    , errmsg(nullptr)
    , guid(nullptr)
    , argc(0)
    , prompt(nullptr)
    , max_prompts(0)
    , num_prompts(0)
    , api(0)
    , auto_display_messages(false)
    , user_notified_msg_pending(false)
    , automount(false)
    , quit(false)
    , verbose(false)
    , batch(false)
    , gui(false)
    , runscript(false)
    , pint32_val(0)
    , int32_val(0)
    , int64_val(0)
    , send(nullptr)
    , cmddef(nullptr)
{
  for (int i = 0; i < MAX_CMD_ARGS; i++) argk[i] = nullptr;
  for (int i = 0; i < MAX_CMD_ARGS; i++) argv[i] = nullptr;
}

/**
 * Create a UaContext for a Job that is running so that
 *   it can the User Agent routines and
 *   to ensure that the Job gets the proper output.
 *   This is a sort of mini-kludge, and should be
 *   unified at some point.
 */
UaContext* new_ua_context(JobControlRecord* jcr)
{
  UaContext* ua;

  ua = (UaContext*)malloc(sizeof(UaContext));
  ua = new (ua) UaContext(); /* placement new instead of memset */
  ua->jcr = jcr;
  ua->db = jcr->db;
  ua->cmd = GetPoolMemory(PM_FNAME);
  ua->args = GetPoolMemory(PM_FNAME);
  ua->errmsg = GetPoolMemory(PM_FNAME);
  ua->verbose = true;
  ua->automount = true;
  ua->send = new OutputFormatter(printit, ua, filterit, ua);

  return ua;
}

void FreeUaContext(UaContext* ua)
{
  if (ua->guid) { FreeGuidList(ua->guid); }
  if (ua->cmd) { FreePoolMemory(ua->cmd); }
  if (ua->args) { FreePoolMemory(ua->args); }
  if (ua->errmsg) { FreePoolMemory(ua->errmsg); }
  if (ua->prompt) { free(ua->prompt); }
  if (ua->send) { delete ua->send; }
  if (ua->UA_sock) {
    ua->UA_sock->close();
    ua->UA_sock = NULL;
  }
  free(ua);
}

RunContext::RunContext() { store = new UnifiedStorageResource; }

RunContext::~RunContext()
{
  if (store) { delete store; }
}
} /* namespace directordaemon */
