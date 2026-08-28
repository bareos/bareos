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
                "bareos-webui-proxy", "apache2-mod_ssl", "postgresql-server"}));
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
                "bareos-webui-proxy", "apache2-mod_ssl"}));
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

TEST(BareosSetupStepsShared, SuggestsSingleChangerAssignments)
{
  const std::vector<TapeChangerInfo> changers = {
      {"/dev/tape/by-id/changer-0",
       "/dev/sg4",
       "Changer",
       "",
       "",
       "",
       "",
       "",
       {},
       {},
       {}},
  };
  const std::vector<TapeDriveInfo> drives = {
      {"/dev/tape/by-id/drive-0-nst",
       "/dev/nst0",
       "Drive0",
       "",
       "",
       "",
       "",
       "",
       {},
       {}},
      {"/dev/tape/by-id/drive-1-nst",
       "/dev/nst1",
       "Drive1",
       "",
       "",
       "",
       "",
       "",
       {},
       {}},
  };

  const auto assignments = SuggestTapeAssignments(changers, drives);

  ASSERT_EQ(assignments.size(), 1U);
  EXPECT_EQ(assignments.front().changer_path, "/dev/tape/by-id/changer-0");
  EXPECT_EQ(assignments.front().drive_paths,
            (std::vector<std::string>{"/dev/tape/by-id/drive-0-nst",
                                      "/dev/tape/by-id/drive-1-nst"}));
}

TEST(BareosSetupStepsShared, SuggestsAllDrivesForEveryChangerWithoutMatches)
{
  const std::vector<TapeChangerInfo> changers = {
      {"/dev/tape/by-id/changer-0",
       "/dev/sg4",
       "Changer0",
       "",
       "",
       "",
       "",
       "",
       {},
       {},
       {}},
      {"/dev/tape/by-id/changer-1",
       "/dev/sg5",
       "Changer1",
       "",
       "",
       "",
       "",
       "",
       {},
       {},
       {}},
  };
  const std::vector<TapeDriveInfo> drives = {
      {"/dev/tape/by-id/drive-0-nst",
       "/dev/nst0",
       "Drive0",
       "",
       "",
       "",
       "",
       "",
       {},
       {}},
      {"/dev/tape/by-id/drive-1-nst",
       "/dev/nst1",
       "Drive1",
       "",
       "",
       "",
       "",
       "",
       {},
       {}},
  };

  const auto assignments = SuggestTapeAssignments(changers, drives);

  ASSERT_EQ(assignments.size(), 2U);
  EXPECT_EQ(assignments[0].drive_paths,
            (std::vector<std::string>{"/dev/tape/by-id/drive-0-nst",
                                      "/dev/tape/by-id/drive-1-nst"}));
  EXPECT_EQ(assignments[1].drive_paths,
            (std::vector<std::string>{"/dev/tape/by-id/drive-0-nst",
                                      "/dev/tape/by-id/drive-1-nst"}));
}

TEST(BareosSetupStepsShared, ParsesVpdPageDeviceIdentifiers)
{
  const std::vector<uint8_t> page = {
      0x01, 0x83, 0x00, 0x42, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50, 0x45,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75,
      0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x30,
      0x31, 0x46, 0x41, 0x42, 0x31, 0x32, 0x38, 0x31, 0x32, 0x01, 0x03,
      0x00, 0x08, 0x20, 0x30, 0x31, 0x46, 0x41, 0x32, 0x38, 0x32, 0x01,
      0x94, 0x00, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x93, 0x00, 0x08,
      0x21, 0x30, 0x31, 0x46, 0x41, 0x32, 0x38, 0x32,
  };

  const auto identifiers = ParseDeviceIdentifiersVpdPage(page);

  ASSERT_EQ(identifiers.size(), 2U);
  EXPECT_EQ(identifiers[0].front(), 0x02);
  EXPECT_EQ(identifiers[0][1], 0x01);
  EXPECT_EQ(identifiers[0].back(), 0x32);
  EXPECT_EQ(DescribeDeviceIdentifier(identifiers[0]),
            "HPE Ultrium 9-SCSI 01FAB12812");
}

TEST(BareosSetupStepsShared, ParsesLibraryDriveIdentifiers)
{
  const std::vector<uint8_t> status = {
      0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0xd0, 0x04, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0xc8, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50, 0x45, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75, 0x6d, 0x20,
      0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x34, 0x45, 0x37, 0x37,
      0x46, 0x45, 0x34, 0x31, 0x35, 0x46, 0x01, 0x01, 0x08, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50,
      0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75,
      0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x34, 0x33,
      0x35, 0x42, 0x30, 0x30, 0x39, 0x42, 0x34, 0x33, 0x01, 0x02, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x22,
      0x48, 0x50, 0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72,
      0x69, 0x75, 0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20,
      0x34, 0x37, 0x43, 0x31, 0x33, 0x35, 0x41, 0x31, 0x34, 0x37, 0x01, 0x03,
      0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01,
      0x00, 0x22, 0x48, 0x50, 0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c,
      0x74, 0x72, 0x69, 0x75, 0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49,
      0x20, 0x20, 0x30, 0x31, 0x46, 0x41, 0x42, 0x31, 0x32, 0x38, 0x31, 0x32,
  };

  const auto identifiers = ParseTapeLibraryDriveIdentifiers(status);

  ASSERT_EQ(identifiers.size(), 4U);
  EXPECT_EQ(identifiers[0].element_address, 256);
  ASSERT_EQ(identifiers[0].device_identifiers.size(), 1U);
  EXPECT_EQ(identifiers[0].device_identifiers[0][0], 0x02);
  EXPECT_EQ(identifiers[3].device_identifiers[0].back(), 0x32);
}

