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

uint32_t TlsConfigCert::GetPolicy() const
{
   uint32_t result = TlsConfigBase::BNET_TLS_NONE;
   if (enable_) {
      result = TlsConfigBase::BNET_TLS_ENABLED;
   }
   if (require_) {
      result = TlsConfigBase::BNET_TLS_REQUIRED;
   }
   return result;
}

std::vector<std::string> TlsConfigCert::AllowedCertificateCommonNames() const
{
   std::vector<std::string> list;

   if (allowed_certificate_common_names_) {
      const char *s;
      foreach_alist(s, allowed_certificate_common_names_) {
         list.push_back(std::string(s));
      }
   }

   return list;
}

TlsConfigCert::~TlsConfigCert()
{
   if (allowed_certificate_common_names_) {
      delete allowed_certificate_common_names_;
      allowed_certificate_common_names_ = nullptr;
   }
}
