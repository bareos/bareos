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

uint32_t GetLocalTlsPolicyFromConfiguration(TlsResource *tls_resource)
{
  uint32_t local_policy = TlsConfigBase::BNET_TLS_NONE;

#if defined(HAVE_TLS)
  local_policy = tls_resource->tls_cert.GetPolicy(); /* backward compatibility: before 18.2 never psk */
  Dmsg1(100, "GetLocalTlsPolicyFromConfiguration: %u\n", local_policy);
#else
  Dmsg1(100, "Ignore configuration no tls compiled in: %u\n", local_policy);
#endif
  return local_policy;
}

int SelectTlsPolicy(TlsResource *tls_resource, uint32_t remote_policy)
{
  if (remote_policy == TlsConfigBase::BNET_TLS_AUTO) {
    return TlsConfigBase::BNET_TLS_AUTO;
  }
  uint32_t local_policy = GetLocalTlsPolicyFromConfiguration(tls_resource);

  if ((remote_policy == 0 && local_policy == 0) || (remote_policy == 0 && local_policy == 1) ||
      (remote_policy == 1 && local_policy == 0)) {
    return TlsConfigBase::BNET_TLS_NONE;
  }
  if ((remote_policy == 0 && local_policy == 2) || (remote_policy == 2 && local_policy == 0)) {
    return TlsConfigBase::BNET_TLS_DENY;
  }
  return TlsConfigBase::BNET_TLS_ENABLED;
}

TlsConfigBase *SelectTlsFromPolicy(TlsResource *tls_resource, uint32_t remote_policy)
{
  return &tls_resource->tls_cert;
}
