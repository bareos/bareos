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

#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "dird/dird_conf.h"
#include "dird/dird_globals.h"

namespace {

namespace fs = std::filesystem;

struct CommandResult {
  int exit_code{-1};
  std::string output;
};

CommandResult RunCommandWithEnvironment(
    const std::vector<std::string>& arguments,
    const std::vector<std::pair<std::string, std::string>>& environment);

bool Contains(std::string_view haystack, std::string_view needle)
{
  return haystack.find(needle) != std::string_view::npos;
}

fs::path PrepareConfig(const char* test_name)
{
  const fs::path source{BAREOS_DIR_TEST_CONFIG_DIR};
  const fs::path destination = fs::path(TEST_TEMP_DIR) / test_name;

  std::error_code ec;
  fs::remove_all(destination, ec);
  fs::create_directories(destination);
  fs::copy(source, destination,
           fs::copy_options::recursive | fs::copy_options::overwrite_existing);
  return destination;
}

void WriteTextFile(const fs::path& path, std::string_view content)
{
  fs::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  ASSERT_TRUE(output.is_open());
  output << content;
  ASSERT_TRUE(output.good());
}

CommandResult RunCommand(const std::vector<std::string>& arguments)
{
  return RunCommandWithEnvironment(arguments, {});
}

CommandResult RunCommandWithEnvironment(
    const std::vector<std::string>& arguments,
    const std::vector<std::pair<std::string, std::string>>& environment)
{
  std::array<int, 2> pipefd{};
  if (pipe(pipefd.data()) != 0) { return {}; }

  const pid_t pid = fork();
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

    for (const auto& [name, value] : environment) {
      setenv(name.c_str(), value.c_str(), 1);
    }

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

void MakeExecutable(const fs::path& path)
{
  const auto permissions = fs::perms::owner_read | fs::perms::owner_write
                           | fs::perms::owner_exec | fs::perms::group_read
                           | fs::perms::group_exec | fs::perms::others_read
                           | fs::perms::others_exec;
  fs::permissions(path, permissions, fs::perm_options::replace);
}

std::unique_ptr<ConfigurationParser> ParseDirectorConfig(const fs::path& config_root)
{
  auto parser = std::unique_ptr<ConfigurationParser>(
      directordaemon::InitDirConfig(config_root.c_str(), M_ERROR_TERM));
  EXPECT_NE(parser, nullptr);
  if (!parser) { return nullptr; }

  directordaemon::my_config = parser.get();
  EXPECT_TRUE(parser->ParseConfig());
  return parser;
}

TEST(DirSetupActivation, ActivatesStagedClientIntoLiveConfig)
{
  const auto config = PrepareConfig("dir-setup-activation-success");
  const auto staging_dir = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-staging";
  const auto stage_path
      = staging_dir / "bareos-dir.d" / "client" / "newclient-fd.conf";
  const auto live_path
      = config / "bareos-dir.d" / "client" / "newclient-fd.conf";

  std::error_code ec;
  fs::remove_all(staging_dir, ec);
  WriteTextFile(stage_path,
                "Client {\n"
                "  Name = newclient-fd\n"
                "  Address = newclient.example.com\n"
                "  Password = \"secret\"\n"
                "}\n");

  const auto result
      = RunCommand({BAREOS_DIR_ACTIVATE_BIN, "--config", config.string(),
                    "--staging-dir", staging_dir.string(), "--client",
                    "newclient-fd"});

  EXPECT_EQ(result.exit_code, 0) << result.output;
  EXPECT_TRUE(Contains(result.output, "Activated staged client \"newclient-fd\""))
      << result.output;
  EXPECT_FALSE(fs::exists(stage_path));
  EXPECT_TRUE(fs::exists(live_path));

  auto parser = ParseDirectorConfig(config);
  ASSERT_NE(parser, nullptr);
  EXPECT_NE(parser->GetResWithName(directordaemon::R_CLIENT, "newclient-fd"),
            nullptr);
  parser.reset();
  directordaemon::my_config = nullptr;
  directordaemon::me = nullptr;
}

TEST(DirSetupActivation, ListsAvailableStagedClientsWhenNoClientIsSpecified)
{
  const auto config = PrepareConfig("dir-setup-activation-list");
  const auto staging_dir = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-list-staging";

  std::error_code ec;
  fs::remove_all(staging_dir, ec);
  WriteTextFile(staging_dir / "bareos-dir.d" / "client" / "alpha-fd.conf",
                "Client {\n"
                "  Name = alpha-fd\n"
                "  Address = alpha.example.com\n"
                "  Password = \"secret\"\n"
                "}\n");
  WriteTextFile(staging_dir / "bareos-dir.d" / "client" / "zulu-fd.conf",
                "Client {\n"
                "  Name = zulu-fd\n"
                "  Address = zulu.example.com\n"
                "  Password = \"secret\"\n"
                "}\n");

  const auto result = RunCommand({BAREOS_DIR_ACTIVATE_BIN, "--config", config.string(),
                                  "--staging-dir", staging_dir.string()});

  EXPECT_EQ(result.exit_code, 0) << result.output;
  EXPECT_TRUE(Contains(result.output, "Staged clients available for activation:"))
      << result.output;
  EXPECT_TRUE(Contains(result.output, "  alpha-fd")) << result.output;
  EXPECT_TRUE(Contains(result.output, "  zulu-fd")) << result.output;
  EXPECT_TRUE(fs::exists(staging_dir / "bareos-dir.d" / "client" / "alpha-fd.conf"));
  EXPECT_TRUE(fs::exists(staging_dir / "bareos-dir.d" / "client" / "zulu-fd.conf"));
}

TEST(DirSetupActivation, ReportsMissingStagedClientsWhenListing)
{
  const auto config = PrepareConfig("dir-setup-activation-list-empty");
  const auto staging_dir
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-list-empty-staging";

  std::error_code ec;
  fs::remove_all(staging_dir, ec);

  const auto result = RunCommand({BAREOS_DIR_ACTIVATE_BIN, "--config", config.string(),
                                  "--staging-dir", staging_dir.string()});

  EXPECT_EQ(result.exit_code, 1);
  EXPECT_TRUE(Contains(result.output, "No staged client configs found"))
      << result.output;
}

TEST(DirSetupActivation, RejectsDuplicateLiveClient)
{
  const auto config = PrepareConfig("dir-setup-activation-duplicate");
  const auto staging_dir
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-duplicate-staging";
  const auto stage_path
      = staging_dir / "bareos-dir.d" / "client" / "bareos-fd.conf";

  std::error_code ec;
  fs::remove_all(staging_dir, ec);
  WriteTextFile(stage_path,
                "Client {\n"
                "  Name = bareos-fd\n"
                "  Address = another-host.example.com\n"
                "  Password = \"secret\"\n"
                "}\n");

  const auto result
      = RunCommand({BAREOS_DIR_ACTIVATE_BIN, "--config", config.string(),
                    "--staging-dir", staging_dir.string(), "--client",
                    "bareos-fd"});

  EXPECT_EQ(result.exit_code, 1);
  EXPECT_TRUE(Contains(result.output, "already exists")) << result.output;
  EXPECT_TRUE(fs::exists(stage_path));
}

TEST(DirSetupActivation, RejectsInvalidStagedConfig)
{
  const auto config = PrepareConfig("dir-setup-activation-invalid");
  const auto staging_dir
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-invalid-staging";
  const auto stage_path
      = staging_dir / "bareos-dir.d" / "client" / "broken-fd.conf";
  const auto live_path = config / "bareos-dir.d" / "client" / "broken-fd.conf";

  std::error_code ec;
  fs::remove_all(staging_dir, ec);
  WriteTextFile(stage_path,
                "Client {\n"
                "  Name = broken-fd\n"
                "  Address = broken.example.com\n");

  const auto result
      = RunCommand({BAREOS_DIR_ACTIVATE_BIN, "--config", config.string(),
                    "--staging-dir", staging_dir.string(), "--client",
                    "broken-fd"});

  EXPECT_EQ(result.exit_code, 1);
  EXPECT_TRUE(Contains(result.output, "is not valid Director config"))
      << result.output;
  EXPECT_TRUE(fs::exists(stage_path));
  EXPECT_FALSE(fs::exists(live_path));
}

TEST(DirSetupActivation, RejectsMismatchedStagedClientName)
{
  const auto config = PrepareConfig("dir-setup-activation-name-mismatch");
  const auto staging_dir
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-name-mismatch-staging";
  const auto stage_path
      = staging_dir / "bareos-dir.d" / "client" / "expected-fd.conf";
  const auto live_path
      = config / "bareos-dir.d" / "client" / "expected-fd.conf";

  std::error_code ec;
  fs::remove_all(staging_dir, ec);
  WriteTextFile(stage_path,
                "Client {\n"
                "  Name = different-fd\n"
                "  Address = mismatch.example.com\n"
                "  Password = \"secret\"\n"
                "}\n");

  const auto result
      = RunCommand({BAREOS_DIR_ACTIVATE_BIN, "--config", config.string(),
                    "--staging-dir", staging_dir.string(), "--client",
                    "expected-fd"});

  EXPECT_EQ(result.exit_code, 1);
  EXPECT_TRUE(Contains(result.output, "did not define Client \"expected-fd\""))
      << result.output;
  EXPECT_TRUE(fs::exists(stage_path));
  EXPECT_FALSE(fs::exists(live_path));
}

TEST(DirSetupActivation, ReloadsDirectorViaSystemdWhenRequested)
{
  const auto config = PrepareConfig("dir-setup-activation-reload");
  const auto staging_dir
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-reload-staging";
  const auto fake_bin
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-reload-bin";
  const auto log_path
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-reload.log";
  const auto stage_path
      = staging_dir / "bareos-dir.d" / "client" / "reload-fd.conf";

  std::error_code ec;
  fs::remove_all(staging_dir, ec);
  fs::remove_all(fake_bin, ec);
  fs::remove(log_path, ec);
  WriteTextFile(stage_path,
                "Client {\n"
                "  Name = reload-fd\n"
                "  Address = reload.example.com\n"
                "  Password = \"secret\"\n"
                "}\n");

  const auto script_path = fake_bin / "systemctl";
  WriteTextFile(script_path,
                "#!/bin/sh\n"
                "printf '%s %s\\n' \"$1\" \"$2\" > \"" TEST_TEMP_DIR
                "/dir-setup-activation-reload.log\"\n");
  MakeExecutable(script_path);

  const auto inherited_path = getenv("PATH");
  ASSERT_NE(inherited_path, nullptr);
  const auto path_value = fake_bin.string() + ":" + inherited_path;
  const auto result = RunCommandWithEnvironment(
      {BAREOS_DIR_ACTIVATE_BIN, "--config", config.string(), "--staging-dir",
       staging_dir.string(), "--client", "reload-fd", "--reload"},
      {{"PATH", path_value}});

  EXPECT_EQ(result.exit_code, 0) << result.output;
  EXPECT_TRUE(Contains(result.output, "Reloaded Director via systemd unit"))
      << result.output;

  std::ifstream input(log_path);
  ASSERT_TRUE(input.is_open());
  std::string log_line;
  std::getline(input, log_line);
  EXPECT_EQ(log_line, "reload bareos-dir");
}

TEST(DirSetupActivation, ReportsReloadFailureAfterActivation)
{
  const auto config = PrepareConfig("dir-setup-activation-reload-failure");
  const auto staging_dir
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-reload-failure-staging";
  const auto fake_bin
      = fs::path(TEST_TEMP_DIR) / "dir-setup-activation-reload-failure-bin";
  const auto stage_path
      = staging_dir / "bareos-dir.d" / "client" / "reload-failure-fd.conf";
  const auto live_path
      = config / "bareos-dir.d" / "client" / "reload-failure-fd.conf";

  std::error_code ec;
  fs::remove_all(staging_dir, ec);
  fs::remove_all(fake_bin, ec);
  WriteTextFile(stage_path,
                "Client {\n"
                "  Name = reload-failure-fd\n"
                "  Address = reload-failure.example.com\n"
                "  Password = \"secret\"\n"
                "}\n");

  const auto script_path = fake_bin / "systemctl";
  WriteTextFile(script_path,
                "#!/bin/sh\n"
                "echo 'reload failed' >&2\n"
                "exit 23\n");
  MakeExecutable(script_path);

  const auto inherited_path = getenv("PATH");
  ASSERT_NE(inherited_path, nullptr);
  const auto path_value = fake_bin.string() + ":" + inherited_path;
  const auto result = RunCommandWithEnvironment(
      {BAREOS_DIR_ACTIVATE_BIN, "--config", config.string(), "--staging-dir",
       staging_dir.string(), "--client", "reload-failure-fd", "--reload"},
      {{"PATH", path_value}});

  EXPECT_EQ(result.exit_code, 1);
  EXPECT_TRUE(Contains(result.output, "activated on disk, but reloading the Director"))
      << result.output;
  EXPECT_TRUE(Contains(result.output, "reload failed")) << result.output;
  EXPECT_TRUE(fs::exists(live_path));
  EXPECT_FALSE(fs::exists(stage_path));
}

}  // namespace
