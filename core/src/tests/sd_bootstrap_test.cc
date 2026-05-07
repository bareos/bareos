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

#include "gtest/gtest.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <jansson.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "include/bareos.h"
#include "include/exit_codes.h"
#include "stored/sd_bootstrap.h"

#if !HAVE_WIN32
#  include <sys/wait.h>
#endif

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;
using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;

namespace {

class ScopedDirectory {
 public:
  ScopedDirectory()
  {
    const auto unique
        = "sd-bootstrap-test-"
          + std::to_string(
              std::chrono::steady_clock::now().time_since_epoch().count());
    path_ = std::filesystem::temp_directory_path() / unique;
    std::filesystem::create_directories(path_);
  }

  ~ScopedDirectory()
  {
    std::error_code error_code;
    std::filesystem::remove_all(path_, error_code);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_{};
};

class ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const char* name, std::string value) : name_(name)
  {
    if (const char* existing = std::getenv(name_); existing) {
      previous_ = existing;
    }
#if HAVE_WIN32
    _putenv_s(name_, value.c_str());
#else
    setenv(name_, value.c_str(), 1);
#endif
  }

  ~ScopedEnvironmentVariable()
  {
    if (previous_) {
#if HAVE_WIN32
      _putenv_s(name_, previous_->c_str());
#else
      setenv(name_, previous_->c_str(), 1);
#endif
    } else {
#if HAVE_WIN32
      _putenv_s(name_, "");
#else
      unsetenv(name_);
#endif
    }
  }

 private:
  const char* name_;
  std::optional<std::string> previous_{};
};

void ParseArgs(CLI::App& app, const std::vector<std::string>& args)
{
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (const auto& arg : args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }

  app.parse(static_cast<int>(argv.size()), argv.data());
}

std::string ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

#if defined(HAVE_LINUX_OS)
void WriteBinaryFile(const std::filesystem::path& path,
                     std::string_view content)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string BuildPage80(std::string_view serial)
{
  std::string page;
  page.push_back('\0');
  page.push_back(static_cast<char>(0x80));
  page.push_back(static_cast<char>((serial.size() >> 8) & 0xff));
  page.push_back(static_cast<char>(serial.size() & 0xff));
  page.append(serial);
  return page;
}

std::string BuildPage83Naa(std::string_view designator)
{
  std::string page;
  page.push_back('\0');
  page.push_back(static_cast<char>(0x83));
  page.push_back('\0');
  page.push_back(static_cast<char>(4 + designator.size()));
  page.push_back(static_cast<char>(0x01));
  page.push_back(static_cast<char>(0x03));
  page.push_back('\0');
  page.push_back(static_cast<char>(designator.size()));
  page.append(designator);
  return page;
}

std::string BuildReadElementStatusDataTransferDescriptor(
    uint16_t element_address,
    std::string_view designator)
{
  std::string descriptor(12, '\0');
  descriptor[0] = static_cast<char>((element_address >> 8) & 0xff);
  descriptor[1] = static_cast<char>(element_address & 0xff);
  descriptor[2] = static_cast<char>(0x09);
  descriptor.append(1, static_cast<char>(0x01));
  descriptor.append(1, static_cast<char>(0x03));
  descriptor.append(1, '\0');
  descriptor.append(1, static_cast<char>(designator.size()));
  descriptor.append(designator);
  return descriptor;
}

std::string BuildReadElementStatusData(
    std::initializer_list<std::string_view> descriptors)
{
  std::string page(8, '\0');
  page[0] = static_cast<char>(0x04);
  std::size_t descriptor_size = 0;
  std::size_t descriptor_length = 0;
  for (const auto descriptor : descriptors) {
    descriptor_size += descriptor.size();
    descriptor_length = descriptor.size();
    page.append(descriptor);
  }
  page[2] = static_cast<char>((descriptor_length >> 8) & 0xff);
  page[3] = static_cast<char>(descriptor_length & 0xff);
  page[5] = static_cast<char>((descriptor_size >> 16) & 0xff);
  page[6] = static_cast<char>((descriptor_size >> 8) & 0xff);
  page[7] = static_cast<char>(descriptor_size & 0xff);

  std::string payload(8, '\0');
  payload[2] = '\0';
  payload[3] = '\1';
  payload[5] = static_cast<char>((page.size() >> 16) & 0xff);
  payload[6] = static_cast<char>((page.size() >> 8) & 0xff);
  payload[7] = static_cast<char>(page.size() & 0xff);
  payload.append(page);
  return payload;
}
#endif

