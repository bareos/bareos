/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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

UaContext::UaContext(JobControlRecord* jcr_in) : jcr{jcr_in}, db{jcr->db}
{
  cmd = GetPoolMemory(PM_FNAME);
  args = GetPoolMemory(PM_FNAME);
  send = std::make_unique<OutputFormatter>(sprintit, this, filterit, this);
}

UaContext::~UaContext()
{
  if (guid) { FreeGuidList(guid); }
  if (cmd) { FreePoolMemory(cmd); }
  if (args) { FreePoolMemory(args); }
  if (UA_sock) {
    UA_sock->close();
    UA_sock = NULL;
  }
}

RunContext::RunContext() { store = new UnifiedStorageResource; }

RunContext::~RunContext()
{
  if (store) { delete store; }
}

char RestoreContext::FilterIdentifier(RestoreContext::JobTypeFilter filter)
{
  switch (filter) {
    case RestoreContext::JobTypeFilter::Archive: {
      return 'A';
    } break;
    case RestoreContext::JobTypeFilter::Backup: {
      return 'B';
    } break;
    default: {
      ASSERT(!"Invalid job type filter.");
    }
  }

  // this should not be reachable
  return '\0';
}
} /* namespace directordaemon */
