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

#include "include/config.h"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <CLI/CLI.hpp>

namespace {

namespace fs = std::filesystem;

constexpr std::string_view kProtocolHello = "BAREOS-SETUP 1";
constexpr const char* kDefaultSetupStagingDir = PATH_BAREOS_WORKINGDIR "/setup";

enum class ConnectionDirection
{
  kClientConnects,
  kDirectorConnects,
};

struct Options {
  ConnectionDirection connection_direction = ConnectionDirection::kClientConnects;
  std::string listen_address = "127.0.0.1";
  uint16_t port = 10443;
  std::string connect_back_address;
  uint16_t connect_back_port = 0;
  uint16_t connect_timeout = 30;
  std::string certificate;
  std::string key;
  std::string director_name;
  std::string director_address;
  uint16_t director_port = 9101;
  fs::path staging_dir{kDefaultSetupStagingDir};
  std::string token;
  std::optional<std::string> shared_password;
  bool force = false;
};

struct SocketCloser {
  explicit SocketCloser(int fd = -1) : fd_(fd) {}
  ~SocketCloser()
  {
    if (fd_ >= 0) { close(fd_); }
  }
  SocketCloser(const SocketCloser&) = delete;
  SocketCloser& operator=(const SocketCloser&) = delete;
  SocketCloser(SocketCloser&& other) noexcept : fd_(other.release()) {}
  SocketCloser& operator=(SocketCloser&& other) noexcept
  {
    if (this != &other) {
      if (fd_ >= 0) { close(fd_); }
      fd_ = other.release();
    }
    return *this;
  }
  int get() const { return fd_; }
  int release()
  {
    int fd = fd_;
    fd_ = -1;
    return fd;
  }

 private:
  int fd_{-1};
};

struct AddrinfoDeleter {
  void operator()(addrinfo* addr) const { freeaddrinfo(addr); }
};

struct SslCtxDeleter {
  void operator()(SSL_CTX* ctx) const { SSL_CTX_free(ctx); }
};

class SslConnection {
 public:
  SslConnection(SSL* ssl, int fd) : ssl_(ssl), fd_(fd) {}
  ~SslConnection()
  {
    if (ssl_) {
      SSL_shutdown(ssl_);
      SSL_free(ssl_);
    }
    if (fd_ >= 0) { close(fd_); }
  }
  SslConnection(const SslConnection&) = delete;
  SslConnection& operator=(const SslConnection&) = delete;
  SSL* get() const { return ssl_; }

 private:
  SSL* ssl_{nullptr};
  int fd_{-1};
};

volatile sig_atomic_t should_stop = 0;

[[noreturn]] void Throw(const std::string& message)
{
  throw std::runtime_error(message);
}

std::string OpenSslError()
{
  std::array<char, 256> buffer{};
  unsigned long error = ERR_get_error();
  if (error == 0) { return "unknown OpenSSL error"; }
  ERR_error_string_n(error, buffer.data(), buffer.size());
  return buffer.data();
}

ConnectionDirection ParseConnectionDirection(const std::string& value)
{
  if (value == "client-connects") {
    return ConnectionDirection::kClientConnects;
  }
  if (value == "director-connects") {
    return ConnectionDirection::kDirectorConnects;
  }
  Throw("Unsupported connection direction \"" + value + "\".");
}

void ValidateName(std::string_view field, std::string_view name)
{
  if (name.empty()) { Throw(std::string{field} + " must not be empty."); }
  if (std::isspace(static_cast<unsigned char>(name.front()))
      || std::isspace(static_cast<unsigned char>(name.back()))) {
    Throw(std::string{field} + " must not start or end with whitespace.");
  }
  if (name.size() > 127) { Throw(std::string{field} + " is too long."); }
  if (name.find('/') != std::string_view::npos
      || name.find('\\') != std::string_view::npos) {
    Throw(std::string{field} + " must not contain path separators.");
  }
}

bool ConstantTimeEquals(std::string_view left, std::string_view right)
{
  if (left.size() != right.size()) { return false; }
  return CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

std::string GenerateSharedPassword()
{
  std::array<unsigned char, 33> bytes{};
  if (RAND_bytes(bytes.data(), bytes.size()) != 1) {
    Throw("Failed to generate a setup shared password: " + OpenSslError());
  }

  std::string encoded(EVP_ENCODE_LENGTH(bytes.size()), '\0');
  const int encoded_length
      = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(encoded.data()),
                        bytes.data(), bytes.size());
  if (encoded_length <= 0) {
    Throw("Failed to encode the setup shared password.");
  }
  encoded.resize(static_cast<size_t>(encoded_length));
  for (char& ch : encoded) {
    if (ch == '+') {
      ch = '-';
    } else if (ch == '/') {
      ch = '_';
    }
  }
  encoded.erase(std::remove(encoded.begin(), encoded.end(), '='),
                encoded.end());
  return encoded;
}

void WriteTextFileAtomic(const fs::path& path,
                         const std::string& content,
                         bool force)
{
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    Throw("Failed to create directory \"" + path.parent_path().string()
          + "\": " + ec.message());
  }