std::string ExtractPath(std::string_view target)
{
  return std::string{target.substr(0, target.find('?'))};
}

JsonPtr MakeJson(json_t* value) { return JsonPtr(value, &json_decref); }

std::string DumpJson(json_t* value)
{
  char* dump = json_dumps(value, JSON_COMPACT);
  std::string text = dump ? std::string{dump} : std::string{};
  std::free(dump);
  return text;
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

class FakeBootstrapServer {
 public:
  explicit FakeBootstrapServer(std::string config_bundle_body)
      : acceptor_(io_context_,
                  tcp::endpoint{net::ip::make_address("127.0.0.1"), 0})
      , config_bundle_body_(std::move(config_bundle_body))
  {
    port_ = acceptor_.local_endpoint().port();
    thread_ = std::thread([this]() { Run(); });
  }

  ~FakeBootstrapServer()
  {
    stop_.store(true);
    try {
      net::io_context io_context;
      tcp::socket socket{io_context};
      socket.connect(tcp::endpoint{net::ip::make_address("127.0.0.1"), port_});
    } catch (...) {
    }
    if (thread_.joinable()) { thread_.join(); }
  }

  std::string base_url() const
  {
    return "http://127.0.0.1:" + std::to_string(port_);
  }

  std::string register_body() const
  {
    std::lock_guard lock{mutex_};
    return register_body_;
  }

  std::string discovery_body() const
  {
    std::lock_guard lock{mutex_};
    return discovery_body_;
  }

  std::string applied_body() const
  {
    std::lock_guard lock{mutex_};
    return applied_body_;
  }

 private:
  void Run()
  {
    for (;;) {
      boost::system::error_code error_code;
      tcp::socket socket{io_context_};
      acceptor_.accept(socket, error_code);
      if (stop_.load()) { break; }
      if (error_code) { continue; }

      beast::flat_buffer buffer;
      http::request<http::string_body> request;
      http::read(socket, buffer, request, error_code);
      if (error_code) { continue; }

      auto response = HandleRequest(request);
      http::write(socket, response, error_code);
      socket.shutdown(tcp::socket::shutdown_both, error_code);
    }
  }

  http::response<http::string_body> HandleRequest(
      const http::request<http::string_body>& request)
  {
    const auto path = ExtractPath(request.target());
    http::response<http::string_body> response{http::status::ok,
                                               request.version()};
    response.set(http::field::content_type, "application/json");
    response.keep_alive(false);

    if (request.method() == http::verb::post
        && path == "/v1/bootstrap/storage-sessions/session-123/register") {
      std::lock_guard lock{mutex_};
      register_body_ = request.body();
      response.body()
          = R"({"storage_bootstrap_session":{"id":"session-123","status":"registered"}})";
    } else if (
        request.method() == http::verb::post
        && path == "/v1/bootstrap/storage-sessions/session-123/discovery") {
      std::lock_guard lock{mutex_};
      discovery_body_ = request.body();
      discovered_ = true;
      response.body()
          = R"({"storage_bootstrap_session":{"id":"session-123","status":"discovered"}})";
    } else if (request.method() == http::verb::get
               && path == "/v1/bootstrap/storage-sessions/session-123") {
      response.body()
          = discovered_
                ? R"({"storage_bootstrap_session":{"id":"session-123","status":"selected"}})"
                : R"({"storage_bootstrap_session":{"id":"session-123","status":"registered"}})";
    } else if (
        request.method() == http::verb::get
        && path == "/v1/bootstrap/storage-sessions/session-123/config-bundle") {
      response.body() = config_bundle_body_;
    } else if (request.method() == http::verb::post
               && path
                      == "/v1/bootstrap/storage-sessions/session-123/applied") {
      std::lock_guard lock{mutex_};
      applied_body_ = request.body();
      response.body()
          = R"({"storage_bootstrap_session":{"id":"session-123","status":"applied"}})";
    } else {
      response.result(http::status::not_found);
      response.body() = R"({"error":"unexpected route"})";
    }

    response.prepare_payload();
    return response;
  }

  mutable std::mutex mutex_{};
  net::io_context io_context_{};
  tcp::acceptor acceptor_;
  std::thread thread_{};
  std::atomic<bool> stop_{false};
  uint16_t port_{0};
  bool discovered_{false};
  std::string config_bundle_body_{};
  std::string register_body_{};
  std::string discovery_body_{};
  std::string applied_body_{};
};

std::string BuildConfigBundleResponse(
    const std::filesystem::path& working_directory)
{
  auto root = MakeJson(json_object());
  auto bundle = MakeJson(json_object());
  json_object_set_new(bundle.get(), "path_base",
                      json_string("storage_config_root"));

  auto directories = MakeJson(json_array());
  for (const auto* path :
       {"bareos-sd.d", "bareos-sd.d/storage", "bareos-sd.d/director",
        "bareos-sd.d/device", "bareos-sd.d/messages"}) {
    auto entry = MakeJson(json_object());
    json_object_set_new(entry.get(), "path", json_string(path));
    json_object_set_new(entry.get(), "owner", json_string("root"));
    json_object_set_new(entry.get(), "group", json_string("bareos"));
    json_object_set_new(entry.get(), "mode", json_string("0750"));
    json_array_append_new(directories.get(), entry.release());
  }
  json_object_set_new(bundle.get(), "directories", directories.release());

  auto files = MakeJson(json_array());

  auto storage = MakeJson(json_object());
  std::ostringstream storage_content;
  storage_content << "Storage {\n"
                  << "  Name = \"bareos-sd\"\n";
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  storage_content << "  Backend Directory = \"" << BACKEND_OUTPUT_DIRECTORY
                  << "\"\n";
#endif
  storage_content << "  Working Directory = \"" << working_directory.string()
                  << "\"\n"
                  << "}\n";
  json_object_set_new(storage.get(), "path",
                      json_string("bareos-sd.d/storage/bareos-sd.conf"));
  json_object_set_new(storage.get(), "owner", json_string("root"));
  json_object_set_new(storage.get(), "group", json_string("bareos"));
  json_object_set_new(storage.get(), "mode", json_string("0640"));
  json_object_set_new(storage.get(), "content",
                      json_string(storage_content.str().c_str()));
  json_array_append_new(files.get(), storage.release());

  auto director = MakeJson(json_object());
  json_object_set_new(director.get(), "path",
                      json_string("bareos-sd.d/director/bareos-dir.conf"));
  json_object_set_new(director.get(), "owner", json_string("root"));
  json_object_set_new(director.get(), "group", json_string("bareos"));
  json_object_set_new(director.get(), "mode", json_string("0640"));
  json_object_set_new(
      director.get(), "content",
      json_string("Director {\n  Name = \"bareos-dir\"\n  Password = "
                  "\"storage-director-secret\"\n}\n"));
  json_array_append_new(files.get(), director.release());

  auto messages = MakeJson(json_object());
  json_object_set_new(messages.get(), "path",
                      json_string("bareos-sd.d/messages/Standard.conf"));
  json_object_set_new(messages.get(), "owner", json_string("root"));
  json_object_set_new(messages.get(), "group", json_string("bareos"));
  json_object_set_new(messages.get(), "mode", json_string("0640"));
  json_object_set_new(messages.get(), "content",
                      json_string("Messages {\n"
                                  "  Name = Standard\n"
                                  "  director = bareos-dir = all\n"
                                  "}\n"));
  json_array_append_new(files.get(), messages.release());

  auto device = MakeJson(json_object());
  json_object_set_new(device.get(), "path",
                      json_string("bareos-sd.d/device/FileStorage.conf"));
  json_object_set_new(device.get(), "owner", json_string("root"));
  json_object_set_new(device.get(), "group", json_string("bareos"));
  json_object_set_new(device.get(), "mode", json_string("0640"));
  json_object_set_new(
      device.get(), "content",
      json_string("Device {\n"
                  "  Name = \"FileStorage\"\n"
                  "  Media Type = File\n"
                  "  Device Type = \"File\"\n"
                  "  Archive Device = /srv/storage/bareos/storage\n"
                  "  Label Media = yes\n"
                  "  Random Access = yes\n"
                  "  Automatic Mount = yes\n"
                  "  Removable Media = no\n"
                  "  Always Open = no\n"
                  "}\n"));
  json_array_append_new(files.get(), device.release());

  json_object_set_new(bundle.get(), "files", files.release());
  json_object_set_new(root.get(), "config_bundle", bundle.release());
  return DumpJson(root.get());
}

TEST(SdBootstrap, DiscoveryModeRequiresBootstrapParameters)
{
  CLI::App app;
  InitCLIApp(app, "test app");

  storagedaemon::BootstrapModeOptions options;
  storagedaemon::AddBootstrapOptions(app, options);

  std::vector<std::string> args{"bareos-sd", "--discovery"};
  EXPECT_NO_THROW(ParseArgs(app, args));
  EXPECT_EQ(storagedaemon::ValidateBootstrapOptions(options),
            std::optional<std::string>{
                "--discovery requires --bootstrap-url, --bootstrap-token, and "
                "--bootstrap-session."});
}

TEST(SdBootstrap, BootstrapParametersRequireDiscoveryMode)
{
  CLI::App app;
  InitCLIApp(app, "test app");

  storagedaemon::BootstrapModeOptions options;
  storagedaemon::AddBootstrapOptions(app, options);

  std::vector<std::string> args{"bareos-sd", "--bootstrap-url",
                                "http://127.0.0.1:9103"};
  EXPECT_NO_THROW(ParseArgs(app, args));
  EXPECT_EQ(storagedaemon::ValidateBootstrapOptions(options),
            std::optional<std::string>{
                "--bootstrap-url, --bootstrap-token, and --bootstrap-session "
                "require --discovery."});
}

TEST(SdBootstrap, DiscoveryModeAcceptsCompleteBootstrapConfiguration)
{
  CLI::App app;
  InitCLIApp(app, "test app");

  storagedaemon::BootstrapModeOptions options;
  storagedaemon::AddBootstrapOptions(app, options);

  std::vector<std::string> args{"bareos-sd",           "--discovery",
                                "--bootstrap-url",     "http://127.0.0.1:9103",
                                "--bootstrap-token",   "secret-token",
                                "--bootstrap-session", "session-123"};
  EXPECT_NO_THROW(ParseArgs(app, args));
  EXPECT_EQ(storagedaemon::ValidateBootstrapOptions(options), std::nullopt);
  EXPECT_TRUE(options.enabled);
  EXPECT_EQ(options.bootstrap_url, "http://127.0.0.1:9103");
  EXPECT_EQ(options.bootstrap_token, "secret-token");
  EXPECT_EQ(options.bootstrap_session, "session-123");
}

TEST(SdBootstrap, InstallsBootstrapConfigBundleAndReportsApplySuccess)
{
  ScopedDirectory config_root;
  ScopedDirectory working_directory;
  FakeBootstrapServer server{
      BuildConfigBundleResponse(working_directory.path())};
  ScopedEnvironmentVariable config_root_override{
      "BAREOS_SD_BOOTSTRAP_CONFIG_ROOT", config_root.path().string()};
  std::optional<ScopedEnvironmentVariable> validate_binary_override;
  if (!std::getenv("BAREOS_SD_BOOTSTRAP_VALIDATE_BINARY")) {
    validate_binary_override.emplace("BAREOS_SD_BOOTSTRAP_VALIDATE_BINARY",
                                     BAREOS_SD_BINARY);
  }
  ScopedEnvironmentVariable poll_interval_override{
      "BAREOS_SD_BOOTSTRAP_POLL_INTERVAL_MS", "1"};
  ScopedEnvironmentVariable poll_attempts_override{
      "BAREOS_SD_BOOTSTRAP_MAX_POLL_ATTEMPTS", "10"};

  storagedaemon::BootstrapModeOptions options{
      .enabled = true,
      .bootstrap_url = server.base_url(),
      .bootstrap_token = "secret-token",
      .bootstrap_session = "session-123",
  };

  const std::string command
      = std::string{QuoteShellArgument(BAREOS_SD_BINARY)} + " --discovery"
        + " --bootstrap-url " + QuoteShellArgument(options.bootstrap_url)
        + " --bootstrap-token " + QuoteShellArgument(options.bootstrap_token)
        + " --bootstrap-session "
        + QuoteShellArgument(options.bootstrap_session);

  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), BEXIT_SUCCESS);

  const auto installed_device
      = config_root.path() / "bareos-sd.d/device/FileStorage.conf";
  const auto installed_storage
      = config_root.path() / "bareos-sd.d/storage/bareos-sd.conf";
  EXPECT_TRUE(std::filesystem::exists(installed_device));
  EXPECT_TRUE(std::filesystem::exists(installed_storage));
  EXPECT_NE(ReadTextFile(installed_device).find("/srv/storage/bareos/storage"),
            std::string::npos);

  json_error_t error{};
  auto register_body
      = MakeJson(json_loads(server.register_body().c_str(), 0, &error));
  ASSERT_NE(register_body.get(), nullptr);
  EXPECT_STREQ(json_string_value(
                   json_object_get(register_body.get(), "bootstrap_token")),
               "secret-token");

  auto discovery_body
      = MakeJson(json_loads(server.discovery_body().c_str(), 0, &error));
  ASSERT_NE(discovery_body.get(), nullptr);
  EXPECT_STREQ(json_string_value(
                   json_object_get(discovery_body.get(), "bootstrap_token")),
               "secret-token");
  auto* report = json_object_get(discovery_body.get(), "report");
  ASSERT_TRUE(json_is_object(report));
  EXPECT_TRUE(json_object_get(report, "filesystems") != nullptr);

  auto applied_body
      = MakeJson(json_loads(server.applied_body().c_str(), 0, &error));
  ASSERT_NE(applied_body.get(), nullptr);
  EXPECT_TRUE(json_is_true(json_object_get(applied_body.get(), "success")));
  EXPECT_STREQ(
      json_string_value(json_object_get(applied_body.get(), "bootstrap_token")),
      "secret-token");
}

