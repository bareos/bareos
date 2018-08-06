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

#ifndef BAREOS_LIB_TLS_GNUTLS_H_
#define BAREOS_LIB_TLS_GNUTLS_H_

#include "lib/tls.h"

class TlsGnuTls : public Tls
{
public:
/* derived publice interfaces */
   DLL_IMP_EXP std::shared_ptr<Tls> new_tls_context(const char *CaCertfile, const char *CaCertdir,
                             const char *crlfile, const char *certfile,
                             const char *keyfile,
                             CRYPTO_PEM_PASSWD_CB *pem_callback,
                             const void *pem_userdata, const char *dhfile,
                             const char *cipherlist, bool VerifyPeer) override;
   DLL_IMP_EXP TLS_CONNECTION_CONTEXT *new_tls_connection(std::shared_ptr<Tls> ctx, int fd, bool server) override;
   DLL_IMP_EXP void FreeTlsConnection() override;
   DLL_IMP_EXP void FreeTlsContext(std::shared_ptr<Tls> &ctx) override;
   DLL_IMP_EXP bool TlsPostconnectVerifyHost(JobControlRecord *jcr, const char *host) override;
   DLL_IMP_EXP bool TlsPostconnectVerifyCn(JobControlRecord *jcr, alist *verify_list) override;

   DLL_IMP_EXP bool TlsBsockAccept(BareosSocket *bsock) override;
   DLL_IMP_EXP int TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes) override;
   DLL_IMP_EXP int TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes) override;
   DLL_IMP_EXP bool TlsBsockConnect(BareosSocket *bsock) override;
   DLL_IMP_EXP void TlsBsockShutdown(BareosSocket *bsock) override;
};

#endif /* BAREOS_LIB_TLS_GNUTLS_H_ */
