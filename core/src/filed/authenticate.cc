/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * Authenticate Director who is attempting to connect.
 */

#include "include/baconfig.h"
#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/filed_jcr_impl.h"
#include "filed/authenticate.h"
#include "filed/restore.h"
#include "lib/bnet.h"
#include "lib/bsock.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

namespace filedaemon {

/* Version at end of Hello
 *   prior to 10Mar08 no version
 *   1 10Mar08
 *   2 13Mar09 - Added the ability to restore from multiple storages
 *   3 03Sep10 - Added the restore object command for vss plugin 4.0
 *   4 25Nov10 - Added bandwidth command 5.1
 *   5 24Nov11 - Added new restore object command format (pluginname) 6.0
 *
 *  51 21Mar13 - Added reverse datachannel initialization
 *  52 13Jul13 - Added plugin options
 *  53 02Apr15 - Added setdebug timestamp
 *  54 29Oct15 - Added getSecureEraseCmd
 */

const TlsResource* Auth::get(global_resource::Type auth_type,
                             std::string_view name)
{
  auto* myself
      = dynamic_cast<ClientResource*>(p->GetNextRes(R_CLIENT, nullptr));

  if (!myself) {
    Emsg1(M_ERROR, 0, "Could not find myself during connection attempt.\n");
    return nullptr;
  }

  char tbuf[MAX_TIME_LENGTH];

  int name_len = (int)std::min((size_t)INT_MAX, name.size());
  const char* name_ptr = name.data();

  switch (auth_type) {
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

      if (!res->conn_from_dir_to_fd) {
        Emsg2(M_ERROR, 0, "Connection from director %s is not allowed\n",
              res->resource_name_);
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

      return data.res;

    } break;
    case global_resource::Type::Storage: {
      auto* jcr = get_jcr_for_authentication(name);
      if (!jcr) {
        Dmsg1(50, "Authentication for job %.*s not possible\n", name_len,
              name_ptr);
        return nullptr;
      }

      Dmsg1(50, "Found Job %.*s\n", name_len, name_ptr);

      auto& data = storage.emplace();
      data.jcr = jcr;

      data.job = *myself;
      data.job.password_.value = jcr->sd_auth_key;

      return &data.job;
    } break;
    default: {
    } break;
  }

  auto type_name = global_resource::GetNameFromType(auth_type);
  Dmsg1(60, "Unknown login: %.*s::%.*s\n", (int)type_name.size(),
        type_name.data(), name_len, name_ptr);
  return nullptr;
}
} /* namespace filedaemon */
