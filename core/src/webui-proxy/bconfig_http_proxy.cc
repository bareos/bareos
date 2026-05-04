/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include <stdexcept>

#include <sys/socket.h>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

namespace {

bool HasPathPrefix(std::string_view path, std::string_view prefix)
{
  return path == prefix
         || (path.size() > prefix.size() && path.starts_with(prefix)
             && path[prefix.size()] == '/');
}

std::string_view ExtractPath(std::string_view target)
{
  return target.substr(0, target.find('?'));
}

http::response<http::string_body> MakeErrorResponse(
    const http::request<http::string_body>& request,
    http::status status,
    std::string_view message)
{
  http::response<http::string_body> response{status, request.version()};
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

  http::request<http::string_body> upstream{
      request.method(), std::string{request.target()}, request.version()};
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

http::response<http::string_body> HandleHttpRequest(
    const BconfigHttpProxyConfig& cfg,
    const http::request<http::string_body>& request)
{
  if (!IsBconfigProxyRoute(
          std::string_view{request.target().data(), request.target().size()})) {
    return MakeErrorResponse(request, http::status::not_found,
                             "route not found.");
  }

  try {
    return ForwardRequest(cfg, request);
  } catch (const std::exception& ex) {
    return MakeErrorResponse(request, http::status::bad_gateway, ex.what());
  }
}

tcp::socket MakeSocket(net::io_context& io_context, int fd, int address_family)
{
  tcp::socket socket{io_context};
  switch (address_family) {
    case AF_INET6:
      socket.assign(tcp::v6(), fd);
      return socket;
    case AF_INET:
    default:
      socket.assign(tcp::v4(), fd);
      return socket;
  }
}

void HandleHttpSession(int fd,
                       int address_family,
                       const BconfigHttpProxyConfig& cfg)
{
  net::io_context io_context;
  auto socket = MakeSocket(io_context, fd, address_family);
  beast::flat_buffer buffer;
  boost::system::error_code ec;

  for (;;) {
    http::request<http::string_body> request;
    http::read(socket, buffer, request, ec);
    if (ec == http::error::end_of_stream) { break; }
    if (ec) { break; }

    auto response = HandleHttpRequest(cfg, request);

    http::write(socket, response, ec);
    if (ec || response.need_eof() || !request.keep_alive()) { break; }
  }

  socket.shutdown(tcp::socket::shutdown_send, ec);
}

}  // namespace

bool IsBconfigProxyRoute(std::string_view target)
{
  const auto path = ExtractPath(target);
  return HasPathPrefix(path, "/api/bconfig") || HasPathPrefix(path, "/v1")
         || HasPathPrefix(path, "/ui");
}

void RunBconfigHttpProxySession(int fd,
                                int address_family,
                                const BconfigHttpProxyConfig& cfg)
{
  HandleHttpSession(fd, address_family, cfg);
}
