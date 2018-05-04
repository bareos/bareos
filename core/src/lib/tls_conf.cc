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

TlsCert::~TlsCert() {
   if (AllowedCns) {
      delete AllowedCns;
      AllowedCns = nullptr;
   }
}

uint32_t TlsCert::GetPolicy() const {
   uint32_t result = TlsBase::BNET_TLS_NONE;
   if (enable) {
      result = TlsBase::BNET_TLS_ENABLED;
   }
   if (require) {
      result = TlsBase::BNET_TLS_REQUIRED | TlsBase::BNET_TLS_ENABLED;
   }
   return result << TlsCert::policy_offset;
}

TlsPsk::~TlsPsk() {
   if (cipherlist != nullptr) {
      free(cipherlist);
   }
}

uint32_t TlsPsk::GetPolicy() const {
   uint32_t result = TlsBase::BNET_TLS_NONE;
   if (enable) {
      result = TlsBase::BNET_TLS_ENABLED;
   }
   if (require) {
      result = TlsBase::BNET_TLS_REQUIRED | TlsBase::BNET_TLS_ENABLED;
   }

   return result << TlsPsk::policy_offset;
}
