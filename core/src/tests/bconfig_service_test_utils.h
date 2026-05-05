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

#ifndef BAREOS_TESTS_BCONFIG_SERVICE_TEST_UTILS_H_
#define BAREOS_TESTS_BCONFIG_SERVICE_TEST_UTILS_H_

#include "tools/bconfig_service.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <jansson.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>

#if HAVE_WIN32
#  include <process.h>
#else
#  include <csignal>
#  include <fcntl.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace bconfig::service {
namespace {

namespace net = boost::asio;
namespace http = boost::beast::http;
using tcp = net::ip::tcp;

[[maybe_unused]] std::filesystem::path MakeTempPath()
{
  static std::atomic<uint64_t> counter{0};
  const auto unique_suffix = std::to_string(static_cast<unsigned long long>(
      std::chrono::steady_clock::now().time_since_epoch().count()));
#if HAVE_WIN32
  const auto process_id = std::to_string(static_cast<unsigned>(_getpid()));
#else
  const auto process_id = std::to_string(static_cast<unsigned>(getpid()));
#endif
  return std::filesystem::temp_directory_path()
         / ("bconfig-service-test-" + process_id + "-"
            + std::to_string(++counter) + "-" + unique_suffix);
}

class [[maybe_unused]] ScopedDirectory {
 public:
  explicit ScopedDirectory(std::filesystem::path path) : path_{std::move(path)}
  {
  }

  ~ScopedDirectory() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

class [[maybe_unused]] ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const char* name, const std::string& value)
      : name_{name}
  {
    if (const char* current = std::getenv(name_)) { previous_value_ = current; }
    had_previous_value_ = previous_value_.has_value();
    Set(value);
  }

  ~ScopedEnvironmentVariable()
  {
    if (had_previous_value_) {
      Set(*previous_value_);
    } else {
#if HAVE_WIN32
      _putenv_s(name_, "");
#else
      unsetenv(name_);
#endif
    }
  }

 private:
  void Set(const std::string& value)
  {
#if HAVE_WIN32
    _putenv_s(name_, value.c_str());
#else
    setenv(name_, value.c_str(), 1);
#endif
  }

  const char* name_;
  std::optional<std::string> previous_value_{};
  bool had_previous_value_{false};
};

[[maybe_unused]] std::filesystem::path FindFixtureRoot()
{
  auto cursor = std::filesystem::current_path();
  for (;;) {
    const auto build_tree_path = cursor / "configs/bareos-configparser-tests";
    if (std::filesystem::is_directory(build_tree_path)) {
      return build_tree_path;
    }

    const auto source_tree_path
        = cursor / "core/src/tests/configs/bareos-configparser-tests";
    if (std::filesystem::is_directory(source_tree_path)) {
      return source_tree_path;
    }

    if (cursor == cursor.root_path()) { break; }
    cursor = cursor.parent_path();
  }

  return {};
}

[[maybe_unused]] std::string ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream file{path};
  EXPECT_TRUE(file.good());
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

[[maybe_unused]] void WriteTextFile(const std::filesystem::path& path,
                                    std::string_view content)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file{path};
  ASSERT_TRUE(file.good());
  file << content;
  ASSERT_TRUE(file.good());
}

struct [[maybe_unused]] HttpResponse {
  unsigned status_code{};
  std::string body{};
};

using JsonHandle = std::unique_ptr<json_t, decltype(&json_decref)>;

[[maybe_unused]] JsonHandle ParseJson(std::string_view body)
{
  json_error_t error{};
  return JsonHandle{json_loadb(body.data(), body.size(), 0, &error),
                    &json_decref};
}

[[maybe_unused]] uint16_t FindUnusedTcpPort()
{
  net::io_context io_context;
  tcp::acceptor acceptor{io_context, {net::ip::make_address("127.0.0.1"), 0}};
  return acceptor.local_endpoint().port();
}

#if !HAVE_WIN32
[[maybe_unused]] std::filesystem::path FindBuiltBconfigServiceBinary()
{
  const auto self = std::filesystem::canonical("/proc/self/exe");
  return self.parent_path().parent_path() / "tools" / "bconfig-service";
}

