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
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/ua_select.h"
#include "dird/fd_cmds.h"
#include "dird/sd_cmds.h"
#include "dird/storage.h"
#include "include/auth_protocol_types.h"
#include "lib/display_report.h"
#include "lib/parse_conf.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace directordaemon {
void DoDirectorReport(UaContext* ua, const char* msg) {
  std::unordered_map parsed = shared::ParseReportCommands(msg);

  if (auto found = parsed.find("about");
      found != parsed.end()) {
    if (found->second == "perf") {
      std::ostringstream out;
      static_cast<void>(shared::PerformanceReport(out, parsed));
      ua->SendMsg("%s", out.str().c_str());
    } else {
      // the map does not contain cstrings but string_views.  As such
      // we need to create a string first if we want to print them with %s.
      std::string s{found->second};
      ua->SendMsg(_("Bad report command; unknown report %s.\n"),
		 s.c_str());
    }
  } else {
    // since about -> perf is the default, this should never happen
    ua->SendMsg(_("Bad report command; no report type selected.\n"));
  }
}

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

static socket_ptr NativeConnectToStorage(UaContext* ua, StorageResource* storage)
{
  UnifiedStorageResource lstore;

  lstore.store = storage;
  PmStrcpy(lstore.store_source, _("unknown source"));
  SetWstorage(ua->jcr, &lstore);

  if (!ua->api) {
    ua->SendMsg(_("Connecting to Storage daemon %s at %s:%d\n"),
		  storage->resource_name_, storage->address, storage->SDport);
  }

  /* the next call will set ua->jcr->store_bsock */
  if (!ConnectToStorageDaemon(ua->jcr, 1, 15, false)) {
    ua->SendMsg(_("\nFailed to connect to Storage daemon %s.\n====\n"),
		  storage->resource_name_);
    return nullptr;
  }

  Dmsg0(20, _("Connected to storage daemon\n"));

  BareosSocket* sd = ua->jcr->store_bsock;
  ua->jcr->store_bsock = nullptr;
  return {sd, connection_closer{}};
}

bool ClientReport(UaContext* ua, ClientResource* client, const char* msg)
{
    if (client->Protocol != APT_NATIVE) {
      ua->SendMsg("client protocol not supported.\n");
      return false;
    }
    socket_ptr fd = NativeConnectToClient(ua, client);
    if (!fd) { return false; }

    fd->fsend(msg);

    while (fd->recv() >= 0) { ua->SendMsg("%s", fd->msg); }
    return true;
}

bool StorageReport(UaContext* ua, StorageResource* storage, const char* msg)
{
  if (storage->Protocol != APT_NATIVE) {
    ua->SendMsg("storage protocol not supported.\n");
    return false;
  }
  socket_ptr sd = NativeConnectToStorage(ua, storage);
  if (!sd) { return false; }

  sd->fsend(msg);

  while (sd->recv() >= 0) { ua->SendMsg("%s", sd->msg); }
  return true;
}

struct connection
{
  std::string_view address;
  std::uint32_t    port;

  bool operator==(const connection& other) const
  {
    return address == other.address && port == other.port;
  }
};

struct connection_hash
{
  std::size_t operator()(const connection& val) const
  {
    std::size_t h1 = std::hash<std::string_view>{}(val.address);
    std::size_t h2 = std::hash<std::uint32_t>{}(val.port);
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
  }
};

std::vector<StorageResource*> FindUniqueStorages(UaContext* ua)
{
  std::unordered_set<connection, connection_hash> combinations;
  std::vector<StorageResource*> storages;

  StorageResource *store;
  foreach_res (store, R_STORAGE) {
    if (!ua->AclAccessOk(Storage_ACL, store->resource_name_)) { continue; }
    if (combinations.find(connection{std::string_view{store->address},
				     store->SDport}) != combinations.end()) { continue; }
    storages.push_back(store);
  }
  return storages;
}

std::vector<ClientResource*> FindUniqueClients(UaContext* ua)
{
  std::unordered_set<connection, connection_hash> combinations;
  std::vector<ClientResource*> clients;

  ClientResource *store;
  foreach_res (store, R_CLIENT) {
    if (!ua->AclAccessOk(Client_ACL, store->resource_name_)) { continue; }
    if (combinations.find(connection{std::string_view{store->address},
				     store->FDport}) != combinations.end()) { continue; }
    clients.push_back(store);
  }
  return clients;
}

bool ReportCmd(UaContext* ua, const char*)
{
  if (ua->argc < 2) {
    ua->SendMsg(
        "1900 Bad report command; Usage report <target> <args...>.\n");
    return false;
  }

  const char* target = ua->argk[1];

  std::string msg{"report"};

  for (int i = 2; i < ua->argc; ++i) {
    msg += " ";
    if (ua->argv[i]) {
      msg += ua->argk[i];
      msg += "=";
      msg += ua->argv[i];
    } else {
      msg += ua->argk[i];
    }
  }

  if (Bstrcasecmp(target, "all")) {
    DoDirectorReport(ua, msg.c_str());

    std::vector storages = FindUniqueStorages(ua);
    std::vector clients = FindUniqueClients(ua);

    for (auto* storage : storages) {
      StorageReport(ua, storage, msg.c_str());
    }

    for (auto* client : clients) {
      ClientReport(ua, client, msg.c_str());
    }
  } else if (Bstrcasecmp(target, "dir")) {
    DoDirectorReport(ua, msg.c_str());
  } else if (Bstrcasecmp(target, "storage")) {
    StorageResource* storage = get_storage_resource(ua);
    return StorageReport(ua, storage, msg.c_str());
  } else if (Bstrcasecmp(target, "client")) {
    ClientResource* client = get_client_resource(ua);
    return ClientReport(ua, client, msg.c_str());
  } else {
    ua->SendMsg("1900 Bad report command: unkown target '%s'.\n", target);
    return false;
  }

  return true;
}

};  // namespace directordaemon
