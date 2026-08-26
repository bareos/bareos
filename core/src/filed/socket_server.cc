/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, October MM
 * Extracted from other source files by Marco van Wieringen, October 2014
 */
/**
 * @file
 * This file handles external connections made to the File daemon.
 */

#include "include/bareos.h"
#include "filed/filed_conf.h"
#include "filed/authenticate.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/dir_cmd.h"
#include "filed/sd_cmds.h"
#include "include/version_hex.h"
#include "lib/bsock.h"
#include "lib/bnet_server_tcp.h"
#include "lib/thread_list.h"
#include "lib/try_tls_handshake_as_a_server.h"

namespace filedaemon {

/* Global variables */
static ThreadList thread_list;
static pthread_t tcp_server_tid;
static std::atomic<bool> server_running = false;

/**
 * Connection request. We accept connections either from the Director or the
 * Storage Daemon
 *
 * NOTE! We are running as a separate thread
 *
 * Send output one line at a time followed by a zero length transmission.
 * Return when the connection is terminated or there is an error.
 *
 * Basic tasks done here:
 *  - If it was a connection from an SD, call handle_stored_connection()
 *  - Otherwise it was a connection from the DIR, call
 * handle_director_connection()
 */
static void* HandleConnectionRequest(ConfigurationParser* parser, void* arg)
{
  BareosSocket* bs = (BareosSocket*)arg;

  auto config = parser->GetCurrentConfiguration();

  UseConfigAndJcrs tls_secret_provider{config, parser->resource_definitions_};

  auto* myself
      = dynamic_cast<ClientResource*>(config->GetNextRes(R_CLIENT, nullptr));

  bs->SetEnableKtls(myself->enable_ktls);

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
                        global_resource::Type::Client, myself->resource_name_),
                    myself, &tls_secret_provider, &auth)) {
    return error_and_close(bs);
  }

  switch (auth.GetType()) {
    case Auth::inbound_type::Director: {
      if (std::optional error
          = tls_secret_provider.is_resource_name_different_from_tls_name(
              R_DIRECTOR, auth.director->res->resource_name_)) {
        Emsg2(M_ERROR, 0, "Invalid connection from %s: ERR=%s\n", bs->who(),
              error->c_str());
        return error_and_close(bs);
      }

      bs->sleep_time_after_authentication_error = 0;

      if (!auth.director->res->conn_from_dir_to_fd) {
        Emsg2(M_ERROR, 0,
              "Connection from director %s (as %s) is not allowed\n", bs->who(),
              auth.director->res->resource_name_);
        return error_and_close(bs);
      }

      if (!bs->fsend("2000 OK Hello %d\n", FD_PROTOCOL_VERSION)) {
        Emsg2(M_ERROR, 0, "Could not send OK Hello to %s: ERR=%s\n", bs->who(),
              bs->bstrerror());
        return error_and_close(bs);
      }

      return handle_director_connection(bs, auth.director->res);
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
    }
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
    Dmsg1(10, "filed: listening on port %d\n", p->GetPortHostOrder());
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
  Dmsg0(100, "StopSocketServer\n");
  if (server_running) {
    BnetStopAndWaitForThreadServerTcp(tcp_server_tid);
    server_running = false;
  }
}
} /* namespace filedaemon */
