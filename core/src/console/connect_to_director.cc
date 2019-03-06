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

#include "include/bareos.h"
#include "console/console_conf.h"
#include "console/console_globals.h"
#include "console/console_output.h"
#include "console/console_conf.h"
#include "include/jcr.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/bstringlist.h"
#include "lib/bsock_tcp.h"

namespace console {
BareosSocket* ConnectToDirector(JobControlRecord& jcr,
                                utime_t heart_beat,
                                BStringList& response_args,
                                uint32_t& response_id)
{
  BareosSocketTCP* UA_sock = New(BareosSocketTCP);
  if (!UA_sock->connect(NULL, 5, 15, heart_beat, "Director daemon",
                        director_resource->address, NULL,
                        director_resource->DIRport, false)) {
    delete UA_sock;
    return nullptr;
  }
  jcr.dir_bsock = UA_sock;

  const char* name;
  s_password* password = NULL;

  TlsResource* local_tls_resource;
  if (console_resource) {
    name = console_resource->name();
    ASSERT(console_resource->password.encoding == p_encoding_md5);
    password = &console_resource->password;
    local_tls_resource = console_resource;
  } else { /* default console */
    name = "*UserAgent*";
    ASSERT(director_resource->password_.encoding == p_encoding_md5);
    password = &director_resource->password_;
    local_tls_resource = director_resource;
  }

  if (local_tls_resource->IsTlsConfigured()) {
    std::string qualified_resource_name;
    if (!my_config->GetQualifiedResourceNameTypeConverter()->ResourceToString(
            name, my_config->r_own_, qualified_resource_name)) {
      delete UA_sock;
      return nullptr;
    }

    if (!UA_sock->DoTlsHandshake(TlsPolicy::kBnetTlsAuto, local_tls_resource,
                                 false, qualified_resource_name.c_str(),
                                 password->value, &jcr)) {
      delete UA_sock;
      return nullptr;
    }
  } /* IsTlsConfigured */

  if (!UA_sock->ConsoleAuthenticateWithDirector(&jcr, name, *password,
                                                director_resource,
                                                response_args, response_id)) {
    delete UA_sock;
    return nullptr;
  }
  return UA_sock;
}

} /* namespace console */
