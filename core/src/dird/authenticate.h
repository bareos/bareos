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

#ifndef BAREOS_DIRD_AUTHENTICATE_H_
#define BAREOS_DIRD_AUTHENTICATE_H_

#include "dird/console_connection_lease.h"
#include "dird/ua.h"
#include "lib/bsock.h"
#include "dird/dird_conf.h"
#include "lib/parse_conf.h"

#include <string>
#include <memory>
#include <optional>

namespace directordaemon {

class StorageResource;
class UaContext;

bool AuthenticateWithFileDaemon(JobControlRecord* jcr);

struct DirectorAuth : ::TlsConfigProvider {
  DirectorAuth(std::shared_ptr<LoadedConfiguration> conf) : p{std::move(conf)}
  {
  }

  const TlsResource* get(global_resource::Type type,
                         std::string_view name) override;

  enum class inbound_type
  {
    Unknown,
    Client,
    Console
  };

  struct console_data {
    ConsoleResource* res{};
    ConsoleConnectionLease lease;

    // the default console is allowed to do everything
    bool is_default{false};

    TlsResource tls;
  };

  struct client_data {
    ClientResource* res;
  };

  inbound_type GetType() const { return type; }

  std::optional<console_data> console;
  std::optional<client_data> client;

 private:
  std::shared_ptr<LoadedConfiguration> p;
  inbound_type type{inbound_type::Unknown};
};

std::unique_ptr<UserAcl> AuthenticatePamUser(BareosSocket* socket,
                                             LoadedConfiguration* conf);

} /* namespace directordaemon */

#endif  // BAREOS_DIRD_AUTHENTICATE_H_
