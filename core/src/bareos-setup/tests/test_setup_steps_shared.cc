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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "command_runner.h"
#include "os_detector.h"
#include "setup_session.h"

TEST(BareosSetupStepsShared, BuildsDefaultPackageListForDnf)
{
  EXPECT_EQ(BuildDefaultPackageList("dnf"),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "mod_ssl", "postgresql-server"}));
}

TEST(BareosSetupStepsShared, BuildsDefaultPackageListForApt)
{
  EXPECT_EQ(BuildDefaultPackageList("apt"),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "postgresql"}));
}

TEST(BareosSetupStepsShared, BuildsDefaultPackageListForZypper)
{
  EXPECT_EQ(BuildDefaultPackageList("zypper"),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "postgresql-server"}));
}

TEST(BareosSetupStepsShared, BuildsPackageListWithoutPostgresServer)
{
  EXPECT_EQ(BuildPackageListWithoutPostgresServer("apt"),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy"}));
  EXPECT_EQ(BuildPackageListWithoutPostgresServer("dnf"),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy", "mod_ssl"}));
  EXPECT_EQ(BuildPackageListWithoutPostgresServer("zypper"),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-tape", "bareos-storage-dedupable",
                "bareos-database-tools", "bareos-tools", "bareos-webui-new",
                "bareos-webui-proxy"}));
}

TEST(BareosSetupStepsShared, BuildsPackageListWithoutTapeStorage)
{
  EXPECT_EQ(
      BuildPackageListWithoutTapeStorage("zypper"),
      (std::vector<std::string>{
          "bareos-filedaemon", "bareos-director", "bareos-storage",
          "bareos-storage-dedupable", "bareos-database-tools", "bareos-tools",
          "bareos-webui-new", "bareos-webui-proxy", "postgresql-server"}));
  EXPECT_EQ(BuildPackageListWithoutTapeStorage("dnf"),
            (std::vector<std::string>{
                "bareos-filedaemon", "bareos-director", "bareos-storage",
                "bareos-storage-dedupable", "bareos-database-tools",
                "bareos-tools", "bareos-webui-new", "bareos-webui-proxy",
                "mod_ssl", "postgresql-server"}));
}

