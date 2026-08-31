/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, October 2000
/**
 * @file
 * Authenticate caller
 */

#include "include/bareos.h"
#include "lib/s_password.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/stored_jcr_impl.h"
#include "stored/authenticate.h"
#include <climits>
#include "lib/parse_conf.h"
#include "lib/bsock.h"
#include "lib/bnet_network_dump.h"
#include "lib/util.h"
#include "include/version_hex.h"

namespace storagedaemon {

const TlsResource* Auth::get(global_resource::Type auth_type,
                             std::string_view name)
{
  auto* myself
      = dynamic_cast<StorageResource*>(p->GetNextRes(R_STORAGE, nullptr));

  if (!myself) {
    Emsg1(M_ERROR, 0, "Could not find myself during connection attempt.\n");
    return nullptr;
  }

  int name_len = (int)std::min((size_t)INT_MAX, name.size());
  const char* name_ptr = name.data();
  char tbuf[MAX_TIME_LENGTH];

  switch (auth_type) {
    case global_resource::Type::Job: {
      auto* jcr = get_jcr_for_authentication(name);
      if (!jcr) {
        Dmsg1(50, "Authentication for job %.*s not possible\n", name_len,
              name_ptr);
        return nullptr;
      }

      auto& data = job.emplace();
      data.jcr = jcr;
      data.job = *myself;
      data.job.password_.value = jcr->sd_auth_key;

      // we cannot require tls, as old clients only do cleartext auth
      // and tls_require prevents this
      // TODO: check with master
      // data.client.tls_require_ = false;

      return &data.job;

    } break;
    case global_resource::Type::Director: {
      auto* res = dynamic_cast<DirectorResource*>(
          p->GetResWithName(R_DIRECTOR, name));
      if (!res) {
        Dmsg1(60, "Got a DIR connection at %s from unknown dir %.*s\n",
              bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)), name_len,
              name_ptr);
        return nullptr;
      }

      if (res->password_.encoding != p_encoding_md5) {
        Emsg1(M_ERROR, 0, "Bad password type for dir %.*s\n", name_len,
              name_ptr);
        return nullptr;
      }

      {
        JobControlRecord* ojcr;
        unsigned int cnt = 0;

        foreach_jcr (ojcr) { cnt++; }
        endeach_jcr(ojcr);

        if (cnt >= myself->MaxConcurrentJobs) {
          Emsg0(M_ERROR, 0,
                T_("Number of Jobs exhausted, please increase "
                   "MaximumConcurrentJobs\n"));
          return nullptr;
        }
      }


      auto& data = director.emplace();
      data.res = res;

      return res;
    } break;
    default: {
    } break;
  }

  return nullptr;
}
} /* namespace storagedaemon */
