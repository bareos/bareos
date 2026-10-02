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
#include "setup_steps.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "command_runner.h"
#include "os_detector.h"
#include "setup_session.h"
#include "tui_wizard.h"

TEST(BareosSetupStepsShared, ValidatesCustomRepositoryUrls)
{
  EXPECT_TRUE(IsValidSetupRepositoryUrl("https://ci.example:8443/pr/42/EL_9"));
  EXPECT_TRUE(
      IsValidSetupRepositoryUrl("http://ci.example/build/xUbuntu_24.04"));
  for (const auto* url :
       {"", "file:///tmp/repo", "https://",
        "https://user:password@example/repo",
        "https://example/repo?token=secret", "https://example/repo\nURL=bad",
        "https://example/$(touch_bad)", "https://example/repo';exit 0"}) {
    EXPECT_FALSE(IsValidSetupRepositoryUrl(url)) << url;
  }
}

TEST(BareosSetupStepsShared, RewritesOnlyRepositoryUrlAssignment)
{
  const std::string script
      = "#!/bin/sh\nDOWNLOADSERVER=\"download.bareos.org\"\n"
        "URL=\"https://download.bareos.org/current/EL_9\"\n"
        "echo \"$URL\"\n";
  EXPECT_EQ(
      RewriteSetupRepositoryScript(script, "http://ci.example/pr/42/EL_9"),
      "#!/bin/sh\nDOWNLOADSERVER=\"download.bareos.org\"\n"
      "URL='http://ci.example/pr/42/EL_9'\necho \"$URL\"\n");
  EXPECT_THROW(
      RewriteSetupRepositoryScript("echo no_url\n", "https://ci.example/EL_9"),
      std::runtime_error);
  EXPECT_THROW(RewriteSetupRepositoryScript(script, "file:///tmp/repo"),
               std::invalid_argument);
}

TEST(BareosSetupStepsShared, BuildsDefaultPackageListForDnf)
{
  EXPECT_EQ(BuildDefaultPackageList(PackageManager::Dnf),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "policycoreutils", "mod_ssl",
                "postgresql-server"}));
}

TEST(BareosSetupStepsShared, BuildsDefaultPackageListForApt)
{
  EXPECT_EQ(BuildDefaultPackageList(PackageManager::Apt),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "policycoreutils", "postgresql"}));
}

TEST(BareosSetupStepsShared, BuildsDefaultPackageListForZypper)
{
  EXPECT_EQ(BuildDefaultPackageList(PackageManager::Zypper),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "policycoreutils", "postgresql-server"}));
}

TEST(BareosSetupStepsShared, BuildsPackageListWithoutPostgresServer)
{
  EXPECT_EQ(BuildPackageListWithoutPostgresServer(PackageManager::Apt),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "policycoreutils"}));
  EXPECT_EQ(BuildPackageListWithoutPostgresServer(PackageManager::Dnf),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "policycoreutils", "mod_ssl"}));
  EXPECT_EQ(BuildPackageListWithoutPostgresServer(PackageManager::Zypper),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "policycoreutils"}));
}

TEST(BareosSetupStepsShared, BuildsPackageListWithoutTapeStorage)
{
  EXPECT_EQ(BuildPackageListWithoutTapeStorage(PackageManager::Zypper),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-dedupable", "bareos-database-tools",
                "bareos-tools", "bareos-webui-new", "bareos-webui-proxy",
                "policycoreutils", "postgresql-server"}));
  EXPECT_EQ(BuildPackageListWithoutTapeStorage(PackageManager::Dnf),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-dedupable", "bareos-database-tools",
                "bareos-tools", "bareos-webui-new", "bareos-webui-proxy",
                "policycoreutils", "mod_ssl", "postgresql-server"}));
}

TEST(BareosSetupStepsShared, BuildsCatalogInitScriptsOnlyWhenNeeded)
{
  EXPECT_TRUE(BuildCatalogInitScripts(PackageManager::Apt).empty());
  EXPECT_EQ(BuildCatalogInitScripts(PackageManager::Dnf),
            (std::vector<std::string>{
                "/usr/lib/bareos/scripts/create_bareos_database",
                "/usr/lib/bareos/scripts/make_bareos_tables",
                "/usr/lib/bareos/scripts/grant_bareos_privileges"}));
  EXPECT_EQ(BuildCatalogInitScripts(PackageManager::Yum),
            BuildCatalogInitScripts(PackageManager::Dnf));
  EXPECT_EQ(BuildCatalogInitScripts(PackageManager::Zypper),
            BuildCatalogInitScripts(PackageManager::Dnf));
}

namespace {

// Prepends a temp directory of no-op shim executables (named after the
// given tools) to PATH, so RunStep()'s real orchestration logic (which
// commands to run, and in what order) can be exercised without touching
// the real system. Each shim just appends its own invocation to a shared
// log file and exits 0. The "sudo" shim is special: it passes through to
// its argv so tests behave the same whether they run as root or not.
class FakeToolPath {
 public:
  explicit FakeToolPath(const std::vector<std::string>& tools,
                        bool include_system_path = true)
  {
    std::string pattern
        = (std::filesystem::temp_directory_path() / "bareos-setup-test-XXXXXX")
              .string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    if (mkdtemp(buffer.data()) == nullptr) {
      throw std::runtime_error("mkdtemp failed");
    }
    dir_ = buffer.data();
    log_path_ = dir_ / "log.txt";
    for (const auto& tool : tools) {
      const std::filesystem::path shim = dir_ / tool;
      std::ofstream out(shim);
      if (tool == "sudo") {
        out << "#!/bin/sh\nexec \"$@\"\n";
      } else {
        out << "#!/bin/sh\necho \"" << tool << " $*\" >> '"
            << log_path_.string() << "'\nexit 0\n";
      }
      out.close();
      std::filesystem::permissions(shim,
                                   std::filesystem::perms::owner_all
                                       | std::filesystem::perms::group_read
                                       | std::filesystem::perms::group_exec
                                       | std::filesystem::perms::others_read
                                       | std::filesystem::perms::others_exec);
    }
    const char* current = getenv("PATH");
    old_path_ = current != nullptr ? current : "";
    const std::string path
        = include_system_path ? dir_.string() + ":" + old_path_ : dir_.string();
    setenv("PATH", path.c_str(), 1);
  }

