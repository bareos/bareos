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

#include "command_runner.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <sys/random.h>

namespace {

std::string MajorVersion(const std::string& version)
{
  const auto dot = version.find('.');
  return dot == std::string::npos ? version : version.substr(0, dot);
}

bool IsElDistro(const std::string& distro)
{
  return distro == "almalinux" || distro == "centos" || distro == "ol"
         || distro == "openela" || distro == "oracle" || distro == "rhel"
         || distro == "rocky";
}

std::string RepoBaseUrl(const std::string& repo_type)
{
  return (repo_type == "subscription")
             ? "https://download.bareos.com/bareos/release/latest"
             : "https://download.bareos.org/current";
}

std::string CurlConfigQuote(const std::string& value)
{
  std::string quoted = "\"";
  for (const char ch : value) {
    if (ch == '\\' || ch == '"') quoted += '\\';
    quoted += ch;
  }
  quoted += '"';
  return quoted;
}

void FillRandomBytes(std::vector<unsigned char>& random)
{
  size_t offset = 0;
  while (offset < random.size()) {
    const ssize_t result
        = getrandom(random.data() + offset, random.size() - offset, 0);
    if (result > 0) {
      offset += static_cast<size_t>(result);
      continue;
    }
    if (result < 0 && errno == EINTR) continue;
    break;
  }
  if (offset == random.size()) return;

  std::ifstream urandom("/dev/urandom", std::ios::binary);
  if (!urandom) { throw std::runtime_error("Unable to open /dev/urandom"); }
  urandom.read(reinterpret_cast<char*>(random.data() + offset),
               static_cast<std::streamsize>(random.size() - offset));
  if (!urandom) {
    throw std::runtime_error(
        "Unable to obtain cryptographically secure random data");
  }
}

}  // namespace

std::string CapFirst(std::string s)
{
  if (!s.empty()) s[0] = static_cast<char>(std::toupper(s[0]));
  return s;
}

std::string Trim(std::string value)
{
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!value.empty() && is_space(value.front())) value.erase(0, 1);
  while (!value.empty() && is_space(value.back())) value.pop_back();
  return value;
}

std::vector<std::string> BuildDefaultPackageList(const std::string& pkg_mgr)
{
  std::vector<std::string> packages = {"bareos-filedaemon",
                                       "bareos-director",
                                       "bareos-storage",
                                       "bareos-storage-tape",
                                       "bareos-storage-dedupable",
                                       "bareos-database-tools",
                                       "bareos-tools",
                                       "bareos-webui-new",
                                       "bareos-webui-proxy"};
  if (pkg_mgr == "dnf" || pkg_mgr == "yum") { packages.push_back("mod_ssl"); }
  // The Bareos catalog packages do not pull in a local PostgreSQL server
  // (Bareos also supports remote catalogs), so the wizard has to add it
  // explicitly for a single-host setup. Package names/splits differ by
  // distribution family.
  if (pkg_mgr == "apt") {
    // Debian/Ubuntu's "postgresql" metapackage includes both server and
    // client (psql) and auto-initializes a default cluster on install.
    packages.push_back("postgresql");
  } else {
    // Fedora/RHEL/openSUSE family: postgresql-server pulls in the
    // postgresql client package (providing psql) as a dependency.
    packages.push_back("postgresql-server");
  }
  return packages;
}

std::vector<std::string> BuildPackageListWithoutTapeStorage(
    const std::string& pkg_mgr)
{
  auto packages = BuildDefaultPackageList(pkg_mgr);
  packages.erase(
      std::remove(packages.begin(), packages.end(), "bareos-storage-tape"),
      packages.end());
  return packages;
}

std::vector<std::string> BuildPackageListWithoutPostgresServer(
    const std::string& pkg_mgr)
{
  auto packages = BuildDefaultPackageList(pkg_mgr);
  packages.erase(std::remove_if(packages.begin(), packages.end(),
                                [](const std::string& package) {
                                  return package == "postgresql"
                                         || package == "postgresql-server";
                                }),
                 packages.end());
  return packages;
}

std::vector<std::string> BuildCatalogInitScripts(const std::string& pkg_mgr)
{
  if (pkg_mgr == "apt") return {};
  return {"/usr/lib/bareos/scripts/create_bareos_database",
          "/usr/lib/bareos/scripts/make_bareos_tables",
          "/usr/lib/bareos/scripts/grant_bareos_privileges"};
}

