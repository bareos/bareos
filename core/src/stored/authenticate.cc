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
#include "lib/parse_conf.h"
#include "lib/bsock.h"
#include "lib/bnet_network_dump.h"
#include "lib/util.h"
#include "include/version_hex.h"

namespace storagedaemon {

TlsResource* Auth::parse(std::string_view hello)
{
  char name[MAX_NAME_LENGTH];
  char tbuf[MAX_TIME_LENGTH];

  unsigned major = 0;
  unsigned minor = 0;
  unsigned patch = 0;

  std::string cpy{hello};

  auto* myself
      = dynamic_cast<StorageResource*>(p->GetNextRes(R_STORAGE, nullptr));

  if (!myself) {
    Emsg1(M_ERROR, 0, "Could not find myself during connection attempt.\n");
    return nullptr;
  }

  if (bsscanf(cpy.c_str(), "Hello Start Job %127s Version=\"%u.%u.%u\"", name,
              &major, &minor, &patch)
          == 4
      || bsscanf(cpy.c_str(), "Hello Start Job %127s", name) == 1) {
    Dmsg1(110, "Got a FD connection at %s\n",
          bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));

    type = inbound_type::Client;
    remote_version = VERSION_HEX(major, minor, patch);

    auto* jcr = get_jcr_for_authentication(name);
    if (!jcr) {
      Dmsg1(50, "Authentication for job %s not possible\n", name);
      return nullptr;
    }

    auto& data = client.emplace();
    data.jcr = jcr;
    data.client = *myself;
    data.client.password_.value = jcr->sd_auth_key;

    // we cannot require tls, as old clients only do cleartext auth
    // and tls_require prevents this
    data.client.tls_require_ = false;

    return &data.client;
  } else if (bsscanf(cpy.c_str(),
                     "Hello Start Storage Job %127s Version=\"%u.%u.%u\"", name,
                     &major, &minor, &patch)
                 == 4
             || bsscanf(cpy.c_str(), "Hello Start Storage Job %127s", name)
                    == 1) {
    Dmsg1(110, "Got a SD connection at %s\n",
          bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));

    type = inbound_type::Storage;
    remote_version = VERSION_HEX(major, minor, patch);

    auto* jcr = get_jcr_for_authentication(name);
    if (!jcr) {
      Dmsg1(50, "Authentication for job %s not possible\n", name);
      return nullptr;
    }

    auto& data = storage.emplace();
    data.jcr = jcr;
    data.storage = *myself;
    data.storage.password_.value = jcr->sd_auth_key;

    return &data.storage;
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

    return res;
  } else {
    Dmsg1(60, "Bad hello message: %s\n", cpy.c_str());
    return nullptr;
  }
}
} /* namespace storagedaemon */
