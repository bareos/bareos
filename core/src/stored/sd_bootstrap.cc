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

#include "stored/sd_bootstrap.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <jansson.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#include "include/bareos.h"
#include "include/baconfig.h"
#include "include/exit_codes.h"
#include "lib/parse_conf.h"
#include "stored/sd_discovery_probe.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"

#if !HAVE_WIN32
#  include <grp.h>
#  include <pwd.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#  include <unistd.h>
#else
#  include <windows.h>
#endif

namespace storagedaemon {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;

template <typename T> struct BootstrapResult {
  std::optional<T> value{};
  std::string error{};

  explicit operator bool() const { return value.has_value(); }
};

struct ParsedBootstrapUrl {
  std::string scheme{};
  std::string host{};
  std::string port{};
  std::string api_base_path{};
};

struct BundleDirectoryEntry {
  std::string path{};
  std::optional<std::string> owner{};
  std::optional<std::string> group{};
  std::string mode{};
};

struct BundleFileEntry {
  std::string path{};
  std::optional<std::string> owner{};
  std::optional<std::string> group{};
  std::string mode{};
  std::string content{};
};

struct ConfigBundle {
  std::string path_base{};
  std::vector<BundleDirectoryEntry> directories{};
  std::vector<BundleFileEntry> files{};
};

class ScopedDirectory {
 public:
  explicit ScopedDirectory(std::filesystem::path path) : path_(std::move(path))
  {
  }
  ScopedDirectory(const ScopedDirectory&) = delete;
  ScopedDirectory& operator=(const ScopedDirectory&) = delete;
  ScopedDirectory(ScopedDirectory&& other) noexcept
      : path_(std::move(other.path_))
  {
    other.path_.clear();
  }
  ScopedDirectory& operator=(ScopedDirectory&& other) noexcept
  {
    if (this == &other) { return *this; }
    Cleanup();
    path_ = std::move(other.path_);
    other.path_.clear();
    return *this;
  }
  ~ScopedDirectory() { Cleanup(); }

  const std::filesystem::path& path() const { return path_; }

 private:
  void Cleanup()
  {
    if (path_.empty()) { return; }
    std::error_code error_code;
    std::filesystem::remove_all(path_, error_code);
    path_.clear();
  }