class [[maybe_unused]] ScopedBconfigServiceServer {
 public:
  explicit ScopedBconfigServiceServer(const std::filesystem::path& state_dir)
      : port_{FindUnusedTcpPort()}
  {
    const auto server_path = FindBuiltBconfigServiceBinary();
    if (!std::filesystem::exists(server_path)) {
      startup_error_
          = "bconfig-service executable not found at " + server_path.string();
      return;
    }

    child_pid_ = fork();
    if (child_pid_ < 0) {
      startup_error_ = "failed to fork bconfig-service test server";
      return;
    }

    if (child_pid_ == 0) {
      const auto null_fd = open("/dev/null", O_WRONLY);
      if (null_fd >= 0) {
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
        close(null_fd);
      }

      const auto port = std::to_string(port_);
      const auto state_dir_string = state_dir.string();
      execl(server_path.c_str(), server_path.c_str(), "--address", "127.0.0.1",
            "--port", port.c_str(), "--state-dir", state_dir_string.c_str(),
            static_cast<char*>(nullptr));
      _exit(127);
    }

    ready_ = WaitUntilReady();
    if (!ready_ && startup_error_.empty()) {
      startup_error_ = "bconfig-service test server did not become ready";
    }
  }

  ~ScopedBconfigServiceServer()
  {
    if (child_pid_ <= 0) { return; }

    kill(child_pid_, SIGTERM);

    int status = 0;
    for (int attempt = 0; attempt < 20; ++attempt) {
      const auto waited = waitpid(child_pid_, &status, WNOHANG);
      if (waited == child_pid_) { return; }
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    kill(child_pid_, SIGKILL);
    waitpid(child_pid_, &status, 0);
  }

  bool ready() const { return ready_; }
  const std::string& startup_error() const { return startup_error_; }

  HttpResponse Get(std::string_view target) const
  {
    net::io_context io_context;
    tcp::resolver resolver{io_context};
    tcp::socket socket{io_context};
    const auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port_));
    net::connect(socket, endpoints);

    http::request<http::empty_body> request{http::verb::get,
                                            std::string{target}, 11};
    request.set(http::field::host, "127.0.0.1");
    request.set(http::field::user_agent, "bconfig-service-test");
    http::write(socket, request);

    boost::beast::flat_buffer buffer;
    http::response<http::string_body> response;
    http::read(socket, buffer, response);

    boost::system::error_code error_code;
    socket.shutdown(tcp::socket::shutdown_both, error_code);
    return {.status_code = response.result_int(), .body = response.body()};
  }

 private:
  bool WaitUntilReady()
  {
    for (int attempt = 0; attempt < 100; ++attempt) {
      int status = 0;
      const auto waited = waitpid(child_pid_, &status, WNOHANG);
      if (waited == child_pid_) {
        startup_error_ = "bconfig-service exited during startup";
        return false;
      }

      try {
        const auto response = Get("/v1/health");
        if (response.status_code == 200) { return true; }
      } catch (...) {
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    return false;
  }

  pid_t child_pid_{-1};
  uint16_t port_{0};
  bool ready_{false};
  std::string startup_error_{};
};
#endif

[[maybe_unused]] void AddCounterFixtures(
    const std::filesystem::path& source_root)
{
  WriteTextFile(source_root / "bareos-dir.d/counter/WrapSeed.conf",
                "Counter {\n"
                "  Name = \"WrapSeed\"\n"
                "  Minimum = 1\n"
                "  Maximum = 5\n"
                "}\n");
  WriteTextFile(source_root / "bareos-dir.d/counter/ExistingCounter.conf",
                "Counter {\n"
                "  Name = \"ExistingCounter\"\n"
                "  Description = \"Existing counter\"\n"
                "  Minimum = 7\n"
                "  Maximum = 99\n"
                "  WrapCounter = WrapSeed\n"
                "  Catalog = MyCatalog\n"
                "}\n");
}

[[maybe_unused]] void AddConsoleImportFixture(
    const std::filesystem::path& source_root)
{
  WriteTextFile(source_root / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "}\n");
}

[[maybe_unused]] std::optional<JobRecord> WaitForJobTerminal(
    ServiceState& state,
    std::string_view id)
{
  for (int attempt = 0; attempt < 400; ++attempt) {
    auto job = state.GetJob(id);
    if (job
        && (job->status == JobStatus::kSucceeded
            || job->status == JobStatus::kFailed)) {
      return job;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  return state.GetJob(id);
}

}  // namespace
}  // namespace bconfig::service

#endif  // BAREOS_TESTS_BCONFIG_SERVICE_TEST_UTILS_H_
