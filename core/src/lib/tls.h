/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
#include "lib/crypto.h"
#include "lib/global_resource.h"

#include <memory>
#include <span>

class BareosSocket;
class JobControlRecord;
class PskCredentials;
class TlsResource;

struct TlsConfigProvider {
  virtual const TlsResource* get_tls_config_for(global_resource::Type type,
                                                std::string_view idenity)
      = 0;
};

class Tls {
 public:
  Tls() = default;

  Tls(const Tls& other) = delete;
  Tls(Tls&& other) = delete;
  Tls& operator=(const Tls& other) = delete;
  Tls& operator=(Tls&& other) = delete;

  virtual ~Tls() = default;

  enum class ImplementationType
  {
    kUnknown,
    kOpenSsl
  };
  static std::unique_ptr<Tls> CreateServerContext(Tls::ImplementationType type,
                                                  const TlsResource* res,
                                                  TlsConfigProvider* config);

  static std::unique_ptr<Tls> CreateClientContext(Tls::ImplementationType type,
                                                  const TlsResource* res,
                                                  const PskCredentials* creds);

  virtual bool TlsBsockAccept(BareosSocket* bsock) = 0;
  virtual int TlsBsockWriten(BareosSocket* bsock, char* ptr, int32_t nbytes)
      = 0;

  virtual int TlsPendingBytes() = 0;
  virtual int TlsBsockReadn(BareosSocket* bsock, char* ptr, int32_t nbytes) = 0;
  virtual bool TlsBsockConnect(JobControlRecord* jcr, BareosSocket* bsock) = 0;
  virtual int TlsBsockPeekn(const BareosSocket* bsock,
                            char* ptr,
                            int32_t nbytes)
      = 0;
  virtual void TlsBsockShutdown(BareosSocket* bsock) = 0;
  virtual void TlsLogConninfo(JobControlRecord* jcr,
                              const char* host,
                              int port,
                              const char* who) const
      = 0;
  virtual std::string TlsCipherGetName() const { return std::string(); }

  enum class PskIdentityStatus
  {
    IsInUse,
    DifferentIdentityInUse,
    NoIdentityInUse,
  };
  virtual PskIdentityStatus IsPskIdentityInUse(std::string_view identity) const
      = 0;
  virtual bool KtlsSendStatus() = 0;
  virtual bool KtlsRecvStatus() = 0;
};

#endif  // BAREOS_LIB_TLS_H_
