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
#include "include/fcntl_def.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "lib/address_conf.h"
#include "lib/parse_conf.h"

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <netdb.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <cstdint>
#include <ctime>
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

fs::path GetDefaultSetupStagingDir()
{
#if defined(HAVE_WIN32)
  return fs::path(ConfigurationParser::GetDefaultConfigDir()) / "setup";
#else
  return fs::path(PATH_BAREOS_WORKINGDIR) / "setup";
#endif
}

enum class ConnectionDirection
{
  kClientConnects,
  kDirectorConnects,
};

struct Options {
  ConnectionDirection connection_direction = ConnectionDirection::kClientConnects;
  std::string config_path = ConfigurationParser::GetDefaultConfigDir();
  std::string listen_address = "all";
  uint16_t port = 10443;
  std::string connect_back_address;
  uint16_t connect_back_port = 0;
  uint16_t connect_timeout = 30;
  std::string certificate;
  std::string key;
  std::string director_name;
  std::string director_address;
  uint16_t director_port = 9101;
  fs::path staging_dir{GetDefaultSetupStagingDir()};
  std::string token;
  std::optional<std::string> shared_password;
  bool force = false;
};

struct DerivedDefaults {
  std::string certificate;
  std::string key;
  std::string director_name;
  std::string director_address;
  uint16_t director_port = 0;
  fs::path setup_tls_dir;
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

struct EvpPkeyCtxDeleter {
  void operator()(EVP_PKEY_CTX* ctx) const { EVP_PKEY_CTX_free(ctx); }
};

struct EvpPkeyDeleter {
  void operator()(EVP_PKEY* pkey) const { EVP_PKEY_free(pkey); }
};

struct X509Deleter {
  void operator()(X509* cert) const { X509_free(cert); }
};

struct X509ExtensionDeleter {
  void operator()(X509_EXTENSION* extension) const { X509_EXTENSION_free(extension); }
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

struct GeneratedTlsMaterial {
  fs::path certificate_path;
  fs::path key_path;
};

struct ListenerSocket {
  SocketCloser socket;
  sockaddr_storage bound_address{};
};

struct ListenerSet {
  std::vector<ListenerSocket> sockets;
  std::vector<std::string> warnings;
};

[[noreturn]] void Throw(const std::string& message)
{
  throw std::runtime_error(message);
}

std::string Iso8601TimestampNow()
{
  const auto now = std::chrono::system_clock::now();
  const auto timestamp = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(HAVE_WIN32)
  localtime_s(&tm, &timestamp);
#else
  localtime_r(&timestamp, &tm);
#endif

  std::array<char, 32> buffer{};
  if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%S%z", &tm)
      == 0) {
    return "1970-01-01T00:00:00+00:00";
  }