  std::filesystem::path path_{};
};

std::optional<std::string> GetEnvironmentVariable(std::string_view name)
{
  if (const char* value = std::getenv(std::string{name}.c_str());
      value && value[0] != '\0') {
    return std::string{value};
  }
  return std::nullopt;
}

uint32_t GetEnvironmentUint(std::string_view name, uint32_t fallback)
{
  auto value = GetEnvironmentVariable(name);
  if (!value) { return fallback; }

  uint64_t parsed = 0;
  for (const auto ch : *value) {
    if (ch < '0' || ch > '9') { return fallback; }
    parsed = parsed * 10 + static_cast<uint64_t>(ch - '0');
    if (parsed > std::numeric_limits<uint32_t>::max()) { return fallback; }
  }
  return static_cast<uint32_t>(parsed);
}

std::string TrimTrailingSlashes(std::string path)
{
  while (path.size() > 1 && path.back() == '/') { path.pop_back(); }
  return path;
}

std::string QuoteShellArgument(std::string_view value)
{
#if HAVE_WIN32
  return "\"" + std::string{value} + "\"";
#else
  std::string quoted{"'"};
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted += "'";
  return quoted;
#endif
}

int ExtractExitStatus(int result)
{
#if HAVE_WIN32
  return result;
#else
  if (result == -1) { return -1; }
  if (WIFEXITED(result)) { return WEXITSTATUS(result); }
  return -1;
#endif
}

bool EndsWith(std::string_view value, std::string_view suffix)
{
  return value.size() >= suffix.size()
         && value.substr(value.size() - suffix.size()) == suffix;
}

std::string NormalizeBootstrapApiBasePath(std::string path)
{
  if (path.empty() || path == "/") { return "/v1"; }
  path = TrimTrailingSlashes(std::move(path));
  if (EndsWith(path, "/api/bconfig/v1") || EndsWith(path, "/v1")) {
    return path;
  }
  if (EndsWith(path, "/api/bconfig")) { return path + "/v1"; }
  return path + "/v1";
}

BootstrapResult<ParsedBootstrapUrl> ParseBootstrapUrl(std::string_view url)
{
  const auto scheme_separator = url.find("://");
  if (scheme_separator == std::string_view::npos) {
    return {.error = "bootstrap URL must include a scheme such as http://."};
  }

  ParsedBootstrapUrl parsed;
  parsed.scheme = std::string{url.substr(0, scheme_separator)};
  if (parsed.scheme != "http") {
    return {.error = "bootstrap URL scheme must be http."};
  }

  const auto remainder = url.substr(scheme_separator + 3);
  const auto slash = remainder.find('/');
  const auto authority = remainder.substr(0, slash);
  if (authority.empty()) {
    return {.error = "bootstrap URL must include a host name."};
  }

  std::string_view host = authority;
  std::string_view port;
  if (authority.front() == '[') {
    const auto bracket = authority.find(']');
    if (bracket == std::string_view::npos) {
      return {.error = "bootstrap URL has an invalid IPv6 host."};
    }
    host = authority.substr(1, bracket - 1);
    if (bracket + 1 < authority.size()) {
      if (authority[bracket + 1] != ':') {
        return {.error = "bootstrap URL has an invalid authority section."};
      }
      port = authority.substr(bracket + 2);
    }
  } else {
    const auto colon = authority.rfind(':');
    if (colon != std::string_view::npos
        && authority.find(':') == authority.rfind(':')) {
      host = authority.substr(0, colon);
      port = authority.substr(colon + 1);
    }
  }

  if (host.empty()) {
    return {.error = "bootstrap URL host must not be empty."};
  }
  parsed.host = std::string{host};
  parsed.port = port.empty() ? "80" : std::string{port};
  parsed.api_base_path = NormalizeBootstrapApiBasePath(
      slash == std::string_view::npos ? "/"
                                      : std::string{remainder.substr(slash)});
  return {.value = std::move(parsed)};
}

BootstrapResult<std::string> SendHttpRequest(const ParsedBootstrapUrl& url,
                                             http::verb method,
                                             std::string target,
                                             std::optional<std::string> body)
{
  try {
    net::io_context io_context;
    tcp::resolver resolver{io_context};
    beast::tcp_stream stream{io_context};
    const auto endpoints = resolver.resolve(url.host, url.port);
    stream.connect(endpoints);

    http::request<http::string_body> request{method, std::move(target), 11};
    request.set(http::field::host, url.host + ":" + url.port);
    request.set(http::field::user_agent, "bareos-sd-bootstrap");
    request.set(http::field::accept, "application/json");
    request.keep_alive(false);
    if (body) {
      request.set(http::field::content_type, "application/json");
      request.body() = std::move(*body);
      request.prepare_payload();
    }

    http::write(stream, request);

    beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(stream, buffer, response);
    boost::system::error_code error_code;
    stream.socket().shutdown(tcp::socket::shutdown_both, error_code);

    if (response.result_int() >= 400) {
      std::ostringstream error;
      error << "bootstrap service request failed (" << response.result_int()
            << ")";
      if (!response.body().empty()) { error << ": " << response.body(); }
      return {.error = error.str()};
    }
    return {.value = std::move(response.body())};
  } catch (const std::exception& exception) {
    return {.error = exception.what()};
  }
}

JsonPtr MakeJson(json_t* value) { return JsonPtr(value, &json_decref); }

BootstrapResult<JsonPtr> ParseJsonObject(std::string_view body)
{
  json_error_t error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &error));
  if (!root) {
    return {.error = "failed to parse bootstrap service JSON response: "
                     + std::string{error.text}};
  }
  if (!json_is_object(root.get())) {
    return {.error = "bootstrap service response must be a JSON object."};
  }
  return {.value = std::move(root)};
}

