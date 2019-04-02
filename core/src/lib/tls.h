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

#include "include/bareos.h"
#include "lib/get_tls_psk_by_fqname_callback.h"

class BareosSocket;
class JobControlRecord;
class PskCredentials;

class Tls {
 public:
  Tls();
  virtual ~Tls();
  Tls(Tls& other) = delete;

  virtual bool init() = 0;

  enum class TlsImplementationType
  {
    kTlsUnknown,
    kTlsOpenSsl
  };
  static Tls* CreateNewTlsContext(Tls::TlsImplementationType type);

  virtual void SetTlsPskClientContext(const PskCredentials& credentials) = 0;
  virtual void SetTlsPskServerContext(ConfigurationParser* config) = 0;

  virtual bool TlsPostconnectVerifyHost(JobControlRecord* jcr,
                                        const char* host) = 0;
  virtual bool TlsPostconnectVerifyCn(
      JobControlRecord* jcr,
      const std::vector<std::string>& verify_list) = 0;

  virtual bool TlsBsockAccept(BareosSocket* bsock) = 0;
  virtual int TlsBsockWriten(BareosSocket* bsock,
                             char* ptr,
                             int32_t nbytes) = 0;
  virtual int TlsBsockReadn(BareosSocket* bsock, char* ptr, int32_t nbytes) = 0;
  virtual bool TlsBsockConnect(BareosSocket* bsock) = 0;
  virtual void TlsBsockShutdown(BareosSocket* bsock) = 0;
  virtual void TlsLogConninfo(JobControlRecord* jcr,
                              const char* host,
                              int port,
                              const char* who) const = 0;
  virtual std::string TlsCipherGetName() const { return std::string(); }

  virtual void SetCipherList(const std::string& cipherlist) = 0;

  virtual void Setca_certfile_(const std::string& ca_certfile) = 0;
  virtual void SetCaCertdir(const std::string& ca_certdir) = 0;
  virtual void SetCrlfile(const std::string& crlfile_) = 0;
  virtual void SetCertfile(const std::string& certfile_) = 0;
  virtual void SetKeyfile(const std::string& keyfile_) = 0;
  virtual void SetPemCallback(CRYPTO_PEM_PASSWD_CB pem_callback) = 0;
  virtual void SetPemUserdata(void* pem_userdata) = 0;
  virtual void SetDhFile(const std::string& dhfile_) = 0;
  virtual void SetVerifyPeer(const bool& verify_peer) = 0;
  virtual void SetTcpFileDescriptor(const int& fd) = 0;
};

#endif /* BAREOS_LIB_TLS_H_ */
