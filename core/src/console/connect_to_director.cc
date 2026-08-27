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

#include "include/bareos.h"
#include "console/console_conf.h"
#include "console/console_globals.h"
#include "console/console_output.h"
#include "console/console_conf.h"
#include "include/jcr.h"
#include "lib/global_resource.h"
#include "lib/bstringlist.h"
#include "lib/bsock_tcp.h"
#include "lib/version.h"
#include "lib/default_console.h"

namespace console {
BareosSocket* ConnectToDirector(JobControlRecord& jcr,
                                utime_t heart_beat,
                                BStringList& response_args,
                                uint32_t& response_id)
{
  BareosSocketTCP* UA_sock = new BareosSocketTCP;
  if (!UA_sock->connect(NULL, 5, 15, heart_beat, "Director daemon",
                        director_resource->address, NULL,
                        director_resource->DIRport, false)) {
    delete UA_sock;
    UA_sock = nullptr;
    return nullptr;
  }
  jcr.dir_bsock = UA_sock;

  const char* name;

  TlsResource* local_tls_resource;
  if (console_resource) {
    name = console_resource->resource_name_;
    ASSERT(console_resource->password_.encoding == p_encoding_md5);
    local_tls_resource = console_resource;
  } else { /* default console */
    name = DEFAULT_CONSOLE_NAME;
    ASSERT(director_resource->password_.encoding == p_encoding_md5);
    local_tls_resource = director_resource;
  }

  std::string qualified_resource_name
      = global_resource::QualifiedName(global_resource::Type::Console, name);

  std::string cpy{name};
  BashSpaces(cpy.data());
  PoolMem hello_msg;
  hello_msg.bsprintf("Hello %s calling version %s Version=\"%u.%u.%u\"\n",
                     cpy.c_str(), kBareosVersionStrings.Full,
                     kBareosVersion.Major, kBareosVersion.Minor,
                     kBareosVersion.Patch);

  if (!BareosConnect(&jcr, UA_sock, qualified_resource_name, local_tls_resource,
                     hello_msg.c_str())) {
    delete UA_sock;
    UA_sock = nullptr;
    jcr.dir_bsock = nullptr;
    return nullptr;
  }

  if (!UA_sock->ReceiveAndEvaluateResponseMessage(response_id, response_args)) {
    delete UA_sock;
    UA_sock = nullptr;
    jcr.dir_bsock = nullptr;
    return nullptr;
  }

  return UA_sock;
}

} /* namespace console */
