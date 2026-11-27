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

#ifndef BAREOS_LIB_CONNECT_ACCEPT_H_
#define BAREOS_LIB_CONNECT_ACCEPT_H_

#include "include/jcr.h"
#include "lib/bauth.h"
#include "lib/bauth/cram_md5.h"
#include "lib/tls.h"

bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   const std::string& qualified_name,
                   const TlsResource* res,
                   auth::OutboundAuthenticator* auth,
                   std::string_view hello_msg,
                   bool cleartext_authentication = false);

struct Md5OutboundAuthenticator : auth::OutboundAuthenticator {
  Md5OutboundAuthenticator();
  Md5OutboundAuthenticator(std::string identity)
      : cram_identity{std::move(identity)}
  {
  }

  bool authenticate(auth::OutboundArgs args) override;

 private:
  std::string cram_identity;
};

struct Md5InboundAuthenticator : auth::InboundAuthenticator {
  Md5InboundAuthenticator(TlsResource res);
  Md5InboundAuthenticator(std::string identity, TlsResource res)
      : tls{std::move(res)}, cram_identity{std::move(identity)}
  {
  }

  bool authenticate(auth::InboundArgs args) override;
  const TlsResource* tls_settings() override { return &tls; }
  const char* identity() { return cram_identity.c_str(); }

 private:
  TlsResource tls;
  std::string cram_identity;
};

template <global_resource::Type type, global_resource::Type target_type>
bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   std::string_view name,
                   const TlsResource* res,
                   auth::OutboundAuthenticator* auth,
                   bool cleartext_authentication = false)
{
  using formatter = hello_formatter<type, target_type>;
  auto qualified_name
      = global_resource::QualifiedName(formatter::auth_type, name);
  auto hello = formatter::format(name);

  return BareosConnect(jcr, socket, qualified_name, res, auth, hello,
                       cleartext_authentication);
}

template <global_resource::Type type, global_resource::Type target_type>
bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   std::string_view name,
                   const TlsResource* res,
                   bool cleartext_authentication = false)
{
  Md5OutboundAuthenticator auth;
  return BareosConnect<type, target_type>(jcr, socket, name, res, &auth,
                                          cleartext_authentication);
}

struct AuthenticationProvider : public TlsConfigProvider {
  virtual auth::InboundAuthenticator* get_authenticator_for(
      global_resource::Type type,
      std::string_view idenity)
      = 0;

  const TlsResource* get_tls_config_for(global_resource::Type type,
                                        std::string_view name) override
  {
    auth = get_authenticator_for(type, name);
    if (!auth) { return nullptr; }
    psk_type = type;
    psk_name.assign(name);
    return auth->tls_settings();
  }

  struct result {
    auth::InboundAuthenticator* auth{};
    std::optional<std::string> bad_name{};
    std::optional<global_resource::Type> bad_type{};
  };

  result get_or_fetch(global_resource::Type type, std::string_view name)
  {
    result res{};
    if (!auth) {
      auth = get_authenticator_for(type, name);
      psk_type = type;
      psk_name.assign(name);
      res.auth = auth;
    } else if (psk_type != type) {
      res.bad_type = psk_type;
    } else if (name != psk_name) {
      res.bad_name = psk_name;
    } else {
      res.auth = auth;
    }
    return res;
  }

 private:
  auth::InboundAuthenticator* auth{};
  global_resource::Type psk_type{};
  std::string psk_name{};
};

std::optional<ParsedHello> BareosAccept(BareosSocket* socket,
                                        global_resource::Type type,
                                        const TlsResource* initial_tls,
                                        AuthenticationProvider* provider);

#endif  // BAREOS_LIB_CONNECT_ACCEPT_H_
