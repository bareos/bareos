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

#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "filed/filed_conf.h"
#include "filed/filed_globals.h"
#include "filed/filed_utils.h"
#include "lib/parse_conf.h"

namespace {

namespace fs = std::filesystem;

struct CommandResult {
  int exit_code{-1};
  std::string output;
};

struct AsyncProcess {
  pid_t pid{-1};
  int pipe_fd{-1};
};

bool Contains(std::string_view haystack, std::string_view needle)
{
  return haystack.find(needle) != std::string_view::npos;
}

fs::path PrepareConfig(const char* test_name)
{
  const fs::path source{BAREOS_FD_TEST_CONFIG_DIR};
  const fs::path destination = fs::path(TEST_TEMP_DIR) / test_name;

  std::error_code ec;
  fs::remove_all(destination, ec);
  fs::create_directories(destination);
  fs::copy(source, destination,
           fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  fs::remove(destination / "bareos-fd.d" / "director" / "bareos-dir.conf", ec);
  return destination;
}

void ReplaceFileContent(const fs::path& path,
                        std::string_view needle,
                        std::string_view replacement)
{
  std::ifstream input(path);
  ASSERT_TRUE(input.is_open());
  std::string content((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());
  input.close();

  const auto position = content.find(needle);
  ASSERT_NE(position, std::string::npos);
  content.replace(position, needle.size(), replacement);

  std::ofstream output(path);
  ASSERT_TRUE(output.is_open());
  output << content;
}

CommandResult RunCommand(const std::vector<std::string>& arguments)
{
  std::array<int, 2> pipefd{};
  if (pipe(pipefd.data()) != 0) { return {}; }

  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return {};
  }

  if (pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    std::vector<char*> argv;
    for (const auto& argument : arguments) {
      argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    execv(argv.front(), argv.data());
    _exit(127);
  }

  close(pipefd[1]);

  std::string output;
  char buffer[4096];
  ssize_t bytes_read = 0;
  while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
    output.append(buffer, bytes_read);
  }
  close(pipefd[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  CommandResult result;
  result.output = std::move(output);
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  }
  return result;
}

std::string ReadLineFromFd(int fd)
{
  std::string line;
  for (;;) {
    char ch = '\0';
    const ssize_t bytes_read = read(fd, &ch, 1);
    if (bytes_read <= 0) { return line; }
    if (ch == '\n') { return line; }
    line.push_back(ch);
  }
}

uint16_t ReserveLoopbackPort()
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) { throw std::runtime_error("Failed to reserve a loopback port."); }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;

  if (bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    close(fd);
    throw std::runtime_error("Failed to bind a temporary loopback socket.");
  }

  socklen_t address_length = sizeof(address);
  if (getsockname(fd, reinterpret_cast<sockaddr*>(&address), &address_length)
      != 0) {
    close(fd);
    throw std::runtime_error("Failed to inspect the temporary loopback socket.");
  }
  close(fd);
  return ntohs(address.sin_port);
}

AsyncProcess StartAsyncProcess(const std::vector<std::string>& arguments)
{
  std::array<int, 2> pipefd{};
  if (pipe(pipefd.data()) != 0) {
    throw std::runtime_error("pipe() failed for the async process.");
  }

  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    throw std::runtime_error("fork() failed for the async process.");
  }

  if (pid == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    dup2(pipefd[1], STDERR_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);

    std::vector<char*> argv;
    for (const auto& argument : arguments) {
      argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    execv(argv.front(), argv.data());
    _exit(127);
  }

  close(pipefd[1]);
  return {.pid = pid, .pipe_fd = pipefd[0]};
}

CommandResult WaitProcess(AsyncProcess& process)
{
  std::string output;
  char buffer[4096];
  ssize_t bytes_read = 0;
  while ((bytes_read = read(process.pipe_fd, buffer, sizeof(buffer))) > 0) {
    output.append(buffer, bytes_read);
  }
  close(process.pipe_fd);
  process.pipe_fd = -1;

  int status = 0;
  waitpid(process.pid, &status, 0);
  process.pid = -1;

  CommandResult result;
  result.output = std::move(output);
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exit_code = 128 + WTERMSIG(status);
  }
  return result;
}

