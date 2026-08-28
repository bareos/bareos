/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.
*/
#include "tui_wizard.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "command_runner.h"
#include "os_detector.h"
#include "setup_steps.h"

namespace {

std::string Prompt(const std::string& label, const std::string& fallback = {})
{
  std::cout << label;
  if (!fallback.empty()) std::cout << " [" << fallback << "]";
  std::cout << ": " << std::flush;
  std::string value;
  if (!std::getline(std::cin, value) || value.empty()) return fallback;
  return value;
}

bool Run(const std::vector<std::string>& command, bool dry_run)
{
  if (dry_run) {
    std::cout << "[preview] " << command.front() << " (approved command)\n";
    return true;
  }
  return RunCommand(command, true,
                    [](const std::string& line, const std::string&) {
                      std::cout << line << "\n";
                    })
         == 0;
}

}  // namespace

int RunTuiWizard(bool dry_run)
{
  std::cout << "Bareos Setup\n\n";
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    std::cerr << "This Linux distribution is not supported.\n";
    return 1;
  }
  std::cout << "Detected " << os.pretty_name << " (" << os.pkg_mgr << ")\n";

  const auto repository
      = Prompt("Repository (community/subscription)", "subscription");
  if (repository != "community" && repository != "subscription") {
    std::cerr << "Choose community or subscription.\n";
    return 1;
  }
  std::string login;
  std::string password;
  if (repository == "subscription") {
    login = Prompt("Subscription login");
    password = Prompt("Subscription password");
    if (login.empty() || password.empty()) return 1;
  }

  std::cout << "Checking connectivity to the Bareos download server...\n";
  if (!Run(BuildNetworkCheckCmd(repository), dry_run)) return 1;

  std::filesystem::path repository_script;
  if (dry_run) {
    repository_script = "/tmp/bareos-setup-repository.sh";
  } else {
    std::string pattern = (std::filesystem::temp_directory_path()
                           / "bareos-setup-repository-XXXXXX")
                              .string();
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    const int fd = ::mkstemp(name.data());
    if (fd < 0) {
      std::cerr << "Unable to create a private repository setup file.\n";
      return 1;
    }
    ::close(fd);
    ::chmod(name.data(), 0600);
    repository_script = name.data();
  }

  auto add_repo_cmd
      = BuildAddRepoCmd(os.distro, os.version, repository, login, password);
  add_repo_cmd.insert(add_repo_cmd.end() - 1,
                      {"--output", repository_script.string()});
  if (!Run(add_repo_cmd, dry_run)) {
    if (!dry_run) std::filesystem::remove(repository_script);
    return 1;
  }
  if (!Run({"bash", repository_script.string()}, dry_run)) {
    if (!dry_run) std::filesystem::remove(repository_script);
    return 1;
  }
  if (!dry_run) std::filesystem::remove(repository_script);
  const auto update_cmd = BuildPackageCacheUpdateCmd(os.pkg_mgr);
  if (!update_cmd.empty()) {
    std::cout << "Refreshing package metadata.\n";
    if (!Run(update_cmd, dry_run)) return 1;
  }

  std::cout << "Disk storage: /var/lib/bareos/storage\n";
  if (os.pkg_mgr == "apt") {
    std::cout << "Installing PostgreSQL package.\n";
    if (!Run(BuildInstallCmd(os.pkg_mgr, {"postgresql"}), dry_run)) return 1;
    if (!Run({"systemctl", "enable", "--now", "postgresql"}, dry_run)) {
      return 1;
    }
  }
  const auto packages = os.pkg_mgr == "apt"
                            ? BuildPackageListWithoutPostgresServer(os.pkg_mgr)
                            : BuildDefaultPackageList(os.pkg_mgr);
  if (!Run(BuildInstallCmd(os.pkg_mgr, packages), dry_run)) {
    std::cerr << "Package installation failed.\n";
    return 1;
  }
  if (!dry_run) {
    const auto init_cmd = BuildPostgresInitCmd();
    if (!init_cmd.empty() && !Run(init_cmd, false)) return 1;
    if (!Run({"systemctl", "enable", "--now", "postgresql"}, false)) return 1;
    // Non-Debian packages need the manual catalog scripts. Debian/Ubuntu
    // packages run dbconfig-common during package configuration instead.
    for (const auto& script : BuildCatalogInitScripts(os.pkg_mgr)) {
      if (!Run(BuildRunAsPostgresCmd(script), false)) return 1;
    }
  }

  const std::string admin_password = GenerateSetupSecret();
  if (!dry_run) {
    const std::string resource
        = "Console {\n  Name = admin\n  Password = \"" + admin_password
          + "\"\n  Profile = \"webui-admin\"\n  TLS Enable = No\n}\n";
    if (RunCommandWithInput({"install", "-D", "-m", "0640", "/dev/stdin",
                             "/etc/bareos/bareos-dir.d/console/admin.conf"},
                            resource, true,
                            [](const std::string&, const std::string&) {})
        != 0) {
      return 1;
    }
    if (!Run({"chown", "root:bareos",
              "/etc/bareos/bareos-dir.d/console/admin.conf"},
             false)) {
      return 1;
    }
    const std::string proxy_config
        = "[listen]\n"
          "address = 127.0.0.1\n"
          "port = 9104\n"
          "\n"
          "[bareos-dir]\n"
          "address = 127.0.0.1\n"
          "port = 9101\n"
          "director_name = bareos-dir\n"
          "tls_psk_disable = no\n";
    const std::string proxy_path
        = "/etc/bareos-webui-proxy/bareos-webui-proxy.ini";
    if (RunCommandWithInput(
            {"install", "-D", "-m", "0640", "/dev/stdin", proxy_path},
            proxy_config, true, [](const std::string&, const std::string&) {})
        != 0) {
      return 1;
    }
    // bareos-webui-proxy.service runs as User=bareos/Group=bareos (not
    // root), so the config file must be group-readable by "bareos" or
    // the service fails to start with "cannot load" on every restart.
    if (!Run({"chown", "root:bareos", proxy_path}, false)) return 1;
  }
  if (!Run({"systemctl", "enable", "--now", "bareos-dir", "bareos-sd",
            "bareos-fd", "bareos-webui-proxy"},
           dry_run)) {
    return 1;
  }
  if (!Run({"systemctl", "enable", "--now",
            BuildWebServerServiceName(os.pkg_mgr)},
           dry_run)) {
    return 1;
  }
  for (const auto& service :
       {"bareos-dir", "bareos-sd", "bareos-fd", "bareos-webui-proxy"}) {
    if (!Run({"systemctl", "is-active", service}, dry_run)) return 1;
  }
  if (!Run({"systemctl", "is-active", BuildWebServerServiceName(os.pkg_mgr)},
           dry_run)) {
    return 1;
  }
  std::cout << "\nSetup complete. WebUI username: admin\n"
            << "Initial WebUI password: " << admin_password << "\n";
  return 0;
}
