/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2008 Free Software Foundation Europe e.V.
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
/*
 * Bareos authentication. Provides authentication with File and Storage daemons.
 *
 * Nicolas Boichat, August MMIV
 */

#include "monitoritem.h"
#include "authenticate.h"
#include "include/jcr.h"
#include "monitoritemthread.h"

#include "lib/tls_conf.h"
#include "lib/bsock.h"
#include "lib/bnet.h"
#include "lib/global_resource.h"
#include "lib/bstringlist.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/version.h"

const int debuglevel = 50;

/* Commands sent to Storage daemon and File daemon and received
 *  from the User Agent */
inline constexpr const char SDFDhello[]
    = "Hello Director %s calling Version=\"%u.%u.%u\"\n";
inline constexpr const char Dirhello[]
    = "Hello %s calling Version=\"%u.%u.%u\"\n";

/* Response from SD */
inline constexpr const char SDOKhello[] = "3000 OK Hello";
/* Response from FD */
inline constexpr const char FDOKhello[] = "2000 OK Hello";
/* Response from DIR */
inline constexpr const char DIROKhello[] = "1000\x1eOK";

static std::map<AuthenticationResult, std::string>
    authentication_error_to_string_map{
        {AuthenticationResult::kNoError, "No Error"},
        {AuthenticationResult::kAlreadyAuthenticated, "Already authenticated"},
        {AuthenticationResult::kQualifiedResourceNameFailed,
         "Could not generate a qualified resource name"},
        {AuthenticationResult::kTlsHandshakeFailed, "TLS handshake failed"},
        {AuthenticationResult::kSendHelloMessageFailed,
         "Send of hello handshake message failed"},
        {AuthenticationResult::kCramMd5HandshakeFailed,
         "Challenge response handshake failed"},
        {AuthenticationResult::kDaemonResponseFailed,
         "Daemon response could not be read"},
        {AuthenticationResult::kRejectedByDaemon,
         "Authentication was rejected by the daemon"},
        {AuthenticationResult::kUnknownDaemon, "Unknown daemon type"}};

bool GetAuthenticationResultString(AuthenticationResult err,
                                   std::string& buffer)
{
  if (authentication_error_to_string_map.find(err)
      != authentication_error_to_string_map.end()) {
    buffer = authentication_error_to_string_map.at(err);
    return true;
  }
  return false;
}

