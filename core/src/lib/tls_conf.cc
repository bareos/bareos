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

DLL_IMP_EXP uint32_t GetLocalTlsPolicyFromConfiguration(TlsResource *tls_configuration) {
   uint32_t merged_policy = 0;

#if defined(HAVE_TLS)
   merged_policy = tls_configuration->tls_cert.GetPolicy() | tls_configuration->tls_psk.GetPolicy();
   Dmsg1(100, "GetLocalTlsPolicyFromConfiguration: %u\n", merged_policy);
#else
   Dmsg1(100, "Ignore configuration no tls compiled in: %u\n", merged_policy);
#endif
   return merged_policy;
}

TlsBase *SelectTlsFromPolicy(
   TlsResource *tls_configuration, uint32_t remote_policy) {

   if ((tls_configuration->tls_cert.require && TlsCert::enabled(remote_policy))
      || (tls_configuration->tls_cert.enable && TlsCert::required(remote_policy))) {
      Dmsg0(100, "SelectTlsFromPolicy: take required cert\n");

      // one requires the other accepts cert
      return &(tls_configuration->tls_cert);
   }
   if ((tls_configuration->tls_psk.require && TlsPsk::enabled(remote_policy))
      || (tls_configuration->tls_psk.enable && TlsPsk::required(remote_policy))) {

      Dmsg0(100, "SelectTlsFromPolicy: take required  psk\n");
      // one requires the other accepts psk
      return &(tls_configuration->tls_psk);
   }
   if (tls_configuration->tls_cert.enable && TlsCert::enabled(remote_policy)) {

      Dmsg0(100, "SelectTlsFromPolicy: take cert\n");
      // both accept cert
      return &(tls_configuration->tls_cert);
   }
   if (tls_configuration->tls_psk.enable && TlsPsk::enabled(remote_policy)) {

      Dmsg0(100, "SelectTlsFromPolicy: take psk\n");
      // both accept psk
      return &(tls_configuration->tls_psk);
   }

   Dmsg0(100, "SelectTlsFromPolicy: take cleartext\n");
   // fallback to cleartext
   return nullptr;
}
