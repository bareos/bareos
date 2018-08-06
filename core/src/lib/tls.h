/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2009 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU Lesser General
   Public License as published by the Free Software Foundation plus
   additions in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Lesser Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * tls.h TLS support functions
 *
 * Author: Landon Fuller <landonf@threerings.net>
 */

#ifndef BAREOS_LIB_TLS_H_
#define BAREOS_LIB_TLS_H_

#include "lib/tls_psk_credentials.h"

typedef std::shared_ptr<PskCredentials> sharedPskCredentials;
class BareosSocket;

class Tls
{
public:
   Tls();
   ~Tls();

   enum class TlsImplementationType { kTlsUnknown, kTlsOpenSsl, kTlsGnuTls };

/* beachten: Code teilweise in constructor oder fabric verschieben */
   virtual DLL_IMP_EXP std::shared_ptr<Tls> new_tls_context(const char *CaCertfile, const char *CaCertdir,
                             const char *crlfile, const char *certfile,
                             const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata, const char *dhfile,
                             const char *cipherlist, bool VerifyPeer) = 0;
   virtual DLL_IMP_EXP Tls *new_tls_connection(std::shared_ptr<Tls> ctx, int fd, bool server) = 0;
   virtual DLL_IMP_EXP void FreeTlsConnection(Tls *tls_conn) = 0;
   virtual DLL_IMP_EXP void FreeTlsContext(std::shared_ptr<Tls> &ctx) = 0;
/* ********************* */

   virtual DLL_IMP_EXP void SetTlsPskClientContext(const char *cipherlist, const PskCredentials &credentials) = 0;
   virtual DLL_IMP_EXP void SetTlsPskServerContext(const char *cipherlist, const PskCredentials &credentials) = 0;


/* beachten: tls_conn aus den Funktionsparamentern entfernen und die jeweiligen Klassenvariablen zusammenführen */
   virtual DLL_IMP_EXP bool TlsPostconnectVerifyHost(JobControlRecord *jcr, const char *host) = 0;
   virtual DLL_IMP_EXP bool TlsPostconnectVerifyCn(JobControlRecord *jcr, alist *verify_list) = 0;
/* ********************* */

   virtual DLL_IMP_EXP bool TlsBsockAccept(BareosSocket *bsock) = 0;
   virtual DLL_IMP_EXP int TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes) = 0;
   virtual DLL_IMP_EXP int TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes) = 0;
   virtual DLL_IMP_EXP bool TlsBsockConnect(BareosSocket *bsock) = 0;
   virtual DLL_IMP_EXP void TlsBsockShutdown(BareosSocket *bsock) = 0;
   virtual DLL_IMP_EXP void TlsLogConninfo(JobControlRecord *jcr, const char *host, int port, const char *who) const = 0;
};

DLL_IMP_EXP std::unique_ptr<Tls> CreateNewTlsContext(Tls::TlsImplementationType type);

#endif /* BAREOS_LIB_TLS_H_ */
