/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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

#include "lib/tls_conf_cert.h"
#include "lib/s_password.h"
#include <cstdint>

enum TlsPolicy : uint32_t
{
  kBnetTlsNone = 0,    /*!< No TLS configured */
  kBnetTlsEnabled = 1, /*!< TLS with certificates is allowed but not required */
  kBnetTlsRequired = 2,  /*!< TLS with certificates is required */
  kBnetTlsAuto = 4,      /*!< TLS mode will be negotiated by ssl handshake */
  kBnetTlsUnknown = 0xFE /*!< initializer constant */
};

class TlsResource {
 public:
  s_password password_;      /* UA server password */
  TlsConfigCert tls_cert_;   /* TLS structure */
  std::string cipherlist_;   /* TLS Cipher List */
  std::string ciphersuites_; /* TLS v1.3 Cipher Suites */
  std::string protocol_;
  bool authenticate_{false}; /* Authenticate only with TLS */


  /* Do not get confused by these variables
   * tls_require is only respected for clients[0] and old consoles.
   * for everything else tls_enable_ acts as tls_require_,
   * at least when they are set via configuration, in the sense that
   * non-tls connections will get rejected.
   *
   * [0] keep in mind that clients only authenticate themselves as clients,
   *     during client-initiated connections to the director.  When they connect
   *     to the storage, they identify themselves as jobs, so this exception
   *     does not happen.
   */

  bool tls_enable_{false};
  bool tls_require_{false};

  bool IsTlsConfigured() const;
  TlsPolicy GetPolicy() const;
};

enum class TlsStatus
{
  Error,
  Disabled,
  Enabled,
};

// Given two parties (left, right) with their respective TlsPolicies,
// this checks whether both parties will use or not use tls, or if no
// common setting exists
TlsStatus select_tls_status(TlsPolicy left, TlsPolicy right);

#endif  // BAREOS_LIB_TLS_CONF_H_
