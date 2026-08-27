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

#include "include/bareos.h"
#include "dird/ua_server.h"
#include "dird.h"
#include "include/version_hex.h"
#include "include/version_numbers.h"
#include "lib/default_console.h"
#include "lib/s_password.h"
#include "dird/authenticate.h"
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

#include <array>

namespace directordaemon {

static const int debuglevel = 50;

/*
 * Commands sent to Storage daemon and File daemon and received from the User
 * Agent
 */
static char hello[] = "Hello Director %s calling Version=\"%u.%u.%u\"\n";

// Response from Storage daemon
static char FDOKhello[] = "2000 OK Hello\n";
static char FDOKnewHello[] = "2000 OK Hello %d\n";

bool AuthenticateWithFileDaemon(JobControlRecord* jcr)
{
  if (jcr->authenticated) { return true; }

  auto config = jcr->dir_impl->used_config_for_job;
  if (!config) { config = my_config->GetCurrentConfiguration(); }

  auto myself = dynamic_cast<DirectorResource*>(
      config->GetNextRes(R_DIRECTOR, nullptr));
  if (!myself) { return false; }

  auto* fd = jcr->file_bsock;
  auto* client = jcr->dir_impl->res.client;
  fd->SetEnableKtls(myself->enable_ktls);

  std::string qualified_resource_name = global_resource::QualifiedName(
      global_resource::Type::Director, myself->resource_name_);

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

  std::string cpy{myself->resource_name_};
  BashSpaces(cpy.data());
  PoolMem hello_msg;
  hello_msg.bsprintf(hello, cpy.c_str(), kBareosVersion.Major,
                     kBareosVersion.Minor, kBareosVersion.Patch);

  if (!BareosConnect(jcr, fd, qualified_resource_name, client,
                     hello_msg.c_str(), old_style_tls)) {
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

inline constexpr const char hello_client_with_version_v2[]
    = "Hello Client %127s FdProtocolVersion=%d calling Version=\"%u.%u.%u\"";

inline constexpr const char hello_client_with_version[]
    = "Hello Client %127s FdProtocolVersion=%d calling";

inline constexpr const char hello_client[] = "Hello Client %127s calling";

inline constexpr const char hello_console[] = "Hello %127s calling";
inline constexpr const char hello_console_with_version[]
    = "Hello %127s calling version %127s Version=\"%u.%u.%u\"";

TlsResource* DirectorAuth::parse(std::string_view msg)
{
  char version[MAX_NAME_LENGTH]{};
  char tbuf[MAX_TIME_LENGTH];
  char name[MAX_NAME_LENGTH]{};
  int fd_protocol_version{0};

  unsigned major{}, minor{}, patch{};
  std::string cpy{msg};
  if ((bsscanf(cpy.c_str(), hello_client_with_version_v2, name,
               &fd_protocol_version, &major, &minor, &patch)
       == 5)
      || (bsscanf(cpy.c_str(), hello_client_with_version, name,
                  &fd_protocol_version)
          == 2)
      || (bsscanf(cpy.c_str(), hello_client, name) == 1)) {
    type = inbound_type::Client;
  } else if (bsscanf(cpy.c_str(), hello_console_with_version, name, version,
                     &major, &minor, &patch)
                 == 5
             || bsscanf(cpy.c_str(), hello_console, name) == 1) {
    type = inbound_type::Console;
  }

  remote_version = VERSION_HEX(major, minor, patch);
  UnbashSpaces(name);

  bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL));

  auto* myself
      = dynamic_cast<DirectorResource*>(p->GetNextRes(R_DIRECTOR, nullptr));

  if (!myself) { return nullptr; }

  switch (type) {
    case inbound_type::Client: {
      Dmsg1(110, "Got a FD connection from %s at %s\n", name, tbuf);
      auto* res
          = dynamic_cast<ClientResource*>(p->GetResWithName(R_CLIENT, name));

      if (!res) {
        Dmsg1(50, "Unknown FD %s for new connection\n", name);
        return nullptr;
      }

      if (res->password_.encoding != p_encoding_md5) {
        Dmsg1(50, "Bad password for FD %s: md5 is required\n", name);
        return nullptr;
      }

      auto& data = client.emplace();
      data.res = res;
      data.protocol_version = fd_protocol_version;

      return res;
    } break;
    case inbound_type::Console: {
      Dmsg1(110, "Got a Console connection from %s at %s\n", name, tbuf);
      auto* res
          = dynamic_cast<ConsoleResource*>(p->GetResWithName(R_CONSOLE, name));
      if (!res) {
        Dmsg1(50, "Unknown Console %s for new connection\n", name);
        return nullptr;
      }

      if (res->password_.encoding != p_encoding_md5) {
        Dmsg1(50, "Bad password for Console %s: md5 is required\n", name);
        return nullptr;
      }

      auto& data = console.emplace();
      auto num_leases = ConsoleConnectionLease::get_num_leases();

      bool is_default_console
          = bstrcmp(res->resource_name_, DEFAULT_CONSOLE_NAME);

      if (num_leases > myself->MaxConsoleConnections) {
        if (is_default_console) {
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

      data.res = res;

      if (is_default_console) {
        data.tls = *myself;
        data.is_root = true;
      } else {
        data.tls = *res;
        data.is_root = false;
      }

      auto parsed_version = parse_version(version);
      if (parsed_version >= BareosVersionNumber::kRelease_18_2) {
        data.is_old = false;
        if (data.tls.tls_enable_) { data.tls.tls_require_ = true; }
      } else {
        data.is_old = true;
      }

      return &data.tls;
    } break;
    default:
      [[fallthrough]];
    case inbound_type::Unknown: {
      Dmsg1(110, "received bad hello at %s\n", tbuf);
      return nullptr;
    } break;
  }
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
  uint32_t response_id;
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
