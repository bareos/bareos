/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "include/jcr.h"
#include "stored/ndmp_session_registry.h"
#include "stored/session_token_registry.h"

namespace storagedaemon {

namespace {
SessionTokenRegistry<JobControlRecord*>& NdmpSessions()
{
  static SessionTokenRegistry<JobControlRecord*> registry;

  return registry;
}
}  // namespace

void RegisterNdmpSessionToken(const char* token, JobControlRecord* jcr)
{
  if (token == nullptr) { return; }

  if (!NdmpSessions().Add(token, jcr)) {
    Jmsg0(jcr, M_FATAL, 0,
          T_("Could not register NDMP session, the generated authentication "
             "key is unusable.\n"));
  }
}

void UnregisterNdmpSessionToken(const char* token)
{
  if (token == nullptr) { return; }

  NdmpSessions().Remove(token);
}

JobControlRecord* AcquireJcrByNdmpSessionToken(const char* token)
{
  JobControlRecord* found = nullptr;

  if (token == nullptr) { return nullptr; }

  /* The use count is incremented while the registry is locked, so the job
   * cannot be freed between finding it and returning it. */
  NdmpSessions().Visit(token, [&found](JobControlRecord* jcr) {
    jcr->IncUseCount();
    found = jcr;
  });

  return found;
}

}  // namespace storagedaemon
