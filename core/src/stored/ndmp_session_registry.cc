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

#include <string>

#include "include/bareos.h"
#include "include/jcr.h"
#include "stored/ndmp_session_registry.h"
#include "stored/session_token_registry.h"

namespace storagedaemon {

namespace {
/* The registry maps a token to the unique job name instead of to the job
 * itself. A JobControlRecord may reach a use count of zero and be unlinked
 * from the job chain while it is still registered here, so keeping a raw
 * pointer would allow a lookup to resurrect an object that is already being
 * destroyed. Looking the job up by name goes through the job chain lock,
 * which is also what serialises the final release, so a job that is gone is
 * simply not found any more. */
SessionTokenRegistry<std::string>& NdmpSessions()
{
  static SessionTokenRegistry<std::string> registry;

  return registry;
}
}  // namespace

void RegisterNdmpSessionToken(const char* token, JobControlRecord* jcr)
{
  if (token == nullptr) { return; }

  if (!NdmpSessions().Add(token, jcr->Job)) {
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
  std::string job_name;

  if (token == nullptr) { return nullptr; }

  NdmpSessions().Visit(
      token, [&job_name](const std::string& name) { job_name = name; });

  if (job_name.empty()) { return nullptr; }

  /* Acquires the reference under the job chain lock, and only finds jobs that
   * are still linked into the chain. */
  return get_jcr_by_full_name(job_name);
}

}  // namespace storagedaemon
