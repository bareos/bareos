/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, May MMI
/**
 * @file
 * handles authorization of Consoles, Storage and File daemons.
 *
 * This routine runs as a thread and must be thread reentrant.
 */

#include "dird/dird_conf.h"
#include "include/bareos.h"
#include "dird/ua_server.h"
#include "dird.h"
#include "include/version_hex.h"
#include "include/version_numbers.h"
#include "lib/bauth.h"
#include "lib/connect_accept.h"
#include "lib/default_console.h"
#include "lib/hello.h"
#include "lib/s_password.h"
#include "dird/authenticate.h"
#include <climits>
#if defined(HAVE_PAM)
#  include "dird/auth_pam.h"
#endif
#include "dird/fd_cmds.h"
#include "dird/client_connection_handshake_mode.h"
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "lib/bnet.h"
#include "lib/global_resource.h"
#include "lib/bstringlist.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/bauth/cram_md5.h"
#include "lib/bauth/oidc/verifier.h"

#include <array>

namespace directordaemon {

static const int debuglevel = 50;

// Response from Storage daemon
static char FDOKhello[] = "2000 OK Hello\n";
static char FDOKnewHello[] = "2000 OK Hello %d\n";

bool AuthenticateWithFileDaemon(JobControlRecord* jcr, ClientResource* client)
{
  if (jcr->authenticated) { return true; }

  auto config = jcr->dir_impl->used_config_for_job;
  if (!config) { config = my_config->GetCurrentConfiguration(); }

  auto myself = dynamic_cast<DirectorResource*>(
      config->GetNextRes(R_DIRECTOR, nullptr));
  if (!myself) { return false; }

  auto* fd = jcr->file_bsock;
  fd->SetEnableKtls(myself->enable_ktls);

  using global_resource::Type;

  std::string qualified_resource_name
      = global_resource::QualifiedName(Type::Director, myself->resource_name_);

  bool old_style_tls{false};

  switch (jcr->dir_impl->connection_handshake_try_) {
    case ClientConnectionHandshakeMode::kTlsFirst: {
      old_style_tls = false;
    } break;
    case ClientConnectionHandshakeMode::kCleartextFirst: {
      old_style_tls = true;
    } break;
    case ClientConnectionHandshakeMode::kUndefined:
    case ClientConnectionHandshakeMode::kFailed: {
      return false;
    } break;
  }

  if (!BareosConnect<Type::Director, Type::Client>(
          jcr, fd, myself->resource_name_, client, old_style_tls)) {
    Jmsg(jcr, M_FATAL, 0,
         T_("Error connecting to File daemon at \"%s:%d\". ERR=%s\n"),
         fd->host(), fd->port(), fd->bstrerror());
    return false;
  }

  Dmsg1(116, ">filed: %s", fd->msg);
  if (fd->recv() <= 0) {
    Dmsg1(debuglevel,
          T_("Bad response from File daemon to Hello command: ERR=%s\n"),
          BnetStrerror(fd));
    Jmsg(jcr, M_FATAL, 0,
         T_("Bad response from File daemon at \"%s:%d\" to Hello command: "
            "ERR=%s\n"),
         fd->host(), fd->port(), fd->bstrerror());
    return false;
  }

  Dmsg1(110, "<filed: %s", fd->msg);
  jcr->dir_impl->FDVersion = 0;

  if (!bstrncmp(fd->msg, FDOKhello, sizeof(FDOKhello))
      && bsscanf(fd->msg, FDOKnewHello, &jcr->dir_impl->FDVersion) != 1) {
    Dmsg0(debuglevel, T_("File daemon rejected Hello command\n"));
    Jmsg(jcr, M_FATAL, 0,
         T_("File daemon at \"%s:%d\" rejected Hello command\n"), fd->host(),
         fd->port());
    return false;
  }

  return true;
}

bool ConsoleInboundAuthenticator::authenticate(auth::InboundArgs args)
{
  auth::Algorithms algs;

  auto password = tls_settings()->password_.value;

  algs.add<auth::CramMd5::Prover>(identity(), password);

  if (res->auth_list) {
    for (auto* auth : *res->auth_list) {
      if (auto* oidc = dynamic_cast<OidcResource*>(auth)) {
        auto oidc_url = oidc->url;
        auto client_id = oidc->client_id;
        auto roles_name = oidc->roles_name;

        if (!oidc->IsMemberPresent("ProfilesClaim")) { roles_name = "roles"; }

        auth::Oidc::config conf = {
            .oidc_url = std::move(oidc_url),
            .client_id = std::move(client_id),
            .roles_name = std::move(roles_name),
        };

        algs.add<auth::Oidc::Verifier>(oidc->resource_name_, std::move(conf));
      } else {
        Dmsg0(50, "unknown auth (%p) received passed (Skipping ...)\n", auth);
      }
    }
  } else {
    // use the default auth
    algs.add<auth::CramMd5::Verifier>("Bareos Auth", identity(), password);
  }

  if (!authenticate_inbound(algs, args)) { return false; }

  auth::Verifier* verifier_used{nullptr};
  for (auto& verifier : algs.verifier) {
    if (verifier.impl->was_used()) {
      verifier_used = verifier.impl.get();
      break;
    }
  }

  if (!verifier_used) {
    Dmsg0(100, "Auth was successful, but no verifier was used !?\n");
    return false;
  }

  if (auto* oidc = dynamic_cast<auth::Oidc::Verifier*>(verifier_used)) {
    std::vector<ProfileResource*> profiles;
    for (const auto& role : oidc->roles) {
      auto* profile = dynamic_cast<ProfileResource*>(
          config->GetResWithName(R_PROFILE, role.c_str()));
      if (!profile) {
        Dmsg0(50, "valid access token contains bad profile %s ---> Rejected!\n",
              role.c_str());
        return false;
      }

      profiles.push_back(profile);
    }

    authenticated_acls = UserAcl::from_profiles(oidc->user_name, profiles);
  } else if (auto* cram
             = dynamic_cast<auth::CramMd5::Verifier*>(verifier_used)) {
    (void)cram;
    authenticated_acls = UserAcl::from_config(res);
  } else {
    Dmsg0(100, "Auth was successful, but unknown verifier was used !?\n");
    return false;
  }
  return true;
}

auth::InboundAuthenticator* DirectorAuth::get_authenticator_for(
    global_resource::Type auth_type,
    std::string_view name)
{
  char tbuf[MAX_TIME_LENGTH];
  bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL));

  int name_len = (int)std::min((size_t)INT_MAX, name.size());
  const char* name_ptr = name.data();

  auto* myself
      = dynamic_cast<DirectorResource*>(p->GetNextRes(R_DIRECTOR, nullptr));

  if (!myself) { return nullptr; }

  switch (auth_type) {
    case global_resource::Type::Client: {
      Dmsg1(110, "Got a FD connection from %.*s at %s\n", name_len, name_ptr,
            tbuf);
      auto* res
          = dynamic_cast<ClientResource*>(p->GetResWithName(R_CLIENT, name));

      if (!res) {
        Dmsg1(50, "Unknown FD %.*s for new connection\n", name_len, name_ptr);
        return nullptr;
      }

      if (res->password_.encoding != p_encoding_md5) {
        Dmsg1(50, "Bad password for FD %.*s: md5 is required\n", name_len,
              name_ptr);
        return nullptr;
      }

      auto& data = client.emplace(res, Md5InboundAuthenticator(*res));

      type = inbound_type::Client;
      return &data.auth;
    } break;
    case global_resource::Type::Console: {
      Dmsg1(110, "Got a Console connection from %.*s at %s\n", name_len,
            name_ptr, tbuf);
      auto* res
          = dynamic_cast<ConsoleResource*>(p->GetResWithName(R_CONSOLE, name));
      if (!res) {
        Dmsg1(50, "Unknown Console %.*s for new connection\n", name_len,
              name_ptr);
        return nullptr;
      }

      if (res->password_.encoding != p_encoding_md5) {
        Dmsg1(50, "Bad password for Console %.*s: md5 is required\n", name_len,
              name_ptr);
        return nullptr;
      }

      ConsoleConnectionLease lease;

      auto num_leases = ConsoleConnectionLease::get_num_leases();

      bool is_default = bstrcmp(res->resource_name_, DEFAULT_CONSOLE_NAME);

      if (num_leases > myself->MaxConsoleConnections) {
        if (is_default) {
          Emsg0(M_INFO, 0,
                T_("Number of console connections exceeded "
                   "Maximum :%u, Current: %" PRIuz "\n"),
                myself->MaxConsoleConnections, num_leases);
        } else {
          Emsg0(M_ERROR, 0,
                T_("Number of console connections exceeded "
                   "Maximum :%u, Current: %" PRIuz "\n"),
                myself->MaxConsoleConnections, num_leases);
          return nullptr;
        }
      }

      const TlsResource* tls;

      if (is_default) {
        /* while the default console has a console resource, for some reason
         * it does not have the correct tls settings; instead of using the
         * directors tls settings, it is set up with some kind of default tls
         * settings.  As such we need to use the tls settings of the director
         * explicitly. */
        tls = myself;
      } else {
        tls = res;
      }

      auto& data = console.emplace(
          res, std::move(lease),
          ConsoleInboundAuthenticator(*tls, p.get(), res), is_default, *tls);
      type = inbound_type::Console;

      return &data.auth;
    } break;
    default: {
    }
  }

  return nullptr;
}

