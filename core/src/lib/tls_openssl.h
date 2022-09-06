/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2020 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include <memory>

class TlsOpenSslPrivate;
class ConfigResourcesContainer;

class TlsOpenSsl : public Tls {
 public:
  TlsOpenSsl();
  virtual ~TlsOpenSsl();
  TlsOpenSsl(TlsOpenSsl& other) = delete;

  bool init() override;

  bool TlsPostconnectVerifyHost(JobControlRecord* jcr,
                                const char* host) override;
  bool TlsPostconnectVerifyCn(
      JobControlRecord* jcr,
      const std::vector<std::string>& verify_list) override;

  bool TlsBsockAccept(BareosSocket* bsock) override;
  int TlsBsockWriten(BareosSocket* bsock, char* ptr, int32_t nbytes) override;
  int TlsBsockReadn(BareosSocket* bsock, char* ptr, int32_t nbytes) override;
  bool TlsBsockConnect(BareosSocket* bsock) override;
  void TlsBsockShutdown(BareosSocket* bsock) override;

  std::string TlsCipherGetName() const override;
  void SetCipherList(const std::string& cipherlist) override;
  void SetProtocol(const std::string& protocol) override;
  void TlsLogConninfo(JobControlRecord* jcr,
                      const char* host,
                      int port,
                      const char* who) const override;
  void SetTlsPskClientContext(const PskCredentials& credentials) override;
  void SetTlsPskServerContext(ConfigurationParser* config) override;

  void Setca_certfile_(const std::string& ca_certfile) override;
  void SetCaCertdir(const std::string& ca_certdir) override;
  void SetCrlfile(const std::string& crlfile_) override;
  void SetCertfile(const std::string& certfile_) override;
  void SetKeyfile(const std::string& keyfile_) override;
  void SetPemCallback(CRYPTO_PEM_PASSWD_CB pem_callback) override;
  void SetPemUserdata(void* pem_userdata) override;
  void SetDhFile(const std::string& dhfile_) override;
  void SetVerifyPeer(const bool& verify_peer) override;
  void SetTcpFileDescriptor(const int& fd) override;

 private:
  std::unique_ptr<TlsOpenSslPrivate> d_; /* private data */
};
#endif  // BAREOS_LIB_TLS_OPENSSL_H_