class SetupServiceProcess {
 public:
  SetupServiceProcess(const fs::path& staging_dir,
                      const char* cert_path,
                      const char* key_path,
                      const char* director_name,
                      bool force,
                      const char* token)
  {
    std::array<int, 2> pipefd{};
    if (pipe(pipefd.data()) != 0) {
      throw std::runtime_error("pipe() failed for the setup service process.");
    }

    pid_ = fork();
    if (pid_ == -1) {
      close(pipefd[0]);
      close(pipefd[1]);
      throw std::runtime_error("fork() failed for the setup service process.");
    }

    if (pid_ == 0) {
      dup2(pipefd[1], STDOUT_FILENO);
      dup2(pipefd[1], STDERR_FILENO);
      close(pipefd[0]);
      close(pipefd[1]);

      std::vector<std::string> arguments{
          PYTHON_EXECUTABLE,        BAREOS_DIR_SETUP_SERVICE,
          "--listen-address",       "127.0.0.1",
          "--port",                 "0",
          "--certificate",          cert_path,
          "--key",                  key_path,
          "--director-name",        director_name,
          "--director-address",     "director.example.com",
          "--director-port",        "9101",
          "--staging-dir",          staging_dir.string(),
          "--token",                token};
      if (force) { arguments.emplace_back("--force"); }

      std::vector<char*> argv;
      for (auto& argument : arguments) {
        argv.push_back(argument.data());
      }
      argv.push_back(nullptr);
      execv(argv.front(), argv.data());
      _exit(127);
    }

    close(pipefd[1]);
    pipe_fd_ = pipefd[0];

    const auto line = ReadLineFromFd(pipe_fd_);
    if (!Contains(line, "LISTENING ")) {
      throw std::runtime_error("The setup service did not report a listening port: "
                               + line);
    }
    port_ = static_cast<uint16_t>(std::stoi(line.substr(std::string("LISTENING ").size())));
    if (port_ == 0) {
      throw std::runtime_error("The setup service reported an invalid port.");
    }
  }

  ~SetupServiceProcess()
  {
    if (pipe_fd_ >= 0) { close(pipe_fd_); }
    if (pid_ > 0) {
      kill(pid_, SIGTERM);
      for (int i = 0; i < 20; ++i) {
        int status = 0;
        const auto wait_result = waitpid(pid_, &status, WNOHANG);
        if (wait_result == pid_) {
          pid_ = -1;
          return;
        }
        usleep(100000);
      }
      kill(pid_, SIGKILL);
      waitpid(pid_, nullptr, 0);
    }
  }

  uint16_t port() const { return port_; }

 private:
  pid_t pid_{-1};
  int pipe_fd_{-1};
  uint16_t port_{0};
};

void ExpectConfigParses(const fs::path& config_root)
{
  filedaemon::my_config = filedaemon::InitFdConfig(config_root.c_str(), M_ERROR);
  ASSERT_NE(filedaemon::my_config, nullptr);
  ASSERT_TRUE(filedaemon::my_config->ParseConfig());
  EXPECT_TRUE(filedaemon::CheckResources());
  delete filedaemon::my_config;
  filedaemon::my_config = nullptr;
  filedaemon::me = nullptr;
}

TEST(FdSetup, TrustOnFirstUseWritesConfigAndAllowsRepeat)
{
  const auto config = PrepareConfig("fd_setup_success");
  const auto staging_dir = config / "staging";
  const auto trust_file = config / "trusted-setup.sha256";

  SetupServiceProcess service(staging_dir, TEST_SETUP_CERT, TEST_SETUP_KEY,
                              "setup-dir", true, "OneTimeToken");

  const std::vector<std::string> first_run{
      BAREOS_FD_SETUP_BIN,  "--config",         config.string(),
      "--address",          "127.0.0.1",        "--port",
      std::to_string(service.port()),           "--token",
      "OneTimeToken",       "--advertise-address",
      "client.example.com", "--trust-file",     trust_file.string(),
      "--trust-on-first-use"};
  const auto first_result = RunCommand(first_run);
  ASSERT_EQ(first_result.exit_code, 0) << first_result.output;
  EXPECT_TRUE(Contains(first_result.output, "Created setup resource"));
  EXPECT_TRUE(Contains(first_result.output, "Director-side staged config:"));

  const auto generated_fd_config
      = config / "bareos-fd.d" / "director" / "setup-dir.conf";
  ASSERT_TRUE(fs::exists(generated_fd_config));
  ASSERT_TRUE(fs::exists(trust_file));

  const auto staged_client_config
      = staging_dir / "bareos-dir.d" / "client" / "backup-bareos-test-fd.conf";
  ASSERT_TRUE(fs::exists(staged_client_config));

  std::ifstream staged_input(staged_client_config);
  ASSERT_TRUE(staged_input.is_open());
  std::string staged_content((std::istreambuf_iterator<char>(staged_input)),
                             std::istreambuf_iterator<char>());
  EXPECT_TRUE(Contains(staged_content, "Address = client.example.com"));
  EXPECT_TRUE(Contains(staged_content, "Port = 42002"));

  const std::vector<std::string> second_run{
      BAREOS_FD_SETUP_BIN, "--config",     config.string(), "--address",
      "127.0.0.1",         "--port",       std::to_string(service.port()),
      "--token",           "OneTimeToken", "--trust-file",
      trust_file.string(), "--force"};
  const auto second_result = RunCommand(second_run);
  ASSERT_EQ(second_result.exit_code, 0) << second_result.output;

  ExpectConfigParses(config);
}

