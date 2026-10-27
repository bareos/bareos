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

#include "lib/tls.h"
#include "lib/tls/openssl.h"

std::unique_ptr<Tls> Tls::CreateClientContext(Tls::ImplementationType type,
                                              const TlsResource* tls,
                                              const PskCredentials* creds)
{
  switch (type) {
    case ImplementationType::kOpenSsl: {
      return make_openssl_client_tls(tls, creds);
    }
    case ImplementationType::kUnknown:
      [[fallthrough]];
    default:
      return {};
  }
}

std::unique_ptr<Tls> Tls::CreateServerContext(Tls::ImplementationType type,
                                              const TlsResource* tls,
                                              TlsConfigProvider* config)
{
  switch (type) {
    case ImplementationType::kOpenSsl: {
      return make_openssl_server_tls(tls, config);
    }
    case ImplementationType::kUnknown:
      [[fallthrough]];
    default:
      return {};
  }
}