std::string DumpJsonCompact(json_t* json)
{
  char* dump = json_dumps(json, JSON_COMPACT);
  if (!dump) { return {}; }
  std::string text{dump};
  std::free(dump);
  return text;
}

BootstrapResult<std::string> PostJson(const ParsedBootstrapUrl& url,
                                      std::string target,
                                      json_t* body)
{
  return SendHttpRequest(url, http::verb::post, std::move(target),
                         DumpJsonCompact(body));
}

BootstrapResult<std::string> GetJson(const ParsedBootstrapUrl& url,
                                     std::string target)
{
  return SendHttpRequest(url, http::verb::get, std::move(target), std::nullopt);
}

std::string BuildSessionRoute(const ParsedBootstrapUrl& url,
                              const BootstrapModeOptions& options)
{
  return url.api_base_path + "/bootstrap/storage-sessions/"
         + options.bootstrap_session;
}

BootstrapResult<std::string> RegisterBootstrapSession(
    const ParsedBootstrapUrl& url,
    const BootstrapModeOptions& options,
    const StorageDiscoveryReport& report)
{
  auto body = MakeJson(json_object());
  json_object_set_new(body.get(), "bootstrap_token",
                      json_string(options.bootstrap_token.c_str()));
  if (!report.hostname.empty()) {
    json_object_set_new(body.get(), "hostname",
                        json_string(report.hostname.c_str()));
  }
  if (!report.fqdn.empty()) {
    json_object_set_new(body.get(), "fqdn", json_string(report.fqdn.c_str()));
  }
  return PostJson(url, BuildSessionRoute(url, options) + "/register",
                  body.get());
}

BootstrapResult<std::string> SubmitBootstrapDiscovery(
    const ParsedBootstrapUrl& url,
    const BootstrapModeOptions& options,
    const StorageDiscoveryReport& report)
{
  const auto report_text = StorageDiscoveryReportToJson(report);
  json_error_t error{};
  auto report_json = MakeJson(
      json_loadb(report_text.c_str(), report_text.size(), 0, &error));
  if (!report_json) {
    return {.error = "failed to serialize discovery report: "
                     + std::string{error.text}};
  }

  auto body = MakeJson(json_object());
  json_object_set_new(body.get(), "bootstrap_token",
                      json_string(options.bootstrap_token.c_str()));
  if (!report.hostname.empty()) {
    json_object_set_new(body.get(), "hostname",
                        json_string(report.hostname.c_str()));
  }
  if (!report.fqdn.empty()) {
    json_object_set_new(body.get(), "fqdn", json_string(report.fqdn.c_str()));
  }
  json_object_set_new(body.get(), "report", report_json.release());
  return PostJson(url, BuildSessionRoute(url, options) + "/discovery",
                  body.get());
}

