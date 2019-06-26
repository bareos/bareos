/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, May MMI
 */
/**
 * @file
 * handles authorization of Consoles, Storage and File daemons.
 *
 * This routine runs as a thread and must be thread reentrant.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/authenticate.h"
#if defined(HAVE_PAM)
#include "dird/auth_pam.h"
#endif
#include "dird/fd_cmds.h"
#include "dird/client_connection_handshake_mode.h"
#include "dird/dird_globals.h"
#include "lib/bnet.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/bstringlist.h"
#include "lib/parse_conf.h"
#include "lib/util.h"

namespace directordaemon {

static const int debuglevel = 50;

/*
 * Commands sent to Storage daemon and File daemon and received from the User
 * Agent
 */
static char hello[] = "Hello Director %s calling\n";

/*
 * Response from Storage daemon
 */
static char OKhello[] = "3000 OK Hello\n";
static char FDOKhello[] = "2000 OK Hello\n";
static char FDOKnewHello[] = "2000 OK Hello %d\n";

static char dir_not_authorized_message[] = "1999 You are not authorized.\n";

bool AuthenticateWithStorageDaemon(BareosSocket* sd,
                                   JobControlRecord* jcr,
                                   StorageResource* store)
{
  char dirname[MAX_NAME_LENGTH];
  bstrncpy(dirname, me->resource_name_, sizeof(dirname));
  BashSpaces(dirname);

  sd->InitBnetDump(
      my_config->CreateOwnQualifiedNameForNetworkDump());
  if (!sd->fsend(hello, dirname)) {
    Dmsg1(debuglevel, _("Error sending Hello to Storage daemon. ERR=%s\n"),
          BnetStrerror(sd));
    Jmsg(jcr, M_FATAL, 0, _("Error sending Hello to Storage daemon. ERR=%s\n"),
         BnetStrerror(sd));
    return false;
  }

  bool auth_success = false;
  auth_success = sd->AuthenticateOutboundConnection(
      jcr, my_config->CreateOwnQualifiedNameForNetworkDump(), dirname,
      store->password_, store);
  if (!auth_success) {
    Dmsg2(debuglevel,
          "Director unable to authenticate with Storage daemon at \"%s:%d\"\n",
          sd->host(), sd->port());
    Jmsg(jcr, M_FATAL, 0,
         _("Director unable to authenticate with Storage daemon at \"%s:%d\". "
           "Possible causes:\n"
           "Passwords or names not the same or\n"
           "TLS negotiation problem or\n"
           "Maximum Concurrent Jobs exceeded on the SD or\n"
           "SD networking messed up (restart daemon).\n"),
         sd->host(), sd->port());
    return false;
  }

  Dmsg1(116, ">stored: %s", sd->msg);
  if (sd->recv() <= 0) {
    Jmsg3(jcr, M_FATAL, 0,
          _("dir<stored: \"%s:%s\" bad response to Hello command: ERR=%s\n"),
          sd->who(), sd->host(), sd->bstrerror());
    return false;
  }

  Dmsg1(110, "<stored: %s", sd->msg);
  if (!bstrncmp(sd->msg, OKhello, sizeof(OKhello))) {
    Dmsg0(debuglevel, _("Storage daemon rejected Hello command\n"));
    Jmsg2(jcr, M_FATAL, 0,
          _("Storage daemon at \"%s:%d\" rejected Hello command\n"), sd->host(),
          sd->port());
    return false;
  }

  return true;
}

