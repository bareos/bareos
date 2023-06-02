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

#include "websocketjsonrpcserver.h"
#include "dird/ua_status.h"

// Define a callback to handle incoming messages
void WebsocketJsonRpcServer::on_message(wsasioserver* wsserver,
                                        jsonrpccxx::JsonRpc2Server* rpcServer,
                                        websocketpp::connection_hdl hdl,
                                        message_ptr msg)
{
  try {
    std::string response = rpcServer->HandleRequest(msg->get_payload());

    wsserver->send(hdl, response, msg->get_opcode());
  } catch (websocketpp::exception const& e) {
    std::cout << "Operation failed because: "
              << "(" << e.what() << ")" << std::endl;
  }
}

void WebsocketJsonRpcServer::on_open(wsasioserver* wsserver,
                                     websocketpp::connection_hdl hdl)
{
  try {
    wsserver->send(hdl,
                   "welcome to the websocket console! enter you command below:",
                   websocketpp::frame::opcode::text);
  } catch (websocketpp::exception const& e) {
    std::cout << "Operation failed because: "
              << "(" << e.what() << ")" << std::endl;
  }
}

void WebsocketJsonRpcServer::on_close(wsasioserver* wsserver,
                                      websocketpp::connection_hdl hdl)
{
  try {
    wsserver->send(hdl, "Ciao!", websocketpp::frame::opcode::text);
  } catch (websocketpp::exception const& e) {
    std::cout << "Operation failed because: "
              << "(" << e.what() << ")" << std::endl;
  }
}

WebsocketJsonRpcServer::WebsocketJsonRpcServer(
    std::unique_ptr<jsonrpccxx::JsonRpc2Server> rpcserv)
    : rpcServer(std::move(rpcserv))
{
}

bool WebsocketJsonRpcServer::StartListening()
{
  // Set logging settings
  wsserver.set_access_channels(websocketpp::log::alevel::all);
  wsserver.clear_access_channels(websocketpp::log::alevel::frame_payload);

  // Initialize Asio
  wsserver.init_asio();

  // Register our message handler
  wsserver.set_message_handler(bind(&WebsocketJsonRpcServer::on_message, this,
                                    &wsserver, rpcServer.get(), ::_1, ::_2));

  wsserver.set_open_handler(
      bind(&WebsocketJsonRpcServer::on_open, this, &wsserver, ::_1));
  wsserver.set_close_handler(
      bind(&WebsocketJsonRpcServer::on_close, this, &wsserver, ::_1));

  // Listen on port 9002
  wsserver.listen(9002);

  // Start the server accept loop
  wsserver.start_accept();

  wsserver.run();
  return true;
}

void WebsocketJsonRpcServer::StopWebsocketThread() { listenning_thread.join(); }

void WebsocketJsonRpcServer::StartWebsocketThread()
{
  listenning_thread = std::thread([this]() { StartListening(); });
}