BootstrapResult<std::string> WaitForConfigSelection(
    const ParsedBootstrapUrl& url,
    const BootstrapModeOptions& options)
{
  const auto attempts
      = GetEnvironmentUint("BAREOS_SD_BOOTSTRAP_MAX_POLL_ATTEMPTS", 300);
  const auto interval_ms
      = GetEnvironmentUint("BAREOS_SD_BOOTSTRAP_POLL_INTERVAL_MS", 1000);

  for (uint32_t attempt = 0; attempt < attempts; ++attempt) {
    auto response = GetJson(url, BuildSessionRoute(url, options));
    if (!response) { return {.error = response.error}; }

    auto root = ParseJsonObject(*response.value);
    if (!root) { return {.error = root.error}; }
    auto* session
        = json_object_get(root.value->get(), "storage_bootstrap_session");
    if (!json_is_object(session)) {
      return {.error
              = "bootstrap service session response is missing "
                "'storage_bootstrap_session'."};
    }

    auto* status = json_object_get(session, "status");
    if (!json_is_string(status)) {
      return {.error
              = "bootstrap service session response is missing "
                "a string 'status'."};
    }
    const std::string status_text{json_string_value(status)};
    if (status_text == "selected" || status_text == "config_ready") {
      return {.value = status_text};
    }
    if (status_text == "failed" || status_text == "expired") {
      auto* last_error = json_object_get(session, "last_error");
      if (json_is_string(last_error)) {
        return {.error = std::string{json_string_value(last_error)}};
      }
      return {.error = "storage bootstrap session ended in state '"
                       + status_text + "'."};
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
  }

  return {.error = "timed out waiting for storage bootstrap selection."};
}

BootstrapResult<ConfigBundle> ParseConfigBundleResponse(std::string_view body)
{
  auto root = ParseJsonObject(body);
  if (!root) { return {.error = root.error}; }

  auto* bundle = json_object_get(root.value->get(), "config_bundle");
  if (!json_is_object(bundle)) {
    return {.error = "bootstrap service response is missing 'config_bundle'."};
  }

  auto* path_base = json_object_get(bundle, "path_base");
  auto* directories = json_object_get(bundle, "directories");
  auto* files = json_object_get(bundle, "files");
  if (!json_is_string(path_base) || !json_is_array(directories)
      || !json_is_array(files)) {
    return {.error
            = "bootstrap service config bundle is missing required "
              "fields."};
  }

  ConfigBundle parsed{.path_base = json_string_value(path_base)};
  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(directories, index, entry)
  {
    auto* path = json_object_get(entry, "path");
    auto* mode = json_object_get(entry, "mode");
    auto* owner = json_object_get(entry, "owner");
    auto* group = json_object_get(entry, "group");
    if (!json_is_object(entry) || !json_is_string(path) || !json_is_string(mode)
        || (owner && !json_is_null(owner) && !json_is_string(owner))
        || (group && !json_is_null(group) && !json_is_string(group))) {
      return {.error
              = "bootstrap service config bundle contains an invalid "
                "directory entry."};
    }
    BundleDirectoryEntry directory{
        .path = json_string_value(path),
        .owner = owner && json_is_string(owner)
                     ? std::optional<std::string>{json_string_value(owner)}
                     : std::nullopt,
        .group = group && json_is_string(group)
                     ? std::optional<std::string>{json_string_value(group)}
                     : std::nullopt,
        .mode = json_string_value(mode),
    };
    parsed.directories.emplace_back(std::move(directory));
  }

  json_array_foreach(files, index, entry)
  {
    auto* path = json_object_get(entry, "path");
    auto* mode = json_object_get(entry, "mode");
    auto* owner = json_object_get(entry, "owner");
    auto* group = json_object_get(entry, "group");
    auto* content = json_object_get(entry, "content");
    if (!json_is_object(entry) || !json_is_string(path) || !json_is_string(mode)
        || !json_is_string(content)
        || (owner && !json_is_null(owner) && !json_is_string(owner))
        || (group && !json_is_null(group) && !json_is_string(group))) {
      return {.error
              = "bootstrap service config bundle contains an invalid "
                "file entry."};
    }
    BundleFileEntry file{
        .path = json_string_value(path),
        .owner = owner && json_is_string(owner)
                     ? std::optional<std::string>{json_string_value(owner)}
                     : std::nullopt,
        .group = group && json_is_string(group)
                     ? std::optional<std::string>{json_string_value(group)}
                     : std::nullopt,
        .mode = json_string_value(mode),
        .content = json_string_value(content),
    };
    parsed.files.emplace_back(std::move(file));
  }

  return {.value = std::move(parsed)};
}

std::filesystem::path DefaultBootstrapConfigRoot()
{
  if (auto configured
      = GetEnvironmentVariable("BAREOS_SD_BOOTSTRAP_CONFIG_ROOT")) {
    return *configured;
  }
  return CONFDIR;
}

BootstrapResult<std::filesystem::path> ResolveValidationExecutable()
{
  if (auto configured
      = GetEnvironmentVariable("BAREOS_SD_BOOTSTRAP_VALIDATE_BINARY")) {
    return {.value = std::filesystem::path{*configured}};
  }

#if HAVE_WIN32
  std::vector<char> buffer(MAX_PATH);
  const auto length = GetModuleFileNameA(nullptr, buffer.data(), buffer.size());
  if (length > 0) {
    return {.value = std::filesystem::path{
                std::string{buffer.data(), static_cast<std::size_t>(length)}}};
  }
#elif defined(HAVE_FREEBSD_OS)
  std::error_code error_code;
  const auto self
      = std::filesystem::canonical("/proc/curproc/file", error_code);
  if (!error_code) { return {.value = self}; }
#else
  std::error_code error_code;
  const auto self = std::filesystem::canonical("/proc/self/exe", error_code);
  if (!error_code) { return {.value = self}; }
#endif

  return {.value = std::filesystem::path{"bareos-sd"}};
}

BootstrapResult<std::filesystem::path> ResolveBundlePath(
    const std::filesystem::path& root,
    std::string_view relative_path)
{
  std::filesystem::path relative{std::string{relative_path}};
  if (relative.empty() || relative.is_absolute()) {
    return {.error = "bootstrap config bundle paths must be relative."};
  }

  for (const auto& part : relative) {
    if (part == "." || part == "..") {
      return {.error
              = "bootstrap config bundle paths must not contain '.' or "
                "'..' segments."};
    }
  }
  return {.value = root / relative};
}

BootstrapResult<std::filesystem::perms> ParseMode(std::string_view text)
{
  if (text.empty() || text.size() > 4) {
    return {.error = "bootstrap config bundle mode must be an octal string."};
  }
  unsigned value = 0;
  for (const auto ch : text) {
    if (ch < '0' || ch > '7') {
      return {.error = "bootstrap config bundle mode must be an octal string."};
    }
    value = (value << 3) + static_cast<unsigned>(ch - '0');
  }
  return {.value = static_cast<std::filesystem::perms>(value)};
}

BootstrapResult<std::monostate> WriteFile(const std::filesystem::path& path,
                                          std::string_view content)
{
  std::error_code error_code;
  std::filesystem::create_directories(path.parent_path(), error_code);
  if (error_code) {
    return {.error = "failed to create directory '"
                     + path.parent_path().string()
                     + "': " + error_code.message()};
  }

  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return {.error = "failed to open bootstrap config file '" + path.string()
                     + "' for writing."};
  }
  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!stream) {
    return {.error
            = "failed to write bootstrap config file '" + path.string() + "'."};
  }
  return {.value = std::monostate{}};
}

