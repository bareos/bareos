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
#ifndef BAREOS_LIB_TLS_OPENSSL_H_
#define BAREOS_LIB_TLS_OPENSSL_H_

#include "lib/tls.h"

#include <bareos.h>
#include <memory>

class TlsOpenSslPrivate;

class TlsOpenSsl : public Tls {
 public:
  TlsOpenSsl();
  virtual ~TlsOpenSsl();

  DLL_IMP_EXP bool init() override;

  DLL_IMP_EXP bool TlsPostconnectVerifyHost(JobControlRecord *jcr, const char *host) override;
  DLL_IMP_EXP bool TlsPostconnectVerifyCn(JobControlRecord *jcr,
                                          const std::vector<std::string> &verify_list) override;

  DLL_IMP_EXP bool TlsBsockAccept(BareosSocket *bsock) override;
  DLL_IMP_EXP int TlsBsockWriten(BareosSocket *bsock, char *ptr, int32_t nbytes) override;
  DLL_IMP_EXP int TlsBsockReadn(BareosSocket *bsock, char *ptr, int32_t nbytes) override;
  DLL_IMP_EXP bool TlsBsockConnect(BareosSocket *bsock) override;
  DLL_IMP_EXP void TlsBsockShutdown(BareosSocket *bsock) override;

  DLL_IMP_EXP std::string TlsCipherGetName() const;
  DLL_IMP_EXP void SetCipherList(const std::string &cipherlist) override;
  DLL_IMP_EXP void TlsLogConninfo(JobControlRecord *jcr,
                                  const char *host,
                                  int port,
                                  const char *who) const override;
  DLL_IMP_EXP void SetTlsPskClientContext(const PskCredentials &credentials) override;
  DLL_IMP_EXP void SetTlsPskServerContext(GetTlsPskByFullyQualifiedResourceNameCb_t cb) override;

  DLL_IMP_EXP void SetCaCertfile(const std::string &ca_certfile) override;
  DLL_IMP_EXP void SetCaCertdir(const std::string &ca_certdir) override;
  DLL_IMP_EXP void SetCrlfile(const std::string &crlfile) override;
  DLL_IMP_EXP void SetCertfile(const std::string &certfile) override;
  DLL_IMP_EXP void SetKeyfile(const std::string &keyfile) override;
  DLL_IMP_EXP void SetPemCallback(CRYPTO_PEM_PASSWD_CB pem_callback) override;
  DLL_IMP_EXP void SetPemUserdata(void *pem_userdata) override;
  DLL_IMP_EXP void SetDhFile(const std::string &dhfile) override;
  DLL_IMP_EXP void SetVerifyPeer(const bool &verify_peer) override;
  DLL_IMP_EXP void SetTcpFileDescriptor(const int &fd) override;

 private:
  std::unique_ptr<TlsOpenSslPrivate> d_; /* private data */
};
#endif  // BAREOS_LIB_TLS_OPENSSL_H_