AuthenticationResult AuthenticateWithDaemon(MonitorItem* item,
                                            JobControlRecord* jcr)
{
  if (jcr->authenticated) {
    return AuthenticationResult::kAlreadyAuthenticated;
  }

  MonitorResource* monitor = MonitorItemThread::instance()->getMonitor();

  PoolMem hello;

  switch (item->type()) {
    case R_DIRECTOR: {
      auto* sock = jcr->dir_bsock;
      auto* dir = static_cast<DirectorResource*>(item->resource());
      std::string bashed{monitor->resource_name_};
      BashSpaces(bashed.data());
      hello.bsprintf(Dirhello, bashed.c_str(), kBareosVersion.Major,
                     kBareosVersion.Minor, kBareosVersion.Patch);

      TlsResource custom = *dir;
      // bareos is consistently inconsistent, so we obviously use the
      // monitor password here, not the director one ...
      custom.password_.value = monitor->password.value;

      if (!BareosConnect(
              jcr, sock,
              global_resource::QualifiedName(global_resource::Type::Console,
                                             monitor->resource_name_),
              &custom, hello.c_str())) {
        Jmsg(jcr, M_FATAL, 0, T_("Failed to authenticate with %s\n"),
             dir->resource_name_);
        return AuthenticationResult::kCramMd5HandshakeFailed;
      }

      if (sock->recv() <= 0) {
        Jmsg3(jcr, M_FATAL, 0,
              T_("console<dir: \"%s:%s\" bad response to Hello command: "
                 "ERR=%s\n"),
              sock->who(), sock->host(), sock->bstrerror());
        return AuthenticationResult::kDaemonResponseFailed;
      }

      if (!bstrncmp(sock->msg, DIROKhello, sizeof(DIROKhello) - 1)) {
        Dmsg0(debuglevel, T_("Director daemon rejected Hello command\n"));
        Jmsg2(jcr, M_FATAL, 0,
              T_("Director daemon at \"%s:%d\" rejected Hello command\n"),
              sock->host(), sock->port());
        return AuthenticationResult::kRejectedByDaemon;
      }
    } break;
    case R_CLIENT: {
      auto* fd = jcr->file_bsock;
      auto* client = static_cast<ClientResource*>(item->resource());
      std::string bashed{monitor->resource_name_};
      BashSpaces(bashed.data());
      hello.bsprintf(SDFDhello, bashed.data(), kBareosVersion.Major,
                     kBareosVersion.Minor, kBareosVersion.Patch);

      if (!BareosConnect(
              jcr, fd,
              global_resource::QualifiedName(global_resource::Type::Director,
                                             monitor->resource_name_),
              client, hello.c_str())) {
        Jmsg(jcr, M_FATAL, 0, "Failed to authenticate with %s\n",
             client->resource_name_);
        return AuthenticationResult::kCramMd5HandshakeFailed;
      }

      if (fd->recv() <= 0) {
        Dmsg1(debuglevel,
              T_("Bad response from File daemon to Hello command: ERR=%s\n"),
              BnetStrerror(fd));
        Jmsg(jcr, M_FATAL, 0,
             T_("Bad response from File daemon at \"%s:%d\" to Hello command: "
                "ERR=%s\n"),
             fd->host(), fd->port(), fd->bstrerror());
        return AuthenticationResult::kDaemonResponseFailed;
      }

      Dmsg1(110, "<filed: %s", fd->msg);
      if (strncmp(fd->msg, FDOKhello, sizeof(FDOKhello) - 1) != 0) {
        Jmsg(jcr, M_FATAL, 0, T_("File daemon rejected Hello command\n"));
        return AuthenticationResult::kRejectedByDaemon;
      }
    } break;
    case R_STORAGE: {
      auto* sd = jcr->store_bsock;
      auto* storage = static_cast<StorageResource*>(item->resource());
      std::string bashed{monitor->resource_name_};
      BashSpaces(bashed.data());
      hello.bsprintf(SDFDhello, bashed.data(), kBareosVersion.Major,
                     kBareosVersion.Minor, kBareosVersion.Patch);
      if (!BareosConnect(
              jcr, sd,
              global_resource::QualifiedName(global_resource::Type::Director,
                                             monitor->resource_name_),
              storage, hello.c_str())) {
        Jmsg(jcr, M_FATAL, 0, T_("Failed to authenticate with %s\n"),
             storage->resource_name_);
        return AuthenticationResult::kCramMd5HandshakeFailed;
      }

      if (sd->recv() <= 0) {
        Jmsg3(
            jcr, M_FATAL, 0,
            T_("dir<stored: \"%s:%s\" bad response to Hello command: ERR=%s\n"),
            sd->who(), sd->host(), sd->bstrerror());
        return AuthenticationResult::kDaemonResponseFailed;
      }

      if (!bstrncmp(sd->msg, SDOKhello, sizeof(SDOKhello) - 1)) {
        Dmsg0(debuglevel, T_("Storage daemon rejected Hello command\n"));
        Jmsg2(jcr, M_FATAL, 0,
              T_("Storage daemon at \"%s:%d\" rejected Hello command\n"),
              sd->host(), sd->port());
        return AuthenticationResult::kRejectedByDaemon;
      }
    } break;
    default:
      printf(
          T_("Error, currentitem is neither a Client, a Storage nor a "
             "Director.\n"));
      return AuthenticationResult::kUnknownDaemon;
  }

  return AuthenticationResult::kNoError;
}
