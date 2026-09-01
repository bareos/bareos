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
  using global_resource::Type;

  BareosSocket* bs = (BareosSocket*)arg;

  bs->SetEnableKtls(me->enable_ktls);

  auto config = parser->GetCurrentConfiguration();

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

  std::optional parsed_hello = BareosAccept(bs, Type::Storage, myself, &auth);
  if (!parsed_hello) {
    Emsg2(M_ERROR, 0, "could not accept connection from %s\n", bs->who());
    return error_and_close(bs);
  }

  switch (auth.GetType()) {
    case Auth::inbound_type::Director: {
      bs->sleep_time_after_authentication_error = 0;

      if (!bs->fsend("3000 OK Hello\n")) {
        Emsg2(M_ERROR, 0, "Could not send OK Hello to %s\n", bs->who());
        return error_and_close(bs);
      }
      return HandleDirectorConnection(bs, auth.director->res);
    } break;
    case Auth::inbound_type::Job: {
      auto* jcr = auth.job->jcr;
      auth.job->jcr = nullptr;

      if (parsed_hello->from == Type::Client) {
        jcr->authenticated = true;
        return HandleFiledConnection(bs, jcr);
      } else if (parsed_hello->from == Type::Storage) {
        jcr->authenticated = true;
        return handle_stored_connection(bs, jcr);
      }
    } break;
    default: {
    } break;
  }
  Emsg2(M_ERROR, 0, "could not accept connection from %s\n", bs->who());
  return error_and_close(bs);
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
