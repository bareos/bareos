/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_LIB_BAUTH_H_
#define BAREOS_LIB_BAUTH_H_

#include "lib/bsock.h"
#include "lib/hello.h"

struct Authenticator {
  struct OutboundArgs {
    JobControlRecord* jcr;
    BareosSocket* socket;
    const TlsResource* target;
  };

  struct InboundArgs {
    BareosSocket* socket;
    const TlsResource* target;
  };

  virtual bool authenticate_outbound(OutboundArgs args) = 0;
  virtual bool authenticate_inbound(InboundArgs args) = 0;
  virtual ~Authenticator() = default;
};

struct Md5Authenticator : Authenticator {
  bool authenticate_outbound(OutboundArgs args) override;
  bool authenticate_inbound(InboundArgs args) override;

  Md5Authenticator();
  Md5Authenticator(std::string identity);

  /* a cram-md5 challenge consists of three parts:
   *  - a current timestamp,
   *  - a random value, and
   *  - some way to identify our own challenges
   * cram_identity is used for the third part.  It makes sure
   * that you cannot use us, to solve our own challenge.
   * This value can be anything, but it should always be the same for the
   * livetime of the program, otherwise it will not do its job!
   * This string shall _NOT_ contain whitespace! */
  std::string cram_identity;
};

bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   const std::string& qualified_name,
                   const TlsResource* res,
                   std::string_view hello_msg,
                   Authenticator* auth,
                   bool cleartext_authentication = false);

template <global_resource::Type type, global_resource::Type target_type>
bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   std::string_view name,
                   const TlsResource* res,
                   bool cleartext_authentication = false)
{
  using formatter = hello_formatter<type, target_type>;
  auto qualified_name
      = global_resource::QualifiedName(formatter::auth_type, name);
  auto hello = formatter::format(name);
  Md5Authenticator auth{qualified_name};
  return BareosConnect(jcr, socket, qualified_name, res, hello, &auth,
                       cleartext_authentication);
}

std::optional<ParsedHello> BareosAccept(BareosSocket* socket,
                                        global_resource::Type type,
                                        const TlsResource* initial_tls,
                                        TlsConfigProvider* provider,
                                        Authenticator* auth);

static inline auto BareosAccept(BareosSocket* socket,
                                global_resource::Type type,
                                const TlsResource* initial_tls,
                                TlsConfigProvider* provider)
{
  Md5Authenticator auth{};
  return BareosAccept(socket, type, initial_tls, provider, &auth);
}

#endif  // BAREOS_LIB_BAUTH_H_
