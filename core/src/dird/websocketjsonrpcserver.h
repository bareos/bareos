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

#ifndef BAREOS_LIB_WEBSOCKETJSONRPCSERVER_H
#define BAREOS_LIB_WEBSOCKETJSONRPCSERVER_H

#include "dird/bjsonrpcserver.h"
#include "websocketpp/config/asio_no_tls.hpp"
#include "websocketpp/server.hpp"
#include "lib/bsock.h"

typedef websocketpp::server<websocketpp::config::asio> wsasioserver;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// pull out the type of messages sent by our config
typedef wsasioserver::message_ptr message_ptr;

namespace directordaemon {

class WebsocketJsonRpcServer {
 public:
  WebsocketJsonRpcServer(JsonRpcServer* rpcserv);
  bool StartListening();

  void StopWebsocketThread();

  void StartWebsocketThread();

  void on_message(wsasioserver* wsserver,
                  websocketpp::connection_hdl hdl,
                  message_ptr msg);

  void on_close(wsasioserver* wsserver, websocketpp::connection_hdl hdl);
  void on_open(wsasioserver* wsserver, websocketpp::connection_hdl hdl);

 private:
  wsasioserver wsserver_;
  JsonRpcServer* rpcServer_;
  std::thread listenning_thread;
};

}  // namespace directordaemon

#endif  // BAREOS_LIB_WEBSOCKETJSONRPCSERVER_H
