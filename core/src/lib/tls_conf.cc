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
#include "lib/tls_conf.h"

TlsResource::TlsResource()
 : cipherlist_(nullptr)
 , authenticate_(false)
 , tls_enable_(false)
 , tls_require_(false)
{
  return;
}

bool TlsResource::IsTlsConfigured() const
{
  return tls_enable_ || tls_require_;
}

TlsPolicy TlsResource::GetPolicy() const
{
 TlsPolicy result = TlsPolicy::kBnetTlsNone;
 if (tls_enable_) {
    result = TlsPolicy::kBnetTlsEnabled;
 }
 if (tls_require_) {
    result = TlsPolicy::kBnetTlsRequired;
 }
 return result;
}

int TlsResource::SelectTlsPolicy(TlsPolicy remote_policy) const
{
  if (remote_policy == TlsPolicy::kBnetTlsAuto) {
    return TlsPolicy::kBnetTlsAuto;
  }
  TlsPolicy local_policy = GetPolicy();

  if ((remote_policy == 0 && local_policy == 0) || (remote_policy == 0 && local_policy == 1) ||
      (remote_policy == 1 && local_policy == 0)) {
    return TlsPolicy::kBnetTlsNone;
  }
  if ((remote_policy == 0 && local_policy == 2) || (remote_policy == 2 && local_policy == 0)) {
    return TlsPolicy::kBnetTlsDeny;
  }
  return TlsPolicy::kBnetTlsEnabled;
}