#if defined(HAVE_LINUX_OS)
TEST(SdBootstrap, UploadsTapeDiscoveryTopology)
{
  ScopedDirectory config_root;
  ScopedDirectory working_directory;
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  FakeBootstrapServer server{
      BuildConfigBundleResponse(working_directory.path())};
  ScopedEnvironmentVariable config_root_override{
      "BAREOS_SD_BOOTSTRAP_CONFIG_ROOT", config_root.path().string()};
  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};
  std::optional<ScopedEnvironmentVariable> validate_binary_override;
  if (!std::getenv("BAREOS_SD_BOOTSTRAP_VALIDATE_BINARY")) {
    validate_binary_override.emplace("BAREOS_SD_BOOTSTRAP_VALIDATE_BINARY",
                                     BAREOS_SD_BINARY);
  }
  ScopedEnvironmentVariable poll_interval_override{
      "BAREOS_SD_BOOTSTRAP_POLL_INTERVAL_MS", "1"};
  ScopedEnvironmentVariable poll_attempts_override{
      "BAREOS_SD_BOOTSTRAP_MAX_POLL_ATTEMPTS", "10"};

  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/model",
                  "ULTRIUM-HH8 \n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vpd_pg80",
                  BuildPage80("TAPE123"));
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vpd_pg83",
                  BuildPage83Naa("\x11\x22\x33\x44"));
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_tape/nst0/device/scsi_generic/sg3");
  WriteBinaryFile(dev_root.path() / "nst0", "");
  WriteBinaryFile(dev_root.path() / "sg3", "");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vpd_pg80",
                  BuildPage80("CHANGER42"));
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vpd_pg83",
                  BuildPage83Naa("\xaa\xbb\xcc\xdd"));
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  const std::string command
      = std::string{QuoteShellArgument(BAREOS_SD_BINARY)} + " --discovery"
        + " --bootstrap-url " + QuoteShellArgument(server.base_url())
        + " --bootstrap-token " + QuoteShellArgument("secret-token")
        + " --bootstrap-session " + QuoteShellArgument("session-123");

  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), BEXIT_SUCCESS);

  json_error_t error{};
  auto discovery_body
      = MakeJson(json_loads(server.discovery_body().c_str(), 0, &error));
  ASSERT_NE(discovery_body.get(), nullptr);
  auto* report = json_object_get(discovery_body.get(), "report");
  ASSERT_TRUE(json_is_object(report));

  auto* tape_devices = json_object_get(report, "tape_devices");
  ASSERT_TRUE(json_is_array(tape_devices));
  ASSERT_EQ(json_array_size(tape_devices), 1U);
  auto* tape_device = json_array_get(tape_devices, 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "device_node")),
               (dev_root.path() / "nst0").string().c_str());
  EXPECT_STREQ(
      json_string_value(json_object_get(tape_device, "generic_device_node")),
      (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(
      json_string_value(json_object_get(tape_device, "device_identifier")),
      "naa.11223344");
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "serial")),
               "TAPE123");

  auto* changers = json_object_get(report, "changers");
  ASSERT_TRUE(json_is_array(changers));
  ASSERT_EQ(json_array_size(changers), 1U);
  auto* changer = json_array_get(changers, 0);
  ASSERT_NE(changer, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(changer, "device_node")),
               (dev_root.path() / "sg4").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(changer, "device_identifier")),
               "naa.aabbccdd");

  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               (dev_root.path() / "nst0").string().c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_EQ(json_integer_value(json_object_get(drive, "drive_element_address")),
            256);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "naa.11223344");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               (dev_root.path() / "nst0").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")), "TAPE123");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");
}
#endif

}  // namespace
