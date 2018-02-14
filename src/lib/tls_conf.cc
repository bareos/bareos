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
#include "bareos.h"
#include "tls_conf.h"

tls_cert_t::~tls_cert_t() {
   if (allowed_cns) {
      delete allowed_cns;
      allowed_cns = nullptr;
   }
}

uint32_t tls_cert_t::GetPolicy() const {
   uint32_t result = tls_base_t::BNET_TLS_NONE;
   if (enable) {
      result = tls_base_t::BNET_TLS_ENABLED;
   }
   if (require) {
      result = tls_base_t::BNET_TLS_REQUIRED | tls_base_t::BNET_TLS_ENABLED;
   }
   return result << tls_cert_t::policy_offset;
}

tls_psk_t::~tls_psk_t() {
   if (cipherlist != nullptr) {
      free(cipherlist);
   }
}

uint32_t tls_psk_t::GetPolicy() const {
   uint32_t result = tls_base_t::BNET_TLS_NONE;
   if (enable) {
      result = tls_base_t::BNET_TLS_ENABLED;
   }
   if (require) {
      result = tls_base_t::BNET_TLS_REQUIRED | tls_base_t::BNET_TLS_ENABLED;
   }

   return result << tls_psk_t::policy_offset;
}