std::unique_ptr<UserAcl> AuthenticatePamUser(BareosSocket* socket,
                                             LoadedConfiguration* conf)
{
#if !defined(HAVE_PAM)
  (void)socket;
  (void)conf;
  Emsg0(M_ERROR, 0, T_("PAM is not available on this director\n"));
  return nullptr;
#else  /* HAVE_PAM */
  uint32_t response_id{kMessageIdUnknown};
  BStringList message_arguments;

  if (!socket->ReceiveAndEvaluateResponseMessage(response_id,
                                                 message_arguments)) {
    Dmsg2(100, "Could not evaluate response_id: %" PRIu32 " - %s\n",
          response_id, message_arguments.JoinReadable().c_str());
    return nullptr;
  }

  std::string pam_username;
  std::string pam_password;

  if (response_id == kMessageIdPamUserCredentials) {
    Dmsg0(200, "Console chooses PAM direct credentials\n");
    if (message_arguments.size() < 3) {
      Dmsg0(200, "Console sent wrong number of credentials\n");
      return nullptr;
    } else {
      pam_username = message_arguments.at(1);
      pam_password = message_arguments.at(2);
    }
  } else if (response_id == kMessageIdPamInteractive) {
    Dmsg0(200, "Console chooses PAM interactive\n");
  } else {
    Dmsg0(200, "Console did not answer correctly: response_id=%" PRIu32 "\n",
          response_id);
    return nullptr;
  }

  Dmsg1(200, "Try to authenticate user using PAM:%s\n", pam_username.c_str());

  std::string authenticated_username;
  if (!PamAuthenticateUser(socket, pam_username, pam_password,
                           authenticated_username)) {
    return nullptr;
  }
  auto* user = dynamic_cast<UserResource*>(
      conf->GetResWithName(R_USER, authenticated_username.c_str()));
  if (!user) {
    Dmsg1(200, "No user config found for user %s\n",
          authenticated_username.c_str());
    return nullptr;
  }
  auto acls = UserAcl::from_config(user);
  return acls;
#endif /* !HAVE_PAM */
}
} /* namespace directordaemon */
