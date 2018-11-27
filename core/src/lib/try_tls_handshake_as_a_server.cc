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
#include "try_tls_handshake_as_a_server.h"

#include "lib/bsock_tcp.h"

enum class ConnectionHandshakeMode {
  PerformTlsHandshake,
  PerformCleartextHandshake,
  CloseConnection
};

static ConnectionHandshakeMode GetHandshakeMode(BareosSocket *bs,
                                                ConfigurationParser *config)
{
  bool cleartext_hello;
  std::string client_name;
  std::string r_code_str;

  if (!bs->EvaluateCleartextBareosHello(cleartext_hello,
                                        client_name,
                                        r_code_str)) {
    Dmsg0(100, "Error occured when trying to peek cleartext hello\n");
    return ConnectionHandshakeMode::CloseConnection;
  }

  if (cleartext_hello) {
    TlsPolicy tls_policy;
    if (!config->GetConfiguredTlsPolicy(r_code_str, client_name, tls_policy)) {
      Dmsg0(100, "Could not read out cleartext configuration\n");
      return ConnectionHandshakeMode::CloseConnection;
    }
    if (r_code_str == std::string("R_CLIENT")) {
      if (tls_policy == kBnetTlsRequired) {
        return ConnectionHandshakeMode::CloseConnection;
      } else { /* kBnetTlsNone or kBnetTlsEnabled */
        return ConnectionHandshakeMode::PerformCleartextHandshake;
      }
    } else { /* not R_CLIENT */
      if (tls_policy == kBnetTlsNone) {
        return ConnectionHandshakeMode::PerformCleartextHandshake;
      } else {
        return ConnectionHandshakeMode::CloseConnection;
      }
    }
  } else { /* not cleartext */
    return ConnectionHandshakeMode::PerformTlsHandshake;
  } /* if (cleartext_hello) */
}

bool TryTlsHandshakeAsAServer(BareosSocket *bs, ConfigurationParser *config)
{
   ConnectionHandshakeMode mode = GetHandshakeMode(bs, config);

   bool success = false;

   switch(mode) {
   case ConnectionHandshakeMode::PerformTlsHandshake:
      if (bs->DoTlsHandshakeAsAServer(config)) {
        success = true;
      }
      break;
   case ConnectionHandshakeMode::PerformCleartextHandshake:
      /* do tls handshake later */
      success = true;
      break;
   default:
   case ConnectionHandshakeMode::CloseConnection:
      success = false;
      break;
   }

   return success;
}