  ~FakeToolPath()
  {
    setenv("PATH", old_path_.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  FakeToolPath(const FakeToolPath&) = delete;
  FakeToolPath& operator=(const FakeToolPath&) = delete;

  void SetToolScript(const std::string& tool, const std::string& script)
  {
    std::ofstream out(dir_ / tool);
    out << "#!/bin/sh\n" << script;
    if (!out) throw std::runtime_error("Writing fake tool failed");
  }

  std::vector<std::string> LoggedCommands() const
  {
    std::vector<std::string> lines;
    std::ifstream in(log_path_);
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
  }

 private:
  std::filesystem::path dir_;
  std::filesystem::path log_path_;
  std::string old_path_;
};

}  // namespace

TEST(BareosSetupUnattended, RejectsInvalidOptionsBeforeExecutingCommands)
{
  FakeToolPath tools({"curl", "bash", "apt-get", "systemctl", "sudo"});
  SetupContext context;
  UnattendedSetupOptions options;
  EXPECT_EQ(RunUnattendedSetup(context, options), 1);
  options.repository_urls
      = {"https://ci.example/Debian_12/", "https://ci.example/Debian_13"};
  EXPECT_EQ(RunUnattendedSetup(context, options), 1);
  options.repository_urls = {"https://ci.example/Debian_12/"};
  EXPECT_EQ(RunUnattendedSetup(context, options), 1);
  options.repository_urls = {"https://ci.example/Debian_12"};
  options.extra_packages = {"--allow-unauthenticated"};
  EXPECT_EQ(RunUnattendedSetup(context, options), 1);
  EXPECT_TRUE(tools.LoggedCommands().empty());
}

TEST(BareosSetupUnattended, DryRunInstallsFullServerWithoutPrompts)
{
  SetupContext context(true);
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/Debian_12"};
  options.extra_packages = {"bareos-storage-droplet"};
  testing::internal::CaptureStdout();
  const int result = RunUnattendedSetup(context, options);
  const auto output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(result, 0);
  EXPECT_NE(output.find("bareos-storage-droplet"), std::string::npos);
  EXPECT_NE(output.find("bareos-webui-proxy"), std::string::npos);
  EXPECT_NE(output.find("is-active"), std::string::npos);
  EXPECT_EQ(output.find("Repository (community/subscription)"),
            std::string::npos);
  EXPECT_EQ(output.find("Initial WebUI password:"), std::string::npos);
}

TEST(BareosSetupUnattended, CanOmitWebUiWithoutOmittingCatalogOrDaemons)
{
  SetupContext context(true);
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/EL_9"};
  options.webui = false;
  testing::internal::CaptureStdout();
  const int result = RunUnattendedSetup(context, options);
  const auto output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(result, 0);
  EXPECT_NE(output.find("postgresql"), std::string::npos);
  EXPECT_NE(output.find("bareos-director"), std::string::npos);
  EXPECT_NE(output.find("is-active"), std::string::npos);
  EXPECT_EQ(output.find("bareos-webui"), std::string::npos);
  EXPECT_EQ(output.find("admin.conf"), std::string::npos);
}

TEST(BareosSetupUnattended, StopsIfRepositoryDownloadFails)
{
  FakeToolPath tools(
      {"curl", "bash", "apt-get", "dnf", "yum", "zypper", "systemctl", "sudo"});
  tools.SetToolScript("curl", "exit 23\n");
  SetupContext context;
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/Debian_12"};
  EXPECT_EQ(RunUnattendedSetup(context, options), 1);
  EXPECT_TRUE(tools.LoggedCommands().empty());
}

TEST(BareosSetupUnattended, StopsIfRepositoryHelperCannotBeRewritten)
{
  FakeToolPath tools(
      {"curl", "bash", "apt-get", "dnf", "yum", "zypper", "systemctl", "sudo"});
  SetupContext context;
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/Debian_12"};
  EXPECT_EQ(RunUnattendedSetup(context, options), 1);
  ASSERT_EQ(tools.LoggedCommands().size(), 1);
  EXPECT_TRUE(tools.LoggedCommands().front().starts_with("curl "));
}

TEST(BareosSetupUnattended, RunsRepositoryPackagesCatalogAndDaemonSteps)
{
  FakeToolPath tools({"curl", "bash", "apt-get", "dnf", "yum", "zypper",
                      "systemctl", "su", "sudo", "postgresql-setup"});
  tools.SetToolScript("curl", "printf 'URL=\"https://example/repo\"\\n'\n");
  SetupContext context;
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/EL_9"};
  options.webui = false;
  EXPECT_EQ(RunUnattendedSetup(context, options), 0);
  const auto commands = tools.LoggedCommands();
  ASSERT_FALSE(commands.empty());
  EXPECT_TRUE(commands.front().starts_with("bash "));
  EXPECT_NE(std::find(commands.begin(), commands.end(),
                      "systemctl enable --now postgresql"),
            commands.end());
  EXPECT_NE(
      std::find(commands.begin(), commands.end(), "postgresql-setup --initdb"),
      commands.end());
  EXPECT_TRUE(
      std::any_of(commands.begin(), commands.end(), [](const auto& command) {
        return command.find("install") != std::string::npos
               && command.find("bareos-director") != std::string::npos;
      }));
  EXPECT_FALSE(std::filesystem::exists(commands.front().substr(5)));
}

TEST(BareosSetupUnattended, ConfiguresWebUiWithoutPrintingAdminPassword)
{
  FakeToolPath tools({"curl", "bash", "apt-get", "dnf", "yum", "zypper",
                      "systemctl", "su", "sudo", "postgresql-setup", "sh",
                      "install", "chown", "a2enmod", "a2ensite", "a2enflag",
                      "openssl", "chmod", "cat"});
  tools.SetToolScript("curl", "printf 'URL=\"https://example/repo\"\\n'\n");
  SetupContext context;
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/EL_9"};
  testing::internal::CaptureStdout();
  const int result = RunUnattendedSetup(context, options);
  const auto output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(result, 0);
  EXPECT_EQ(output.find("Initial WebUI password:"), std::string::npos);
  EXPECT_EQ(output.find("Password ="), std::string::npos);
  EXPECT_NE(output.find("not printed in unattended logs"), std::string::npos);
  const auto commands = tools.LoggedCommands();
  EXPECT_NE(std::find(commands.begin(), commands.end(),
                      "systemctl is-active bareos-webui-proxy"),
            commands.end());
  EXPECT_TRUE(
      std::any_of(commands.begin(), commands.end(), [](const auto& command) {
        return command.starts_with("install ")
               && command.find("console/admin.conf") != std::string::npos;
      }));
}

TEST(BareosSetupUnattended, PropagatesServiceVerificationFailure)
{
  FakeToolPath tools({"curl", "bash", "apt-get", "dnf", "yum", "zypper",
                      "systemctl", "su", "sudo", "postgresql-setup"});
  tools.SetToolScript("curl", "printf 'URL=\"https://example/repo\"\\n'\n");
  tools.SetToolScript("systemctl",
                      "if [ \"$1\" = is-active ]; then exit 9; fi\nexit 0\n");
  SetupContext context;
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/EL_9"};
  options.webui = false;
  testing::internal::CaptureStderr();
  const int result = RunUnattendedSetup(context, options);
  const auto output = testing::internal::GetCapturedStderr();
  EXPECT_EQ(result, 1);
  EXPECT_NE(output.find("Command failed (exit 9): systemctl is-active"),
            std::string::npos);
}

TEST(BareosSetupUnattended, ReportsAdminConfigurationWriteFailure)
{
  FakeToolPath tools({"curl", "bash", "apt-get", "dnf", "yum", "zypper",
                      "systemctl", "su", "sudo", "postgresql-setup", "sh",
                      "install", "chown", "a2enmod", "a2ensite", "a2enflag",
                      "openssl", "chmod", "cat"});
  tools.SetToolScript("curl", "printf 'URL=\"https://example/repo\"\\n'\n");
  tools.SetToolScript("install", "exit 19\n");
  SetupContext context;
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/EL_9"};
  testing::internal::CaptureStderr();
  const int result = RunUnattendedSetup(context, options);
  const auto output = testing::internal::GetCapturedStderr();
  EXPECT_EQ(result, 1);
  EXPECT_NE(output.find("Command failed (exit 19): install"),
            std::string::npos);
  EXPECT_EQ(output.find("Password ="), std::string::npos);
}

TEST(BareosSetupUnattended, PropagatesPackageInstallationFailure)
{
  FakeToolPath tools({"curl", "bash", "apt-get", "dnf", "yum", "zypper",
                      "systemctl", "su", "sudo", "postgresql-setup"});
  tools.SetToolScript("curl", "printf 'URL=\"https://example/repo\"\\n'\n");
  for (const auto* tool : {"apt-get", "dnf", "yum", "zypper"}) {
    tools.SetToolScript(tool, "exit 17\n");
  }
  SetupContext context;
  UnattendedSetupOptions options;
  options.repository_urls = {"https://ci.example/build/EL_9"};
  options.webui = false;
  EXPECT_EQ(RunUnattendedSetup(context, options), 1);
  const auto commands = tools.LoggedCommands();
  EXPECT_FALSE(
      std::any_of(commands.begin(), commands.end(), [](const auto& command) {
        return command.find("is-active") != std::string::npos
               || command.find("create_bareos_database") != std::string::npos;
      }));
}

TEST(BareosSetupTui, PreservesInteractiveCommunityDryRun)
{
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "Interactive test needs a recognized Linux distribution";
  }
  SetupContext context(true);
  std::istringstream input("community\n");
  auto* original_input = std::cin.rdbuf(input.rdbuf());
  testing::internal::CaptureStdout();
  const int result = RunTuiWizard(context);
  std::cin.rdbuf(original_input);
  const auto output = testing::internal::GetCapturedStdout();
  EXPECT_EQ(result, 0);
  EXPECT_NE(output.find("Repository (community/subscription)"),
            std::string::npos);
  EXPECT_NE(output.find("bareos-webui-proxy"), std::string::npos);
}

TEST(BareosSetupCommandRunner, FindsToolPresentInPath)
{
  // "sh" is guaranteed to exist on every supported Linux platform.
  SetupContext context;
  EXPECT_TRUE(context.IsToolAvailable(SetupTool::Sh));
}

TEST(BareosSetupCommandRunner, DoesNotFindNonexistentTool)
{
  FakeToolPath fake_tools({}, false);
  SetupContext context;
  EXPECT_FALSE(context.IsToolAvailable(SetupTool::Sh));
}

TEST(BareosSetupCommandRunner, DeliversBoundedStandardInput)
{
  std::string output;
  SetupContext context;
  EXPECT_EQ(
      context.RunWithInput(Sh({"-c", "cat"}), "setup input", false,
                           [&output](std::string_view line, std::string_view) {
                             output += line;
                           }),
      0);
  EXPECT_EQ(output, "setup input");
}

TEST(BareosSetupCommandRunner, RejectsInputExceedingPipeBuf)
{
  SetupContext context;
  EXPECT_THROW(
      context.RunWithInput(Sh({"-c", "cat"}), std::string(PIPE_BUF + 1, 'x'),
                           false, [](std::string_view, std::string_view) {}),
      std::invalid_argument);
}

TEST(BareosSetupCommandRunner, SetupContextOwnsDryRunAndRemovalBehavior)
{
  std::string pattern = (std::filesystem::temp_directory_path()
                         / "bareos-setup-context-remove-XXXXXX")
                            .string();
  std::vector<char> buffer(pattern.begin(), pattern.end());
  buffer.push_back('\0');
  const int fd = mkstemp(buffer.data());
  ASSERT_GE(fd, 0);
  close(fd);
  const std::filesystem::path path{buffer.data()};

  SetupContext dry_context(true);
  bool command_logged = false;
  bool preview_called = false;
  EXPECT_EQ(dry_context.Run(
                Sh({"-c", "exit 1"}), false,
                [](std::string_view, std::string_view) {},
                [&command_logged](const SetupCommand&, bool dry_run, bool) {
                  command_logged = dry_run;
                },
                [&preview_called](bool) { preview_called = true; }),
            0);
  EXPECT_TRUE(command_logged);
  EXPECT_TRUE(preview_called);
  dry_context.Remove(path);
  EXPECT_TRUE(std::filesystem::exists(path));

  SetupContext live_context;
  live_context.Remove(path);
  EXPECT_FALSE(std::filesystem::exists(path));
}

TEST(BareosSetupCommandRunner, SetupContextRefreshesMissingToolPaths)
{
  FakeToolPath empty_path({}, false);
  SetupContext context;
  EXPECT_FALSE(context.IsToolAvailable(SetupTool::SensibleBrowser));
  {
    FakeToolPath installed_tool({"sensible-browser"}, false);
    EXPECT_TRUE(context.IsToolAvailable(SetupTool::SensibleBrowser));
    EXPECT_EQ(context.Run(SensibleBrowser({}), false,
                          [](std::string_view, std::string_view) {}),
              0);
  }
  EXPECT_FALSE(context.IsToolAvailable(SetupTool::SensibleBrowser));
}

TEST(BareosSetupCommandRunner, CreatesTemporaryFilesInProtectedDirectory)
{
  SetupContext context;
  const auto path = context.CreateTemporaryFile("bareos-setup-test");
  EXPECT_EQ(path.parent_path().parent_path(), "/tmp");
  const auto permissions = std::filesystem::status(path).permissions();
  EXPECT_EQ(
      permissions & std::filesystem::perms::all,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
  const auto directory_permissions
      = std::filesystem::status(path.parent_path()).permissions();
  EXPECT_EQ(directory_permissions & std::filesystem::perms::all,
            std::filesystem::perms::owner_all);
  context.Remove(path);
  EXPECT_FALSE(std::filesystem::exists(path));
  EXPECT_FALSE(std::filesystem::exists(path.parent_path()));

  SetupContext dry_context(true);
  EXPECT_THROW(dry_context.CreateTemporaryFile("bareos-setup-test"),
               std::logic_error);
}

TEST(BareosSetupCommandRunner, ReportsNoMissingToolsWhenAllPresent)
{
  FakeToolPath fake_tools(
      {"curl", "bash", "install", "chown", "systemctl", "su", "sh", "apt-get"});
  SetupContext context;
  EXPECT_TRUE(context.MissingRequiredTools(PackageManager::Apt).empty());
}

TEST(BareosSetupCommandRunner, ReportsMissingPackageManager)
{
  FakeToolPath fake_tools(
      {"curl", "bash", "install", "chown", "systemctl", "su", "sh"});
  SetupContext context;
  const auto missing = context.MissingRequiredTools(PackageManager::Zypper);
  EXPECT_NE(std::find(missing.begin(), missing.end(), "zypper"), missing.end());
}

TEST(BareosSetupCommandRunner, ChecksPackageSpecificPostInstallTools)
{
  {
    FakeToolPath fake_tools({"a2enmod", "a2ensite"}, false);
    SetupContext context;
    EXPECT_TRUE(context.MissingPostInstallTools(PackageManager::Apt).empty());
  }
  {
    FakeToolPath fake_tools({"a2enmod", "a2enflag", "openssl", "chmod", "cat"},
                            false);
    SetupContext context;
    EXPECT_TRUE(
        context.MissingPostInstallTools(PackageManager::Zypper).empty());
  }
  {
    FakeToolPath fake_tools({"a2enmod", "a2ensite", "a2enflag"}, false);
    SetupContext context;
    const auto missing
        = context.MissingPostInstallTools(PackageManager::Zypper);
    EXPECT_EQ(missing, (std::vector<std::string>{"openssl", "chmod", "cat"}));
  }
}

TEST(BareosSetupCommandRunner, RequiresSuForRunningCatalogScriptsAsPostgres)
{
  // "su" is required so the catalog scripts (which must run as the
  // "postgres" OS user) can be started at all; verify it is part of the
  // fixed required-tools set regardless of the detected package manager.
  FakeToolPath fake_tools(
      {"curl", "bash", "install", "chown", "systemctl", "su", "sh", "dnf"});
  SetupContext context;
  ASSERT_TRUE(context.IsToolAvailable(SetupTool::Su));
  EXPECT_TRUE(context.MissingRequiredTools(PackageManager::Dnf).empty());
}

TEST(BareosSetupStepsShared, BuildsPostgresInitCmdConsistentlyWithToolLookup)
{
  SetupContext context;
  const auto init_cmd = BuildPostgresInitCmd(context);
  if (context.IsToolAvailable(SetupTool::PostgresqlSetup)) {
    ASSERT_TRUE(init_cmd);
    EXPECT_EQ(*init_cmd, PostgresqlSetup({"--initdb"}));
  } else {
    EXPECT_FALSE(init_cmd);
  }
}

TEST(BareosSetupStepsShared, BuildsRunAsPostgresCmd)
{
  EXPECT_EQ(
      BuildRunAsPostgresCmd("/usr/lib/bareos/scripts/create_bareos_database"),
      Su({"postgres", "-c", "/usr/lib/bareos/scripts/create_bareos_database"}));
  EXPECT_EQ(
      BuildRunAsPostgresCmd("/usr/lib/bareos/scripts/make_bareos_tables"),
      Su({"postgres", "-c", "/usr/lib/bareos/scripts/make_bareos_tables"}));
  EXPECT_EQ(
      BuildRunAsPostgresCmd("/usr/lib/bareos/scripts/grant_bareos_privileges"),
      Su({"postgres", "-c",
          "/usr/lib/bareos/scripts/grant_bareos_privileges"}));
}

TEST(BareosSetupStepsShared, BuildsNetworkCheckCmdForCommunityRepo)
{
  const auto command = BuildNetworkCheckCmd("community");
  ASSERT_TRUE(command);
  const auto argv = command->Argv();

  // The URL must end in a slash so the check does not merely observe a
  // redirect, and the response must be discarded rather than written to
  // stdout: a reachability probe has no use for the body, and writing it
  // made curl fail with "client returned ERROR on write" in the installer.
  EXPECT_EQ(argv.back(), "https://download.bareos.org/current/");
  EXPECT_NE(std::find(argv.begin(), argv.end(), "--location"), argv.end());
  const auto output = std::find(argv.begin(), argv.end(), "--output");
  ASSERT_NE(output, argv.end());
  EXPECT_EQ(*(output + 1), "/dev/null");
}

TEST(BareosSetupStepsShared, BuildsNoUnauthenticatedSubscriptionNetworkCheck)
{
  EXPECT_FALSE(BuildNetworkCheckCmd("subscription"));
}

TEST(BareosSetupStepsShared, BuildsMtxAvailabilityCheck)
{
  EXPECT_EQ(BuildMtxAvailabilityCheckCmd(),
            Zypper({"--non-interactive", "search", "--match-exact", "--type",
                    "package", "mtx"}));
}

TEST(BareosSetupStepsShared, BuildsOpenSuseRepositoryPath)
{
  EXPECT_EQ(BuildRepoOsPath("opensuse-leap", "15.6"), "SUSE_15");
  EXPECT_EQ(BuildRepoOsPath("opensuse-tumbleweed", "20260828"),
            "SUSE_20260828");
}

TEST(BareosSetupStepsShared, BuildsSlesRepositoryPath)
{
  EXPECT_EQ(BuildRepoOsPath("sles", "16.0"), "SUSE_16");
}

TEST(BareosSetupStepsShared, BuildsRhelRepositoryPath)
{
  EXPECT_EQ(BuildRepoOsPath("rhel", "9.6"), "EL_9");
}

TEST(BareosSetupStepsShared, BuildsUbuntuRepositoryPath)
{
  EXPECT_EQ(BuildRepoOsPath("ubuntu", "24.04"), "xUbuntu_24.04");
}

TEST(BareosSetupStepsShared, SupportsOpenSuseLeapPlatform)
{
  EXPECT_TRUE(
      IsSupportedSetupPlatform("opensuse-leap", PackageManager::Zypper));
  EXPECT_FALSE(IsSupportedSetupPlatform("opensuse", PackageManager::Zypper));
  EXPECT_TRUE(IsSupportedSetupPlatform("sles", PackageManager::Zypper));
}

TEST(BareosSetupStepsShared, BuildsWebServerServiceNameForPackageManager)
{
  EXPECT_EQ(BuildWebServerServiceName(PackageManager::Apt), "apache2");
  EXPECT_EQ(BuildWebServerServiceName(PackageManager::Dnf), "httpd");
  EXPECT_EQ(BuildWebServerServiceName(PackageManager::Yum), "httpd");
  EXPECT_EQ(BuildWebServerServiceName(PackageManager::Zypper), "apache2");
}

TEST(BareosSetupStepsShared, BuildsWebServerHttpsSetup)
{
  EXPECT_EQ(
      BuildWebServerHttpsSetupCmds(PackageManager::Apt),
      (std::vector<SetupCommand>{A2enmod({"ssl"}), A2ensite({"default-ssl"})}));
  const auto zypper_cmds = BuildWebServerHttpsSetupCmds(PackageManager::Zypper);
  ASSERT_EQ(zypper_cmds.size(), 3U);
  EXPECT_EQ(zypper_cmds[0], A2enmod({"ssl"}));
  EXPECT_EQ(zypper_cmds[1], A2enflag({"SSL"}));
  ASSERT_EQ(zypper_cmds[2].arguments.size(), 2U);
  EXPECT_EQ(zypper_cmds[2].tool, SetupTool::Sh);
  EXPECT_EQ(zypper_cmds[2].arguments[0], "-c");
  EXPECT_NE(zypper_cmds[2].arguments[1].find("bareos-setup-ssl.conf"),
            std::string::npos);
  EXPECT_NE(zypper_cmds[2].arguments[1].find("SSLEngine on"),
            std::string::npos);
  EXPECT_TRUE(BuildWebServerHttpsSetupCmds(PackageManager::Dnf).empty());
  EXPECT_TRUE(BuildWebServerHttpsSetupCmds(PackageManager::Yum).empty());
}

TEST(BareosSetupStepsShared, BuildsBareosDaemonServicesForPackageManager)
{
  EXPECT_EQ(BuildBareosDaemonServiceNames(PackageManager::Apt),
            (std::vector<std::string>{"bareos-director", "bareos-storage",
                                      "bareos-filedaemon"}));
  EXPECT_EQ(BuildBareosDaemonServiceNames(PackageManager::Dnf),
            (std::vector<std::string>{"bareos-dir", "bareos-sd", "bareos-fd"}));
  EXPECT_EQ(BuildBareosDaemonServiceNames(PackageManager::Yum),
            BuildBareosDaemonServiceNames(PackageManager::Dnf));
  EXPECT_EQ(BuildBareosDaemonServiceNames(PackageManager::Zypper),
            BuildBareosDaemonServiceNames(PackageManager::Dnf));
}

TEST(BareosSetupStepsShared, BuildsPackageCacheUpdateForAptAndZypper)
{
  EXPECT_EQ(BuildPackageCacheUpdateCmd(PackageManager::Apt),
            std::optional<SetupCommand>{AptGet({"update"})});
  EXPECT_EQ(BuildPackageCacheUpdateCmd(PackageManager::Zypper),
            std::optional<SetupCommand>{Zypper(
                {"--non-interactive", "--gpg-auto-import-keys", "refresh"})});
  EXPECT_FALSE(BuildPackageCacheUpdateCmd(PackageManager::Dnf));
  EXPECT_FALSE(BuildPackageCacheUpdateCmd(PackageManager::Yum));
}

TEST(BareosSetupStepsShared, BuildsZypperInstallWithAutoKeyImport)
{
  EXPECT_EQ(BuildInstallCmd(PackageManager::Zypper, {"bareos-director"}),
            Zypper({"--non-interactive", "--gpg-auto-import-keys", "install",
                    "bareos-director"}));
}

TEST(BareosSetupStepsShared, JoinsSimpleCommandForDisplayWithoutQuoting)
{
  EXPECT_EQ(
      JoinCommandForDisplay({"systemctl", "enable", "--now", "bareos-dir"}),
      "systemctl enable --now bareos-dir");
}

TEST(BareosSetupStepsShared, JoinsCommandForDisplayQuotingArgsWithSpaces)
{
  EXPECT_EQ(JoinCommandForDisplay({"echo", "hello world"}),
            "echo 'hello world'");
}

TEST(BareosSetupStepsShared, JoinsCommandForDisplayEscapingEmbeddedQuotes)
{
  EXPECT_EQ(JoinCommandForDisplay({"su", "postgres", "-c", "it's fine"}),
            "su postgres -c 'it'\\''s fine'");
}

TEST(BareosSetupStepsShared, BuildsSubscriptionRepoCommandWithoutCredentials)
{
  const auto command
      = BuildAddRepoCmd("sles", "16.0", "subscription", true, "25");
  const auto argv = command.Argv();

  EXPECT_EQ(std::find(argv.begin(), argv.end(), "--config"), argv.end() - 3);
  EXPECT_EQ(std::find(argv.begin(), argv.end(), "-"), argv.end() - 2);
  EXPECT_EQ(std::find(argv.begin(), argv.end(), "login:hunter2"), argv.end());
  EXPECT_EQ(argv.back(),
            "https://download.bareos.com/bareos/release/25/SUSE_16/"
            "add_bareos_repositories.sh");
}

TEST(BareosSetupStepsShared, BuildsCurlUserConfig)
{
  EXPECT_EQ(BuildCurlUserConfig("login", "hunter2"),
            "user = \"login:hunter2\"\n");
  EXPECT_EQ(BuildCurlUserConfig("login", "quote\"slash\\"),
            "user = \"login:quote\\\"slash\\\\\"\n");
}

TEST(BareosSetupStepsShared, RejectsExistingSetupConfigsBeforeOverwrite)
{
  const std::string dir_path = (std::filesystem::temp_directory_path()
                                / "bareos-setup-test-existing-config-XXXXXX")
                                   .string();
  std::vector<char> buffer(dir_path.begin(), dir_path.end());
  buffer.push_back('\0');
  ASSERT_NE(mkdtemp(buffer.data()), nullptr);
  const std::filesystem::path fake_dir = buffer.data();
  const std::filesystem::path admin_path = fake_dir / "admin.conf";
  const std::filesystem::path absent_path = fake_dir / "not-created.conf";
  {
    std::ofstream out(admin_path);
    out << "preexisting\n";
  }

  // The check itself is a privileged shell command, because the wizard
  // config directories are not readable for unprivileged users.
  const auto existing_cmd = BuildFileAbsentCheckCmd(admin_path.string());
  const auto existing_argv = existing_cmd.Argv();
  ASSERT_EQ(existing_argv.size(), 5U);
  EXPECT_EQ(existing_argv[0], "sh");
  EXPECT_EQ(existing_argv[4], admin_path.string());
  SetupContext context;
  EXPECT_NE(context.Run(existing_cmd, false,
                        [](std::string_view, std::string_view) {}),
            0);
  EXPECT_EQ(context.Run(BuildFileAbsentCheckCmd(absent_path.string()), false,
                        [](std::string_view, std::string_view) {}),
            0);

  const std::string message
      = BuildExistingSetupConfigError({admin_path.string()});
  EXPECT_NE(message.find(admin_path.string()), std::string::npos);
  EXPECT_EQ(message.find(absent_path.string()), std::string::npos);
  EXPECT_NE(message.find("Refusing to continue"), std::string::npos);

  std::error_code ec;
  std::filesystem::remove_all(fake_dir, ec);
}

TEST(BareosSetupStepsShared, OwnsOnlyTheAdminConfigPath)
{
  // The WebUI proxy configuration is deliberately not setup-owned: setup
  // never writes it, so an administrator's own file must not block setup.
  EXPECT_EQ(SetupOwnedConfigPaths(),
            (std::vector<std::string>{SetupAdminConfigPath()}));
  EXPECT_EQ(SetupAdminConfigPath(),
            "/etc/bareos/bareos-dir.d/console/admin.conf");
}

TEST(BareosSetupStepsShared, AcceptsSameOriginRequests)
{
  EXPECT_TRUE(IsValidSetupOrigin("http://127.0.0.1:19101", "127.0.0.1:19101"));
  EXPECT_TRUE(IsValidSetupOrigin("http://localhost:19101", "localhost:19101"));
  EXPECT_TRUE(IsValidSetupOrigin("http://[::1]:19101", "[::1]:19101"));
  // An admin-chosen --listen address/port is accepted too, as long as
  // Origin and Host agree.
  EXPECT_TRUE(
      IsValidSetupOrigin("http://192.168.1.5:19101", "192.168.1.5:19101"));
}

TEST(BareosSetupStepsShared, RejectsCrossOriginOrMissingHeaders)
{
  EXPECT_FALSE(
      IsValidSetupOrigin("http://evil.example:19101", "127.0.0.1:19101"));
  EXPECT_FALSE(IsValidSetupOrigin("http://127.0.0.1:19101", ""));
  EXPECT_FALSE(IsValidSetupOrigin("", "127.0.0.1:19101"));
  // Port mismatch between Origin and Host must be rejected too.
  EXPECT_FALSE(IsValidSetupOrigin("http://127.0.0.1:19102", "127.0.0.1:19101"));
}

namespace {

// Runs one setup step against a real (but unconnected-to-a-browser)
// WsCodec so RunSetupStepForTests() has a valid fd to send "output"/"done"
// messages to; the messages themselves are discarded since these tests
// only assert on which commands were executed.
int RunStepDiscardingOutput(const std::string& step,
                            const std::string& json_message = "{}",
                            bool peer_is_loopback = true,
                            bool dry_run = false)
{
  int sockets[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0) {
    throw std::runtime_error("socketpair failed");
  }
  std::thread drain([fd = sockets[1]]() {
    char buffer[4096];
    while (read(fd, buffer, sizeof(buffer)) > 0) {}
    close(fd);
  });
  // Ensure the drain thread is always joined, even if RunSetupStepForTests()
  // throws (e.g. an unsupported-platform check) -- otherwise a joinable
  // std::thread destructing during stack unwinding calls std::terminate().
  try {
    const int result = RunSetupStepForTests(sockets[0], step, json_message,
                                            peer_is_loopback, dry_run);
    close(sockets[0]);
    drain.join();
    return result;
  } catch (...) {
    close(sockets[0]);
    drain.join();
    throw;
  }
}

}  // namespace

TEST(BareosSetupSessionOrchestration,
     CatalogStepEnablesDaemonsAfterInitialization)
{
  // Regression test: InstallPackages() alone never enabled/started the
  // bareos-dir/bareos-sd/bareos-fd services, so CreateAdmin()'s
  // "systemctl restart bareos-dir" used to fail with "Unit cannot be
  // restarted because it is inactive." This asserts the catalog step's
  // command sequence still ends with enabling all three daemons.
  // "sudo" is included as a fake shim too: every command Run() issues is
  // wrapped with "sudo" unless already root (see command_runner.cc's
  // IsRoot() check), so a real "sudo" would otherwise intercept these
  // commands before they ever reach the other fake shims.
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "bareos-setup orchestration is Linux-only";
  }
  FakeToolPath fake_tools(
      {"sudo", "postgresql-setup", "systemctl", "su", "install"});
  ASSERT_EQ(RunStepDiscardingOutput("catalog"), 0);
  const auto commands = fake_tools.LoggedCommands();
  ASSERT_FALSE(commands.empty());
  const auto daemon_services = BuildBareosDaemonServiceNames(os.pkg_mgr);
  const auto enable_it = std::find_if(
      commands.begin(), commands.end(), [&](const auto& line) {
        return line.find("systemctl enable --now") != std::string::npos
               && std::all_of(daemon_services.begin(), daemon_services.end(),
                              [&](const auto& service) {
                                return line.find(service) != std::string::npos;
                              });
      });
  ASSERT_NE(enable_it, commands.end());
  // The daemons must be enabled only after the catalog scripts have run,
  // not before.
  if (BuildCatalogInitScripts(os.pkg_mgr).empty()) {
    const auto marker_it
        = std::find_if(commands.begin(), commands.end(), [](const auto& line) {
            return line.find("/var/lib/bareos/.catalog-initialized")
                   != std::string::npos;
          });
    ASSERT_NE(marker_it, commands.end());
    EXPECT_LT(marker_it - commands.begin(), enable_it - commands.begin());
  } else {
    const auto grant_it
        = std::find_if(commands.begin(), commands.end(), [](const auto& line) {
            return line.find("grant_bareos_privileges") != std::string::npos;
          });
    ASSERT_NE(grant_it, commands.end());
    EXPECT_LT(grant_it - commands.begin(), enable_it - commands.begin());
  }
}

TEST(BareosSetupSessionOrchestration, AdminStepWritesWebuiTlsPskConsole)
{
  const std::string dir_path = (std::filesystem::temp_directory_path()
                                / "bareos-setup-test-admin-conf-XXXXXX")
                                   .string();
  std::vector<char> buffer(dir_path.begin(), dir_path.end());
  buffer.push_back('\0');
  ASSERT_NE(mkdtemp(buffer.data()), nullptr);
  const std::filesystem::path fake_dir = buffer.data();
  const std::filesystem::path log_path = fake_dir / "log.txt";
  const std::filesystem::path resource_path = fake_dir / "admin.conf";

  const auto write_shim
      = [&](const std::string& name, const std::string& body) {
          const std::filesystem::path shim = fake_dir / name;
          std::ofstream out(shim);
          out << "#!/bin/sh\n" << body;
          out.close();
          std::filesystem::permissions(
              shim, std::filesystem::perms::owner_all
                        | std::filesystem::perms::group_read
                        | std::filesystem::perms::group_exec
                        | std::filesystem::perms::others_read
                        | std::filesystem::perms::others_exec);
        };
  write_shim("sudo", "exec \"$@\"\n");
  write_shim("install",
             "echo \"install $*\" >> '" + log_path.string() + "'\n"
             "cat > '" + resource_path.string() + "'\n"
             "exit 0\n");
  write_shim("chown",
             "echo \"chown $*\" >> '" + log_path.string() + "'\nexit 0\n");
  write_shim("systemctl",
             "echo \"systemctl $*\" >> '" + log_path.string() + "'\nexit 0\n");

  const char* current_path = getenv("PATH");
  const std::string old_path = current_path != nullptr ? current_path : "";
  setenv("PATH", (fake_dir.string() + ":" + old_path).c_str(), 1);

  const int result = RunStepDiscardingOutput("admin");

  setenv("PATH", old_path.c_str(), 1);
  std::ifstream resource(resource_path);
  const std::string content((std::istreambuf_iterator<char>(resource)),
                            std::istreambuf_iterator<char>());
  std::ifstream log(log_path);
  const std::string commands((std::istreambuf_iterator<char>(log)),
                             std::istreambuf_iterator<char>());
  std::error_code ec;
  std::filesystem::remove_all(fake_dir, ec);

  ASSERT_EQ(result, 0);
  EXPECT_NE(content.find("Profile = \"webui-admin\""), std::string::npos);
  EXPECT_NE(content.find("TLS Enable = No"), std::string::npos);
  EXPECT_EQ(content.find("TLS Enable = yes"), std::string::npos);
  EXPECT_NE(commands.find("systemctl restart bareos-dir"), std::string::npos);
}

TEST(BareosSetupSessionOrchestration,
     AdminStepAbortsBeforeOverwritingExistingConfig)
{
  const std::string dir_path = (std::filesystem::temp_directory_path()
                                / "bareos-setup-test-existing-check-XXXXXX")
                                   .string();
  std::vector<char> buffer(dir_path.begin(), dir_path.end());
  buffer.push_back('\0');
  ASSERT_NE(mkdtemp(buffer.data()), nullptr);
  const std::filesystem::path fake_dir = buffer.data();
  const std::filesystem::path log_path = fake_dir / "log.txt";

  const auto write_shim
      = [&](const std::string& name, const std::string& body) {
          const std::filesystem::path shim = fake_dir / name;
          std::ofstream out(shim);
          out << "#!/bin/sh\n" << body;
          out.close();
          std::filesystem::permissions(
              shim, std::filesystem::perms::owner_all
                        | std::filesystem::perms::group_read
                        | std::filesystem::perms::group_exec
                        | std::filesystem::perms::others_read
                        | std::filesystem::perms::others_exec);
        };
  write_shim("sudo", "exec \"$@\"\n");
  write_shim("sh", "echo \"sh $*\" >> '" + log_path.string() + "'\nexit 1\n");
  write_shim("install",
             "echo \"install $*\" >> '" + log_path.string() + "'\nexit 0\n");

  const char* current_path = getenv("PATH");
  const std::string old_path = current_path != nullptr ? current_path : "";
  setenv("PATH", (fake_dir.string() + ":" + old_path).c_str(), 1);

  EXPECT_THROW(RunStepDiscardingOutput("admin"), std::runtime_error);

  setenv("PATH", old_path.c_str(), 1);
  std::ifstream log(log_path);
  const std::string commands((std::istreambuf_iterator<char>(log)),
                             std::istreambuf_iterator<char>());
  std::error_code ec;
  std::filesystem::remove_all(fake_dir, ec);

  EXPECT_NE(commands.find("sh -c test ! -e"), std::string::npos);
  EXPECT_EQ(commands.find("install "), std::string::npos);
}

TEST(BareosSetupSessionOrchestration, ProxyStepWritesNoProxyConfiguration)
{
  // bareos-webui-proxy's built-in defaults already describe exactly the
  // layout setup creates (loopback listener on 9104, bareos-dir on 9101),
  // and the service falls back to them when no configuration file exists.
  // Writing one would only restate the defaults -- and, because the
  // service runs as User=bareos/Group=bareos rather than root, it would
  // also need an easy-to-forget chown to stay readable. Not writing it at
  // all avoids that class of bug and leaves an administrator's own
  // configuration untouched.
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "bareos-setup orchestration is Linux-only";
  }
  FakeToolPath fake_tools({"sudo", "install", "chown", "systemctl", "a2enmod",
                           "a2ensite", "a2enflag", "openssl", "chmod", "cat",
                           "sh"});
  ASSERT_EQ(RunStepDiscardingOutput("proxy"), 0);
  const auto commands = fake_tools.LoggedCommands();
  ASSERT_FALSE(commands.empty());
  for (const auto& line : commands) {
    EXPECT_EQ(line.find("bareos-webui-proxy.ini"), std::string::npos) << line;
  }
  const auto enable_it
      = std::find_if(commands.begin(), commands.end(), [](const auto& line) {
          return line.find("systemctl enable --now bareos-webui-proxy")
                 != std::string::npos;
        });
  ASSERT_NE(enable_it, commands.end());
  const auto selinux_it
      = std::find_if(commands.begin(), commands.end(), [](const auto& line) {
          return line.find("httpd_can_network_connect") != std::string::npos;
        });
  ASSERT_NE(selinux_it, commands.end());
  const std::string web_server
      = "systemctl enable --now " + BuildWebServerServiceName(os.pkg_mgr);
  const auto web_server_it = std::find_if(
      commands.begin(), commands.end(), [&web_server](const auto& line) {
        return line.find(web_server) != std::string::npos;
      });
  ASSERT_NE(web_server_it, commands.end());
  EXPECT_LT(enable_it - commands.begin(), web_server_it - commands.begin());
  EXPECT_LT(selinux_it - commands.begin(), web_server_it - commands.begin());
}

