/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, August MM
 * Extracted from other source files by Marco van Wieringen, October 2014
 */
/**
 * @file
 * This file handles external connections made to the director.
 */

#include "include/bareos.h"
#include "dird/ua.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/fd_cmds.h"
#include "dird/ua_server.h"
#include "lib/berrno.h"
#include "lib/bnet.h"
#include "lib/bnet_server_tcp.h"
#include "lib/bsock.h"
#include "lib/bsys.h"
#include "lib/global_resource.h"
#include "lib/thread_list.h"
#include "lib/thread_specific_data.h"
#include "dird/authenticate.h"
#include "lib/version.h"

#include <atomic>

namespace directordaemon {

/* Global variables */
static ThreadList thread_list;
static std::atomic<bool> server_running;
static pthread_t tcp_server_tid;
static std::unique_ptr<connection_pool> client_connections{nullptr};

static std::atomic<BnetServerState> server_state(BnetServerState::kUndefined);

struct s_addr_port {
  char* addr;
  char* port;
};

// Sanity check for the lengths of the Hello messages.
#define MIN_MSG_LEN 15
#define MAX_MSG_LEN (int)sizeof(name) + 25

connection_pool& get_client_connections() { return *client_connections.get(); }

static void* HandleConnectionRequest(ConfigurationParser* parser, void* arg)
{
  BareosSocket* bs = (BareosSocket*)arg;
  auto config = parser->GetCurrentConfiguration();

  auto* myself = dynamic_cast<DirectorResource*>(
      config->GetNextRes(R_DIRECTOR, nullptr));

  auto error_and_close = [](BareosSocket* socket) {
    Bmicrosleep(socket->sleep_time_after_authentication_error, 0);
    socket->signal(BNET_TERMINATE);
    socket->close();
    delete socket;
    return nullptr;
  };
  if (!myself) {
    Emsg2(M_ERROR, 0,
          "Could not find myself during connection attempt from %s\n",
          bs->who());
    return error_and_close(bs);
  }

  DirectorAuth auth{config};

  bs->SetEnableKtls(myself->enable_ktls);

  using global_resource::Type;

  std::optional parsed_hello = BareosAccept(bs, Type::Director, myself, &auth);
  if (!parsed_hello) { return error_and_close(bs); }

  switch (auth.GetType()) {
    case DirectorAuth::inbound_type::Client: {
      // we are authenticated now, so the client does not need to wait anymore
      bs->sleep_time_after_authentication_error = 0;

      if (!IsConnectFromClientAllowed(auth.client->res)) {
        /* clients ignore our response message anyways, so it does not make
         * sense to send any message.
         * There also is not a good response id for this. */
        Emsg2(M_ERROR, 0, "Client '%s' (as %s) is not allowed to connect!\n",
              bs->who(), auth.client->res->resource_name_);
        return error_and_close(bs);
      }

      if (!FormatAndSendResponseMessage(
              bs, kMessageIdOk, "OK: %s Version %s (%s)\n", my_name,
              kBareosVersionStrings.Full, kBareosVersionStrings.Date)) {
        return error_and_close(bs);
      }

      return HandleFiledConnection(*client_connections.get(), bs,
                                   auth.client->res,
                                   parsed_hello->fd_protocol_version);
    } break;
    case DirectorAuth::inbound_type::Console: {
      // Now that the _console_ connection is authenticated, we still
      // need to do the authorization part

      std::unique_ptr<UserAcl> user_acl{};
      if (!auth.console->is_default) {
        user_acl = UserAcl::from_config(auth.console->res);
      }

      if (auth.console->res->use_pam_authentication_) {
        // if pam authentication is used, then the client is additionally
        // authenticated as a user, and we use that users acls.

        if (!SendResponseMessage(bs, kMessageIdPamRequired, "")) {
          Emsg4(M_ERROR, 0,
                T_("Unable to pam authenticate console \"%s\" at %s:%s:%d.\n"),
                auth.console->res->resource_name_, bs->who(), bs->host(),
                bs->port());
          return error_and_close(bs);
        }

        user_acl = AuthenticatePamUser(bs, config.get());

        if (!user_acl) {
          Emsg4(M_ERROR, 0,
                T_("Unable to pam authenticate console \"%s\" at %s:%s:%d.\n"),
                auth.console->res->resource_name_, bs->who(), bs->host(),
                bs->port());
          return error_and_close(bs);
        }
      }

      // we are authenticated now, so the client does not need to wait anymore
      bs->sleep_time_after_authentication_error = 0;

      if (!FormatAndSendResponseMessage(
              bs, kMessageIdOk, "OK: %s Version %s (%s)\n", my_name,
              kBareosVersionStrings.Full, kBareosVersionStrings.Date)) {
        Emsg4(M_ERROR, 0,
              T_("Unable to authenticate console \"%s\" at %s:%s:%d.\n"),
              auth.console->res->resource_name_, bs->who(), bs->host(),
              bs->port());
        return error_and_close(bs);
      }

      std::string message;
      message += kBareosVersionStrings.ServicesMessage;
      message += "\n";
      message += "You are ";
      if (user_acl) {
        message += "logged in as: ";
        message += user_acl->name;
      } else {
        message += "connected using the default console";
      }
      if (!SendResponseMessage(bs, kMessageIdInfoMessage, message.c_str())) {
        Emsg4(M_ERROR, 0,
              T_("Unable to authenticate console \"%s\" at %s:%s:%d.\n"),
              auth.console->res->resource_name_, bs->who(), bs->host(),
              bs->port());
        return error_and_close(bs);
      }

      JobControlRecord* console_jcr = new_control_jcr("-Console-", JT_CONSOLE);
      auto* ctx = new UaContext(console_jcr);
      ctx->UA_sock = bs;
      ctx->user_acl = std::move(user_acl);

      return HandleUserAgentClientRequest(ctx);
    } break;
    case DirectorAuth::inbound_type::Unknown: {
      // intentionally left empty
    } break;
  }

  Emsg1(M_ERROR, 0, T_("Connection request from %s failed (unknown type).\n"),
        bs->who());
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

static void CleanupConnectionPool()
{
  if (client_connections) { client_connections->cleanup(); }
}

extern "C" void* connect_with_bound_thread(void* arg)
{
  SetJcrInThreadSpecificData(nullptr);

  auto bound_sockets = std::move(*(std::vector<s_sockfd>*)arg);
  if (bound_sockets.size()) {
    server_running = true;
    BnetThreadServerTcp(std::move(bound_sockets), thread_list,
                        HandleConnectionRequest, my_config, &server_state,
                        UserAgentShutdownCallback, CleanupConnectionPool);

  } else {
    server_state = BnetServerState::kError;
  }

  return NULL;
}
#include <errno.h>
/**
 * Called here by Director daemon to start UA (user agent)
 * command thread. This routine creates the thread and then
 * returns.
 */
bool StartSocketServer(std::vector<s_sockfd>&& bound_sockets)
{
  if (!client_connections) { client_connections.reset(new connection_pool()); }

  server_state.store(BnetServerState::kUndefined);

  if (int status
      = pthread_create(&tcp_server_tid, nullptr, connect_with_bound_thread,
                       (void*)&bound_sockets);
      status != 0) {
    BErrNo be;
    Emsg1(M_ABORT, 0, T_("Cannot create UA thread: %s\n"),
          be.bstrerror(status));
  }

  int tries = 200; /* consider bind() tries in BnetThreadServerTcp */
  int wait_ms = 100;
  do {
    Bmicrosleep(0, wait_ms * 1000);

    auto current_state = server_state.load();

    if (current_state == BnetServerState::kStarted
        || current_state == BnetServerState::kError) {
      // waiting longer won't change anything
      break;
    }
  } while (--tries);

  if (server_state != BnetServerState::kStarted) {
    if (client_connections) { client_connections.reset(nullptr); }
    return false;
  }
  return true;
}

bool StartSocketServer(dlist<IPADDR>* addrs)
{
  auto bound_sockets = OpenAndBindSockets(addrs);

  return StartSocketServer(std::move(bound_sockets));
}

void StopSocketServer()
{
  if (server_running) {
    BnetStopAndWaitForThreadServerTcp(tcp_server_tid);
    server_running = false;
  }
  if (client_connections) {
    // client_connections can be NULL if the socket server was never started.
    client_connections->clear();
  }
}
} /* namespace directordaemon */