BootstrapResult<std::monostate> CopyDirectoryContents(
    const std::filesystem::path& source_root,
    const std::filesystem::path& destination_root)
{
  if (!std::filesystem::exists(source_root)) {
    return {.value = std::monostate{}};
  }

  std::error_code error_code;
  std::filesystem::create_directories(destination_root, error_code);
  if (error_code) {
    return {.error = "failed to create staging root '"
                     + destination_root.string()
                     + "': " + error_code.message()};
  }

  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(source_root)) {
    const auto relative
        = std::filesystem::relative(entry.path(), source_root, error_code);
    if (error_code) {
      return {.error = "failed to build relative path while staging '"
                       + entry.path().string() + "': " + error_code.message()};
    }
    const auto target = destination_root / relative;
    if (entry.is_directory()) {
      std::filesystem::create_directories(target, error_code);
      if (error_code) {
        return {.error = "failed to create staged directory '" + target.string()
                         + "': " + error_code.message()};
      }
      continue;
    }
    if (!entry.is_regular_file()) { continue; }
    std::filesystem::create_directories(target.parent_path(), error_code);
    if (error_code) {
      return {.error = "failed to create staged parent directory '"
                       + target.parent_path().string()
                       + "': " + error_code.message()};
    }
    std::filesystem::copy_file(
        entry.path(), target, std::filesystem::copy_options::overwrite_existing,
        error_code);
    if (error_code) {
      return {.error = "failed to stage config file '" + target.string()
                       + "': " + error_code.message()};
    }
  }

  return {.value = std::monostate{}};
}

BootstrapResult<std::filesystem::path> CreateStagingRoot()
{
  const auto unique
      = "bareos-sd-bootstrap-"
        + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
  const auto path = std::filesystem::temp_directory_path() / unique;
  std::error_code error_code;
  std::filesystem::create_directories(path, error_code);
  if (error_code) {
    return {.error = "failed to create bootstrap staging directory '"
                     + path.string() + "': " + error_code.message()};
  }
  return {.value = path};
}