TEST(BareosSetupSessionOrchestration, InstallPackagesRunsThePackageManager)
{
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "bareos-setup orchestration is Linux-only";
  }
  const auto install_cmd = BuildInstallCmd(os.pkg_mgr, {"bareos-filedaemon"});
  const auto install_argv = install_cmd.Argv();
  ASSERT_FALSE(install_argv.empty());
  FakeToolPath fake_tools(
      {"sudo", install_argv.front(), "systemctl", "zypper"});
  RunStepDiscardingOutput("packages");
  const auto commands = fake_tools.LoggedCommands();
  ASSERT_FALSE(commands.empty());
  EXPECT_NE(std::find_if(commands.begin(), commands.end(),
                         [&install_cmd](const auto& line) {
                           return line.find(install_cmd.Argv().front())
                                  != std::string::npos;
                         }),
            commands.end());
}

TEST(BareosSetupSessionOrchestration, StorageCustomizationStepIsRemoved)
{
  EXPECT_THROW(RunStepDiscardingOutput("storage"), std::runtime_error);
}

TEST(BareosSetupSessionOrchestration,
     RepositoryStepRejectsRemoteSubscriptionCredentials)
{
  const auto os = DetectOs();
  const std::string json_message
      = "{\"distro\":\"" + os.distro + "\",\"version\":\"" + os.version
        + "\",\"repository\":\"subscription\",\"repository_login\":\"login\","
          "\"repository_password\":\"hunter2\"}";

  EXPECT_THROW(RunStepDiscardingOutput("repository", json_message, false),
               std::runtime_error);
}