TEST(BareosSetupStepsShared, MatchesLibraryDrivesByIdentifier)
{
  const auto drive_page = ParseDeviceIdentifiersVpdPage({
      0x01, 0x83, 0x00, 0x2a, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50, 0x45,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75,
      0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x30,
      0x31, 0x46, 0x41, 0x42, 0x31, 0x32, 0x38, 0x31, 0x32,
  });
  const auto library = ParseTapeLibraryDriveIdentifiers({
      0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3a, 0x04, 0x00, 0x00,
      0x32, 0x00, 0x00, 0x00, 0x32, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x22, 0x48,
      0x50, 0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72,
      0x69, 0x75, 0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20,
      0x20, 0x30, 0x31, 0x46, 0x41, 0x42, 0x31, 0x32, 0x38, 0x31, 0x32,
  });

  ASSERT_EQ(drive_page.size(), 1U);
  ASSERT_EQ(library.size(), 1U);

  const std::vector<TapeChangerInfo> changers = {
      {"/dev/tape/by-id/changer-0",
       "/dev/sg4",
       "Changer",
       "",
       "",
       "",
       "",
       "",
       {},
       {},
       library},
  };
  const std::vector<TapeDriveInfo> drives = {
      {"/dev/tape/by-id/drive-0-nst",
       "/dev/nst0",
       "Drive0",
       "",
       "",
       "",
       "",
       "",
       {},
       drive_page},
  };

  const auto assignments = SuggestTapeAssignments(changers, drives);

  ASSERT_EQ(assignments.size(), 1U);
  EXPECT_EQ(assignments[0].drive_paths,
            (std::vector<std::string>{"/dev/tape/by-id/drive-0-nst"}));
}

TEST(BareosSetupStepsShared, AllowsReusedTapeDriveAcrossLibraries)
{
  DiskStorageConfig disk_config;
  TapeStorageConfig tape_config;
  tape_config.enabled = true;
  tape_config.assignments = {
      {"/dev/changer0", {"/dev/nst0"}},
      {"/dev/changer1", {"/dev/nst0"}},
  };

  std::string error;
  EXPECT_TRUE(ValidateStorageConfig(disk_config, tape_config, error));
  EXPECT_TRUE(error.empty());
}

TEST(BareosSetupStepsShared, BuildsTapeStorageScript)
{
  DiskStorageConfig disk_config;
  TapeStorageConfig tape_config;
  tape_config.enabled = true;
  tape_config.assignments = {
      {"/dev/tape/by-id/changer-0",
       {"/dev/tape/by-id/drive-0-nst", "/dev/tape/by-id/drive-1-nst"}},
  };

  const auto script = BuildConfigureStorageScript(disk_config, tape_config);

  EXPECT_NE(script.find("Changer Device = /dev/tape/by-id/changer-0"),
            std::string::npos);
  EXPECT_NE(script.find("ArchiveDevice = /dev/tape/by-id/drive-0-nst"),
            std::string::npos);
  EXPECT_NE(script.find("ArchiveDevice = /dev/tape/by-id/drive-1-nst"),
            std::string::npos);
}

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

TEST(BareosSetupStepsShared, BuildsWebServerServiceNameForPackageManager)
{
  EXPECT_EQ(BuildWebServerServiceName("apt"), "apache2");
  EXPECT_EQ(BuildWebServerServiceName("dnf"), "httpd");
  EXPECT_EQ(BuildWebServerServiceName("yum"), "httpd");
  EXPECT_EQ(BuildWebServerServiceName("zypper"), "httpd");
}