std::vector<std::string> BuildPostgresInitCmd()
{
  if (IsToolInPath("postgresql-setup")) {
    return {"postgresql-setup", "--initdb"};
  }
  return {};
}

std::vector<std::string> BuildRunAsPostgresCmd(const std::string& script)
{
  return {"su", "postgres", "-c", script};
}

std::string BuildRepoOsPath(const std::string& distro,
                            const std::string& version)
{
  if (IsElDistro(distro)) return "EL_" + MajorVersion(version);
  if (distro == "opensuse-leap" || distro == "opensuse-tumbleweed"
      || distro == "sles") {
    return "SUSE_" + MajorVersion(version);
  }
  if (distro == "ubuntu") return "xUbuntu_" + version;
  return CapFirst(distro) + "_" + version;
}

std::vector<std::string> BuildAddRepoCmd(const std::string& distro,
                                         const std::string& version,
                                         const std::string& repo_type,
                                         bool read_curl_config_from_stdin)
{
  return BuildAddRepoCmdForPath(BuildRepoOsPath(distro, version), repo_type,
                                read_curl_config_from_stdin);
}

const std::vector<std::string>& KnownRepoOsPaths()
{
  static const std::vector<std::string> paths{
      "EL_10",         "EL_9",      "EL_8",          "Debian_13",
      "Debian_12",     "Debian_11", "xUbuntu_26.04", "xUbuntu_24.04",
      "xUbuntu_22.04", "Fedora_44", "Fedora_43",     "SUSE_16",
      "SUSE_15",
  };
  return paths;
}

bool IsValidRepoOsPath(const std::string& value)
{
  if (!IsSafeSetupIdentifier(value)) return false;
  // IsSafeSetupIdentifier() permits '.', so ".." would pass and escape the
  // release directory of the constructed download URL.
  if (value.find("..") != std::string::npos) return false;
  const auto is_edge_ok = [](char c) { return std::isalnum(c) != 0; };
  return is_edge_ok(value.front()) && is_edge_ok(value.back());
}

bool IsSuseRepoOsPath(const std::string& repo_os_path)
{
  return repo_os_path.rfind("SUSE_", 0) == 0;
}

std::vector<std::string> SuggestRepoOsPaths(const OsInfo& info)
{
  // Determine the family from the ID first, then from ID_LIKE.
  std::string prefix;
  const auto family_of = [](const std::string& id) -> std::string {
    if (IsElDistro(id) || id == "fedora") {
      return id == "fedora" ? "Fedora_" : "EL_";
    }
    if (id == "sles" || id == "suse" || id == "opensuse"
        || id == "opensuse-leap" || id == "opensuse-tumbleweed") {
      return "SUSE_";
    }
    if (id == "ubuntu") return "xUbuntu_";
    if (id == "debian") return "Debian_";
    return {};
  };

  prefix = family_of(info.distro);
  for (const auto& like : info.id_like) {
    if (!prefix.empty()) break;
    prefix = family_of(like);
  }
  if (prefix.empty()) return {};

  // The version is only a guess: a derivative's VERSION_ID often does not
  // match the version its family's repository is named after.
  const std::string guess
      = prefix
        + (prefix == "xUbuntu_" ? info.version : MajorVersion(info.version));

  std::vector<std::string> suggestions;
  const auto& known = KnownRepoOsPaths();
  if (std::find(known.begin(), known.end(), guess) != known.end()) {
    suggestions.push_back(guess);
  }
  for (const auto& path : known) {
    if (path.rfind(prefix, 0) != 0) continue;
    if (std::find(suggestions.begin(), suggestions.end(), path)
        != suggestions.end()) {
      continue;
    }
    suggestions.push_back(path);
  }
  return suggestions;
}

std::vector<std::string> BuildAddRepoCmdForPath(
    const std::string& repo_os_path,
    const std::string& repo_type,
    bool read_curl_config_from_stdin)
{
  const std::string script_url = RepoBaseUrl(repo_type) + "/" + repo_os_path
                                 + "/add_bareos_repositories.sh";

  std::vector<std::string> command
      = {"curl", "--fail", "--silent", "--show-error", "--location"};
  if (read_curl_config_from_stdin)
    command.insert(command.end(), {"--config", "-"});
  command.emplace_back(script_url);
  return command;
}