TEST(BareosSetupSessionOrchestration,
     DryRunRepositoryStepDoesNotExecuteCommands)
{
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "bareos-setup orchestration is Linux-only";
  }
  FakeToolPath fake_tools({"sudo", "curl", "bash",
                           PackageManagerName(os.pkg_mgr), "apt-get", "zypper",
                           "dnf", "yum"});
  const std::string json_message = "{\"distro\":\"" + os.distro
                                   + "\",\"version\":\"" + os.version
                                   + "\",\"repository\":\"subscription\"}";
  EXPECT_EQ(RunStepDiscardingOutput("repository", json_message, false, true),
            0);
  EXPECT_TRUE(fake_tools.LoggedCommands().empty());
}

TEST(BareosSetupSessionOrchestration,
     RepositoryStepBlocksDownloadWhenNetworkCheckFails)
{
  // Regression test: the pre-flight reachability probe added to
  // InstallRepository() must run before the real repository script
  // download/run, and a failing probe must stop the step immediately.
  // Unlike FakeToolPath's generic "sudo" shim (a no-op that always exits
  // 0, used by the other orchestration tests that only care which
  // commands were *attempted*), this test needs "sudo" to actually pass
  // its arguments through to the fake tools below so that a failing
  // "curl" shim's exit code genuinely propagates back to InstallRepository().
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "bareos-setup orchestration is Linux-only";
  }
  const std::string dir_path = (std::filesystem::temp_directory_path()
                                / "bareos-setup-test-curl-fail-XXXXXX")
                                   .string();
  std::vector<char> buffer(dir_path.begin(), dir_path.end());
  buffer.push_back('\0');
  ASSERT_NE(mkdtemp(buffer.data()), nullptr);
  const std::filesystem::path fake_dir = buffer.data();
  const std::filesystem::path log_path = fake_dir / "log.txt";

  const auto write_shim
      = [&](const std::string& name, const std::string& body) {
          const std::filesystem::path shim = fake_dir / name;
          std::ofstream out(shim);
          out << "#!/bin/sh\n" << body;
          out.close();
          std::filesystem::permissions(
              shim, std::filesystem::perms::owner_all
                        | std::filesystem::perms::group_read
                        | std::filesystem::perms::group_exec
                        | std::filesystem::perms::others_read
                        | std::filesystem::perms::others_exec);
        };
  // "sudo" passes its argv straight through so the real (fake) subcommand's
  // exit code is what InstallRepository() actually sees.
  write_shim("sudo", "exec \"$@\"\n");
  write_shim("curl",
             "echo \"curl $*\" >> '" + log_path.string() + "'\nexit 1\n");
  write_shim("bash",
             "echo \"bash $*\" >> '" + log_path.string() + "'\nexit 0\n");

  const char* current_path = getenv("PATH");
  const std::string old_path = current_path != nullptr ? current_path : "";
  setenv("PATH", (fake_dir.string() + ":" + old_path).c_str(), 1);

  const std::string json_message = "{\"distro\":\"" + os.distro
                                   + "\",\"version\":\"" + os.version
                                   + "\",\"repository\":\"community\"}";
  const int result = RunStepDiscardingOutput("repository", json_message);

  setenv("PATH", old_path.c_str(), 1);
  std::vector<std::string> commands;
  {
    std::ifstream in(log_path);
    std::string line;
    while (std::getline(in, line)) commands.push_back(line);
  }
  std::error_code ec;
  std::filesystem::remove_all(fake_dir, ec);

  EXPECT_NE(result, 0);
  ASSERT_FALSE(commands.empty());
  EXPECT_NE(commands[0].find("curl"), std::string::npos);
  EXPECT_TRUE(
      std::find_if(commands.begin(), commands.end(),
                   [](const auto& line) { return line.rfind("bash ", 0) == 0; })
      == commands.end());
}