BootstrapResult<std::monostate> ApplyPermissions(
    const std::filesystem::path& path,
    std::string_view mode)
{
  auto permissions = ParseMode(mode);
  if (!permissions) { return {.error = permissions.error}; }
  std::error_code error_code;
  std::filesystem::permissions(path, *permissions.value,
                               std::filesystem::perm_options::replace,
                               error_code);
  if (error_code) {
    return {.error = "failed to set permissions on '" + path.string()
                     + "': " + error_code.message()};
  }
  return {.value = std::monostate{}};
}

#if !HAVE_WIN32
BootstrapResult<std::monostate> ApplyOwnership(
    const std::filesystem::path& path,
    const std::optional<std::string>& owner,
    const std::optional<std::string>& group)
{
  if (!owner && !group) { return {.value = std::monostate{}}; }
  if (geteuid() != 0) { return {.value = std::monostate{}}; }

  uid_t uid = static_cast<uid_t>(-1);
  gid_t gid = static_cast<gid_t>(-1);
  if (owner) {
    auto* passwd = getpwnam(owner->c_str());
    if (!passwd) {
      return {.error
              = "failed to resolve bootstrap bundle owner '" + *owner + "'."};
    }
    uid = passwd->pw_uid;
  }
  if (group) {
    auto* group_entry = getgrnam(group->c_str());
    if (!group_entry) {
      return {.error
              = "failed to resolve bootstrap bundle group '" + *group + "'."};
    }
    gid = group_entry->gr_gid;
  }
  if (chown(path.c_str(), uid, gid) != 0) {
    return {.error = "failed to assign ownership on '" + path.string()
                     + "': " + std::strerror(errno)};
  }
  return {.value = std::monostate{}};
}
#else
BootstrapResult<std::monostate> ApplyOwnership(
    const std::filesystem::path&,
    const std::optional<std::string>&,
    const std::optional<std::string>&)
{
  return {.value = std::monostate{}};
}
#endif

BootstrapResult<std::monostate> StageBundleIntoRoot(
    const ConfigBundle& bundle,
    const std::filesystem::path& root)
{
  if (bundle.path_base != "storage_config_root") {
    return {.error
            = "bootstrap config bundle path_base must be "
              "'storage_config_root'."};
  }

  for (const auto& directory : bundle.directories) {
    auto path = ResolveBundlePath(root, directory.path);
    if (!path) { return {.error = path.error}; }
    std::error_code error_code;
    std::filesystem::create_directories(*path.value, error_code);
    if (error_code) {
      return {.error = "failed to create bootstrap bundle directory '"
                       + path.value->string() + "': " + error_code.message()};
    }
    if (auto permissions = ApplyPermissions(*path.value, directory.mode);
        !permissions) {
      return {.error = permissions.error};
    }
    if (auto ownership
        = ApplyOwnership(*path.value, directory.owner, directory.group);
        !ownership) {
      return {.error = ownership.error};
    }
  }

  for (const auto& file : bundle.files) {
    auto path = ResolveBundlePath(root, file.path);
    if (!path) { return {.error = path.error}; }
    auto write = WriteFile(*path.value, file.content);
    if (!write) { return {.error = write.error}; }
    if (auto permissions = ApplyPermissions(*path.value, file.mode);
        !permissions) {
      return {.error = permissions.error};
    }
    if (auto ownership = ApplyOwnership(*path.value, file.owner, file.group);
        !ownership) {
      return {.error = ownership.error};
    }
  }

  return {.value = std::monostate{}};
}

BootstrapResult<std::monostate> ValidateInstalledConfig(
    const std::filesystem::path& config_root)
{
  auto executable = ResolveValidationExecutable();
  if (!executable) { return {.error = executable.error}; }

  const std::string command = QuoteShellArgument(executable.value->string())
                              + " --test-config --config "
                              + QuoteShellArgument(config_root.string());
  if (ExtractExitStatus(std::system(command.c_str())) != BEXIT_SUCCESS) {
    return {.error
            = "installed storage-daemon config did not validate "
              "cleanly."};
  }
  return {.value = std::monostate{}};
}

