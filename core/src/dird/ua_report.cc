/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

#include "dird/ua_report.h"

#include "dird/dird_conf.h"
#include "dird/director_jcr_impl.h"
#include "dird/ua_select.h"
#include "dird/fd_cmds.h"
#include "include/auth_protocol_types.h"

#include <memory>

namespace directordaemon {
struct connection_closer {
  void operator()(BareosSocket* sock)
  {
    sock->signal(BNET_TERMINATE);
    sock->close();
    delete sock;
  }
};

using socket_ptr = std::unique_ptr<BareosSocket, connection_closer>;
static_assert(sizeof(socket_ptr) == sizeof(BareosSocket*));

static socket_ptr NativeConnectToClient(UaContext* ua, ClientResource* client)
{
  ua->jcr->dir_impl->res.client = client;
  // Try to connect for 15 seconds
  if (!ua->api) {
    ua->SendMsg(_("Connecting to Client %s at %s:%d\n"), client->resource_name_,
                client->address, client->FDport);
  }

  if (!ConnectToFileDaemon(ua->jcr, 1, 15, false, ua)) {
    ua->SendMsg(_("\nFailed to connect to Client %s.\n====\n"),
                client->resource_name_);
    if (ua->jcr->file_bsock) {
      ua->jcr->file_bsock->close();
      delete ua->jcr->file_bsock;
      ua->jcr->file_bsock = nullptr;
    }
    return nullptr;
  }

  Dmsg0(20, _("Connected to file daemon\n"));

  BareosSocket* fd = ua->jcr->file_bsock;
  ua->jcr->file_bsock = nullptr;
  return {fd, connection_closer{}};
}

bool ReportCmd(UaContext* ua, const char*)
{
  if (ua->argc < 3) {
    ua->SendMsg(
        "1900 Bad report command; Usage report <type> <target> <args...>.\n");
    return false;
  }

  const char* type = ua->argk[1];
  const char* target = ua->argk[2];

  std::string msg{"report "};
  msg += type;

  for (int i = 3; i < ua->argc; ++i) {
    msg += " ";
    if (ua->argv[i]) {
      msg += ua->argk[i];
      msg += "=";
      msg += ua->argv[i];
    } else {
      msg += ua->argk[i];
    }
  }

  if (Bstrcasecmp(target, "dir")) {
    ua->SendMsg("Dir got type=%s\n", type);
  } else if (Bstrcasecmp(target, "storage")) {
    StorageResource* storage = get_storage_resource(ua);
    ua->SendMsg("storage %s got type=%s\n", storage->resource_name_, type);
  } else if (Bstrcasecmp(target, "client")) {
    ClientResource* client = get_client_resource(ua);
    if (client->Protocol != APT_NATIVE) {
      ua->SendMsg("client protocol not supported.\n");
      return false;
    }
    socket_ptr fd = NativeConnectToClient(ua, client);
    if (!fd) { return false; }

    ua->SendMsg("Sending '%s'\n", msg.c_str());
    fd->fsend(msg.c_str());

    while (fd->recv() >= 0) { ua->SendMsg("%s", fd->msg); }
  } else {
    ua->SendMsg("1900 Bad report command: unkown target '%s'.\n", target);
    return false;
  }

  return true;
}

};  // namespace directordaemon
