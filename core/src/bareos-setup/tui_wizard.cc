/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.
*/
#include "tui_wizard.h"

#include <iostream>
#include <string>

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

  std::cout << "Disk storage: /var/lib/bareos/storage\n";
  if (!Run(BuildInstallCmd(os.pkg_mgr, BuildDefaultPackageList()), dry_run)) {
    std::cerr << "Package installation failed.\n";
    return 1;
  }
  if (!dry_run) {
    for (const auto& script :
         {std::vector<std::string>{
              "/usr/lib/bareos/scripts/create_bareos_database"},
          std::vector<std::string>{
              "/usr/lib/bareos/scripts/make_bareos_tables"},
          std::vector<std::string>{
              "/usr/lib/bareos/scripts/grant_bareos_privileges"}}) {
      if (!Run(script, false)) return 1;
    }
  }

  const std::string admin_password = GenerateSetupSecret();
  if (!dry_run) {
    const std::string resource
        = "Console {\n  Name = admin\n  Password = \"" + admin_password
          + "\"\n  Profile = \"webui-admin\"\n  TLS Enable = yes\n}\n";
    if (RunCommandWithInput({"install", "-D", "-m", "0640", "/dev/stdin",
                             "/etc/bareos/bareos-dir.d/console/admin.conf"},
                            resource, true,
                            [](const std::string&, const std::string&) {})
        != 0) {
      return 1;
    }
  }
  if (!Run({"systemctl", "enable", "--now", "bareos-dir", "bareos-sd",
            "bareos-fd", "bareos-webui-proxy"},
           dry_run)) {
    return 1;
  }
  std::cout << "\nSetup complete. WebUI username: admin\n"
            << "Initial WebUI password: " << admin_password << "\n";
  return 0;
}
