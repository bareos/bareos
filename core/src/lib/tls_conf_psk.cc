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
#include "tls_conf.h"
#include "tls_openssl.h"

uint32_t TlsConfigPsk::GetPolicy() const
{
   uint32_t result = TlsConfigBase::BNET_TLS_NONE;
   if (enable) {
      result = TlsConfigBase::BNET_TLS_ENABLED;
   }
   if (require) {
      result = TlsConfigBase::BNET_TLS_REQUIRED | TlsConfigBase::BNET_TLS_ENABLED;
   }

   return result << TlsConfigPsk::policy_offset;
}

bool TlsConfigPsk::enabled(u_int32_t policy)
{
   return ((policy >> TlsConfigPsk::policy_offset) & BNET_TLS_ENABLED) == BNET_TLS_ENABLED;
}

bool TlsConfigPsk::required(u_int32_t policy)
{
   return ((policy >> TlsConfigPsk::policy_offset) & BNET_TLS_REQUIRED) == BNET_TLS_REQUIRED;
}

std::shared_ptr<TLS_CONTEXT> TlsConfigPsk::CreateClientContext() const
{
   ASSERT(psk_credentials_);
   return new_tls_psk_client_context(cipherlist, psk_credentials_);
}

std::shared_ptr<TLS_CONTEXT> TlsConfigPsk::CreateServerContext() const
{
   ASSERT(psk_credentials_);
   return new_tls_psk_server_context(cipherlist, psk_credentials_);
}

TlsConfigPsk::~TlsConfigPsk()
{
   if (cipherlist != nullptr) {
      free(cipherlist);
   }
}