TEST(BareosSetupSessionOrchestration,
     SmokeTestOnlyChecksDaemonStatusNotConfigAsRoot)
{
  // Regression test: this step used to also run "bareos-dir -t"/
  // "bareos-sd -t" directly (as root) to validate the config, which
  // fails with "Peer authentication failed for user \"bareos\"" since
  // these daemons connect to the catalog via PostgreSQL peer auth as
  // their own systemd User= (not root). Since InitializeCatalog()
  // already starts these daemons via "systemctl enable --now", a
  // successful "systemctl is-active" already implies the config parsed
  // and the daemon is running correctly -- so smoke_test must rely on
  // is-active alone and never invoke the daemon binaries directly.
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "bareos-setup orchestration is Linux-only";
  }
  FakeToolPath fake_tools({"sudo", "systemctl"});
  ASSERT_EQ(RunStepDiscardingOutput("smoke_test"), 0);
  const auto commands = fake_tools.LoggedCommands();
  auto services = BuildBareosDaemonServiceNames(os.pkg_mgr);
  services.emplace_back("bareos-webui-proxy");
  for (const auto& service : services) {
    const std::string expected = std::string("systemctl is-active ") + service;
    EXPECT_NE(std::find_if(commands.begin(), commands.end(),
                           [&expected](const auto& line) {
                             return line.find(expected) != std::string::npos;
                           }),
              commands.end());
  }
  const std::string web_server
      = "systemctl is-active " + BuildWebServerServiceName(os.pkg_mgr);
  EXPECT_NE(std::find_if(commands.begin(), commands.end(),
                         [&web_server](const auto& line) {
                           return line.find(web_server) != std::string::npos;
                         }),
            commands.end());
}