  if (!force && fs::exists(path, ec)) {
    Throw("Refusing to overwrite existing staged config \"" + path.string()
          + "\". Use --force.");
  }
  if (ec) {
    Throw("Failed to inspect \"" + path.string() + "\": " + ec.message());
  }

  fs::path temporary = path;
  temporary += ".tmp";
  if (force) { fs::remove(temporary, ec); }

  std::ofstream output(temporary, std::ios::binary);
  if (!output) {
    Throw("Failed to open temporary file \"" + temporary.string() + "\".");
  }
  output << content;
  output.close();
  if (!output) {
    Throw("Failed to write \"" + temporary.string() + "\".");
  }

  fs::rename(temporary, path, ec);
  if (ec) {
    fs::remove(temporary);
    Throw("Failed to replace \"" + path.string() + "\": " + ec.message());
  }
}

std::string BuildDirectorClientConfig(const std::string& client_name,
                                      const std::string& address,
                                      uint16_t port,
                                      const std::string& password)
{
  std::ostringstream config;
  config << "Client {\n";
  config << "  Name = " << client_name << "\n";
  config << "  Description = \"Generated by bareos-dir-setup-service.\"\n";
  config << "  Address = " << address << "\n";
  config << "  Password = \"" << password << "\"\n";
  config << "  Port = " << port << "\n";
  config << "}\n";
  return config.str();
}

std::string BuildFdDirectorConfig(const std::string& director_name,
                                  const std::string& password)
{
  std::ostringstream config;
  config << "Director {\n";
  config << "  Name = " << director_name << "\n";
  config << "  Password = \"" << password << "\"\n";
  config << "  Description = \"Generated by bareos-dir-setup-service.\"\n";
  config << "}\n";
  return config.str();
}

SocketCloser OpenListener(const std::string& address, uint16_t port)
{
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE;

  addrinfo* raw_results = nullptr;
  const auto port_string = std::to_string(port);
  if (getaddrinfo(address.c_str(), port_string.c_str(), &hints, &raw_results)
      != 0) {
    Throw("Failed to resolve the listener address.");
  }
  std::unique_ptr<addrinfo, AddrinfoDeleter> results(raw_results);

  for (addrinfo* current = results.get(); current != nullptr;
       current = current->ai_next) {
    SocketCloser socket(::socket(current->ai_family, current->ai_socktype,
                                 current->ai_protocol));
    if (socket.get() < 0) { continue; }

    int reuse = 1;
    setsockopt(socket.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    if (bind(socket.get(), current->ai_addr, current->ai_addrlen) != 0) {
      continue;
    }
    if (listen(socket.get(), 16) != 0) { continue; }
    return socket;
  }

  Throw("Failed to start the setup listener on " + address + ":"
        + std::to_string(port) + ".");
}

uint16_t GetBoundPort(int socket_fd)
{
  sockaddr_storage address{};
  socklen_t length = sizeof(address);
  if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &length)
      != 0) {
    Throw("Failed to determine the setup listener port.");
  }
  if (address.ss_family == AF_INET) {
    return ntohs(reinterpret_cast<sockaddr_in*>(&address)->sin_port);
  }
  if (address.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<sockaddr_in6*>(&address)->sin6_port);
  }
  Throw("Unsupported socket family for the setup listener.");
}