std::vector<std::string> BuildRepoPathProbeCmd(const std::string& repo_os_path,
                                               const std::string& repo_type,
                                               bool read_curl_config_from_stdin)
{
  const std::string script_url = RepoBaseUrl(repo_type) + "/" + repo_os_path
                                 + "/add_bareos_repositories.sh";

  std::vector<std::string> command
      = {"curl",       "--fail", "--silent",   "--show-error",
         "--location", "--head", "--max-time", "15"};
  if (read_curl_config_from_stdin)
    command.insert(command.end(), {"--config", "-"});
  command.emplace_back(script_url);
  return command;
}

std::string BuildCurlUserConfig(const std::string& login,
                                const std::string& password)
{
  return "user = " + CurlConfigQuote(login + ":" + password) + "\n";
}

std::vector<std::string> BuildNetworkCheckCmd(const std::string& repo_type)
{
  if (repo_type != "community") return {};

  const std::string base = "https://download.bareos.org/current";

  return {"curl",   "--fail",     "--silent", "--show-error",
          "--head", "--max-time", "10",       base};
}

std::string SetupAdminConfigPath()
{
  return "/etc/bareos/bareos-dir.d/console/admin.conf";
}

std::string SetupProxyConfigPath()
{
  return "/etc/bareos-webui-proxy/bareos-webui-proxy.ini";
}

std::vector<std::string> SetupOwnedConfigPaths()
{
  return {SetupAdminConfigPath(), SetupProxyConfigPath()};
}

std::vector<std::string> BuildFileAbsentCheckCmd(const std::string& path)
{
  // The wizard-owned configuration directories are usually root-only, so
  // the check has to run through the privileged command runner instead of
  // stat()ing the path directly.
  return {"sh", "-c", "test ! -e \"$1\"", "bareos-setup", path};
}

std::string BuildExistingSetupConfigError(
    const std::vector<std::string>& existing_paths)
{
  std::ostringstream message;
  message << "Refusing to continue because existing Bareos setup "
             "configuration would be overwritten:";
  for (const auto& path : existing_paths) message << " " << path;
  message << ". Move or back up the file(s) and rerun bareos-setup.";
  return message.str();
}

std::vector<std::string> BuildMtxAvailabilityCheckCmd()
{
  return {"zypper", "--non-interactive", "search", "--match-exact",
          "--type", "package",           "mtx"};
}

std::string BuildWebServerServiceName(const std::string& pkg_mgr)
{
  return (pkg_mgr == "apt" || pkg_mgr == "zypper") ? "apache2" : "httpd";
}

std::vector<std::vector<std::string>> BuildWebServerHttpsSetupCmds(
    const std::string& pkg_mgr)
{
  if (pkg_mgr == "apt") {
    return {{"a2enmod", "ssl"}, {"a2ensite", "default-ssl"}};
  }
  if (pkg_mgr == "zypper") {
    return {{"a2enmod", "ssl"},
            {"a2enflag", "SSL"},
            {"sh", "-c",
             "install -d -m 0755 /etc/apache2/ssl.crt && "
             "install -d -m 0700 /etc/apache2/ssl.key && "
             "test -s /etc/apache2/ssl.crt/bareos-setup.crt || "
             "openssl req -x509 -nodes -newkey rsa:2048 -days 397 "
             "-subj /CN=localhost "
             "-keyout /etc/apache2/ssl.key/bareos-setup.key "
             "-out /etc/apache2/ssl.crt/bareos-setup.crt && "
             "chmod 0600 /etc/apache2/ssl.key/bareos-setup.key && "
             "cat >/etc/apache2/vhosts.d/bareos-setup-ssl.conf <<'EOF'\n"
             "<IfDefine SSL>\n"
             "<IfDefine !NOSSL>\n"
             "<VirtualHost _default_:443>\n"
             "  DocumentRoot \"/srv/www/htdocs\"\n"
             "  ErrorLog /var/log/apache2/error_log\n"
             "  TransferLog /var/log/apache2/access_log\n"
             "  SSLEngine on\n"
             "  SSLCertificateFile /etc/apache2/ssl.crt/bareos-setup.crt\n"
             "  SSLCertificateKeyFile /etc/apache2/ssl.key/bareos-setup.key\n"
             "</VirtualHost>\n"
             "</IfDefine>\n"
             "</IfDefine>\n"
             "EOF"}};
  }
  return {};
}

std::vector<std::string> BuildBareosDaemonServiceNames(
    const std::string& pkg_mgr)
{
  if (pkg_mgr == "apt") {
    return {"bareos-director", "bareos-storage", "bareos-filedaemon"};
  }
  return {"bareos-dir", "bareos-sd", "bareos-fd"};
}