TEST(BareosSetupStepsShared, BuildsCatalogInitScriptsOnlyWhenNeeded)
{
  EXPECT_TRUE(BuildCatalogInitScripts("apt").empty());
  EXPECT_EQ(BuildCatalogInitScripts("dnf"),
            (std::vector<std::string>{
                "/usr/lib/bareos/scripts/create_bareos_database",
                "/usr/lib/bareos/scripts/make_bareos_tables",
                "/usr/lib/bareos/scripts/grant_bareos_privileges"}));
  EXPECT_EQ(BuildCatalogInitScripts("yum"), BuildCatalogInitScripts("dnf"));
  EXPECT_EQ(BuildCatalogInitScripts("zypper"), BuildCatalogInitScripts("dnf"));
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
  explicit FakeToolPath(const std::vector<std::string>& tools)
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
    setenv("PATH", (dir_.string() + ":" + old_path_).c_str(), 1);
  }

  ~FakeToolPath()
  {
    setenv("PATH", old_path_.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
  }

  FakeToolPath(const FakeToolPath&) = delete;
  FakeToolPath& operator=(const FakeToolPath&) = delete;

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

TEST(BareosSetupCommandRunner, FindsToolPresentInPath)
{
  // "sh" is guaranteed to exist on every supported Linux platform.
  EXPECT_TRUE(IsToolInPath("sh"));
}

TEST(BareosSetupCommandRunner, DoesNotFindNonexistentTool)
{
  EXPECT_FALSE(IsToolInPath("definitely-not-a-real-tool-xyz"));
}

TEST(BareosSetupCommandRunner, ReportsNoMissingToolsWhenAllPresent)
{
  // "sh" is used here as a stand-in package manager name since it is
  // always present, so this exercises the "all tools found" path.
  FakeToolPath fake_tools(
      {"curl", "bash", "install", "chown", "systemctl", "su", "sh"});
  EXPECT_TRUE(MissingRequiredTools("sh").empty());
}

TEST(BareosSetupCommandRunner, ReportsMissingPackageManager)
{
  const auto missing = MissingRequiredTools("definitely-not-a-real-tool-xyz");
  EXPECT_NE(std::find(missing.begin(), missing.end(),
                      "definitely-not-a-real-tool-xyz"),
            missing.end());
}

TEST(BareosSetupCommandRunner, RequiresSuForRunningCatalogScriptsAsPostgres)
{
  // "su" is required so the catalog scripts (which must run as the
  // "postgres" OS user) can be started at all; verify it is part of the
  // fixed required-tools set regardless of the detected package manager.
  FakeToolPath fake_tools(
      {"curl", "bash", "install", "chown", "systemctl", "su", "sh"});
  ASSERT_TRUE(IsToolInPath("su"));
  EXPECT_TRUE(MissingRequiredTools("sh").empty());
}

TEST(BareosSetupStepsShared, BuildsPostgresInitCmdConsistentlyWithToolLookup)
{
  const auto init_cmd = BuildPostgresInitCmd();
  if (IsToolInPath("postgresql-setup")) {
    EXPECT_EQ(init_cmd,
              (std::vector<std::string>{"postgresql-setup", "--initdb"}));
  } else {
    EXPECT_TRUE(init_cmd.empty());
  }
}

TEST(BareosSetupStepsShared, BuildsRunAsPostgresCmd)
{
  EXPECT_EQ(
      BuildRunAsPostgresCmd("/usr/lib/bareos/scripts/create_bareos_database"),
      (std::vector<std::string>{
          "su", "postgres", "-c",
          "/usr/lib/bareos/scripts/create_bareos_database"}));
  EXPECT_EQ(
      BuildRunAsPostgresCmd("/usr/lib/bareos/scripts/make_bareos_tables"),
      (std::vector<std::string>{"su", "postgres", "-c",
                                "/usr/lib/bareos/scripts/make_bareos_tables"}));
  EXPECT_EQ(
      BuildRunAsPostgresCmd("/usr/lib/bareos/scripts/grant_bareos_privileges"),
      (std::vector<std::string>{
          "su", "postgres", "-c",
          "/usr/lib/bareos/scripts/grant_bareos_privileges"}));
}

TEST(BareosSetupStepsShared, BuildsNetworkCheckCmdForCommunityRepo)
{
  EXPECT_EQ(BuildNetworkCheckCmd("community"),
            (std::vector<std::string>{
                "curl", "--fail", "--silent", "--show-error", "--head",
                "--max-time", "10", "https://download.bareos.org/current"}));
}

TEST(BareosSetupStepsShared, BuildsNetworkCheckCmdForSubscriptionRepo)
{
  EXPECT_EQ(
      BuildNetworkCheckCmd("subscription"),
      (std::vector<std::string>{
          "curl", "--fail", "--silent", "--show-error", "--head", "--max-time",
          "10", "https://download.bareos.com/bareos/release/latest"}));
}

TEST(BareosSetupStepsShared, BuildsMtxAvailabilityCheck)
{
  EXPECT_EQ(
      BuildMtxAvailabilityCheckCmd(),
      (std::vector<std::string>{"zypper", "--non-interactive", "search",
                                "--match-exact", "--type", "package", "mtx"}));
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

TEST(BareosSetupStepsShared, BuildsUbuntuRepositoryPath)
{
  EXPECT_EQ(BuildRepoOsPath("ubuntu", "24.04"), "xUbuntu_24.04");
}

TEST(BareosSetupStepsShared, SupportsOpenSuseLeapPlatform)
{
  EXPECT_TRUE(IsSupportedSetupPlatform("opensuse-leap", "zypper"));
  EXPECT_FALSE(IsSupportedSetupPlatform("opensuse", "zypper"));
  EXPECT_TRUE(IsSupportedSetupPlatform("sles", "zypper"));
}

TEST(BareosSetupStepsShared, BuildsWebServerServiceNameForPackageManager)
{
  EXPECT_EQ(BuildWebServerServiceName("apt"), "apache2");
  EXPECT_EQ(BuildWebServerServiceName("dnf"), "httpd");
  EXPECT_EQ(BuildWebServerServiceName("yum"), "httpd");
  EXPECT_EQ(BuildWebServerServiceName("zypper"), "apache2");
}

TEST(BareosSetupStepsShared, BuildsWebServerHttpsSetup)
{
  EXPECT_EQ(BuildWebServerHttpsSetupCmds("apt"),
            (std::vector<std::vector<std::string>>{
                {"a2enmod", "ssl"}, {"a2ensite", "default-ssl"}}));
  const auto zypper_cmds = BuildWebServerHttpsSetupCmds("zypper");
  ASSERT_EQ(zypper_cmds.size(), 3U);
  EXPECT_EQ(zypper_cmds[0], (std::vector<std::string>{"a2enmod", "ssl"}));
  EXPECT_EQ(zypper_cmds[1], (std::vector<std::string>{"a2enflag", "SSL"}));
  ASSERT_EQ(zypper_cmds[2].size(), 3U);
  EXPECT_EQ(zypper_cmds[2][0], "sh");
  EXPECT_EQ(zypper_cmds[2][1], "-c");
  EXPECT_NE(zypper_cmds[2][2].find("bareos-setup-ssl.conf"), std::string::npos);
  EXPECT_NE(zypper_cmds[2][2].find("SSLEngine on"), std::string::npos);
  EXPECT_TRUE(BuildWebServerHttpsSetupCmds("dnf").empty());
  EXPECT_TRUE(BuildWebServerHttpsSetupCmds("yum").empty());
}

TEST(BareosSetupStepsShared, BuildsBareosDaemonServicesForPackageManager)
{
  EXPECT_EQ(BuildBareosDaemonServiceNames("apt"),
            (std::vector<std::string>{"bareos-director", "bareos-storage",
                                      "bareos-filedaemon"}));
  EXPECT_EQ(BuildBareosDaemonServiceNames("dnf"),
            (std::vector<std::string>{"bareos-dir", "bareos-sd", "bareos-fd"}));
  EXPECT_EQ(BuildBareosDaemonServiceNames("yum"),
            BuildBareosDaemonServiceNames("dnf"));
  EXPECT_EQ(BuildBareosDaemonServiceNames("zypper"),
            BuildBareosDaemonServiceNames("dnf"));
}

TEST(BareosSetupStepsShared, BuildsPackageCacheUpdateForAptAndZypper)
{
  EXPECT_EQ(BuildPackageCacheUpdateCmd("apt"),
            (std::vector<std::string>{"apt-get", "update"}));
  EXPECT_EQ(BuildPackageCacheUpdateCmd("zypper"),
            (std::vector<std::string>{"zypper", "--non-interactive",
                                      "--gpg-auto-import-keys", "refresh"}));
  EXPECT_TRUE(BuildPackageCacheUpdateCmd("dnf").empty());
  EXPECT_TRUE(BuildPackageCacheUpdateCmd("yum").empty());
}

TEST(BareosSetupStepsShared, BuildsZypperInstallWithAutoKeyImport)
{
  EXPECT_EQ(BuildInstallCmd("zypper", {"bareos-director"}),
            (std::vector<std::string>{"zypper", "--non-interactive",
                                      "--gpg-auto-import-keys", "install",
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
  const auto command = BuildAddRepoCmd("sles", "16.0", "subscription", true);

  EXPECT_EQ(std::find(command.begin(), command.end(), "--config"),
            command.end() - 3);
  EXPECT_EQ(std::find(command.begin(), command.end(), "-"), command.end() - 2);
  EXPECT_EQ(std::find(command.begin(), command.end(), "login:hunter2"),
            command.end());
  EXPECT_NE(command.back().find("SUSE_16/add_bareos_repositories.sh"),
            std::string::npos);
}

TEST(BareosSetupStepsShared, BuildsCurlUserConfig)
{
  EXPECT_EQ(BuildCurlUserConfig("login", "hunter2"),
            "user = \"login:hunter2\"\n");
  EXPECT_EQ(BuildCurlUserConfig("login", "quote\"slash\\"),
            "user = \"login:quote\\\"slash\\\\\"\n");
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
                            bool peer_is_loopback = true)
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
                                            peer_is_loopback);
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
     ProxyStepChownsConfigSoTheBareosUserCanReadIt)
{
  // Regression test: bareos-webui-proxy.service runs as
  // User=bareos/Group=bareos (not root). Without chowning the config
  // file to "root:bareos" after writing it (which install -D leaves
  // owned by root:root at mode 0640), the proxy process cannot read
  // its own config and crash-loops with "cannot load
  // '/etc/bareos-webui-proxy/bareos-webui-proxy.ini'" forever.
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "bareos-setup orchestration is Linux-only";
  }
  FakeToolPath fake_tools({"sudo", "install", "chown", "systemctl", "a2enmod",
                           "a2ensite", "a2enflag", "sh"});
  ASSERT_EQ(RunStepDiscardingOutput("proxy"), 0);
  const auto commands = fake_tools.LoggedCommands();
  ASSERT_FALSE(commands.empty());
  const auto chown_it
      = std::find_if(commands.begin(), commands.end(), [](const auto& line) {
          return line.find("chown root:bareos") != std::string::npos
                 && line.find("bareos-webui-proxy.ini") != std::string::npos;
        });
  ASSERT_NE(chown_it, commands.end());
  const auto enable_it
      = std::find_if(commands.begin(), commands.end(), [](const auto& line) {
          return line.find("systemctl enable --now bareos-webui-proxy")
                 != std::string::npos;
        });
  ASSERT_NE(enable_it, commands.end());
  // The chown must happen before the daemon is (re)started, otherwise a
  // config-file read race could still hit the old ownership.
  EXPECT_LT(chown_it - commands.begin(), enable_it - commands.begin());
  const std::string web_server
      = "systemctl enable --now " + BuildWebServerServiceName(os.pkg_mgr);
  EXPECT_NE(std::find_if(commands.begin(), commands.end(),
                         [&web_server](const auto& line) {
                           return line.find(web_server) != std::string::npos;
                         }),
            commands.end());
}

TEST(BareosSetupSessionOrchestration, InstallPackagesRunsThePackageManager)
{
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    GTEST_SKIP() << "bareos-setup orchestration is Linux-only";
  }
  const auto install_cmd = BuildInstallCmd(os.pkg_mgr, {"bareos-filedaemon"});
  ASSERT_FALSE(install_cmd.empty());
  FakeToolPath fake_tools({"sudo", install_cmd.front(), "systemctl", "zypper"});
  RunStepDiscardingOutput("packages");
  const auto commands = fake_tools.LoggedCommands();
  ASSERT_FALSE(commands.empty());
  EXPECT_NE(std::find_if(commands.begin(), commands.end(),
                         [&install_cmd](const auto& line) {
                           return line.find(install_cmd.front())
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