std::string SocketAddressToString(const sockaddr_storage& address)
{
  std::array<char, NI_MAXHOST> host{};
  const auto* sockaddr_ptr
      = reinterpret_cast<const sockaddr*>(&address);
  const socklen_t length
      = address.ss_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
  if (getnameinfo(sockaddr_ptr, length, host.data(), host.size(), nullptr, 0,
                  NI_NUMERICHOST)
      != 0) {
    return "";
  }
  return host.data();
}

std::unique_ptr<SSL_CTX, SslCtxDeleter> CreateTlsServerContext(
    const Options& options)
{
  std::unique_ptr<SSL_CTX, SslCtxDeleter> ssl_ctx(SSL_CTX_new(TLS_server_method()));
  if (!ssl_ctx) {
    Throw("Failed to initialize the TLS server context: " + OpenSslError());
  }
  SSL_CTX_set_min_proto_version(ssl_ctx.get(), TLS1_2_VERSION);

  if (SSL_CTX_use_certificate_file(ssl_ctx.get(), options.certificate.c_str(),
                                   SSL_FILETYPE_PEM)
      != 1) {
    Throw("Failed to load the TLS certificate: " + OpenSslError());
  }
  if (SSL_CTX_use_PrivateKey_file(ssl_ctx.get(), options.key.c_str(),
                                  SSL_FILETYPE_PEM)
      != 1) {
    Throw("Failed to load the TLS private key: " + OpenSslError());
  }
  if (SSL_CTX_check_private_key(ssl_ctx.get()) != 1) {
    Throw("The TLS certificate and private key do not match.");
  }
  return ssl_ctx;
}

std::unique_ptr<SslConnection> WrapAsTlsServer(SSL_CTX* ssl_ctx, int socket_fd)
{
  SSL* ssl = SSL_new(ssl_ctx);
  if (!ssl) {
    Throw("Failed to allocate a TLS connection object: " + OpenSslError());
  }
  SSL_set_fd(ssl, socket_fd);
  if (SSL_accept(ssl) != 1) {
    SSL_free(ssl);
    return nullptr;
  }
  return std::make_unique<SslConnection>(ssl, socket_fd);
}

