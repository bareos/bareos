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
#include "include/version_hex.h"

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

TlsResource* Auth::parse(std::string_view hello)
{
  char name[MAX_NAME_LENGTH];
  char tbuf[MAX_TIME_LENGTH];

  unsigned major{}, minor{}, patch{};

  std::string cpy{hello};

  auto* myself
      = dynamic_cast<ClientResource*>(p->GetNextRes(R_CLIENT, nullptr));

  if (!myself) {
    Emsg1(M_ERROR, 0, "Could not find myself during connection attempt.\n");
    return nullptr;
  }

  if (bsscanf(cpy.c_str(),
              "Hello Storage calling Start Job %127s Version=\"%u.%u.%u\"",
              name, &major, &minor, &patch)
          == 4

      || bsscanf(cpy.c_str(), "Hello Storage calling Start Job %127s", name)
             == 1) {
    Dmsg1(110, "Got a SD connection at %s\n",
          bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));

    type = inbound_type::Storage;
    remote_version = VERSION_HEX(major, minor, patch);

    auto* jcr = get_jcr_by_full_name(name);
    if (!jcr) {
      Jmsg1(NULL, M_FATAL, 0, T_("SD connect failed: Job name not found: %s\n"),
            name);
      Dmsg1(3, "**** Job \"%s\" not found.\n", name);
      return nullptr;
    }

    Dmsg1(50, "Found Job %s\n", name);

    if (jcr->authenticated) {
      Jmsg2(jcr, M_FATAL, 0,
            T_("Hey!!!! JobId %u Job %s already authenticated.\n"),
            (uint32_t)jcr->JobId, jcr->Job);
      Dmsg2(50, "Hey!!!! JobId %u Job %s already authenticated.\n",
            (uint32_t)jcr->JobId, jcr->Job);
      FreeJcr(jcr);
      return nullptr;
    }

    if (bstrcmp(jcr->sd_auth_key, "dummy")) {
      Jmsg2(jcr, M_FATAL, 0, T_("Hey!!!! JobId %u Job %s has bad auth key.\n"),
            (uint32_t)jcr->JobId, jcr->Job);
      Dmsg2(50, "Hey!!!! JobId %u Job %s has bad auth key.\n",
            (uint32_t)jcr->JobId, jcr->Job);
      FreeJcr(jcr);
      return nullptr;
    }

    auto& data = storage.emplace();
    data.jcr = jcr;

    data.job = *myself;
    data.job.password_.value = jcr->sd_auth_key;

    return &data.job;
  } else if (bsscanf(cpy.c_str(),
                     "Hello Director %127s calling Version=\"%u.%u.%u\"", name,
                     &major, &minor, &patch)
                 == 4
             || bsscanf(cpy.c_str(), "Hello Director %127s calling", name)
                    == 1) {
    Dmsg1(110, "Got a DIR connection at %s\n",
          bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));

    type = inbound_type::Director;
    remote_version = VERSION_HEX(major, minor, patch);

    UnbashSpaces(name);

    auto* res
        = dynamic_cast<DirectorResource*>(p->GetResWithName(R_DIRECTOR, name));
    if (!res) {
      Dmsg1(60, "Got a DIR connection at %s from unknown dir %s\n",
            bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)), name);
      return nullptr;
    }

    if (res->password_.encoding != p_encoding_md5) {
      Emsg1(M_ERROR, 0, "Bad password type for dir %s\n", name);
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
  } else {
    Dmsg1(60, "Bad hello message: %s\n", cpy.c_str());
    return nullptr;
  }
}
} /* namespace filedaemon */