std::vector<std::string> BuildPackageCacheUpdateCmd(const std::string& pkg_mgr)
{
  if (pkg_mgr == "apt") { return {"apt-get", "update"}; }
  if (pkg_mgr == "zypper") {
    return {"zypper", "--non-interactive", "--gpg-auto-import-keys", "refresh"};
  }
  return {};
}

std::vector<std::string> BuildInstallCmd(
    const std::string& pkg_mgr,
    const std::vector<std::string>& packages)
{
  const auto& selected_packages
      = packages.empty() ? BuildDefaultPackageList(pkg_mgr) : packages;

  if (pkg_mgr == "apt") {
    std::vector<std::string> cmd = {"apt-get", "install", "-y"};
    cmd.insert(cmd.end(), selected_packages.begin(), selected_packages.end());
    return cmd;
  } else if (pkg_mgr == "dnf") {
    std::vector<std::string> cmd = {"dnf", "install", "-y"};
    cmd.insert(cmd.end(), selected_packages.begin(), selected_packages.end());
    return cmd;
  } else if (pkg_mgr == "yum") {
    std::vector<std::string> cmd = {"yum", "install", "-y"};
    cmd.insert(cmd.end(), selected_packages.begin(), selected_packages.end());
    return cmd;
  } else if (pkg_mgr == "zypper") {
    std::vector<std::string> cmd
        = {"zypper", "--non-interactive", "--gpg-auto-import-keys", "install"};
    cmd.insert(cmd.end(), selected_packages.begin(), selected_packages.end());
    return cmd;
  }
  return {"echo", "Unsupported package manager: " + pkg_mgr};
}

bool IsSupportedSetupPlatform(const std::string& distro,
                              const std::string& package_manager)
{
  static const std::set<std::string> supported{
      "almalinux",
      "centos",
      "debian",
      "fedora",
      "ol",
      "openela",
      "oracle",
      "rhel",
      "rocky",
      "sles",
      "ubuntu",
      "opensuse-leap",
      "opensuse-tumbleweed",
  };
  return supported.contains(distro)
         && IsSupportedPackageManager(package_manager);
}

bool IsSupportedPackageManager(const std::string& package_manager)
{
  return package_manager == "apt" || package_manager == "dnf"
         || package_manager == "yum" || package_manager == "zypper";
}

bool IsSafeSetupIdentifier(const std::string& value)
{
  if (value.empty() || value.size() > 64) return false;
  return std::all_of(value.begin(), value.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '-' || c == '.';
  });
}

std::string GenerateSetupSecret(size_t length)
{
  static constexpr char alphabet[]
      = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%";
  if (length == 0 || length > 256) {
    throw std::invalid_argument("Invalid setup secret length");
  }

  std::vector<unsigned char> random(length);
  FillRandomBytes(random);
  std::string result;
  result.reserve(length);
  constexpr size_t alphabet_size = sizeof(alphabet) - 1;
  for (const auto byte : random) result += alphabet[byte % alphabet_size];
  return result;
}

std::string RedactSetupSecrets(std::string value,
                               const std::vector<std::string>& secrets)
{
  for (const auto& secret : secrets) {
    if (secret.empty()) continue;
    size_t pos = 0;
    while ((pos = value.find(secret, pos)) != std::string::npos) {
      value.replace(pos, secret.size(), "[redacted]");
      pos += sizeof("[redacted]") - 1;
    }
  }
  return value;
}

std::string JoinCommandForDisplay(const std::vector<std::string>& argv)
{
  constexpr std::string_view needs_quoting = " \t\n\"'\\$`|&;<>()[]{}*?~!#";
  std::string result;
  for (const auto& arg : argv) {
    if (!result.empty()) result += ' ';
    const bool must_quote
        = arg.empty() || arg.find_first_of(needs_quoting) != std::string::npos;
    if (!must_quote) {
      result += arg;
      continue;
    }
    result += '\'';
    for (const char c : arg) {
      // Close the quote, emit an escaped literal quote, then reopen it.
      if (c == '\'')
        result += "'\\''";
      else
        result += c;
    }
    result += '\'';
  }
  return result;
}

bool IsValidSetupOrigin(const std::string& origin, const std::string& host)
{
  // Same-origin check: the browser's Origin header must match the Host
  // header of the very request it sent. This works no matter which
  // interface/hostname the wizard is listening on (loopback-only by
  // default, or an admin-chosen address via --listen), without needing a
  // fixed allow-list of hostnames.
  if (host.empty()) return false;
  return origin == "http://" + host;
}