SocketCloser ConnectBack(const Options& options)
{
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  addrinfo* raw_results = nullptr;
  const auto port_string = std::to_string(options.connect_back_port);
  if (getaddrinfo(options.connect_back_address.c_str(), port_string.c_str(),
                  &hints, &raw_results)
      != 0) {
    Throw("Failed to resolve the connect-back address.");
  }
  std::unique_ptr<addrinfo, AddrinfoDeleter> results(raw_results);

  const auto deadline
      = std::chrono::steady_clock::now()
        + std::chrono::seconds(options.connect_timeout);

  while (std::chrono::steady_clock::now() < deadline) {
    for (addrinfo* current = results.get(); current != nullptr;
         current = current->ai_next) {
      SocketCloser socket(::socket(current->ai_family, current->ai_socktype,
                                   current->ai_protocol));
      if (socket.get() < 0) { continue; }
      if (::connect(socket.get(), current->ai_addr, current->ai_addrlen) == 0) {
        return socket;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  Throw("Timed out connecting back to the File Daemon setup tool at "
        + options.connect_back_address + ":"
        + std::to_string(options.connect_back_port) + ".");
}

void WriteAll(SSL* ssl, std::string_view data)
{
  size_t written = 0;
  while (written < data.size()) {
    const auto chunk = SSL_write(ssl, data.data() + written,
                                 static_cast<int>(data.size() - written));
    if (chunk <= 0) { Throw("The TLS connection broke while sending data."); }
    written += static_cast<size_t>(chunk);
  }
}

std::string ReadLine(SSL* ssl)
{
  std::string line;
  for (;;) {
    char ch = '\0';
    const auto chunk = SSL_read(ssl, &ch, 1);
    if (chunk <= 0) { Throw("Client closed the TLS connection."); }
    if (ch == '\n') {
      if (!line.empty() && line.back() == '\r') { line.pop_back(); }
      return line;
    }
    line.push_back(ch);
  }
}

std::vector<std::pair<std::string, std::string>> ReadHeaders(SSL* ssl)
{
  std::vector<std::pair<std::string, std::string>> headers;
  for (;;) {
    auto line = ReadLine(ssl);
    if (line.empty()) { return headers; }
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
      Throw("Malformed request header: " + line);
    }
    auto key = line.substr(0, separator);
    auto value = line.substr(separator + 1);
    value.erase(0, value.find_first_not_of(' '));
    headers.emplace_back(std::move(key), std::move(value));
  }
}

std::optional<std::string> FirstHeaderValue(
    const std::vector<std::pair<std::string, std::string>>& headers,
    std::string_view key)
{
  for (const auto& [header_key, header_value] : headers) {
    if (header_key == key) { return header_value; }
  }
  return std::nullopt;
}

std::vector<std::string> AllHeaderValues(
    const std::vector<std::pair<std::string, std::string>>& headers,
    std::string_view key)
{
  std::vector<std::string> values;
  for (const auto& [header_key, header_value] : headers) {
    if (header_key == key) { values.push_back(header_value); }
  }
  return values;
}

void SendError(SSL* ssl, const std::string& message)
{
  std::ostringstream response;
  response << "ERROR 1\n";
  response << "message_length: " << message.size() << "\n";
  response << "\n";
  WriteAll(ssl, response.str());
  WriteAll(ssl, message);
}

void HandleConnection(SSL* ssl,
                      const std::string& peer_address,
                      const Options& options)
{
  try {
    const auto hello = ReadLine(ssl);
    if (hello != kProtocolHello) { Throw("Unsupported enrollment protocol."); }

    const auto headers = ReadHeaders(ssl);
    const auto token = FirstHeaderValue(headers, "token");
    if (!token) { Throw("Missing token header."); }
    if (!ConstantTimeEquals(*token, options.token)) {
      Throw("Enrollment token rejected.");
    }

    const auto client_name = FirstHeaderValue(headers, "client_name");
    if (!client_name) { Throw("Missing client_name header."); }
    ValidateName("client_name", *client_name);

    const auto fd_port_header = FirstHeaderValue(headers, "fd_port");
    if (!fd_port_header) { Throw("Missing fd_port header."); }
    const auto fd_port = static_cast<uint16_t>(std::stoul(*fd_port_header));
    if (fd_port == 0) { Throw("fd_port must be between 1 and 65535."); }

    const auto advertise_addresses = AllHeaderValues(headers, "advertise_address");
    const auto client_address
        = advertise_addresses.empty() ? peer_address : advertise_addresses.front();
    if (client_address.empty()) {
      Throw("Could not determine a client address to stage.");
    }

    const auto password
        = options.shared_password ? *options.shared_password : GenerateSharedPassword();
    const auto stage_path
        = options.staging_dir / "bareos-dir.d" / "client"
          / (*client_name + ".conf");

    const auto stage_content
        = BuildDirectorClientConfig(*client_name, client_address, fd_port, password);
    WriteTextFileAtomic(stage_path, stage_content, options.force);

    const auto fd_config = BuildFdDirectorConfig(options.director_name, password);

    std::ostringstream response;
    response << "OK 1\n";
    response << "director_name: " << options.director_name << "\n";
    response << "stage_path: " << stage_path.string() << "\n";
    response << "config_length: " << fd_config.size() << "\n";
    response << "\n";
    WriteAll(ssl, response.str());
    WriteAll(ssl, fd_config);
  } catch (const std::exception& e) {
    SendError(ssl, e.what());
  }
}

void SignalHandler(int)
{
  should_stop = 1;
}

void RunListenMode(const Options& options, SSL_CTX* ssl_ctx)
{
  auto listener = OpenListener(options.listen_address, options.port);
  const auto port = GetBoundPort(listener.get());
  std::cout << "LISTENING " << port << "\n" << std::flush;

  while (!should_stop) {
    pollfd candidate{};
    candidate.fd = listener.get();
    candidate.events = POLLIN;
    const int poll_result = poll(&candidate, 1, 500);
    if (poll_result == 0) { continue; }
    if (poll_result < 0) {
      if (errno == EINTR) { continue; }
      Throw("poll() failed while waiting for setup connections.");
    }

    sockaddr_storage peer{};
    socklen_t peer_length = sizeof(peer);
    SocketCloser accepted(
        accept(listener.get(), reinterpret_cast<sockaddr*>(&peer), &peer_length));
    if (accepted.get() < 0) { continue; }

    auto tls = WrapAsTlsServer(ssl_ctx, accepted.release());
    if (!tls) { continue; }
    HandleConnection(tls->get(), SocketAddressToString(peer), options);
  }
}

void RunConnectBackMode(const Options& options, SSL_CTX* ssl_ctx)
{
  if (options.connect_back_address.empty() || options.connect_back_port == 0) {
    Throw("director-connects mode requires --connect-back-address and"
          " --connect-back-port.");
  }

  auto socket = ConnectBack(options);

  sockaddr_storage peer{};
  socklen_t peer_length = sizeof(peer);
  if (getpeername(socket.get(), reinterpret_cast<sockaddr*>(&peer), &peer_length)
      != 0) {
    Throw("Failed to inspect the connect-back peer address.");
  }

  // The setup service intentionally remains the TLS server even when it opened
  // the TCP connection. Trust stays anchored in this service's certificate, not
  // in the transport direction.
  auto tls = WrapAsTlsServer(ssl_ctx, socket.release());
  if (!tls) { Throw("TLS handshake failed in connect-back mode."); }
  HandleConnection(tls->get(), SocketAddressToString(peer), options);
}

}  // namespace

int main(int argc, char* argv[])
{
  try {
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGINT, SignalHandler);

    SSL_library_init();
    SSL_load_error_strings();

    Options options;
    std::string connection_direction = "client-connects";

    CLI::App app("Serve the Bareos setup-service enrollment endpoint.");
    app.add_option("--connection-direction", connection_direction)
        ->check(CLI::IsMember({"client-connects", "director-connects"}));
    app.add_option("--listen-address", options.listen_address);
    app.add_option("--port", options.port)->check(CLI::Range(0, 65535));
    app.add_option("--connect-back-address", options.connect_back_address);
    app.add_option("--connect-back-port", options.connect_back_port)
        ->check(CLI::Range(1, 65535));
    app.add_option("--connect-timeout", options.connect_timeout)
        ->check(CLI::Range(1, 600));
    app.add_option("--certificate", options.certificate)->required();
    app.add_option("--key", options.key)->required();
    app.add_option("--director-name", options.director_name)->required();
    app.add_option("--director-address", options.director_address)->required();
    app.add_option("--director-port", options.director_port)
        ->check(CLI::Range(1, 65535));
    app.add_option("--staging-dir", options.staging_dir)->capture_default_str();
    app.add_option("--token", options.token)->required();
    app.add_option("--shared-password", options.shared_password);
    app.add_flag("--force", options.force);
    CLI11_PARSE(app, argc, argv);

    options.connection_direction = ParseConnectionDirection(connection_direction);
    ValidateName("director_name", options.director_name);

    auto ssl_ctx = CreateTlsServerContext(options);

    if (options.connection_direction == ConnectionDirection::kDirectorConnects) {
      RunConnectBackMode(options, ssl_ctx.get());
    } else {
      RunListenMode(options, ssl_ctx.get());
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << "\n";
    return 1;
  }
}
