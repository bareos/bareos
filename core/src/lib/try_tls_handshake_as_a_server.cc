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

static bool CheckForCleartextConnection(BareosSocket *bs, ConfigurationParser *config, bool &do_cleartext)
{
  bool cleartext_requested;
  if (!bs->EvaluateCleartextBareosHello(cleartext_requested)) {
    Dmsg0(100, "Could not read out cleartext hello\n");
    return false;
  }

  bool cleartext_configured;
  if (!config->GetCleartextConfigured(cleartext_configured)) {
    Dmsg0(100, "Could not read out cleartext configuration\n");
    return false;
  }

  if (cleartext_requested && !cleartext_configured) {
    Dmsg0(100, "Client wants cleartext connection but tls is configured\n");
    return false;
  }

  if (!cleartext_requested && cleartext_configured) {
    Dmsg0(100, "Client wants tls connection but cleartext is configured\n");
    return false;
  }
  Dmsg0(100, "Client and Server want cleartext connection\n");
  do_cleartext = cleartext_configured; /* this covers the other two cases */
  return true;
}

bool TryTlsHandshakeAsAServer(BareosSocket *bs, ConfigurationParser *config)
{
   bool cleartext;
   if (!CheckForCleartextConnection(bs, config, cleartext)) {
     return false;
   }

   if (!cleartext) {
     if (!bs->DoTlsHandshakeAsAServer(config)) {
       return false;
     }
   }
   /* cleartext - no Tls Handshake */
   return true;
}