TEST(BareosSetupStepsShared, BuildsWebServerHttpsSetupForAptOnly)
{
  EXPECT_EQ(BuildWebServerHttpsSetupCmds("apt"),
            (std::vector<std::vector<std::string>>{
                {"a2enmod", "ssl"}, {"a2ensite", "default-ssl"}}));
  EXPECT_TRUE(BuildWebServerHttpsSetupCmds("dnf").empty());
  EXPECT_TRUE(BuildWebServerHttpsSetupCmds("yum").empty());
  EXPECT_TRUE(BuildWebServerHttpsSetupCmds("zypper").empty());
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

TEST(BareosSetupStepsShared, BuildsPackageCacheUpdateForAptOnly)
{
  EXPECT_EQ(BuildPackageCacheUpdateCmd("apt"),
            (std::vector<std::string>{"apt-get", "update"}));
  EXPECT_TRUE(BuildPackageCacheUpdateCmd("dnf").empty());
  EXPECT_TRUE(BuildPackageCacheUpdateCmd("yum").empty());
  EXPECT_TRUE(BuildPackageCacheUpdateCmd("zypper").empty());
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

TEST(BareosSetupStepsShared, JoinedCommandStillContainsSecretForRedaction)
{
  const auto command
      = std::vector<std::string>{"curl", "--user", "login:hunter2"};
  const std::string display = JoinCommandForDisplay(command);
  EXPECT_EQ(RedactSetupSecrets(display, {"login:hunter2"}),
            "curl --user [redacted]");
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

// Prepends a temp directory of no-op shim executables (named after the
// given tools) to PATH, so RunStep()'s real orchestration logic (which
// commands to run, and in what order) can be exercised without touching
// the real system. Each shim just appends its own invocation to a shared
// log file and exits 0.
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
      out << "#!/bin/sh\necho \"" << tool << " $*\" >> '" << log_path_.string()
          << "'\nexit 0\n";
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

// Runs one setup step against a real (but unconnected-to-a-browser)
// WsCodec so RunSetupStepForTests() has a valid fd to send "output"/"done"
// messages to; the messages themselves are discarded since these tests
// only assert on which commands were executed.
int RunStepDiscardingOutput(const std::string& step,
                            const std::string& json_message = "{}")
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
    const int result = RunSetupStepForTests(sockets[0], step, json_message);
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
  // "systemctl reload bareos-dir" used to fail with "Unit cannot be
  // reloaded because it is inactive." This asserts the catalog step's
  // command sequence still ends with enabling all three daemons.
  // "sudo" is included as a fake shim too: every command Run() issues is
  // wrapped with "sudo" unless already root (see command_runner.cc's
  // IsRoot() check), so a real "sudo" would otherwise intercept these
  // commands before they ever reach the other fake shims.
  FakeToolPath fake_tools(
      {"sudo", "postgresql-setup", "systemctl", "su", "install"});
  ASSERT_EQ(RunStepDiscardingOutput("catalog"), 0);
  const auto commands = fake_tools.LoggedCommands();
  ASSERT_FALSE(commands.empty());
  const auto enable_it
      = std::find_if(commands.begin(), commands.end(), [](const auto& line) {
          return line.find(
                     "systemctl enable --now bareos-dir bareos-sd bareos-fd")
                 != std::string::npos;
        });
  ASSERT_NE(enable_it, commands.end());
  // The daemons must be enabled only after the catalog scripts have run,
  // not before.
  const auto grant_it
      = std::find_if(commands.begin(), commands.end(), [](const auto& line) {
          return line.find("grant_bareos_privileges") != std::string::npos;
        });
  ASSERT_NE(grant_it, commands.end());
  EXPECT_LT(grant_it - commands.begin(), enable_it - commands.begin());
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
  std::error_code ec;
  std::filesystem::remove_all(fake_dir, ec);

  ASSERT_EQ(result, 0);
  EXPECT_NE(content.find("Profile = \"webui-admin\""), std::string::npos);
  EXPECT_NE(content.find("TLS Enable = No"), std::string::npos);
  EXPECT_EQ(content.find("TLS Enable = yes"), std::string::npos);
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
  FakeToolPath fake_tools({"sudo", "install", "chown", "systemctl"});
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
  FakeToolPath fake_tools({"sudo", os.pkg_mgr});
  RunStepDiscardingOutput("packages");
  const auto commands = fake_tools.LoggedCommands();
  ASSERT_FALSE(commands.empty());
  EXPECT_NE(std::find_if(commands.begin(), commands.end(),
                         [&os](const auto& line) {
                           return line.find(os.pkg_mgr) != std::string::npos;
                         }),
            commands.end());
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
  FakeToolPath fake_tools({"sudo", "systemctl"});
  ASSERT_EQ(RunStepDiscardingOutput("smoke_test"), 0);
  const auto commands = fake_tools.LoggedCommands();
  for (const auto& service :
       {"bareos-dir", "bareos-sd", "bareos-fd", "bareos-webui-proxy"}) {
    const std::string expected = std::string("systemctl is-active ") + service;
    EXPECT_NE(std::find_if(commands.begin(), commands.end(),
                           [&expected](const auto& line) {
                             return line.find(expected) != std::string::npos;
                           }),
              commands.end());
  }
  const auto os = DetectOs();
  const std::string web_server
      = "systemctl is-active " + BuildWebServerServiceName(os.pkg_mgr);
  EXPECT_NE(std::find_if(commands.begin(), commands.end(),
                         [&web_server](const auto& line) {
                           return line.find(web_server) != std::string::npos;
                         }),
            commands.end());
}