BootstrapResult<std::monostate> InstallConfigBundle(
    const ConfigBundle& bundle,
    const std::filesystem::path& config_root)
{
  auto staging_root = CreateStagingRoot();
  if (!staging_root) { return {.error = staging_root.error}; }
  ScopedDirectory cleanup{*staging_root.value};

  auto staged_copy = CopyDirectoryContents(config_root, *staging_root.value);
  if (!staged_copy) { return {.error = staged_copy.error}; }
  auto staged_bundle = StageBundleIntoRoot(bundle, *staging_root.value);
  if (!staged_bundle) { return {.error = staged_bundle.error}; }
  auto validation = ValidateInstalledConfig(*staging_root.value);
  if (!validation) { return {.error = validation.error}; }

  std::error_code error_code;
  std::filesystem::create_directories(config_root, error_code);
  if (error_code) {
    return {.error = "failed to create storage config root '"
                     + config_root.string() + "': " + error_code.message()};
  }
  auto live_bundle = StageBundleIntoRoot(bundle, config_root);
  if (!live_bundle) { return {.error = live_bundle.error}; }
  return {.value = std::monostate{}};
}

void ReportBootstrapFailure(const ParsedBootstrapUrl& url,
                            const BootstrapModeOptions& options,
                            std::string_view message)
{
  auto body = MakeJson(json_object());
  json_object_set_new(body.get(), "bootstrap_token",
                      json_string(options.bootstrap_token.c_str()));
  json_object_set_new(body.get(), "success", json_false());
  json_object_set_new(body.get(), "error",
                      json_string(std::string{message}.c_str()));
  auto response
      = PostJson(url, BuildSessionRoute(url, options) + "/applied", body.get());
  if (!response) {
    std::fprintf(stderr,
                 "bareos-sd: failed to report bootstrap failure for session "
                 "'%s': %s\n",
                 options.bootstrap_session.c_str(), response.error.c_str());
  }
}

BootstrapResult<std::string> ReportBootstrapApplied(
    const ParsedBootstrapUrl& url,
    const BootstrapModeOptions& options)
{
  auto body = MakeJson(json_object());
  json_object_set_new(body.get(), "bootstrap_token",
                      json_string(options.bootstrap_token.c_str()));
  json_object_set_new(body.get(), "success", json_true());
  return PostJson(url, BuildSessionRoute(url, options) + "/applied",
                  body.get());
}

}  // namespace

BootstrapModeOptionHandles AddBootstrapOptions(CLI::App& app,
                                               BootstrapModeOptions& options)
{
  BootstrapModeOptionHandles handles;

  handles.discovery
      = app.add_flag("--discovery", options.enabled,
                     "Run in restricted storage discovery bootstrap mode.");
  handles.bootstrap_url
      = app.add_option("--bootstrap-url", options.bootstrap_url,
                       "Bootstrap service URL for discovery mode.");
  handles.bootstrap_token
      = app.add_option("--bootstrap-token", options.bootstrap_token,
                       "One-time bootstrap token for discovery mode.");
  handles.bootstrap_session
      = app.add_option("--bootstrap-session", options.bootstrap_session,
                       "Bootstrap session identifier for discovery mode.");

  handles.bootstrap_url->type_name("<url>");
  handles.bootstrap_token->type_name("<token>");
  handles.bootstrap_session->type_name("<id>");

  return handles;
}

std::optional<std::string> ValidateBootstrapOptions(
    const BootstrapModeOptions& options)
{
  const bool has_url = !options.bootstrap_url.empty();
  const bool has_token = !options.bootstrap_token.empty();
  const bool has_session = !options.bootstrap_session.empty();
  const bool has_bootstrap_details = has_url || has_token || has_session;

  if (!options.enabled && has_bootstrap_details) {
    return "--bootstrap-url, --bootstrap-token, and --bootstrap-session "
           "require --discovery.";
  }

  if (options.enabled && (!has_url || !has_token || !has_session)) {
    return "--discovery requires --bootstrap-url, --bootstrap-token, and "
           "--bootstrap-session.";
  }

  return std::nullopt;
}

