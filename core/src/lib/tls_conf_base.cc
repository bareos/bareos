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

int SelectTlsPolicy(TlsResource *tls_resource, uint32_t remote_policy)
{
  if (remote_policy == TlsConfigBase::BNET_TLS_AUTO) {
    return TlsConfigBase::BNET_TLS_AUTO;
  }
  uint32_t local_policy = tls_resource->GetPolicy();

  if ((remote_policy == 0 && local_policy == 0) || (remote_policy == 0 && local_policy == 1) ||
      (remote_policy == 1 && local_policy == 0)) {
    return TlsConfigBase::BNET_TLS_NONE;
  }
  if ((remote_policy == 0 && local_policy == 2) || (remote_policy == 2 && local_policy == 0)) {
    return TlsConfigBase::BNET_TLS_DENY;
  }
  return TlsConfigBase::BNET_TLS_ENABLED;
}