TEST(BareosSetupStepsShared, ParsesIdLikeFromOsRelease)
{
  const auto info = ParseOsRelease(
      "ID=eurolinux\nID_LIKE=\"rhel centos fedora\"\nVERSION_ID=\"9.4\"\n"
      "PRETTY_NAME=\"EuroLinux 9.4\"\n");

  EXPECT_EQ(info.distro, "eurolinux");
  EXPECT_EQ(info.version, "9.4");
  EXPECT_EQ(info.pretty_name, "EuroLinux 9.4");
  EXPECT_EQ(info.id_like,
            (std::vector<std::string>{"rhel", "centos", "fedora"}));
}

TEST(BareosSetupStepsShared, ParsesOsReleaseWithoutIdLike)
{
  const auto info = ParseOsRelease("ID=debian\nVERSION_ID=\"13\"\n");

  EXPECT_EQ(info.distro, "debian");
  EXPECT_TRUE(info.id_like.empty());
}

TEST(BareosSetupStepsShared, ParsesOsReleaseWithSingleQuotesAndEscapes)
{
  const auto info = ParseOsRelease(
      "# ignored comment=with a value\n"
      "\n"
      "ID='debian'\n"
      "VERSION_ID='13'\n"
      "PRETTY_NAME='Debian GNU/Linux \"bookworm\"'\n"
      "ID_LIKE='rhel centos fedora'\n");

  EXPECT_EQ(info.distro, "debian");
  EXPECT_EQ(info.version, "13");
  EXPECT_EQ(info.pretty_name, "Debian GNU/Linux \"bookworm\"");
  EXPECT_EQ(info.id_like,
            (std::vector<std::string>{"rhel", "centos", "fedora"}));
}