int RunStorageDaemonBootstrap(const BootstrapModeOptions& options)
{
  const auto report = ProbeStorageDiscoveryReport();
  const auto parsed_url = ParseBootstrapUrl(options.bootstrap_url);
  if (!parsed_url) {
    std::fprintf(stderr, "bareos-sd: invalid bootstrap URL: %s\n",
                 parsed_url.error.c_str());
    return BEXIT_FAILURE;
  }

  std::fprintf(stderr,
               "bareos-sd: starting restricted discovery bootstrap mode for "
               "session '%s' via %s\n",
               options.bootstrap_session.c_str(),
               options.bootstrap_url.c_str());
  std::fprintf(stderr,
               "bareos-sd: detected %zu filesystem candidate(s) on %s\n",
               report.filesystems.size(),
               report.fqdn.empty() ? "<unknown-host>" : report.fqdn.c_str());

  auto register_response
      = RegisterBootstrapSession(*parsed_url.value, options, report);
  if (!register_response) {
    std::fprintf(stderr,
                 "bareos-sd: failed to register bootstrap session: %s\n",
                 register_response.error.c_str());
    return BEXIT_FAILURE;
  }

  auto discovery_response
      = SubmitBootstrapDiscovery(*parsed_url.value, options, report);
  if (!discovery_response) {
    std::fprintf(stderr, "bareos-sd: failed to upload discovery report: %s\n",
                 discovery_response.error.c_str());
    ReportBootstrapFailure(*parsed_url.value, options,
                           discovery_response.error);
    return BEXIT_FAILURE;
  }

  auto selection = WaitForConfigSelection(*parsed_url.value, options);
  if (!selection) {
    std::fprintf(stderr,
                 "bareos-sd: bootstrap session did not reach selection: "
                 "%s\n",
                 selection.error.c_str());
    ReportBootstrapFailure(*parsed_url.value, options, selection.error);
    return BEXIT_FAILURE;
  }

  auto bundle_response
      = GetJson(*parsed_url.value,
                BuildSessionRoute(*parsed_url.value, options)
                    + "/config-bundle?token=" + options.bootstrap_token);
  if (!bundle_response) {
    std::fprintf(stderr, "bareos-sd: failed to fetch config bundle: %s\n",
                 bundle_response.error.c_str());
    ReportBootstrapFailure(*parsed_url.value, options, bundle_response.error);
    return BEXIT_FAILURE;
  }

  auto bundle = ParseConfigBundleResponse(*bundle_response.value);
  if (!bundle) {
    std::fprintf(stderr, "bareos-sd: invalid config bundle response: %s\n",
                 bundle.error.c_str());
    ReportBootstrapFailure(*parsed_url.value, options, bundle.error);
    return BEXIT_FAILURE;
  }

  const auto config_root = DefaultBootstrapConfigRoot();
  auto install = InstallConfigBundle(*bundle.value, config_root);
  if (!install) {
    std::fprintf(stderr, "bareos-sd: failed to install config bundle: %s\n",
                 install.error.c_str());
    ReportBootstrapFailure(*parsed_url.value, options, install.error);
    return BEXIT_FAILURE;
  }

  auto applied_response = ReportBootstrapApplied(*parsed_url.value, options);
  if (!applied_response) {
    std::fprintf(stderr,
                 "bareos-sd: failed to report bootstrap apply success: "
                 "%s\n",
                 applied_response.error.c_str());
    return BEXIT_FAILURE;
  }

  std::fprintf(stderr, "bareos-sd: installed bootstrap config bundle into %s\n",
               config_root.c_str());
  std::fprintf(stderr,
               "bareos-sd: bootstrap session '%s' completed successfully\n",
               options.bootstrap_session.c_str());
  return BEXIT_SUCCESS;
}

}  // namespace storagedaemon
