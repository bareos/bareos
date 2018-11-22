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

#ifndef BAREOS_LIB_TLS_CONF_H_
#define BAREOS_LIB_TLS_CONF_H_

#include "lib/tls_psk_credentials.h"
#include "lib/tls_conf_cert.h"
#include "lib/bareos_resource.h"
#include "lib/s_password.h"

enum TlsPolicy : uint32_t
{
  kBnetTlsNone     = 0,   /*!< No TLS configured */
  kBnetTlsEnabled  = 1,   /*!< TLS with certificates is allowed but not required */
  kBnetTlsRequired = 2,   /*!< TLS with certificates is required */
  kBnetTlsAuto     = 4,   /*!< TLS mode will be negotiated by ssl handshake */
  kBnetTlsDeny     = 0xFF /*!< TLS connection not allowed */
};

class TlsResource : public BareosResource {
 public:
  s_password password_;     /* UA server password */
  TlsConfigCert tls_cert_;  /* TLS structure */
  std::string *cipherlist_; /* TLS Cipher List */
  bool authenticate_;       /* Authenticate only with TLS */
  bool tls_enable_;
  bool tls_require_;

  TlsResource();
  bool IsTlsConfigured() const;
  TlsPolicy GetPolicy() const;
  int SelectTlsPolicy(TlsPolicy remote_policy) const;
};

#endif  // BAREOS_LIB_TLS_CONF_H_
