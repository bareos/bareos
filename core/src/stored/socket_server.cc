/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2026 Bareos GmbH & Co. KG

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
 * Extracted from other source files by Marco van Wieringen, October 2014
 */
/**
 * @file
 * This file handles external connections made to the storage daemon.
 */

#include <ranges>
#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/ascii_control_characters.h"
#include "stored/authenticate.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/dir_cmd.h"
#include "stored/fd_cmds.h"
#include "stored/sd_cmds.h"
#include "lib/bnet_server_tcp.h"
#include "lib/bsock.h"
#include "lib/thread_list.h"
#include "lib/try_tls_handshake_as_a_server.h"
#include "include/version_hex.h"

namespace storagedaemon {

static ThreadList thread_list;
static std::atomic<bool> server_running = false;
static pthread_t tcp_server_tid;

/**
 * Connection request. We accept connections either from the
 * Director, Storage Daemon or a Client (File daemon).
 *
 * Note, we are running as a separate thread of the Storage daemon.
 *
 * Basic tasks done here:
 *  - If it was a connection from the FD, call HandleFiledConnection()
 *  - If it was a connection from another SD, call handle_stored_connection()
 *  - Otherwise it was a connection from the DIR, call
 * handle_director_connection()
 */

void* HandleConnectionRequest(ConfigurationParser* parser, void* arg)
{
  BareosSocket* bs = (BareosSocket*)arg;

  bs->SetEnableKtls(me->enable_ktls);

  auto config = parser->GetCurrentConfiguration();
  UseConfigAndJcrs tls_secret_provider{config, parser->resource_definitions_};

  auto* myself
      = dynamic_cast<StorageResource*>(config->GetNextRes(R_STORAGE, nullptr));

  auto error_and_close = [](BareosSocket* socket) {
    Bmicrosleep(socket->sleep_time_after_authentication_error, 0);
    socket->signal(BNET_TERMINATE);
    socket->close();
    delete socket;
    return nullptr;
  };

  if (!myself) {
    Emsg1(M_ERROR, 0, "Could not find myself during connection attempt.\n");
    return error_and_close(bs);
  }

  Auth auth{config};

  if (!BareosAccept(bs,
                    global_resource::QualifiedName(
                        global_resource::Type::Storage, myself->resource_name_),
                    myself, &tls_secret_provider, &auth)) {
    return error_and_close(bs);
  }

  switch (auth.GetType()) {
    case Auth::inbound_type::Client: {
      if (std::optional error
          = tls_secret_provider.check_job_name(auth.client->jcr->Job)) {
        Emsg2(M_ERROR, 0, "Invalid connection from %s: ERR=%s\n", bs->who(),
              error->c_str());
        return error_and_close(bs);
      }

      auto* jcr = auth.client->jcr;
      auth.client->jcr = nullptr;
      jcr->authenticated = true;
      return HandleFiledConnection(bs, jcr);
    } break;
    case Auth::inbound_type::Director: {
      if (std::optional error
          = tls_secret_provider.is_resource_name_different_from_tls_name(
              R_DIRECTOR, auth.director->res->resource_name_)) {
        Emsg2(M_ERROR, 0, "Invalid connection from %s: ERR=%s\n", bs->who(),
              error->c_str());
        return error_and_close(bs);
      }

      bs->sleep_time_after_authentication_error = 0;

      if (!bs->fsend("3000 OK Hello\n")) { return error_and_close(bs); }
      return HandleDirectorConnection(bs, auth.director->res);
    } break;
    case Auth::inbound_type::Storage: {
      if (std::optional error
          = tls_secret_provider.check_job_name(auth.storage->jcr->Job)) {
        Emsg2(M_ERROR, 0, "Invalid connection from %s: ERR=%s\n", bs->who(),
              error->c_str());
        return error_and_close(bs);
      }

      auto* jcr = auth.storage->jcr;
      auth.storage->jcr = nullptr;
      jcr->authenticated = true;
      return handle_stored_connection(bs, jcr);
    } break;

    default: {
      return error_and_close(bs);
    } break;
  }
}

static void* UserAgentShutdownCallback(void* bsock)
{
  if (bsock) {
    BareosSocket* b = reinterpret_cast<BareosSocket*>(bsock);
    b->SetTerminated();
  }
  return nullptr;
}

void StartSocketServer(dlist<IPADDR>* addrs)
{
  IPADDR* p;

  tcp_server_tid = pthread_self();

  // Become server, and handle requests
  foreach_dlist (p, addrs) {
    Dmsg1(10, "stored: listening on port %d\n", p->GetPortHostOrder());
  }

  auto bound_sockets = OpenAndBindSockets(addrs);
  if (bound_sockets.size()) {
    server_running = true;
    BnetThreadServerTcp(std::move(bound_sockets), thread_list,
                        HandleConnectionRequest, my_config, nullptr,
                        UserAgentShutdownCallback);
  }
}

void StopSocketServer()
{
  if (server_running) {
    BnetStopAndWaitForThreadServerTcp(tcp_server_tid);
    server_running = false;
  }
}

} /* namespace storagedaemon */
