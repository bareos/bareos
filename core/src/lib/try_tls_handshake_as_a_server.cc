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
#include "lib/configured_tls_policy_getter.h"
#include "lib/parse_conf.h"

enum class ConnectionHandshakeMode
{
  PerformTlsHandshake,
  PerformCleartextHandshake,
  CloseConnection
};

static ConnectionHandshakeMode GetHandshakeMode(
    BareosSocket* bs,
    const ConfigurationParser& config)
{
  bool cleartext_hello;
  std::string client_name;
  std::string r_code_str;
  BareosVersionNumber version = BareosVersionNumber::kUndefined;

  if (!bs->EvaluateCleartextBareosHello(cleartext_hello, client_name,
                                        r_code_str, version)) {
    Dmsg0(100, "Error occured when trying to peek cleartext hello\n");
    return ConnectionHandshakeMode::CloseConnection;
  }

  bs->connected_daemon_version_ = static_cast<BareosVersionNumber>(version);

  if (cleartext_hello) {
    ConfiguredTlsPolicyGetter tls_policy_getter(config);
    TlsPolicy tls_policy;
    if (!tls_policy_getter.GetConfiguredTlsPolicyFromCleartextHello(
            r_code_str, client_name, tls_policy)) {
      Dmsg0(200, "Could not read out cleartext configuration\n");
      return ConnectionHandshakeMode::CloseConnection;
    }
    if (r_code_str == std::string("R_CLIENT")) {
      if (tls_policy == kBnetTlsRequired) {
        return ConnectionHandshakeMode::CloseConnection;
      } else { /* kBnetTlsNone or kBnetTlsEnabled */
        return ConnectionHandshakeMode::PerformCleartextHandshake;
      }
    } else if (r_code_str == std::string("R_CONSOLE") &&
               version < BareosVersionNumber::kRelease_18_2) {
      return ConnectionHandshakeMode::PerformCleartextHandshake;
    } else {
      if (tls_policy == kBnetTlsNone) {
        return ConnectionHandshakeMode::PerformCleartextHandshake;
      } else {
        Dmsg1(200,
              "Connection to %s will be denied due to configuration mismatch\n",
              client_name.c_str());
        return ConnectionHandshakeMode::CloseConnection;
      }
    }
  } else { /* not cleartext */
    return ConnectionHandshakeMode::PerformTlsHandshake;
  } /* if (cleartext_hello) */
}

bool TryTlsHandshakeAsAServer(BareosSocket* bs, ConfigurationParser* config)
{
  ASSERT(config);
  ConnectionHandshakeMode mode = GetHandshakeMode(bs, *config);

  bool success = false;

  switch (mode) {
    case ConnectionHandshakeMode::PerformTlsHandshake:
      if (bs->DoTlsHandshakeAsAServer(config)) { success = true; }
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
