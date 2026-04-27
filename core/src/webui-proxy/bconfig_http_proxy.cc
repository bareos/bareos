/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#include "bconfig_http_proxy.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <cstdio>
#include <thread>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

namespace {

void WakeTcpListener(const std::string& host, int port)
{
  try {
    net::io_context io_context;
    tcp::resolver resolver{io_context};
    beast::tcp_stream stream{io_context};
    auto endpoints = resolver.resolve(host, std::to_string(port));
    stream.connect(endpoints);
    boost::system::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  } catch (...) {
  }
}

http::response<http::string_body> MakeBadGatewayResponse(
    const http::request<http::string_body>& request,
    std::string_view message)
{
  http::response<http::string_body> response{http::status::bad_gateway,
                                             request.version()};
  response.set(http::field::content_type, "text/plain; charset=utf-8");
  response.body() = std::string{message};
  response.keep_alive(request.keep_alive());
  response.prepare_payload();
  return response;
}

http::response<http::string_body> ForwardRequest(
    const BconfigHttpProxyConfig& cfg,
    const http::request<http::string_body>& request)
{
  net::io_context io_context;
  tcp::resolver resolver{io_context};
  beast::tcp_stream stream{io_context};

  const auto endpoints
      = resolver.resolve(cfg.upstream_host, std::to_string(cfg.upstream_port));
  stream.connect(endpoints);

  http::request<http::string_body> upstream{request.method(),
                                            std::string{request.target()},
                                            request.version()};
  upstream.body() = request.body();
  upstream.keep_alive(false);
  upstream.chunked(false);
  for (const auto& header : request) {
    if (header.name() == http::field::host
        || header.name() == http::field::content_length
        || header.name() == http::field::connection) {
      continue;
    }
    upstream.set(header.name(), header.value());
  }
  upstream.set(http::field::host,
               cfg.upstream_host + ":" + std::to_string(cfg.upstream_port));
  upstream.prepare_payload();

  http::write(stream, upstream);

  beast::flat_buffer buffer;
  http::response<http::string_body> response;
  http::read(stream, buffer, response);

  boost::system::error_code ec;
  stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  response.keep_alive(request.keep_alive());
  return response;
}

void HandleHttpSession(tcp::socket socket, const BconfigHttpProxyConfig& cfg)
{
  beast::flat_buffer buffer;
  boost::system::error_code ec;

  for (;;) {
    http::request<http::string_body> request;
    http::read(socket, buffer, request, ec);
    if (ec == http::error::end_of_stream) { break; }
    if (ec) { break; }

    http::response<http::string_body> response
        = MakeBadGatewayResponse(request, "upstream proxy failure");
    try {
      response = ForwardRequest(cfg, request);
    } catch (const std::exception& ex) {
      response = MakeBadGatewayResponse(request, ex.what());
    }

    http::write(socket, response, ec);
    if (ec || response.need_eof() || !request.keep_alive()) { break; }
  }

  socket.shutdown(tcp::socket::shutdown_send, ec);
}

}  // namespace

BconfigHttpProxyServer::BconfigHttpProxyServer(const BconfigHttpProxyConfig& cfg)
    : cfg_(cfg)
{
}

void BconfigHttpProxyServer::Run()
{
  io_context_ = std::make_unique<net::io_context>();
  tcp::resolver resolver{*io_context_};
  auto endpoints = resolver.resolve(cfg_.bind_host,
                                    std::to_string(cfg_.listen_port),
                                    tcp::resolver::flags::passive);

  auto acceptor = std::make_unique<tcp::acceptor>(*io_context_);
  boost::system::error_code ec;
  bool bound = false;
  for (const auto& endpoint : endpoints) {
    acceptor->open(endpoint.endpoint().protocol(), ec);
    if (ec) { continue; }
    acceptor->set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
      acceptor->close(ec);
      continue;
    }
    acceptor->bind(endpoint.endpoint(), ec);
    if (ec) {
      acceptor->close(ec);
      continue;
    }
    acceptor->listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
      acceptor->close(ec);
      continue;
    }
    bound = true;
    break;
  }

  if (!bound) {
    throw std::runtime_error("bconfig proxy could not bind to " + cfg_.bind_host
                             + ":" + std::to_string(cfg_.listen_port));
  }

  acceptor_ = std::move(acceptor);
  fprintf(stderr, "[proxy] listening on http://%s:%d -> %s:%d\n",
          cfg_.bind_host.c_str(), cfg_.listen_port, cfg_.upstream_host.c_str(),
          cfg_.upstream_port);

  while (!stop_requested_) {
    tcp::socket socket{*io_context_};
    acceptor_->accept(socket, ec);
    if (stop_requested_) { break; }
    if (ec) {
      if (ec == net::error::operation_aborted
          || ec == beast::error::timeout) {
        break;
      }
      continue;
    }

    std::thread([cfg = cfg_, socket = std::move(socket)]() mutable {
      HandleHttpSession(std::move(socket), cfg);
    }).detach();
  }
}

void BconfigHttpProxyServer::Stop()
{
  stop_requested_ = true;
  WakeTcpListener(cfg_.bind_host, cfg_.listen_port);
  if (acceptor_) {
    boost::system::error_code ec;
    acceptor_->close(ec);
  }
  if (io_context_) { io_context_->stop(); }
}