TEST(BareosSetupStepsShared, ParsesEscapedOsReleaseCharacters)
{
  const auto info = ParseOsRelease(
      "  ID = \"debian\"\n"
      "PRETTY_NAME=\"Test \\$release\\\"\"\n");
  EXPECT_EQ(info.distro, "debian");
  EXPECT_EQ(info.pretty_name, "Test $release\"");
}

TEST(BareosSetupStepsShared, DetectsOsWithoutFailingOnUnknownSystems)
{
  // DetectOs() must never throw: the wizard has to stay usable on systems
  // without /etc/os-release so it can offer a manual repository choice.
  const auto info = DetectOs();
  EXPECT_FALSE(info.arch.empty());
}

TEST(BareosSetupStepsShared, ValidatesRepositoryOsPaths)
{
  for (const auto& path : KnownRepoOsPaths()) {
    EXPECT_TRUE(IsValidRepoOsPath(path)) << path;
  }

  // IsSafeSetupIdentifier() permits '.', so ".." must be rejected explicitly
  // or a manual entry could escape the release directory of the download URL.
  EXPECT_FALSE(IsValidRepoOsPath(".."));
  EXPECT_FALSE(IsValidRepoOsPath("EL_9/.."));
  EXPECT_FALSE(IsValidRepoOsPath("../EL_9"));
  EXPECT_FALSE(IsValidRepoOsPath("EL..9"));
  EXPECT_FALSE(IsValidRepoOsPath("."));
  EXPECT_FALSE(IsValidRepoOsPath(""));
  EXPECT_FALSE(IsValidRepoOsPath("-EL_9"));
  EXPECT_FALSE(IsValidRepoOsPath("EL_9."));
  EXPECT_FALSE(IsValidRepoOsPath("EL 9"));
  EXPECT_FALSE(IsValidRepoOsPath(std::string(65, 'a')));
}

TEST(BareosSetupStepsShared, SuggestsRepositoryPathsFromIdLike)
{
  OsInfo el;
  el.distro = "eurolinux";
  el.id_like = {"rhel", "centos", "fedora"};
  el.version = "9.4";
  const auto el_paths = SuggestRepoOsPaths(el);
  ASSERT_FALSE(el_paths.empty());
  EXPECT_EQ(el_paths.front(), "EL_9");

  OsInfo debian;
  debian.distro = "devuan";
  debian.id_like = {"debian"};
  debian.version = "13";
  const auto debian_paths = SuggestRepoOsPaths(debian);
  ASSERT_FALSE(debian_paths.empty());
  EXPECT_EQ(debian_paths.front(), "Debian_13");

  OsInfo ubuntu;
  ubuntu.distro = "linuxmint";
  ubuntu.id_like = {"ubuntu", "debian"};
  ubuntu.version = "24.04";
  const auto ubuntu_paths = SuggestRepoOsPaths(ubuntu);
  ASSERT_FALSE(ubuntu_paths.empty());
  EXPECT_EQ(ubuntu_paths.front(), "xUbuntu_24.04");

  OsInfo suse;
  suse.distro = "suse-derivative";
  suse.id_like = {"suse"};
  suse.version = "16.0";
  const auto suse_paths = SuggestRepoOsPaths(suse);
  ASSERT_FALSE(suse_paths.empty());
  EXPECT_EQ(suse_paths.front(), "SUSE_16");
}

TEST(BareosSetupStepsShared, SuggestsFamilyPathsWhenTheVersionDoesNotMatch)
{
  // Amazon Linux 2023 declares ID_LIKE=fedora but VERSION_ID=2023, which is
  // not a published Bareos repository. The guess must be dropped and only
  // real family paths offered, so the user cannot be sent to a 404.
  OsInfo amazon;
  amazon.distro = "amzn";
  amazon.id_like = {"fedora"};
  amazon.version = "2023";
  const auto paths = SuggestRepoOsPaths(amazon);

  ASSERT_FALSE(paths.empty());
  EXPECT_EQ(std::find(paths.begin(), paths.end(), "Fedora_2023"), paths.end());
  for (const auto& path : paths) {
    EXPECT_NE(
        std::find(KnownRepoOsPaths().begin(), KnownRepoOsPaths().end(), path),
        KnownRepoOsPaths().end())
        << path;
  }
}

TEST(BareosSetupStepsShared, SuggestsNothingForAnUnrelatedDistribution)
{
  OsInfo other;
  other.distro = "nixos";
  other.version = "25.05";
  EXPECT_TRUE(SuggestRepoOsPaths(other).empty());
}