bool AuthenticateWithFileDaemon(JobControlRecord* jcr)
{
  if (jcr->authenticated) { return true; }

  BareosSocket* fd = jcr->file_bsock;
  ClientResource* client = jcr->res.client;

  if (jcr->connection_handshake_try_ ==
      ClientConnectionHandshakeMode::kTlsFirst) {
    std::string qualified_resource_name;
    if (!my_config->GetQualifiedResourceNameTypeConverter()->ResourceToString(
            me->resource_name_, my_config->r_own_, qualified_resource_name)) {
      Dmsg0(
          100,
          "Could not generate qualified resource name for a client resource\n");
      return false;
    }

    if (!fd->DoTlsHandshake(TlsPolicy::kBnetTlsAuto, client, false,
                            qualified_resource_name.c_str(),
                            client->password_.value, jcr)) {
      Dmsg0(100, "Could not DoTlsHandshake() with a file daemon\n");
      return false;
    }
  }

  char dirname[MAX_NAME_LENGTH];
  bstrncpy(dirname, me->resource_name_, sizeof(dirname));
  BashSpaces(dirname);

  fd->InitBnetDump(
      my_config->CreateOwnQualifiedNameForNetworkDump());
  if (!fd->fsend(hello, dirname)) {
    Jmsg(jcr, M_FATAL, 0,
         _("Error sending Hello to File daemon at \"%s:%d\". ERR=%s\n"),
         fd->host(), fd->port(), fd->bstrerror());
    return false;
  }
  Dmsg1(debuglevel, "Sent: %s", fd->msg);

  bool auth_success;
  auth_success = fd->AuthenticateOutboundConnection(
      jcr, my_config->CreateOwnQualifiedNameForNetworkDump(), dirname,
      client->password_, client);

  if (!auth_success) {
    Dmsg2(debuglevel, "Unable to authenticate with File daemon at \"%s:%d\"\n",
          fd->host(), fd->port());
    Jmsg(jcr, M_FATAL, 0,
         _("Unable to authenticate with File daemon at \"%s:%d\". Possible "
           "causes:\n"
           "Passwords or names not the same or\n"
           "TLS negotiation failed or\n"
           "Maximum Concurrent Jobs exceeded on the FD or\n"
           "FD networking messed up (restart daemon).\n"),
         fd->host(), fd->port());
    return false;
  }

  Dmsg1(116, ">filed: %s", fd->msg);
  if (fd->recv() <= 0) {
    Dmsg1(debuglevel,
          _("Bad response from File daemon to Hello command: ERR=%s\n"),
          BnetStrerror(fd));
    Jmsg(jcr, M_FATAL, 0,
         _("Bad response from File daemon at \"%s:%d\" to Hello command: "
           "ERR=%s\n"),
         fd->host(), fd->port(), fd->bstrerror());
    return false;
  }

  Dmsg1(110, "<filed: %s", fd->msg);
  jcr->FDVersion = 0;
  if (!bstrncmp(fd->msg, FDOKhello, sizeof(FDOKhello)) &&
      sscanf(fd->msg, FDOKnewHello, &jcr->FDVersion) != 1) {
    Dmsg0(debuglevel, _("File daemon rejected Hello command\n"));
    Jmsg(jcr, M_FATAL, 0,
         _("File daemon at \"%s:%d\" rejected Hello command\n"), fd->host(),
         fd->port());
    return false;
  }

  return true;
}

bool AuthenticateFileDaemon(BareosSocket* fd, char* client_name)
{
  ClientResource* client;
  bool auth_success = false;

  UnbashSpaces(client_name);
  client = (ClientResource*)my_config->GetResWithName(R_CLIENT, client_name);
  if (client) {
    if (IsConnectFromClientAllowed(client)) {
      auth_success = fd->AuthenticateInboundConnection(
          NULL, my_config, client_name, client->password_, client);
    }
  }

  /*
   * Authorization Completed
   */
  if (!auth_success) {
    fd->fsend("%s", _(dir_not_authorized_message));
    Emsg4(M_ERROR, 0, _("Unable to authenticate client \"%s\" at %s:%s:%d.\n"),
          client_name, fd->who(), fd->host(), fd->port());
    sleep(5);
    return false;
  }
  fd->fsend("1000 OK: %s Version: %s (%s)\n", my_name, VERSION, BDATE);

  return true;
}
} /* namespace directordaemon */
