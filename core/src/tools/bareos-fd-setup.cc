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

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "include/bareos.h"
#include "filed/filed_conf.h"
#include "filed/filed_globals.h"
#include "lib/address_conf.h"
#include "lib/cli.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"

namespace {

namespace fs = std::filesystem;
using filedaemon::ClientResource;

constexpr uint16_t kDefaultSetupPort = 10443;
constexpr std::string_view kProtocolHello = "BAREOS-SETUP 1";

enum class ConnectionDirection
{
  kClientConnects,
  kDirectorConnects,
};

struct Options {
  std::string config_path;
  std::string address = "127.0.0.1";
  uint16_t port = kDefaultSetupPort;
  std::string token;
  std::vector<std::string> advertise_addresses;
  std::string trust_file;
  std::string expected_fingerprint;
  std::string listen_address;
  uint16_t listen_port = 0;
  uint16_t connect_timeout = 30;
  ConnectionDirection connection_direction = ConnectionDirection::kClientConnects;
  bool trust_on_first_use = false;
  bool force = false;
};

struct LocalClientConfig {
  std::string name;
  uint16_t port = 0;
  fs::path config_root;
  ConfigurationParser* parser = nullptr;
};

struct SetupResponse {
  std::string director_name;
  std::string stage_path;
  std::string config_text;
};

struct FingerprintPolicy {
  fs::path trust_path;
  std::optional<std::string> required_fingerprint;
  bool prompt_on_first_use = false;
  bool trust_on_first_use = false;
  bool store_on_success = false;
};

struct AddrinfoDeleter {
  void operator()(addrinfo* addr) const { freeaddrinfo(addr); }
};

struct SslCtxDeleter {
  void operator()(SSL_CTX* ctx) const { SSL_CTX_free(ctx); }
};

struct X509Deleter {
  void operator()(X509* cert) const { X509_free(cert); }
};

struct SocketCloser {
  explicit SocketCloser(int fd) : fd_(fd) {}
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

std::string OpenSslError()
{
  std::array<char, 256> buffer{};
  unsigned long error = ERR_get_error();
  if (error == 0) { return "unknown OpenSSL error"; }
  ERR_error_string_n(error, buffer.data(), buffer.size());
  return buffer.data();
}

[[noreturn]] void Throw(const std::string& message)
{
  throw std::runtime_error(message);
}

bool ContainsPathSeparator(std::string_view name)
{
  return name.find('/') != std::string_view::npos
         || name.find('\\') != std::string_view::npos;
}

void ValidateNameForPath(std::string_view what, std::string_view name)
{
  std::string error;
  if (!IsNameValid(std::string{name}.c_str(), error)) {
    Throw(std::string{what} + ": " + error);
  }
  if (ContainsPathSeparator(name)) {
    Throw(std::string{what}
          + ": path separators are not allowed in setup-generated names.");
  }
}

std::string NormalizeFingerprint(std::string value)
{
  std::string normalized;
  normalized.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isspace(ch)) { continue; }
    normalized.push_back(std::toupper(ch));
  }
  if (normalized.rfind("SHA256=", 0) == 0) {
    normalized.erase(0, std::string{"SHA256="}.size());
  } else if (normalized.rfind("SHA256:", 0) == 0) {
    normalized.erase(0, std::string{"SHA256:"}.size());
  }
  return normalized;
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

std::string FormatFingerprint(X509* cert)
{
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_size = 0;
  if (X509_digest(cert, EVP_sha256(), digest.data(), &digest_size) != 1) {
    Throw("Failed to calculate the setup service certificate fingerprint.");
  }

  std::ostringstream fingerprint;
  fingerprint << std::uppercase << std::hex;
  for (unsigned int i = 0; i < digest_size; ++i) {
    if (i != 0) { fingerprint << ":"; }
    fingerprint.width(2);
    fingerprint.fill('0');
    fingerprint << static_cast<int>(digest[i]);
  }
  return fingerprint.str();
}

std::unique_ptr<SSL_CTX, SslCtxDeleter> CreateTlsClientContext()
{
  std::unique_ptr<SSL_CTX, SslCtxDeleter> ssl_ctx(SSL_CTX_new(TLS_client_method()));
  if (!ssl_ctx) {
    Throw("Failed to initialize the TLS client context: " + OpenSslError());
  }
  SSL_CTX_set_verify(ssl_ctx.get(), SSL_VERIFY_NONE, nullptr);
  return ssl_ctx;
}

fs::path DetermineConfigRoot(const std::string& base_path)
{
  fs::path path{base_path};
  std::error_code ec;
  if (fs::is_directory(path, ec)) { return path; }
  auto parent = path.parent_path();
  if (!parent.empty()) { return parent; }
  return fs::current_path();
}

std::string SanitizeFilenameComponent(std::string_view input)
{
  std::string sanitized;
  sanitized.reserve(input.size());
  for (unsigned char ch : input) {
    if (std::isalnum(ch) || ch == '.' || ch == '-' || ch == '_') {
      sanitized.push_back(static_cast<char>(ch));
    } else {
      sanitized.push_back('_');
    }
  }
  if (sanitized.empty()) { sanitized = "setup-service"; }
  return sanitized;
}

fs::path DefaultTrustFilePath(const fs::path& config_root,
                              std::string_view address,
                              uint16_t port)
{
  std::ostringstream filename;
  filename << SanitizeFilenameComponent(address) << "_" << port << ".sha256";
  return config_root / "bareos-fd.d" / "setup-trust" / filename.str();
}

std::string ReadFileTrimmed(const fs::path& path)
{
  std::ifstream input(path);
  if (!input) {
    Throw("Failed to read trust file \"" + path.string() + "\".");
  }
  std::ostringstream content;
  content << input.rdbuf();
  auto normalized = NormalizeFingerprint(content.str());
  if (normalized.empty()) {
    Throw("Trust file \"" + path.string() + "\" is empty.");
  }
  return normalized;
}

void WriteTextFileAtomic(const fs::path& path,
                         const std::string& content,
                         bool force)
{
  std::error_code ec;
  auto directory = path.parent_path();
  if (!directory.empty()) { fs::create_directories(directory, ec); }
  if (ec) {
    Throw("Failed to create directory \"" + directory.string() + "\": "
          + ec.message());
  }

  if (!force && fs::exists(path, ec)) {
    Throw("Refusing to overwrite existing file \"" + path.string()
          + "\". Use --force to replace it.");
  }
  if (ec) {
    Throw("Failed to inspect \"" + path.string() + "\": " + ec.message());
  }

  fs::path temp_path = path;
  temp_path += ".tmp";
  if (force) { fs::remove(temp_path, ec); }

  std::ofstream output(temp_path, std::ios::binary);
  if (!output) {
    Throw("Failed to open temporary file \"" + temp_path.string() + "\".");
  }
  output << content;
  output.close();
  if (!output) {
    Throw("Failed to write \"" + temp_path.string() + "\".");
  }

  fs::rename(temp_path, path, ec);
  if (ec) {
    fs::remove(temp_path);
    Throw("Failed to replace \"" + path.string() + "\": " + ec.message());
  }
}

LocalClientConfig LoadLocalClientConfig(const Options& options)
{
  auto* parser = filedaemon::InitFdConfig(options.config_path.c_str(), M_ERROR);
  if (parser == nullptr) { Throw("Failed to initialize the FD config parser."); }

  filedaemon::my_config = parser;
  if (!parser->ParseConfig()) {
    std::string config_file = parser->get_base_config_path();
    delete parser;
    filedaemon::my_config = nullptr;
    Throw("Failed to parse the FD config from \"" + config_file + "\".");
  }

  ClientResource* client = nullptr;
  {
    ResLocker lock{parser};
    client = static_cast<ClientResource*>(parser->GetNextRes(filedaemon::R_CLIENT,
                                                             nullptr));
    if (client == nullptr) {
      delete parser;
      filedaemon::my_config = nullptr;
      Throw("No Client resource found in the FD config.");
    }
    if (parser->GetNextRes(filedaemon::R_CLIENT,
                           reinterpret_cast<BareosResource*>(client))
        != nullptr) {
      delete parser;
      filedaemon::my_config = nullptr;
      Throw("Only one Client resource is supported by bareos-fd-setup.");
    }
  }

  ValidateNameForPath("client_name", client->resource_name_);

  const auto port = GetFirstPortHostOrder(client->FDaddrs);
  if (port <= 0 || port > 65535) {
    delete parser;
    filedaemon::my_config = nullptr;
    Throw("The local Client resource does not define a valid FD port.");
  }

  LocalClientConfig result;
  result.name = client->resource_name_;
  result.port = static_cast<uint16_t>(port);
  result.config_root = DetermineConfigRoot(parser->get_base_config_path());
  result.parser = parser;
  return result;
}

FingerprintPolicy PrepareFingerprintPolicy(const Options& options,
                                           const LocalClientConfig& client_config)
{
  FingerprintPolicy policy;
  policy.trust_path = options.trust_file.empty()
                          ? DefaultTrustFilePath(client_config.config_root,
                                                 options.address, options.port)
                          : fs::path{options.trust_file};

  std::error_code ec;
  const bool trust_file_exists = fs::exists(policy.trust_path, ec);
  if (ec) {
    Throw("Failed to inspect trust file \"" + policy.trust_path.string() + "\": "
          + ec.message());
  }

  if (trust_file_exists) {
    policy.required_fingerprint = ReadFileTrimmed(policy.trust_path);
    if (!options.expected_fingerprint.empty()
        && NormalizeFingerprint(options.expected_fingerprint)
               != *policy.required_fingerprint) {
      Throw("The stored setup-service fingerprint does not match the value from "
            "--expected-fingerprint.");
    }
    return policy;
  }

  if (!options.expected_fingerprint.empty()) {
    policy.required_fingerprint
        = NormalizeFingerprint(options.expected_fingerprint);
    policy.store_on_success = true;
    return policy;
  }

  if (options.connection_direction == ConnectionDirection::kDirectorConnects) {
    Throw("director-connects mode requires --expected-fingerprint or a pre-existing"
          " trust file.");
  }

  if (options.trust_on_first_use) {
    policy.trust_on_first_use = true;
    policy.store_on_success = true;
    return policy;
  }

  if (isatty(STDIN_FILENO) != 0) {
    policy.prompt_on_first_use = true;
    policy.store_on_success = true;
    return policy;
  }

  Throw("The setup service is unknown and this session is non-interactive."
        " Re-run with --expected-fingerprint or --trust-on-first-use.");
}

void StoreTrustedFingerprintIfNeeded(const FingerprintPolicy& policy,
                                     const std::string& fingerprint)
{
  if (policy.store_on_success) {
    WriteTextFileAtomic(policy.trust_path, fingerprint + "\n", true);
  }
}

void VerifyOutgoingFingerprint(const FingerprintPolicy& policy,
                               const std::string& fingerprint,
                               const Options& options)
{
  const auto normalized = NormalizeFingerprint(fingerprint);
  if (policy.required_fingerprint) {
    if (*policy.required_fingerprint != normalized) {
      Throw("TLS fingerprint mismatch for the setup service. Expected "
            + *policy.required_fingerprint + " from \""
            + policy.trust_path.string() + "\" but received " + normalized
            + ".");
    }
    StoreTrustedFingerprintIfNeeded(policy, normalized);
    return;
  }

  if (policy.trust_on_first_use) {
    StoreTrustedFingerprintIfNeeded(policy, normalized);
    return;
  }

  std::cout << "First contact with the setup service at " << options.address
            << ":" << options.port << "\n"
            << "SHA256 fingerprint: " << normalized << "\n"
            << "Trust this setup service and continue? [y/N]: " << std::flush;
  std::string answer;
  bool accepted = false;
  if (std::getline(std::cin, answer)) {
    accepted = answer == "y" || answer == "Y" || answer == "yes"
               || answer == "YES";
  }
  if (!accepted) { Throw("Setup canceled because the service was not trusted."); }

  StoreTrustedFingerprintIfNeeded(policy, normalized);
}

std::unique_ptr<SslConnection> ConnectTls(const Options& options)
{
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  addrinfo* raw_results = nullptr;
  const auto port = std::to_string(options.port);
  if (getaddrinfo(options.address.c_str(), port.c_str(), &hints, &raw_results)
      != 0) {
    Throw("Failed to resolve the setup service address \"" + options.address
          + "\".");
  }
  std::unique_ptr<addrinfo, AddrinfoDeleter> results(raw_results);

  auto ssl_ctx = CreateTlsClientContext();

  for (addrinfo* current = results.get(); current != nullptr;
       current = current->ai_next) {
    SocketCloser socket(::socket(current->ai_family, current->ai_socktype,
                                 current->ai_protocol));
    if (socket.get() < 0) { continue; }
    if (::connect(socket.get(), current->ai_addr, current->ai_addrlen) != 0) {
      continue;
    }

    SSL* ssl = SSL_new(ssl_ctx.get());
    if (!ssl) {
      Throw("Failed to allocate a TLS connection object: " + OpenSslError());
    }
    SSL_set_tlsext_host_name(ssl, options.address.c_str());
    SSL_set_fd(ssl, socket.get());

    if (SSL_connect(ssl) != 1) {
      const std::string error = OpenSslError();
      SSL_free(ssl);
      Throw("TLS negotiation with the setup service failed: " + error);
    }

    ssl_ctx.release();
    return std::make_unique<SslConnection>(ssl, socket.release());
  }

  Throw("Failed to connect to the setup service at " + options.address + ":"
        + std::to_string(options.port) + ".");
}

SocketCloser OpenListener(const std::string& address, uint16_t port)
{
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_PASSIVE;

  addrinfo* raw_results = nullptr;
  const auto port_string = std::to_string(port);
  const char* bind_address = address.empty() ? nullptr : address.c_str();
  if (getaddrinfo(bind_address, port_string.c_str(), &hints, &raw_results)
      != 0) {
    Throw("Failed to resolve the local listener address for director-connects"
          " mode.");
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

  Throw("Failed to start the temporary listener for director-connects mode at "
        + address + ":" + std::to_string(port) + ".");
}

std::unique_ptr<SslConnection> WrapAcceptedSocketAsTlsClient(
    int socket_fd,
    const Options& options)
{
  auto ssl_ctx = CreateTlsClientContext();

  SSL* ssl = SSL_new(ssl_ctx.get());
  if (!ssl) {
    Throw("Failed to allocate a TLS connection object: " + OpenSslError());
  }
  SSL_set_tlsext_host_name(ssl, options.address.c_str());
  SSL_set_fd(ssl, socket_fd);

  // The TLS client role intentionally stays on the FD setup tool even when the
  // director side initiated the TCP connection. That keeps trust anchored in the
  // setup-service certificate fingerprint instead of in the transport direction.
  if (SSL_connect(ssl) != 1) {
    SSL_free(ssl);
    return nullptr;
  }

  ssl_ctx.release();
  return std::make_unique<SslConnection>(ssl, socket_fd);
}

void WriteAll(SSL* ssl, std::string_view data)
{
  size_t written = 0;
  while (written < data.size()) {
    const auto chunk = SSL_write(ssl, data.data() + written,
                                 static_cast<int>(data.size() - written));
    if (chunk <= 0) { Throw("The TLS connection to the setup service broke."); }
    written += static_cast<size_t>(chunk);
  }
}

std::string ReadExact(SSL* ssl, size_t size)
{
  std::string result(size, '\0');
  size_t read = 0;
  while (read < size) {
    const auto chunk
        = SSL_read(ssl, result.data() + read, static_cast<int>(size - read));
    if (chunk <= 0) { Throw("The setup service closed the TLS connection."); }
    read += static_cast<size_t>(chunk);
  }
  return result;
}

std::string ReadLine(SSL* ssl)
{
  std::string line;
  for (;;) {
    char ch = '\0';
    const auto chunk = SSL_read(ssl, &ch, 1);
    if (chunk <= 0) { Throw("The setup service closed the TLS connection."); }
    if (ch == '\n') {
      if (!line.empty() && line.back() == '\r') { line.pop_back(); }
      return line;
    }
    line.push_back(ch);
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

std::vector<std::pair<std::string, std::string>> ReadHeaders(SSL* ssl)
{
  std::vector<std::pair<std::string, std::string>> headers;
  for (;;) {
    auto line = ReadLine(ssl);
    if (line.empty()) { return headers; }
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
      Throw("Received malformed setup-service response header: " + line);
    }
    auto key = line.substr(0, separator);
    auto value = line.substr(separator + 1);
    if (!value.empty() && value.front() == ' ') { value.erase(0, 1); }
    headers.emplace_back(std::move(key), std::move(value));
  }
}

std::unique_ptr<SslConnection> WaitForDirectorConnection(
    const Options& options,
    const FingerprintPolicy& policy)
{
  SocketCloser listener = OpenListener(options.listen_address, options.listen_port);
  const auto deadline
      = std::chrono::steady_clock::now()
        + std::chrono::seconds(options.connect_timeout);

  while (std::chrono::steady_clock::now() < deadline) {
    const auto remaining
        = std::chrono::duration_cast<std::chrono::milliseconds>(
              deadline - std::chrono::steady_clock::now())
              .count();
    pollfd candidate{};
    candidate.fd = listener.get();
    candidate.events = POLLIN;
    const int poll_result
        = poll(&candidate, 1, remaining > 1000 ? 1000 : static_cast<int>(remaining));
    if (poll_result == 0) { continue; }
    if (poll_result < 0) {
      if (errno == EINTR) { continue; }
      Throw("Failed while waiting for the director-side setup connection.");
    }

    sockaddr_storage peer_address{};
    socklen_t peer_length = sizeof(peer_address);
    SocketCloser accepted(
        accept(listener.get(), reinterpret_cast<sockaddr*>(&peer_address),
               &peer_length));
    if (accepted.get() < 0) { continue; }

    auto connection = WrapAcceptedSocketAsTlsClient(accepted.release(), options);
    if (!connection) { continue; }

    std::unique_ptr<X509, X509Deleter> peer_cert(
        SSL_get1_peer_certificate(connection->get()));
    if (!peer_cert) { continue; }
    const auto fingerprint = NormalizeFingerprint(FormatFingerprint(peer_cert.get()));
    if (*policy.required_fingerprint != fingerprint) { continue; }

    StoreTrustedFingerprintIfNeeded(policy, fingerprint);
    return connection;
  }

  Throw("Timed out waiting for the director-side setup service to connect to "
        + options.listen_address + ":" + std::to_string(options.listen_port)
        + ".");
}

void SendRequest(SSL* ssl,
                 const Options& options,
                 const LocalClientConfig& client_config)
{
  std::ostringstream request;
  request << kProtocolHello << "\n";
  request << "token: " << options.token << "\n";
  request << "client_name: " << client_config.name << "\n";
  request << "fd_port: " << client_config.port << "\n";
  for (const auto& address : options.advertise_addresses) {
    request << "advertise_address: " << address << "\n";
  }
  request << "\n";
  WriteAll(ssl, request.str());
}

SetupResponse ReadResponse(SSL* ssl)
{
  const auto status_line = ReadLine(ssl);
  const auto headers = ReadHeaders(ssl);

  if (status_line == "ERROR 1") {
    const auto message_length = FirstHeaderValue(headers, "message_length");
    if (!message_length) {
      Throw("The setup service returned an invalid error response.");
    }
    const auto payload
        = ReadExact(ssl, static_cast<size_t>(std::stoul(*message_length)));
    Throw(payload);
  }

  if (status_line != "OK 1") {
    Throw("The setup service returned an unknown response: " + status_line);
  }

  SetupResponse response;
  const auto director_name = FirstHeaderValue(headers, "director_name");
  const auto stage_path = FirstHeaderValue(headers, "stage_path");
  const auto config_length = FirstHeaderValue(headers, "config_length");
  if (!director_name || !stage_path || !config_length) {
    Throw("The setup service response is missing required fields.");
  }

  response.director_name = *director_name;
  response.stage_path = *stage_path;
  response.config_text
      = ReadExact(ssl, static_cast<size_t>(std::stoul(*config_length)));
  return response;
}

fs::path WriteDirectorResource(const LocalClientConfig& client_config,
                               const SetupResponse& response,
                               bool force)
{
  ValidateNameForPath("director_name", response.director_name);

  PoolMem path(PM_FNAME);
  PoolMem temp(PM_FNAME);
  if (!client_config.parser->GetPathOfNewResource(path, temp, nullptr, "director",
                                                  response.director_name.c_str(),
                                                  !force, true)) {
    Throw(temp.c_str());
  }

  WriteTextFileAtomic(path.c_str(), response.config_text, force);
  return fs::path{path.c_str()};
}

}  // namespace

int main(int argc, char* argv[])
{
  CLI::App app;
  try {
    SSL_library_init();
    SSL_load_error_strings();

    Options options;

    InitCLIApp(app,
               "Bootstrap a File Daemon against a separate Bareos setup service.",
               2000);

    app.add_option("-c,--config", options.config_path,
                   "Use <path> as FD configuration file or directory.")
        ->check(CLI::ExistingPath)
        ->required()
        ->type_name("<path>");
    app.add_option("--address", options.address,
                   "Network address of the director-side setup service.")
        ->required();
    app.add_option("--port", options.port,
                   "Network port of the director-side setup service.")
        ->check(CLI::Range(1, 65535));
    app.add_option("--token", options.token,
                   "One-time enrollment token required by the setup service.")
        ->required();
    app.add_option("--advertise-address", options.advertise_addresses,
                   "Address that should be staged in the Director-side Client"
                   " resource. Can be specified more than once.");
    app.add_option("--trust-file", options.trust_file,
                   "Path to the stored setup-service fingerprint.");
    app.add_option("--expected-fingerprint", options.expected_fingerprint,
                   "Expected SHA256 fingerprint of the setup-service"
                   " certificate.");
    std::string connection_direction = "client-connects";
    app.add_option("--connection-direction", connection_direction,
                  "Who opens the TCP connection for the setup session: "
                  "client-connects or director-connects.")
        ->check(CLI::IsMember({"client-connects", "director-connects"}));
    app.add_option("--listen-address", options.listen_address,
                  "Local address to listen on in director-connects mode.");
    app.add_option("--listen-port", options.listen_port,
                  "Local port to listen on in director-connects mode.")
        ->check(CLI::Range(1, 65535));
    app.add_option("--connect-timeout", options.connect_timeout,
                  "Timeout in seconds for setup session establishment.")
        ->check(CLI::Range(1, 600));
    app.add_flag("--trust-on-first-use", options.trust_on_first_use,
                 "Accept and store the setup-service fingerprint on first"
                 " contact without prompting.");
    app.add_flag("--force", options.force,
                 "Overwrite generated setup files if they already exist.");

    ParseBareosApp(app, argc, argv);
    options.connection_direction = ParseConnectionDirection(connection_direction);

    if (options.connection_direction == ConnectionDirection::kDirectorConnects) {
      if (options.listen_address.empty()) {
        Throw("director-connects mode requires --listen-address.");
      }
      if (options.listen_port == 0) {
        Throw("director-connects mode requires --listen-port.");
      }
      if (options.trust_on_first_use) {
        Throw("director-connects mode does not allow --trust-on-first-use."
              " Use --expected-fingerprint or a pre-existing trust file.");
      }
    }

    const auto client_config = LoadLocalClientConfig(options);
    std::unique_ptr<ConfigurationParser> parser_guard(client_config.parser);
    const auto fingerprint_policy = PrepareFingerprintPolicy(options, client_config);

    std::unique_ptr<SslConnection> connection;
    if (options.connection_direction == ConnectionDirection::kClientConnects) {
      connection = ConnectTls(options);
      std::unique_ptr<X509, X509Deleter> peer_cert(
          SSL_get1_peer_certificate(connection->get()));
      if (!peer_cert) {
        Throw("The setup service did not present a TLS certificate.");
      }
      VerifyOutgoingFingerprint(fingerprint_policy,
                                FormatFingerprint(peer_cert.get()), options);
    } else {
      connection = WaitForDirectorConnection(options, fingerprint_policy);
    }

    SendRequest(connection->get(), options, client_config);
    const auto response = ReadResponse(connection->get());
    const auto resource_path
        = WriteDirectorResource(client_config, response, options.force);

    std::cout << "Created setup resource \"" << resource_path.string()
              << "\".\n";
    std::cout << "Director-side staged config: " << response.stage_path << "\n";
    std::cout << "File Daemon restart required.\n";

    filedaemon::my_config = nullptr;
    filedaemon::me = nullptr;
    return 0;
  } catch (const CLI::ParseError& e) {
    return app.exit(e);
  } catch (const std::exception& e) {
    std::cerr << "Failed to enroll File Daemon: " << e.what() << "\n";
    filedaemon::my_config = nullptr;
    filedaemon::me = nullptr;
    return 1;
  }
}