TEST(BareosSetupStepsShared, BuildsAddRepoCommandForAnExplicitPath)
{
  const auto command = BuildAddRepoCmdForPath("EL_10", "community");
  const auto argv = command.Argv();

  EXPECT_EQ(argv.back(),
            "https://download.bareos.org/current/EL_10/"
            "add_bareos_repositories.sh");
  EXPECT_EQ(BuildAddRepoCmdForPath("EL_10", "community"),
            BuildAddRepoCmd("rhel", "10.2", "community"));
}

TEST(BareosSetupStepsShared, BuildsSubscriptionProbeWithoutCredentialsInArgv)
{
  const auto command
      = BuildRepoPathProbeCmd("SUSE_16", "subscription", true, "25");
  const auto argv = command.Argv();

  EXPECT_NE(std::find(argv.begin(), argv.end(), "--head"), argv.end());
  EXPECT_EQ(std::find(argv.begin(), argv.end(), "--config"), argv.end() - 3);
  EXPECT_EQ(std::find(argv.begin(), argv.end(), "login:hunter2"), argv.end());
  EXPECT_EQ(argv.back(),
            "https://download.bareos.com/bareos/release/25/SUSE_16/"
            "add_bareos_repositories.sh");
}

TEST(BareosSetupStepsShared, SelectsLatestSubscriptionRelease)
{
  const std::string index = R"(
    <a href="24/">24/</a>
    <a href="25/">25/</a>
    <a href="25.1/">25.1/</a>
    <a href="100000000000000000000000000000000000000/">100000000000000000000000000000000000000/</a>
    <a href="not-a-release/">not-a-release/</a>
    <a href="26-rc1/">26-rc1/</a>
  )";
  EXPECT_EQ(ParseLatestSubscriptionRelease(index),
            "100000000000000000000000000000000000000");
}

TEST(BareosSetupStepsShared,
     SelectsLatestSubscriptionReleaseWithMixedSegmentCounts)
{
  // Regression test: legacy multi-segment releases (e.g. "12.4") must not be
  // mistaken for newer than later single-segment releases (e.g. "25") just
  // because they sort later in the index and have more version segments.
  const std::string index = R"(
    <a href="25/">25/</a>
    <a href="24/">24/</a>
    <a href="20/">20/</a>
    <a href="19.2/">19.2/</a>
    <a href="13.2/">13.2/</a>
    <a href="12.4/">12.4/</a>
  )";
  EXPECT_EQ(ParseLatestSubscriptionRelease(index), "25");
}

TEST(BareosSetupStepsShared, RejectsInvalidSubscriptionReleaseIndex)
{
  EXPECT_TRUE(
      ParseLatestSubscriptionRelease(
          R"(<a href="latest/">latest/</a><a href="25-rc1/">25-rc1</a>)")
          .empty());
}

TEST(BareosSetupStepsShared, BuildsSubscriptionReleaseIndexCommand)
{
  const auto command = BuildSubscriptionReleaseIndexCmd(true);
  const auto argv = command.Argv();
  EXPECT_EQ(argv.back(), "https://download.bareos.com/bareos/release/");
  EXPECT_NE(std::find(argv.begin(), argv.end(), "--config"), argv.end());
}

TEST(BareosSetupStepsShared, BuildsEnforcingSelinuxWebUiCommand)
{
  EXPECT_EQ(BuildWebUiSelinuxSetupCmd(),
            Sh({"-c",
                "if command -v getenforce >/dev/null 2>&1 && "
                "[ \"$(getenforce)\" = Enforcing ]; then "
                "setsebool -P httpd_can_network_connect on; "
                "fi"}));
}

TEST(BareosSetupStepsShared, IdentifiesSuseRepositoryPaths)
{
  EXPECT_TRUE(IsSuseRepoOsPath("SUSE_16"));
  EXPECT_TRUE(IsSuseRepoOsPath("SUSE_15"));
  EXPECT_FALSE(IsSuseRepoOsPath("EL_9"));
  EXPECT_FALSE(IsSuseRepoOsPath(""));
}

TEST(BareosSetupStepsShared, SeparatesPackageManagerFromDistributionSupport)
{
  // An unknown distribution can still be installed through a manual
  // repository choice, but an unknown package manager cannot.
  EXPECT_TRUE(IsSupportedPackageManager(PackageManager::Apt));
  EXPECT_TRUE(IsSupportedPackageManager(PackageManager::Zypper));
  EXPECT_FALSE(IsSupportedPackageManager(PackageManager::Unknown));
  EXPECT_FALSE(IsSupportedSetupPlatform("eurolinux", PackageManager::Dnf));
  EXPECT_THROW(BuildDefaultPackageList(PackageManager::Unknown),
               std::invalid_argument);
}

TEST(BareosSetupStepsShared, NamesPackageManagers)
{
  EXPECT_STREQ(PackageManagerName(PackageManager::Apt), "apt");
  EXPECT_STREQ(PackageManagerName(PackageManager::Dnf), "dnf");
  EXPECT_STREQ(PackageManagerName(PackageManager::Yum), "yum");
  EXPECT_STREQ(PackageManagerName(PackageManager::Zypper), "zypper");
  EXPECT_STREQ(PackageManagerName(PackageManager::Unknown), "unknown");
}

TEST(BareosSetupSessionOrchestration, RepositoryStepRejectsOverrideWhenDetected)
{
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "requires a recognised distribution";
  }
  // A recognised system must never be pointed at a mismatched repository.
  const std::string message = R"({"distro":")" + os.distro + R"(","version":")"
                              + os.version
                              + R"(","repository":"community",)"
                                R"("repo_os_path":"Debian_13"})";

  EXPECT_THROW(RunStepDiscardingOutput("repository", message, true, true),
               std::runtime_error);
}

TEST(BareosSetupSessionOrchestration,
     RepositoryStepAcceptsTheDetectedPathAsOverride)
{
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "requires a recognised distribution";
  }
  const std::string message
      = R"({"distro":")" + os.distro + R"(","version":")" + os.version
        + R"(","repository":"community",)"
          R"("repo_os_path":")"
        + BuildRepoOsPath(os.distro, os.version) + R"("})";

  EXPECT_EQ(RunStepDiscardingOutput("repository", message, true, true), 0);
}

TEST(BareosSetupSessionOrchestration, RepositoryStepRejectsUnsafeOverridePaths)
{
  const auto os = DetectOs();
  if (IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    // On a recognised system any override is refused already, which is
    // asserted separately; the traversal rejection itself is covered by
    // IsValidRepoOsPath's unit test.
    GTEST_SKIP() << "requires an unrecognised distribution";
  }
  const std::string message = R"({"distro":")" + os.distro + R"(","version":")"
                              + os.version
                              + R"(","repository":"community",)"
                                R"("repo_os_path":".."})";

  EXPECT_THROW(RunStepDiscardingOutput("repository", message, true, true),
               std::runtime_error);
}

TEST(BareosSetupSessionOrchestration,
     RepositoryStepRequiresAChoiceOnUnknownDistributions)
{
  const auto os = DetectOs();
  if (IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "requires an unrecognised distribution";
  }
  const std::string message = R"({"distro":")" + os.distro + R"(","version":")"
                              + os.version + R"(","repository":"community"})";

  EXPECT_THROW(RunStepDiscardingOutput("repository", message, true, true),
               std::runtime_error);
}

TEST(BareosSetupStepsShared, DiscardsProbeResponseBodies)
{
  const auto command = BuildRepoPathProbeCmd("EL_10", "community");
  const auto argv = command.Argv();
  const auto output = std::find(argv.begin(), argv.end(), "--output");
  ASSERT_NE(output, argv.end());
  EXPECT_EQ(*(output + 1), "/dev/null");
}

TEST(BareosSetupStepsShared, CapturesCommandOutputLargerThanTheReadBuffer)
{
  // Regression test: the output drain used to issue a single 4096 byte
  // read per poll wakeup, so anything still buffered when the pipe hung up
  // was silently dropped.
  constexpr int kLines = 4000;
  std::string collected;
  int lines = 0;
  SetupContext context;
  const int rc = context.Run(
      Sh({"-c", "i=0; while [ $i -lt " + std::to_string(kLines)
                    + " ]; do echo aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa; "
                      "i=$((i+1)); done"}),
      false, [&](std::string_view line, std::string_view) {
        collected += line;
        ++lines;
      });

  EXPECT_EQ(rc, 0);
  EXPECT_EQ(lines, kLines);
  EXPECT_EQ(collected.size(), static_cast<size_t>(kLines) * 40);
}
