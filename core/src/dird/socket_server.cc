/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2021 Bareos GmbH & Co. KG

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
 * Kern Sibbald, August MM
 * Extracted from other source files by Marco van Wieringen, October 2014
 */
/**
 * @file
 * This file handles external connections made to the director.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/fd_cmds.h"
#include "dird/ua_server.h"
#include "lib/berrno.h"
#include "lib/bnet_server_tcp.h"
#include "lib/thread_list.h"
#include "lib/thread_specific_data.h"
#include "lib/try_tls_handshake_as_a_server.h"

#include <atomic>

namespace directordaemon {

static char hello_client_with_version[]
    = "Hello Client %127s FdProtocolVersion=%d calling";

static char hello_client[] = "Hello Client %127s calling";

/* Global variables */
static ThreadList thread_list;
static alist<s_sockfd*>* sock_fds = NULL;
static pthread_t tcp_server_tid;
static ConnectionPool* client_connections = NULL;

static std::atomic<BnetServerState> server_state(BnetServerState::kUndefined);

struct s_addr_port {
  char* addr;
  char* port;
};

// Sanity check for the lengths of the Hello messages.
#define MIN_MSG_LEN 15
#define MAX_MSG_LEN (int)sizeof(name) + 25

ConnectionPool* get_client_connections() { return client_connections; }

static void* HandleConnectionRequest(ConfigurationParser* config, void* arg)
{
  BareosSocket* bs = (BareosSocket*)arg;
  char name[MAX_NAME_LENGTH];
  char tbuf[MAX_TIME_LENGTH];
  int fd_protocol_version = 0;

  if (!TryTlsHandshakeAsAServer(bs, config)) {
    bs->signal(BNET_TERMINATE);
    bs->close();
    delete bs;
    return nullptr;
  }

  if (bs->recv() <= 0) {
    Emsg1(M_ERROR, 0, _("Connection request from %s failed.\n"), bs->who());
    Bmicrosleep(5, 0); /* make user wait 5 seconds */
    bs->signal(BNET_TERMINATE);
    bs->close();
    delete bs;
    return nullptr;
  }

  // Do a sanity check on the message received
  if (bs->message_length < MIN_MSG_LEN || bs->message_length > MAX_MSG_LEN) {
    Dmsg1(000, "<filed: %s", bs->msg);
    Emsg2(M_ERROR, 0, _("Invalid connection from %s. Len=%d\n"), bs->who(),
          bs->message_length);
    Bmicrosleep(5, 0); /* make user wait 5 seconds */
    bs->signal(BNET_TERMINATE);
    bs->close();
    delete bs;
    return nullptr;
  }

  Dmsg1(110, "Conn: %s", bs->msg);

  // See if this is a File daemon connection. If so call FD handler.
  if ((sscanf(bs->msg, hello_client_with_version, name, &fd_protocol_version)
       == 2)
      || (sscanf(bs->msg, hello_client, name) == 1)) {
    Dmsg1(110, "Got a FD connection at %s\n",
          bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
    return HandleFiledConnection(client_connections, bs, name,
                                 fd_protocol_version);
  }
  return HandleUserAgentClientRequest(bs);
}

static void* UserAgentShutdownCallback(void* bsock)
{
  if (bsock) {
    BareosSocket* b = reinterpret_cast<BareosSocket*>(bsock);
    b->SetTerminated();
  }
  return nullptr;
}

static void CleanupConnectionPool()
{
  if (client_connections) { client_connections->cleanup(); }
}

extern "C" void* connect_thread(void* arg)
{
  SetJcrInThreadSpecificData(nullptr);

  // Permit MaxConnections connections.
  sock_fds = new alist<s_sockfd*>(10, not_owned_by_alist);
  BnetThreadServerTcp((dlist<IPADDR>*)arg, me->MaxConnections, sock_fds,
                      thread_list, HandleConnectionRequest, my_config,
                      &server_state, UserAgentShutdownCallback,
                      CleanupConnectionPool);

  return NULL;
}
#include <errno.h>
/**
 * Called here by Director daemon to start UA (user agent)
 * command thread. This routine creates the thread and then
 * returns.
 */
bool StartSocketServer(dlist<IPADDR>* addrs)
{
  int status;

  if (client_connections == nullptr) {
    client_connections = new ConnectionPool();
  }

  server_state.store(BnetServerState::kUndefined);

  if ((status
       = pthread_create(&tcp_server_tid, nullptr, connect_thread, (void*)addrs))
      != 0) {
    BErrNo be;
    Emsg1(M_ABORT, 0, _("Cannot create UA thread: %s\n"), be.bstrerror(status));
  }

  int tries = 200; /* consider bind() tries in BnetThreadServerTcp */
  int wait_ms = 100;
  do {
    Bmicrosleep(0, wait_ms * 1000);
    if (server_state.load() != BnetServerState::kUndefined) { break; }
  } while (--tries);

  if (server_state != BnetServerState::kStarted) {
    if (client_connections) {
      delete (client_connections);
      client_connections = nullptr;
    }
    return false;
  }
  return true;
}

void StopSocketServer()
{
  if (sock_fds) {
    BnetStopAndWaitForThreadServerTcp(tcp_server_tid);
    delete sock_fds;
    sock_fds = nullptr;
  }
  if (client_connections) {
    delete (client_connections);
    client_connections = nullptr;
  }
}
} /* namespace directordaemon */