  std::string formatted = buffer.data();
  if (formatted.size() >= 5) {
    const auto offset_pos = formatted.size() - 5;
    if ((formatted[offset_pos] == '+' || formatted[offset_pos] == '-')
        && std::isdigit(static_cast<unsigned char>(formatted[offset_pos + 1]))
        && std::isdigit(static_cast<unsigned char>(formatted[offset_pos + 2]))
        && std::isdigit(static_cast<unsigned char>(formatted[offset_pos + 3]))
        && std::isdigit(static_cast<unsigned char>(formatted[offset_pos + 4]))) {
      formatted.insert(offset_pos + 3, ":");
    }
  }
  return formatted;
}

void LogInfo(const std::string& message)
{
  std::cout << Iso8601TimestampNow() << " INFO " << message << "\n" << std::flush;
}

void LogError(const std::string& message)
{
  std::cerr << Iso8601TimestampNow() << " ERROR " << message << "\n"
            << std::flush;
}

std::string OpenSslError()
{
  std::array<char, 256> buffer{};
  unsigned long error = ERR_get_error();
  if (error == 0) { return "unknown OpenSSL error"; }
  ERR_error_string_n(error, buffer.data(), buffer.size());
  return buffer.data();
}

const char* ConnectionDirectionName(ConnectionDirection direction)
{
  switch (direction) {
    case ConnectionDirection::kClientConnects:
      return "client-connects";
    case ConnectionDirection::kDirectorConnects:
      return "director-connects";
  }
  return "unknown";
}

bool ListenOnAllInterfaces(std::string_view address)
{
  return address.empty() || address == "all";
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

bool AddExtension(X509* cert, int nid, const std::string& value)
{
  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);

  std::unique_ptr<X509_EXTENSION, X509ExtensionDeleter> extension(
      X509V3_EXT_conf_nid(nullptr, &ctx, nid, const_cast<char*>(value.c_str())));
  if (!extension) { return false; }
  return X509_add_ext(cert, extension.get(), -1) == 1;
}

std::string BuildSubjectAlternativeName(std::string_view host)
{
  if (host.empty()) { return ""; }

  in_addr ipv4{};
  if (inet_pton(AF_INET, std::string{host}.c_str(), &ipv4) == 1) {
    return "IP:" + std::string{host};
  }

  in6_addr ipv6{};
  if (inet_pton(AF_INET6, std::string{host}.c_str(), &ipv6) == 1) {
    return "IP:" + std::string{host};
  }

  return "DNS:" + std::string{host};
}

std::unique_ptr<EVP_PKEY, EvpPkeyDeleter> GenerateRsaKey()
{
  std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter> ctx(
      EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
  if (!ctx) {
    Throw("Failed to create the setup-service key generation context: "
          + OpenSslError());
  }

  if (EVP_PKEY_keygen_init(ctx.get()) <= 0
      || EVP_PKEY_CTX_set_rsa_keygen_bits(ctx.get(), 2048) <= 0) {
    Throw("Failed to initialize setup-service RSA key generation: "
          + OpenSslError());
  }

  EVP_PKEY* raw_key = nullptr;
  if (EVP_PKEY_keygen(ctx.get(), &raw_key) <= 0) {
    Throw("Failed to generate a setup-service RSA key: " + OpenSslError());
  }

  return std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>(raw_key);
}

std::unique_ptr<X509, X509Deleter> GenerateSelfSignedCertificate(
    EVP_PKEY* key,
    std::string_view common_name,
    std::string_view subject_alt_name)
{
  std::unique_ptr<X509, X509Deleter> cert(X509_new());
  if (!cert) {
    Throw("Failed to create the setup-service certificate: " + OpenSslError());
  }

  if (X509_set_version(cert.get(), 2) != 1
      || ASN1_INTEGER_set(X509_get_serialNumber(cert.get()),
                          static_cast<long>(std::time(nullptr)))
             != 1
      || X509_gmtime_adj(X509_get_notBefore(cert.get()), 0) == nullptr
      || X509_gmtime_adj(X509_get_notAfter(cert.get()),
                         60L * 60 * 24 * 365 * 10)
             == nullptr
      || X509_set_pubkey(cert.get(), key) != 1) {
    Throw("Failed to initialize the setup-service certificate: "
          + OpenSslError());
  }

  X509_NAME* subject = X509_get_subject_name(cert.get());
  if (!subject
      || X509_NAME_add_entry_by_txt(
             subject, "CN", MBSTRING_ASC,
             reinterpret_cast<const unsigned char*>(common_name.data()), -1, -1,
             0)
             != 1
      || X509_set_issuer_name(cert.get(), subject) != 1) {
    Throw("Failed to set the setup-service certificate subject: "
          + OpenSslError());
  }

  if (!AddExtension(cert.get(), NID_basic_constraints, "critical,CA:FALSE")
      || !AddExtension(cert.get(), NID_key_usage,
                       "critical,digitalSignature,keyEncipherment")
      || !AddExtension(cert.get(), NID_ext_key_usage, "serverAuth")) {
    Throw("Failed to add required extensions to the setup-service"
          " certificate: "
          + OpenSslError());
  }

  if (!subject_alt_name.empty()
      && !AddExtension(cert.get(), NID_subject_alt_name,
                       std::string(subject_alt_name))) {
    Throw("Failed to add the subjectAltName to the setup-service"
          " certificate: "
          + OpenSslError());
  }

  if (X509_sign(cert.get(), key, EVP_sha256()) <= 0) {
    Throw("Failed to sign the setup-service certificate: " + OpenSslError());
  }

  return cert;
}

void SetFileMode(const fs::path& path, int mode)
{
  if (chmod(path.c_str(), mode) != 0) {
    Throw("Failed to set file permissions on \"" + path.string()
          + "\": " + std::strerror(errno));
  }
}

fs::path CreateTemporaryPath(const fs::path& directory, const char* prefix)
{
  std::string pattern = (directory / (std::string(prefix) + ".XXXXXX")).string();
  std::vector<char> buffer(pattern.begin(), pattern.end());
  buffer.push_back('\0');

  const int fd = mkstemp(buffer.data());
  if (fd < 0) {
    Throw("Failed to create a temporary file in \"" + directory.string()
          + "\": " + std::strerror(errno));
  }
  close(fd);
  return fs::path(buffer.data());
}

void WritePrivateKeyPem(const fs::path& path, EVP_PKEY* key)
{
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    Throw("Failed to open \"" + path.string()
          + "\" for writing the setup-service key.");
  }

  const bool ok
      = PEM_write_PrivateKey(file, key, nullptr, nullptr, 0, nullptr, nullptr)
        == 1;
  fclose(file);
  if (!ok) {
    Throw("Failed to write the setup-service key to \"" + path.string()
          + "\": " + OpenSslError());
  }
}

void WriteCertificatePem(const fs::path& path, X509* cert)
{
  FILE* file = fopen(path.c_str(), "wb");
  if (!file) {
    Throw("Failed to open \"" + path.string()
          + "\" for writing the setup-service certificate.");
  }

  const bool ok = PEM_write_X509(file, cert) == 1;
  fclose(file);
  if (!ok) {
    Throw("Failed to write the setup-service certificate to \"" + path.string()
          + "\": " + OpenSslError());
  }
}

GeneratedTlsMaterial EnsureGeneratedTlsMaterial(const DerivedDefaults& defaults)
{
  const auto tls_dir = defaults.setup_tls_dir;
  std::error_code ec;
  fs::create_directories(tls_dir, ec);
  if (ec) {
    Throw("Failed to create the setup-service TLS directory \""
          + tls_dir.string() + "\": " + ec.message());
  }
  SetFileMode(tls_dir, 0700);

  const auto certificate_path = tls_dir / "setup-service-cert.pem";
  const auto key_path = tls_dir / "setup-service-key.pem";
  const auto lock_path = tls_dir / "setup-service-tls.lock";

  const int lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  if (lock_fd < 0) {
    Throw("Failed to open the setup-service TLS lock file \""
          + lock_path.string() + "\": " + std::strerror(errno));
  }

  if (flock(lock_fd, LOCK_EX) != 0) {
    close(lock_fd);
    Throw("Failed to lock the setup-service TLS directory \""
          + tls_dir.string() + "\": " + std::strerror(errno));
  }

  auto unlock = [&]() {
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
  };

  try {
    if (fs::exists(certificate_path) && fs::exists(key_path)) {
      LogInfo("Using existing setup-service TLS certificate \"" + certificate_path.string()
              + "\" and key \"" + key_path.string() + "\".");
      unlock();
      return {.certificate_path = certificate_path, .key_path = key_path};
    }

    fs::remove(certificate_path, ec);
    fs::remove(key_path, ec);

    const auto key = GenerateRsaKey();
    const auto certificate = GenerateSelfSignedCertificate(
        key.get(), defaults.director_name.empty() ? "Bareos Setup Service"
                                                  : defaults.director_name,
        BuildSubjectAlternativeName(defaults.director_address));
    LogInfo("Autocreating setup-service TLS certificate \"" + certificate_path.string()
            + "\" and key \"" + key_path.string() + "\".");

    const auto temporary_key_path = CreateTemporaryPath(tls_dir, "setup-key");
    const auto temporary_cert_path = CreateTemporaryPath(tls_dir, "setup-cert");

    try {
      WritePrivateKeyPem(temporary_key_path, key.get());
      SetFileMode(temporary_key_path, 0600);
      WriteCertificatePem(temporary_cert_path, certificate.get());
      SetFileMode(temporary_cert_path, 0644);

      fs::rename(temporary_key_path, key_path, ec);
      if (ec) {
        Throw("Failed to install the generated setup-service key: "
              + ec.message());
      }
      fs::rename(temporary_cert_path, certificate_path, ec);
      if (ec) {
        Throw("Failed to install the generated setup-service certificate: "
              + ec.message());
      }
    } catch (...) {
      fs::remove(temporary_key_path, ec);
      fs::remove(temporary_cert_path, ec);
      throw;
    }

    LogInfo("Created setup-service TLS certificate \"" + certificate_path.string()
            + "\" and key \"" + key_path.string() + "\".");
    unlock();
    return {.certificate_path = certificate_path, .key_path = key_path};
  } catch (...) {
    unlock();
    throw;
  }
}

std::unique_ptr<ConfigurationParser> LoadDirectorConfig(const std::string& config_path)
{
  auto parser = std::unique_ptr<ConfigurationParser>(
      directordaemon::InitDirConfig(config_path.c_str(), M_CONFIG_ERROR));
  if (!parser) { Throw("Failed to initialize the Director config parser."); }

  directordaemon::my_config = parser.get();
  if (!parser->ParseConfig()) {
    Throw("Failed to parse the Director config from \"" + config_path + "\".");
  }

  directordaemon::me
      = static_cast<directordaemon::DirectorResource*>(
          parser->GetNextRes(directordaemon::R_DIRECTOR, nullptr));
  if (directordaemon::me == nullptr) {
    Throw("No Director resource found in \"" + config_path + "\".");
  }
  directordaemon::my_config->own_resource_ = directordaemon::me;
  return parser;
}

bool IsUnsuitableAdvertisedAddress(std::string_view address)
{
  return address.empty() || address == "0.0.0.0" || address == "::"
         || address == "::0" || address == "127.0.0.1" || address == "::1";
}

std::string DeriveLocalAdvertisedAddress()
{
  std::array<char, NI_MAXHOST> hostname{};
  if (gethostname(hostname.data(), hostname.size()) != 0) {
    Throw("Failed to derive the local host name for the setup service.");
  }
  hostname.back() = '\0';

  addrinfo hints{};
  hints.ai_flags = AI_CANONNAME;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* raw_results = nullptr;
  if (getaddrinfo(hostname.data(), nullptr, &hints, &raw_results) == 0) {
    std::unique_ptr<addrinfo, AddrinfoDeleter> results(raw_results);
    if (results && results->ai_canonname != nullptr
        && results->ai_canonname[0] != '\0') {
      return results->ai_canonname;
    }
  }

  return hostname.data();
}

std::string DeriveDirectorAddress(const directordaemon::DirectorResource& director)
{
  if (director.DIRaddrs != nullptr && director.DIRaddrs->size() > 0) {
    auto* address = static_cast<IPADDR*>(director.DIRaddrs->first());
    if (address != nullptr) {
      std::array<char, 1024> buffer{};
      const std::string candidate = address->GetAddress(buffer.data(), buffer.size());
      if (!IsUnsuitableAdvertisedAddress(candidate)) { return candidate; }
    }
  }

  return DeriveLocalAdvertisedAddress();
}

DerivedDefaults LoadDerivedDefaults(const Options& options)
{
  auto parser = LoadDirectorConfig(options.config_path);

  DerivedDefaults defaults;
  defaults.certificate = directordaemon::me->tls_cert_.certfile_;
  defaults.key = directordaemon::me->tls_cert_.keyfile_;
  defaults.director_name = directordaemon::me->resource_name_;
  defaults.director_address = DeriveDirectorAddress(*directordaemon::me);
  defaults.setup_tls_dir
      = directordaemon::me->working_directory != nullptr
            && directordaemon::me->working_directory[0] != '\0'
          ? fs::path(directordaemon::me->working_directory) / "setup" / "tls"
          : fs::path(PATH_BAREOS_WORKINGDIR) / "setup" / "tls";
  const auto director_port = GetFirstPortHostOrder(directordaemon::me->DIRaddrs);
  if (director_port > 0 && director_port <= 65535) {
    defaults.director_port = static_cast<uint16_t>(director_port);
  } else {
    defaults.director_port = options.director_port;
  }

  parser.reset();
  directordaemon::my_config = nullptr;
  directordaemon::me = nullptr;
  return defaults;
}

void ApplyDerivedDefaults(Options& options,
                          bool certificate_explicit,
                          bool key_explicit,
                          bool director_name_explicit,
                          bool director_address_explicit,
                          bool director_port_explicit)
{
  if (certificate_explicit && key_explicit && director_name_explicit
      && director_address_explicit && director_port_explicit) {
    return;
  }

  const auto defaults = LoadDerivedDefaults(options);

  if (!certificate_explicit) { options.certificate = defaults.certificate; }
  if (!key_explicit) { options.key = defaults.key; }
  if (!director_name_explicit) { options.director_name = defaults.director_name; }
  if (!director_address_explicit) {
    options.director_address = defaults.director_address;
  }
  if (!director_port_explicit) { options.director_port = defaults.director_port; }

  if (options.certificate.empty() != options.key.empty()) {
    Throw("The setup-service TLS certificate and key must either both be set"
          " or both be absent.");
  }
  if (options.certificate.empty() && options.key.empty()) {
    const auto generated = EnsureGeneratedTlsMaterial(defaults);
    options.certificate = generated.certificate_path.string();
    options.key = generated.key_path.string();
  }
  if (options.director_name.empty()) {
    Throw("Could not derive --director-name from the Director config. Use"
          " --director-name explicitly.");
  }
  if (options.director_address.empty()) {
    Throw("Could not derive --director-address from the Director config. Use"
          " --director-address explicitly.");
  }
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
  config << "  Description = \"Generated by bclient-enrolld.\"\n";
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
  config << "  Description = \"Generated by bclient-enrolld.\"\n";
  config << "}\n";
  return config.str();
}

sockaddr_storage GetSocketAddress(int socket_fd)
{
  sockaddr_storage address{};
  socklen_t length = sizeof(address);
  if (getsockname(socket_fd, reinterpret_cast<sockaddr*>(&address), &length)
      != 0) {
    Throw("Failed to determine the setup listener address.");
  }
  return address;
}

ListenerSocket OpenSingleListener(const char* node,
                                  uint16_t port,
                                  int family,
                                  bool ipv6_only,
                                  const std::string& description)
{
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = family;
  hints.ai_flags = AI_PASSIVE;

  addrinfo* raw_results = nullptr;
  const auto port_string = std::to_string(port);
  if (getaddrinfo(node, port_string.c_str(), &hints, &raw_results)
      != 0) {
    Throw("Failed to resolve the listener address \"" + description + "\".");
  }
  std::unique_ptr<addrinfo, AddrinfoDeleter> results(raw_results);

  for (addrinfo* current = results.get(); current != nullptr;
       current = current->ai_next) {
    SocketCloser socket(::socket(current->ai_family, current->ai_socktype,
                                 current->ai_protocol));
    if (socket.get() < 0) { continue; }

    int reuse = 1;
    setsockopt(socket.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef IPV6_V6ONLY
    if (current->ai_family == AF_INET6) {
      int v6only = ipv6_only ? 1 : 0;
      setsockopt(socket.get(), IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
    }
#endif

    if (bind(socket.get(), current->ai_addr, current->ai_addrlen) != 0) {
      continue;
    }
    if (listen(socket.get(), 16) != 0) { continue; }
    auto bound_address = GetSocketAddress(socket.get());
    return {.socket = std::move(socket), .bound_address = bound_address};
  }

  Throw("Failed to start the setup listener on " + description + ":"
        + std::to_string(port) + ".");
}

uint16_t GetSocketPort(const sockaddr_storage& address);

ListenerSet OpenListeners(const std::string& address, uint16_t port)
{
  if (!ListenOnAllInterfaces(address)) {
    ListenerSet listeners;
    listeners.sockets.push_back(
        OpenSingleListener(address.c_str(), port, AF_UNSPEC, false, address));
    return listeners;
  }

  ListenerSet listeners;
  std::optional<uint16_t> bound_port;

  try {
    auto ipv6_listener = OpenSingleListener(nullptr, port, AF_INET6, true, "[::]");
    bound_port = GetSocketPort(ipv6_listener.bound_address);
    listeners.sockets.push_back(std::move(ipv6_listener));
  } catch (const std::exception& e) {
    listeners.warnings.push_back(e.what());
  }

  try {
    auto ipv4_listener
        = OpenSingleListener(nullptr, bound_port.value_or(port), AF_INET, false,
                             "0.0.0.0");
    if (!bound_port) { bound_port = GetSocketPort(ipv4_listener.bound_address); }
    listeners.sockets.push_back(std::move(ipv4_listener));
  } catch (const std::exception& e) {
    listeners.warnings.push_back(e.what());
  }

  if (listeners.sockets.empty()) {
    std::ostringstream message;
    message << "Failed to start the setup listener on all interfaces.";
    for (const auto& warning : listeners.warnings) {
      message << " " << warning;
    }
    Throw(message.str());
  }

  return listeners;
}

uint16_t GetSocketPort(const sockaddr_storage& address)
{
  if (address.ss_family == AF_INET) {
    return ntohs(reinterpret_cast<const sockaddr_in*>(&address)->sin_port);
  }
  if (address.ss_family == AF_INET6) {
    return ntohs(reinterpret_cast<const sockaddr_in6*>(&address)->sin6_port);
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

std::string SocketEndpointToString(const sockaddr_storage& address)
{
  const auto host = SocketAddressToString(address);
  const auto port = GetSocketPort(address);
  if (host.empty()) { return ":" + std::to_string(port); }
  if (address.ss_family == AF_INET6) {
    return "[" + host + "]:" + std::to_string(port);
  }
  return host + ":" + std::to_string(port);
}

void LogStartupConfiguration(const Options& options)
{
  std::ostringstream message;
  message << "bclient-enrolld started in " << ConnectionDirectionName(options.connection_direction)
          << " mode."
          << " Config=" << options.config_path
          << ", listen-address=" << options.listen_address
          << ", staging-dir=" << options.staging_dir.string()
          << ", director=" << options.director_name << "@"
          << options.director_address << ":" << options.director_port
          << ", certificate=" << options.certificate
          << ", key=" << options.key;
  LogInfo(message.str());
}

std::unique_ptr<SSL_CTX, SslCtxDeleter> CreateTlsServerContext(
    const Options& options)
{
  LogInfo("Loading TLS certificate from \"" + options.certificate + "\".");
  LogInfo("Loading TLS private key from \"" + options.key + "\".");
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

std::unique_ptr<SslConnection> WrapAsTlsServer(SSL_CTX* ssl_ctx,
                                               int socket_fd,
                                               std::string_view peer_endpoint)
{
  LogInfo("Starting TLS handshake with " + std::string(peer_endpoint) + ".");
  SSL* ssl = SSL_new(ssl_ctx);
  if (!ssl) {
    Throw("Failed to allocate a TLS connection object: " + OpenSslError());
  }
  SSL_set_fd(ssl, socket_fd);
  if (SSL_accept(ssl) != 1) {
    const auto error = OpenSslError();
    SSL_free(ssl);
    LogError("TLS handshake failed with " + std::string(peer_endpoint) + ": "
             + error);
    return nullptr;
  }
  std::ostringstream message;
  message << "TLS handshake completed with " << peer_endpoint;
  if (const char* version = SSL_get_version(ssl); version != nullptr) {
    message << " using " << version;
  }
  if (const char* cipher = SSL_get_cipher_name(ssl); cipher != nullptr) {
    message << " / " << cipher;
  }
  message << ".";
  LogInfo(message.str());
  return std::make_unique<SslConnection>(ssl, socket_fd);
}

SocketCloser ConnectBack(const Options& options)
{
  LogInfo("Connecting back to File Daemon setup tool at "
          + options.connect_back_address + ":"
          + std::to_string(options.connect_back_port) + ".");
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
                      const std::string& peer_endpoint,
                      const Options& options)
{
  try {
    LogInfo("Receiving enrollment request from " + peer_endpoint + ".");
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

    const auto request_force = FirstHeaderValue(headers, "force");
    const bool force = options.force || (request_force && *request_force == "true");

    const auto advertise_addresses = AllHeaderValues(headers, "advertise_address");
    const auto client_address
        = advertise_addresses.empty() ? peer_address : advertise_addresses.front();
    if (client_address.empty()) {
      Throw("Could not determine a client address to stage.");
    }

    std::ostringstream request_log;
    request_log << "Accepted enrollment request from " << peer_endpoint
                << " for client \"" << *client_name << "\""
                << " with File Daemon port " << fd_port
                << " and staged address \"" << client_address << "\"";
    if (force) { request_log << " (overwrite enabled)"; }
    request_log << ".";
    LogInfo(request_log.str());
    if (!advertise_addresses.empty()) {
      std::ostringstream advertised;
      advertised << "Advertised client addresses:";
      for (const auto& address : advertise_addresses) {
        advertised << " " << address;
      }
      LogInfo(advertised.str());
    } else {
      LogInfo("No advertised client address received; using peer address \""
              + peer_address + "\".");
    }

    const auto password
        = options.shared_password ? *options.shared_password : GenerateSharedPassword();
    const auto stage_path
        = options.staging_dir / "bareos-dir.d" / "client"
          / (*client_name + ".conf");

    const auto stage_content
        = BuildDirectorClientConfig(*client_name, client_address, fd_port, password);
    LogInfo("Writing staged client config to " + stage_path.string() + ".");
    WriteTextFileAtomic(stage_path, stage_content, force);
    LogInfo("Wrote staged client config to " + stage_path.string() + ".");

    const auto fd_config = BuildFdDirectorConfig(options.director_name, password);

    std::ostringstream response;
    response << "OK 1\n";
    response << "director_name: " << options.director_name << "\n";
    response << "stage_path: " << stage_path.string() << "\n";
    response << "config_length: " << fd_config.size() << "\n";
    response << "\n";
    LogInfo("Sending generated File Daemon config for director \""
            + options.director_name + "\" to " + peer_endpoint + ".");
    WriteAll(ssl, response.str());
    WriteAll(ssl, fd_config);
    LogInfo("Enrollment request for client \"" + *client_name
            + "\" completed successfully.");
  } catch (const std::exception& e) {
    LogError("Enrollment request from " + peer_endpoint + " failed: " + e.what());
    SendError(ssl, e.what());
  }
}

void SignalHandler(int)
{
  should_stop = 1;
}

void RunListenMode(const Options& options, SSL_CTX* ssl_ctx)
{
  auto listeners = OpenListeners(options.listen_address, options.port);
  for (const auto& listener : listeners.sockets) {
    const auto bound_endpoint = SocketEndpointToString(listener.bound_address);
    std::cout << "LISTENING " << bound_endpoint << "\n" << std::flush;
    LogInfo("Listening on " + bound_endpoint + ".");
  }
  LogStartupConfiguration(options);
  for (const auto& warning : listeners.warnings) {
    LogError(warning);
  }
  LogInfo("Waiting for incoming setup connections.");

  while (!should_stop) {
    std::vector<pollfd> candidates(listeners.sockets.size());
    for (size_t i = 0; i < listeners.sockets.size(); ++i) {
      candidates[i].fd = listeners.sockets[i].socket.get();
      candidates[i].events = POLLIN;
      candidates[i].revents = 0;
    }
    const int poll_result = poll(candidates.data(), candidates.size(), 500);
    if (poll_result == 0) { continue; }
    if (poll_result < 0) {
      if (errno == EINTR) { continue; }
      Throw("poll() failed while waiting for setup connections.");
    }

    for (size_t i = 0; i < candidates.size(); ++i) {
      if ((candidates[i].revents & POLLIN) == 0) { continue; }

      sockaddr_storage peer{};
      socklen_t peer_length = sizeof(peer);
      SocketCloser accepted(accept(listeners.sockets[i].socket.get(),
                                   reinterpret_cast<sockaddr*>(&peer), &peer_length));
      if (accepted.get() < 0) { continue; }

      const auto peer_address = SocketAddressToString(peer);
      const auto peer_endpoint = SocketEndpointToString(peer);
      LogInfo("Accepted TCP connection from " + peer_endpoint + ".");
      auto tls = WrapAsTlsServer(ssl_ctx, accepted.release(), peer_endpoint);
      if (!tls) { continue; }
      HandleConnection(tls->get(), peer_address, peer_endpoint, options);
      LogInfo("Closed setup connection from " + peer_endpoint + ".");
    }
  }
}

void RunConnectBackMode(const Options& options, SSL_CTX* ssl_ctx)
{
  if (options.connect_back_address.empty() || options.connect_back_port == 0) {
    Throw("director-connects mode requires --connect-back-address and"
          " --connect-back-port.");
  }

  LogStartupConfiguration(options);
  auto socket = ConnectBack(options);

  sockaddr_storage peer{};
  socklen_t peer_length = sizeof(peer);
  if (getpeername(socket.get(), reinterpret_cast<sockaddr*>(&peer), &peer_length)
      != 0) {
    Throw("Failed to inspect the connect-back peer address.");
  }
  const auto peer_address = SocketAddressToString(peer);
  const auto peer_endpoint = SocketEndpointToString(peer);
  LogInfo("Established TCP connect-back connection to " + peer_endpoint + ".");

  // The setup service intentionally remains the TLS server even when it opened
  // the TCP connection. Trust stays anchored in this service's certificate, not
  // in the transport direction.
  auto tls = WrapAsTlsServer(ssl_ctx, socket.release(), peer_endpoint);
  if (!tls) { Throw("TLS handshake failed in connect-back mode."); }
  HandleConnection(tls->get(), peer_address, peer_endpoint, options);
  LogInfo("Closed setup connection to " + peer_endpoint + ".");
}

}  // namespace

int main(int argc, char* argv[])
{
  try {
    OSDependentInit();
    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGINT, SignalHandler);

    SSL_library_init();
    SSL_load_error_strings();

    Options options;
    std::string connection_direction = "client-connects";

    CLI::App app("Serve the Bareos setup-service enrollment endpoint.");
    auto* config_option
        = app.add_option("-c,--config", options.config_path)
              ->check(CLI::ExistingPath)
              ->capture_default_str();
    app.add_option("--connection-direction", connection_direction)
        ->check(CLI::IsMember({"client-connects", "director-connects"}));
    app.add_option("--listen-address", options.listen_address);
    app.add_option("--port", options.port)->check(CLI::Range(0, 65535));
    app.add_option("--connect-back-address", options.connect_back_address);
    app.add_option("--connect-back-port", options.connect_back_port)
        ->check(CLI::Range(1, 65535));
    app.add_option("--connect-timeout", options.connect_timeout)
        ->check(CLI::Range(1, 600));
    auto* certificate_option
        = app.add_option("--certificate", options.certificate)
              ->type_name("<path>");
    auto* key_option = app.add_option("--key", options.key)->type_name("<path>");
    auto* director_name_option
        = app.add_option("--director-name", options.director_name);
    auto* director_address_option
        = app.add_option("--director-address", options.director_address);
    auto* director_port_option
        = app.add_option("--director-port", options.director_port)
              ->check(CLI::Range(1, 65535));
    app.add_option("--staging-dir", options.staging_dir)->capture_default_str();
    app.add_option("--token", options.token)->required();
    app.add_option("--shared-password", options.shared_password);
    app.add_flag("--force", options.force);
    CLI11_PARSE(app, argc, argv);

    (void)config_option;
    options.connection_direction = ParseConnectionDirection(connection_direction);
    ApplyDerivedDefaults(options, certificate_option->count() > 0,
                         key_option->count() > 0,
                         director_name_option->count() > 0,
                         director_address_option->count() > 0,
                         director_port_option->count() > 0);
    ValidateName("director_name", options.director_name);

    auto ssl_ctx = CreateTlsServerContext(options);

    if (options.connection_direction == ConnectionDirection::kDirectorConnects) {
      RunConnectBackMode(options, ssl_ctx.get());
    } else {
      RunListenMode(options, ssl_ctx.get());
    }
    return 0;
  } catch (const std::exception& e) {
    LogError(e.what());
    return 1;
  }
}