TEST(FdSetup, FingerprintMismatchIsRejected)
{
  const auto config = PrepareConfig("fd_setup_fingerprint_mismatch");
  const auto staging_dir = config / "staging";
  const auto trust_file = config / "trusted-setup.sha256";

  {
    std::ofstream trust_output(trust_file);
    ASSERT_TRUE(trust_output.is_open());
    trust_output << TEST_SETUP_FINGERPRINT << "\n";
  }

  SetupServiceProcess service(staging_dir, TEST_SETUP_ALT_CERT, TEST_SETUP_ALT_KEY,
                              "setup-dir", true, "OneTimeToken");

  const auto result = RunCommand(
      {BAREOS_FD_SETUP_BIN, "--config", config.string(), "--address",
       "127.0.0.1", "--port", std::to_string(service.port()), "--token",
       "OneTimeToken", "--trust-file", trust_file.string(), "--force"});

  ASSERT_NE(result.exit_code, 0);
  EXPECT_TRUE(Contains(result.output, "TLS fingerprint mismatch"));
}

TEST(FdSetup, PathLikeClientNameIsRejected)
{
  const auto config = PrepareConfig("fd_setup_invalid_client_name");
  ReplaceFileContent(config / "bareos-fd.d" / "client" / "myself.conf",
                     "Name = backup-bareos-test-fd", "Name = bad/client");

  const auto staging_dir = config / "staging";
  SetupServiceProcess service(staging_dir, TEST_SETUP_CERT, TEST_SETUP_KEY,
                              "setup-dir", true, "OneTimeToken");

  const auto result = RunCommand(
      {BAREOS_FD_SETUP_BIN, "--config", config.string(), "--address",
       "127.0.0.1", "--port", std::to_string(service.port()), "--token",
       "OneTimeToken", "--trust-on-first-use"});

  ASSERT_NE(result.exit_code, 0);
  EXPECT_TRUE(Contains(result.output, "client_name"));
  EXPECT_TRUE(Contains(result.output, "path separators"));
}

TEST(FdSetup, DirectorConnectsWritesConfigWithPinnedFingerprint)
{
  const auto config = PrepareConfig("fd_setup_director_connects");
  const auto staging_dir = config / "staging";
  const auto trust_file = config / "trusted-setup-reverse.sha256";
  const auto listen_port = ReserveLoopbackPort();

  auto service = StartAsyncProcess(
      {PYTHON_EXECUTABLE,
       BAREOS_DIR_SETUP_SERVICE,
       "--connection-direction",
       "director-connects",
       "--connect-back-address",
       "127.0.0.1",
       "--connect-back-port",
       std::to_string(listen_port),
       "--connect-timeout",
       "10",
       "--certificate",
       TEST_SETUP_CERT,
       "--key",
       TEST_SETUP_KEY,
       "--director-name",
       "reverse-dir",
       "--director-address",
       "director.example.com",
       "--director-port",
       "9101",
       "--staging-dir",
       staging_dir.string(),
       "--token",
       "OneTimeToken",
       "--force"});

  const auto client_result = RunCommand(
      {BAREOS_FD_SETUP_BIN,
       "--config",
       config.string(),
       "--connection-direction",
       "director-connects",
       "--address",
       "127.0.0.1",
       "--port",
       "10443",
       "--listen-address",
       "127.0.0.1",
       "--listen-port",
       std::to_string(listen_port),
       "--connect-timeout",
       "10",
       "--token",
       "OneTimeToken",
       "--advertise-address",
       "reverse.example.com",
       "--trust-file",
       trust_file.string(),
       "--expected-fingerprint",
       TEST_SETUP_FINGERPRINT,
       "--force"});
  const auto service_result = WaitProcess(service);

  ASSERT_EQ(service_result.exit_code, 0) << service_result.output;
  ASSERT_EQ(client_result.exit_code, 0) << client_result.output;

  const auto generated_fd_config
      = config / "bareos-fd.d" / "director" / "reverse-dir.conf";
  ASSERT_TRUE(fs::exists(generated_fd_config));
  ASSERT_TRUE(fs::exists(trust_file));

  const auto staged_client_config
      = staging_dir / "bareos-dir.d" / "client" / "backup-bareos-test-fd.conf";
  ASSERT_TRUE(fs::exists(staged_client_config));

  std::ifstream staged_input(staged_client_config);
  ASSERT_TRUE(staged_input.is_open());
  std::string staged_content((std::istreambuf_iterator<char>(staged_input)),
                             std::istreambuf_iterator<char>());
  EXPECT_TRUE(Contains(staged_content, "Address = reverse.example.com"));

  ExpectConfigParses(config);
}

TEST(FdSetup, DirectorConnectsRejectsTrustOnFirstUse)
{
  const auto config = PrepareConfig("fd_setup_reverse_tofu_rejected");

  const auto result = RunCommand(
      {BAREOS_FD_SETUP_BIN,
       "--config",
       config.string(),
       "--connection-direction",
       "director-connects",
       "--address",
       "127.0.0.1",
       "--port",
       "10443",
       "--listen-address",
       "127.0.0.1",
       "--listen-port",
       std::to_string(ReserveLoopbackPort()),
       "--token",
       "OneTimeToken",
       "--trust-on-first-use"});

  ASSERT_NE(result.exit_code, 0);
  EXPECT_TRUE(Contains(result.output, "does not allow --trust-on-first-use"));
}

}  // namespace
