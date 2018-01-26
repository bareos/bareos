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

/**
 * tls configuration
 */
typedef enum {
   BNET_TLS_NONE = 0,      /*!< cannot do TLS */
   BNET_TLS_ALLOWED = 1 << 0, /*!< TLS with certificates is allowed but not required on my end */
   BNET_TLS_REQUIRED = 1 << 1, /*!< TLS with certificates is required */
} Policy_e;

tls_cert_t::~tls_cert_t() {
   if (allowed_cns) {
      delete allowed_cns;
      allowed_cns = nullptr;
   }
}

uint32_t tls_cert_t::GetPolicy() const {
   uint32_t result = Policy_e::BNET_TLS_NONE;
   if (enable) {
      result = BNET_TLS_ENABLED;
   }
   if (require) {
      result = BNET_TLS_REQUIRED;
   }
   return result << tls_cert_t::policy_offset;
}

tls_psk_t::~tls_psk_t() {
   if (cipherlist != nullptr) {
      free(cipherlist);
   }
}

uint32_t tls_psk_t::GetPolicy() const {
   uint32_t result = BNET_TLS_NONE;
   if (enable) {
      result = BNET_TLS_ENABLED;
   } else if (require) {
      result = BNET_TLS_REQUIRED;
   }

   return result << tls_psk_t::policy_offset;
}
