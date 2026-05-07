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
#include "../proxy_server.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace net = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = net::ip::tcp;

namespace {

uint16_t FindUnusedTcpPort()
{
  net::io_context io_context;
  tcp::acceptor acceptor{io_context, {net::ip::make_address("127.0.0.1"), 0}};
  return acceptor.local_endpoint().port();
}

void WakeTcpListener(uint16_t port)
{
  try {
    net::io_context io_context;
    tcp::resolver resolver{io_context};
    beast::tcp_stream stream{io_context};
    auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
    stream.connect(endpoints);
    boost::system::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  } catch (...) {
  }
}

class FakeUpstream {
 public:
  FakeUpstream() : port_{FindUnusedTcpPort()} {}

  void Start()
  {
    thread_ = std::thread([this]() { Run(); });
    for (int attempt = 0; attempt < 100 && !ready_; ++attempt) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  ~FakeUpstream() { Stop(); }

  void Stop()
  {
    stop_requested_ = true;
    WakeTcpListener(port_);
    if (acceptor_) {
      boost::system::error_code ec;
      acceptor_->close(ec);
    }
    if (thread_.joinable()) { thread_.join(); }
  }

  uint16_t port() const { return port_; }
  std::string last_method() const { return last_method_; }
  std::string last_target() const { return last_target_; }
  std::string last_body() const { return last_body_; }

 private:
  void Run()
  {
    net::io_context io_context;
    tcp::acceptor acceptor{io_context,
                           {net::ip::make_address("127.0.0.1"), port_}};
    acceptor_ = &acceptor;
    ready_ = true;

    boost::system::error_code ec;
    while (!stop_requested_) {
      tcp::socket socket{io_context};
      acceptor.accept(socket, ec);
      if (stop_requested_) { break; }
      if (ec) { continue; }

      beast::flat_buffer buffer;
      http::request<http::string_body> request;
      http::read(socket, buffer, request, ec);
      if (ec) { continue; }

      last_method_ = std::string{request.method_string()};
      last_target_ = std::string{request.target()};
      last_body_ = request.body();

      http::response<http::string_body> response{http::status::ok,
                                                 request.version()};
      response.set(http::field::content_type, "text/plain; charset=utf-8");
      response.body() = last_method_ + " " + last_target_ + " " + last_body_;
      response.prepare_payload();
      http::write(socket, response, ec);
      socket.shutdown(tcp::socket::shutdown_both, ec);
    }
  }

  uint16_t port_;
  std::atomic<bool> ready_{false};
  std::atomic<bool> stop_requested_{false};
  std::thread thread_;
  tcp::acceptor* acceptor_{nullptr};
  std::string last_method_;
  std::string last_target_;
  std::string last_body_;
};

http::response<http::string_body> SendRequest(uint16_t port,
                                              http::verb method,
                                              std::string target,
                                              std::string body)
{
  net::io_context io_context;
  tcp::resolver resolver{io_context};
  beast::tcp_stream stream{io_context};
  auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
  stream.connect(endpoints);

  http::request<http::string_body> request{method, std::move(target), 11};
  request.set(http::field::host, "127.0.0.1");
  request.set(http::field::user_agent, "webui-proxy-test");
  request.body() = std::move(body);
  request.prepare_payload();
  http::write(stream, request);

  beast::flat_buffer buffer;
  http::response<http::string_body> response;
  http::read(stream, buffer, response);
  boost::system::error_code ec;
  stream.socket().shutdown(tcp::socket::shutdown_both, ec);
  return response;
}

TEST(BconfigHttpProxy, ForwardsRequestsThroughMainProxyListener)
{
  FakeUpstream upstream;
  upstream.Start();

  ProxyConfig proxy_cfg;
  proxy_cfg.bind_host = "127.0.0.1";
  proxy_cfg.port = FindUnusedTcpPort();

  BconfigHttpProxyConfig cfg;
  cfg.upstream_host = "127.0.0.1";
  cfg.upstream_port = upstream.port();

  ProxyServer proxy{proxy_cfg, cfg};
  std::thread proxy_thread([&]() { proxy.Run(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto response = SendRequest(proxy_cfg.port, http::verb::post,
                              "/api/bconfig/v1/schema/director?verbose=true",
                              "{\"x\":1}");

  proxy.Stop();
  WakeTcpListener(proxy_cfg.port);
  proxy_thread.join();
  upstream.Stop();

  EXPECT_EQ(response.result(), http::status::ok);
  EXPECT_EQ(response.body(),
            "POST /api/bconfig/v1/schema/director?verbose=true {\"x\":1}");
  EXPECT_EQ(upstream.last_method(), "POST");
  EXPECT_EQ(upstream.last_target(),
            "/api/bconfig/v1/schema/director?verbose=true");
  EXPECT_EQ(upstream.last_body(), "{\"x\":1}");
}

TEST(BconfigHttpProxy, RejectsUnknownHttpRoutesOnMainProxyListener)
{
  FakeUpstream upstream;
  upstream.Start();

  ProxyConfig proxy_cfg;
  proxy_cfg.bind_host = "127.0.0.1";
  proxy_cfg.port = FindUnusedTcpPort();

  BconfigHttpProxyConfig cfg;
  cfg.upstream_host = "127.0.0.1";
  cfg.upstream_port = upstream.port();

  ProxyServer proxy{proxy_cfg, cfg};
  std::thread proxy_thread([&]() { proxy.Run(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  auto response
      = SendRequest(proxy_cfg.port, http::verb::get, "/not-bconfig", "");

  proxy.Stop();
  WakeTcpListener(proxy_cfg.port);
  proxy_thread.join();
  upstream.Stop();

  EXPECT_EQ(response.result(), http::status::not_found);
  EXPECT_TRUE(upstream.last_target().empty());
}

}  // namespace
