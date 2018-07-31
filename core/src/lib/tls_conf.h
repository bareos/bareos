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

/*
 * TLS enabling values. Value is important for comparison, ie:
 * if (tls_remote_policy < BNET_TLS_CERTIFICATE_REQUIRED) { ... }

 cert allowed    cert required    psk allowed    psk-required        illegal    combination name
0    0    0    0            none
0    0    0    1        x
0    0    1    0            psk allowed
0    0    1    1            psk required
0    1    0    0        x
0    1    0    1        x
0    1    1    0        x
0    1    1    1        x
1    0    0    0            cert allowed
1    0    0    1        x
1    0    1    0            both allowed
1    0    1    1        x
1    1    0    0            cert required
1    1    0    1        x
1    1    1    0        x
1    1    1    1        x

 * This bitfield has following valid combinations:
                        none         cert allowed    cert required    both allowed    psk allowed     psk required
    none               plain           plain         no connection     plain           plain          no connection
    cert allowed       plain           cert             cert           cert          no connection    no connection
    cert required   no connection      cert             cert           cert          no connection    no connection
    both allowed       plain           cert             cert           cert            psk               psk
    psk allowed        plain        no connection    no connection     psk             psk               psk
    psk required    no connection   no connection    no connection     psk             psk               psk
 */


#include "lib/tls_psk_credentials.h"
#include "lib/tls_conf_base.h"
#include "lib/tls_conf_cert.h"
#include "lib/tls_conf_psk.h"
#include "lib/tls_conf_none.h"

class TlsResource;

uint32_t GetLocalTlsPolicyFromConfiguration(TlsResource *tls_configuration);
TlsConfigBase *SelectTlsFromPolicy(TlsResource *tls_configuration, uint32_t remote_policy);

#endif //BAREOS_LIB_TLS_CONF_H_
