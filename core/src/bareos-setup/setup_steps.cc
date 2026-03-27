/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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

#include <cctype>
#include <string>
#include <vector>

std::string CapFirst(std::string s)
{
  if (!s.empty()) s[0] = static_cast<char>(std::toupper(s[0]));
  return s;
}

std::vector<std::string> BuildAddRepoCmd(const std::string& distro,
                                         const std::string& version,
                                         const std::string& repo_type,
                                         const std::string& login,
                                         const std::string& password)
{
  const std::string base
      = (repo_type == "subscription")
            ? "https://download.bareos.com/bareos/release/latest"
            : "https://download.bareos.org/current";

  const std::string script_url = base + "/" + CapFirst(distro) + "_" + version
                                 + "/add_bareos_repositories.sh";

  std::string curl_auth;
  if (repo_type == "subscription" && !login.empty())
    curl_auth = " -u '" + login + ":" + password + "'";

  return {"bash", "-c",
          "curl -fsSL" + curl_auth + " " + script_url + " | bash"};
}

std::vector<std::string> BuildInstallCmd(
    const std::string& pkg_mgr,
    const std::vector<std::string>& packages)
{
  if (pkg_mgr == "apt") {
    std::vector<std::string> cmd = {"apt-get", "install", "-y"};
    cmd.insert(cmd.end(), packages.begin(), packages.end());
    return cmd;
  } else if (pkg_mgr == "dnf") {
    std::vector<std::string> cmd = {"dnf", "install", "-y"};
    cmd.insert(cmd.end(), packages.begin(), packages.end());
    return cmd;
  } else if (pkg_mgr == "yum") {
    std::vector<std::string> cmd = {"yum", "install", "-y"};
    cmd.insert(cmd.end(), packages.begin(), packages.end());
    return cmd;
  } else if (pkg_mgr == "zypper") {
    std::vector<std::string> cmd = {"zypper", "--non-interactive", "install"};
    cmd.insert(cmd.end(), packages.begin(), packages.end());
    return cmd;
  }
  return {"echo", "Unsupported package manager: " + pkg_mgr};
}

std::vector<std::string> BuildDbCmd()
{
  return {"bash", "-c",
          "/usr/lib/bareos/scripts/create_bareos_database && "
          "/usr/lib/bareos/scripts/make_bareos_tables && "
          "/usr/lib/bareos/scripts/grant_bareos_privileges"};
}

std::vector<std::string> BuildAdminUserCmd(const std::string& username,
                                           const std::string& password)
{
  const std::string console_conf
      = "/etc/bareos/bareos-dir.d/console/" + username + ".conf";
  return {
      "bash", "-c",
      "cat > " + console_conf + " << 'BAREOS_EOF'\n"
      "Console {\n"
      "  Name = " + username + "\n"
      "  Password = \"" + password + "\"\n"
      "  Profile = \"webui-admin\"\n"
      "  TLS Enable = No\n"
      "}\n"
      "BAREOS_EOF\n"
      "chmod 640 " + console_conf + " && "
      "chown root:bareos " + console_conf + " && "
      "systemctl reload bareos-dir 2>/dev/null || true"
  };
}
