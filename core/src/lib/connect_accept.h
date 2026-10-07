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

enum class ConnectionType
{
  /* This connection is insecure, we do not know who we are talking to */
  Insecure,
  /* The connection is secure, but we do not trust the other side yet. */
  Untrusted,
  /* The connection is secure and we trust the other side. */
  Trusted,
};

struct ConnectionInfo {
  virtual const TlsResource* tls_settings() = 0;
  virtual std::vector<std::unique_ptr<auth::Prover>> select_provers(
      ConnectionType type)
      = 0;
  virtual std::vector<std::unique_ptr<auth::Verifier>> select_verifiers(
      ConnectionType type)
      = 0;
  virtual ~ConnectionInfo() = default;
};

bool BareosConnect(JobControlRecord* jcr,
                   BareosSocket* socket,
                   const std::string& qualified_name,
                   const TlsResource* res,
                   auth::OutboundAuthenticator* auth,
                   std::string_view hello_msg,
                   bool cleartext_authentication = false);

struct Md5OutboundAuthenticator : auth::OutboundAuthenticator {
  bool authenticate(auth::OutboundArgs args) override;
};

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

  Md5OutboundAuthenticator auth;

  return BareosConnect(jcr, socket, qualified_name, res, &auth, hello,
                       cleartext_authentication);
}

struct DefaultConnectionInfo : ConnectionInfo {
  DefaultConnectionInfo(TlsResource res) : tls{std::move(res)} {}

  const TlsResource* tls_settings() override { return &tls; };
  std::vector<std::unique_ptr<auth::Prover>> select_provers(
      ConnectionType type) override;
  std::vector<std::unique_ptr<auth::Verifier>> select_verifiers(
      ConnectionType type) override;

  TlsResource tls;
};

struct ConnectionInfoProvider {
  virtual std::unique_ptr<ConnectionInfo>
  get_info_for(global_resource::Type type, std::string_view idenity) = 0;
};

std::optional<ParsedHello> BareosAccept(BareosSocket* socket,
                                        global_resource::Type type,
                                        const TlsResource* initial_tls,
                                        ConnectionInfoProvider* provider);

#endif  // BAREOS_LIB_CONNECT_ACCEPT_H_
