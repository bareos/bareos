/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
   License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#include "tui_wizard.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <termios.h>
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

std::string PromptSecret(const std::string& label)
{
  std::cout << label << ": " << std::flush;
  termios old_term{};
  bool restore_term = false;
  if (isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old_term) == 0) {
    termios new_term = old_term;
    new_term.c_lflag &= ~static_cast<tcflag_t>(ECHO);
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_term) == 0) restore_term = true;
  }

  std::string value;
  std::getline(std::cin, value);
  if (restore_term) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
    std::cout << "\n";
  }
  return value;
}

bool Run(const SetupCommand& command,
         SetupContext& context,
         const std::vector<std::string>& secrets = {})
{
  const int result = context.Run(
      command, true,
      [&secrets](std::string_view line, std::string_view) {
        std::cout << RedactSetupSecrets(line, secrets) << "\n";
      },
      [&secrets](const SetupCommand& logged_command, bool dry_run, bool) {
        if (dry_run) {
          std::cout << "[preview] "
                    << RedactSetupSecrets(JoinCommandForDisplay(logged_command),
                                          secrets)
                    << "\n";
        }
      });
  if (result != 0) {
    std::cerr << "Command failed (exit " << result << "): "
              << RedactSetupSecrets(JoinCommandForDisplay(command), secrets)
              << "\n";
  }
  return result == 0;
}

bool RunWithInput(const SetupCommand& command,
                  const std::string& input,
                  SetupContext& context,
                  const std::vector<std::string>& secrets = {})
{
  const int result = context.RunWithInput(
      command, input, true,
      [&secrets](std::string_view line, std::string_view) {
        std::cout << RedactSetupSecrets(line, secrets) << "\n";
      },
      [&secrets](const SetupCommand& logged_command, bool dry_run, bool) {
        if (dry_run) {
          std::cout << "[preview] "
                    << RedactSetupSecrets(JoinCommandForDisplay(logged_command),
                                          secrets)
                    << "\n";
        }
      });
  if (result != 0) {
    std::cerr << "Command failed (exit " << result << "): "
              << RedactSetupSecrets(JoinCommandForDisplay(command), secrets)
              << "\n";
  }
  return result == 0;
}

std::string DiscoverSubscriptionRelease(SetupContext& context,
                                        const std::string& curl_config,
                                        const std::vector<std::string>& secrets)
{
  const auto command = BuildSubscriptionReleaseIndexCmd(true);
  std::string index;
  if (context.RunWithInput(
          command, curl_config, true,
          [&index](std::string_view line, std::string_view) {
            index += line;
            index += '\n';
          },
          [&secrets](const SetupCommand& logged_command, bool dry_run, bool) {
            if (dry_run) {
              std::cout << "[preview] "
                        << RedactSetupSecrets(
                               JoinCommandForDisplay(logged_command), secrets)
                        << "\n";
            }
          })
      != 0) {
    throw std::runtime_error(
        "Unable to retrieve the Bareos Subscription release index.");
  }
  if (context.dry_run()) return "newest-release";
  const auto release = ParseLatestSubscriptionRelease(index);
  if (release.empty()) {
    throw std::runtime_error(
        "The Bareos Subscription release index contains no valid release.");
  }
  std::cout << "Using Bareos Subscription release " << release << ".\n";
  return release;
}

/**
 * Ask the user which Bareos repository to install from.
 *
 * Only reached when the distribution is not recognised. Returns an empty
 * string if the user aborts or the entry is invalid.
 */
std::string PromptRepoOsPath(const OsInfo& os)
{
  std::cout
      << "\nThis Linux distribution was not recognised.\n"
         "Bareos has no repository specifically for \""
      << (os.pretty_name.empty() ? os.distro : os.pretty_name)
      << "\", but a repository built for a compatible distribution can be\n"
         "used instead.\n\n"
         "WARNING: such a combination is untested and unsupported.\n\n"
         "Available Bareos repositories:\n";

  const auto& known = KnownRepoOsPaths();
  const auto suggestions = SuggestRepoOsPaths(os);
  const std::string suggested = suggestions.empty() ? "" : suggestions.front();
  for (size_t i = 0; i < known.size(); ++i) {
    std::cout << "  " << (i + 1) << ") " << known[i];
    if (known[i] == suggested) std::cout << "   (suggested)";
    std::cout << "\n";
  }

  const auto answer = Prompt(
      "Repository (number, or a repository name not listed above)", suggested);
  if (answer.empty()) return {};

  std::string choice = answer;
  if (std::all_of(answer.begin(), answer.end(),
                  [](unsigned char c) { return std::isdigit(c) != 0; })) {
    const unsigned long index = std::stoul(answer);
    if (index < 1 || index > known.size()) {
      std::cerr << "Invalid selection.\n";
      return {};
    }
    choice = known[index - 1];
  }
  if (!IsValidRepoOsPath(choice)) {
    std::cerr << "Invalid repository name.\n";
    return {};
  }

  if (Prompt("Install from " + choice
                 + " even though this combination is untested? (yes/no)",
             "no")
      != "yes") {
    return {};
  }
  return choice;
}

}  // namespace

std::string_view BorisArtwork(std::string_view lc_all,
                              std::string_view lc_ctype,
                              std::string_view lang)
{
  const auto locale = !lc_all.empty()     ? lc_all
                      : !lc_ctype.empty() ? lc_ctype
                                          : lang;
  const auto dot = locale.find('.');
  auto encoding
      = dot == std::string_view::npos ? locale : locale.substr(dot + 1);
  encoding = encoding.substr(0, encoding.find('@'));
  std::string normalized(encoding);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (normalized == "utf-8" || normalized == "utf8") {
    return R"boris(               ╱╲
              ╱  ╲__
             ╱      ╲
            ╱   ╱╲___╲
           ╱════════╲
      ╭──╮╱__________╲
      │  ╰╯ ●     ●  ╲        ╲ │ ╱
      ╰╮     ╭───╮   │       ── ✧ ──
       │     │ ᴥ │   │         ╱│ ╲
       ╰╮    ╰─┬─╯  ╭╯          │
       ╱╰───────────╯╲___╭──╮───│
      ╱ ╱     ┼      ╭───╰──╯   │
     ╱ ╱      │      │╲         │
    ╱_✧╲      │      ╱✧╲
   ╱____╲           ╱___╲
        ╰───────────╯
          ╰──╯ ╰──╯

)boris";
  }
  return R"boris(               /\
              /  \__
             /      \
            /   /\___\
           /========\
      .--./__________\
      |  \/  o    o  \        \ | /
      '-.    .----.  |       -- * --
        |    | oo |  |         /| \
        '.   '--+-' .'          |
        /'---------' \____.--.--|
       / /    <|>     .---'--'  |
      / /      |      |\        |
     /_* \     |      / *\
    /_____\          /____\
         '------------'
          (___)  (___)

)boris";
}

static int RunWizard(SetupContext& context,
                     const UnattendedSetupOptions* unattended)
{
  const char* lc_all = std::getenv("LC_ALL");
  const char* lc_ctype = std::getenv("LC_CTYPE");
  const char* lang = std::getenv("LANG");
  std::cout << BorisArtwork(lc_all ? lc_all : "", lc_ctype ? lc_ctype : "",
                            lang ? lang : "");
  std::cout << "Bareos Setup\n\n";
  const auto os = DetectOs();
  if (!IsSupportedPackageManager(os.pkg_mgr)) {
    std::cerr << "No supported package manager (apt, dnf, yum or zypper) was "
                 "found. Bareos cannot be installed on this system.\n";
    return 1;
  }
  if (os.pretty_name.empty()) {
    std::cout << "Detected an unknown distribution ("
              << PackageManagerName(os.pkg_mgr) << ")\n";
  } else {
    std::cout << "Detected " << os.pretty_name << " ("
              << PackageManagerName(os.pkg_mgr) << ")\n";
  }

  std::string repo_os_path;
  const bool override_repository
      = unattended && !unattended->override_repository_urls.empty();
  if (override_repository) {
    // The explicit CI URL already includes its repository path.
  } else if (IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    repo_os_path = BuildRepoOsPath(os.distro, os.version);
  } else {
    if (unattended) {
      std::cerr << "No automatically supported repository for this platform. "
                   "Provide --override-repo-url including the distribution "
                   "path.\n";
      return 1;
    }
    repo_os_path = PromptRepoOsPath(os);
    if (repo_os_path.empty()) {
      std::cerr << "No Bareos repository selected.\n";
      return 1;
    }
  }
  const bool manual_repo_choice
      = !IsSupportedSetupPlatform(os.distro, os.pkg_mgr);
  auto tape = unattended ? unattended->tape : TapeSupportOptions{};
  if (!unattended) {
    tape.enabled = Prompt("Install tape support? (yes/no)", "yes") == "yes";
    if (tape.enabled) {
      tape.allow_repositories
          = Prompt(
                "Allow enabling vendor dependency repositories if needed "
                "(entitled RHEL or registered SUSE PackageHub)? (yes/no)",
                "no")
            == "yes";
    }
  }

  if (!override_repository) {
    const auto repository
        = unattended
              ? "community"
              : Prompt("Repository (community/subscription)", "subscription");
    if (repository != "community" && repository != "subscription") {
      std::cerr << "Choose community or subscription.\n";
      return 1;
    }
    std::string login;
    std::string password;
    if (repository == "subscription" && !context.dry_run()) {
      login = Prompt("Subscription login");
      password = PromptSecret("Subscription password");
      if (login.empty() || password.empty()) return 1;
    }

    if (repository == "community") {
      std::cout << "Checking connectivity to the Bareos download server...\n";
      const auto network_check = BuildNetworkCheckCmd(repository);
      if (!network_check) {
        throw std::logic_error("Community repository check command is missing");
      }
      if (!Run(*network_check, context)) return 1;
    } else if (context.dry_run()) {
      std::cout
          << "Dry run: subscription credentials would be requested before "
             "the repository script download.\n";
    } else {
      std::cout << "Downloading the subscription repository script. This "
                   "validates the credentials and download connectivity.\n";
    }

    std::filesystem::path repository_script;
    if (context.dry_run()) {
      repository_script = "bareos-setup-repository.sh";
    } else {
      try {
        repository_script
            = context.CreateTemporaryFile("bareos-setup-repository");
      } catch (const std::runtime_error& error) {
        std::cerr << "Unable to create a private repository setup file.\n";
        std::cerr << error.what() << "\n";
        return 1;
      }
    }

    const bool use_curl_config = repository == "subscription";
    const std::string curl_config
        = context.dry_run() ? "" : BuildCurlUserConfig(login, password);
    std::string release;
    try {
      release = use_curl_config ? DiscoverSubscriptionRelease(
                                      context, curl_config, {login, password})
                                : "";
    } catch (const std::runtime_error& error) {
      std::cerr << error.what() << "\n";
      context.Remove(repository_script);
      return 1;
    }
    if (manual_repo_choice) {
      std::cout << "Verifying that the " << repo_os_path
                << " Bareos repository is available.\n";
      const auto probe_cmd = BuildRepoPathProbeCmd(repo_os_path, repository,
                                                   use_curl_config, release);
      const bool reachable = use_curl_config
                                 ? RunWithInput(probe_cmd, curl_config, context,
                                                {login, password})
                                 : Run(probe_cmd, context);
      if (!reachable) {
        std::cerr
            << "The Bareos repository \"" << repo_os_path
            << "\" could not be reached. Select a repository that matches "
               "this system"
            << (use_curl_config ? " and check your subscription credentials.\n"
                                : ".\n");
        context.Remove(repository_script);
        return 1;
      }
    }
    auto add_repo_cmd = BuildAddRepoCmdForPath(repo_os_path, repository,
                                               use_curl_config, release);
    add_repo_cmd.arguments.insert(add_repo_cmd.arguments.end() - 1,
                                  {"--output", repository_script.string()});
    const bool repo_downloaded = use_curl_config
                                     ? RunWithInput(add_repo_cmd, curl_config,
                                                    context, {login, password})
                                     : Run(add_repo_cmd, context);
    if (!repo_downloaded) {
      context.Remove(repository_script);
      return 1;
    }
    if (!Run(Bash({repository_script.string()}), context)) {
      context.Remove(repository_script);
      return 1;
    }
    context.Remove(repository_script);
  } else {
    const auto& url = unattended->override_repository_urls.front();
    std::cout << "Overriding repository URL with " << url << "\n";
    const auto download = Curl({"--fail", "--silent", "--show-error",
                                "--location", "--proto-redir", "=https",
                                url + "/add_bareos_repositories.sh"});
    std::string script;
    if (context.Run(download, true,
                    [&script](std::string_view line, std::string_view stream) {
                      if (stream == "stderr") {
                        std::cerr << line << "\n";
                      } else {
                        script += line;
                        script += '\n';
                      }
                    })
        != 0) {
      std::cerr << "Repository helper download failed.\n";
      return 1;
    }
    if (context.dry_run()) {
      std::cout << "[preview] " << JoinCommandForDisplay(download) << "\n"
                << "[preview] would override the helper's URL with " << url
                << " and execute it.\n";
    } else {
      const auto rewritten = RewriteSetupRepositoryScript(script, url);
      const auto path = context.CreateTemporaryFile("bareos-setup-repository");
      try {
        std::ofstream output(path);
        output << rewritten;
        output.close();
        if (!output)
          throw std::runtime_error("Writing repository helper failed");
        const bool success = Run(Bash({path.string()}), context);
        context.Remove(path);
        if (!success) {
          std::cerr << "Repository configuration failed.\n";
          return 1;
        }
      } catch (...) {
        context.Remove(path);
        throw;
      }
    }
  }
  const auto update_cmd = BuildPackageCacheUpdateCmd(os.pkg_mgr);
  if (update_cmd) {
    std::cout << "Refreshing package metadata.\n";
    if (!Run(*update_cmd, context)) return 1;
  }

  if (os.pkg_mgr == PackageManager::Apt) {
    std::cout << "Installing PostgreSQL package.\n";
    if (!Run(BuildInstallCmd(os.pkg_mgr, {"postgresql"}), context)) return 1;
    if (!Run(Systemctl({"enable", "--now", "postgresql"}), context)) {
      return 1;
    }
  }
  auto packages = os.pkg_mgr == PackageManager::Apt
                      ? BuildPackageListWithoutPostgresServer(os.pkg_mgr)
                      : BuildDefaultPackageList(os.pkg_mgr);
  if (PrepareTapeSupport(context, os, tape,
                         [](std::string_view line, std::string_view stream) {
                           (stream == "stderr" ? std::cerr : std::cout)
                               << line << "\n";
                         })
      != 0) {
    return 1;
  }
  if (!tape.enabled) { std::erase(packages, "bareos-storage-tape"); }
  const bool webui = !unattended || unattended->webui;
  if (!webui) {
    std::erase_if(packages, [](const std::string& package) {
      return package.starts_with("bareos-webui") || package == "mod_ssl"
             || package == "policycoreutils";
    });
  }
  if (unattended) {
    packages.insert(packages.end(), unattended->extra_packages.begin(),
                    unattended->extra_packages.end());
  }
  if (!Run(BuildInstallCmd(os.pkg_mgr, packages), context)) {
    std::cerr << "Package installation failed.\n";
    return 1;
  }
  if (!context.dry_run() && webui) {
    const auto missing = context.MissingPostInstallTools(os.pkg_mgr);
    if (!missing.empty()) {
      std::cerr << "Required post-install tool(s) not found in PATH:";
      for (const auto& tool : missing) std::cerr << " " << tool;
      std::cerr << "\nInstall the missing tool(s) and try again.\n";
      return 1;
    }
  }
  std::string admin_password;
  if (!context.dry_run() || unattended) {
    const auto init_cmd = BuildPostgresInitCmd(context);
    if (init_cmd && !Run(*init_cmd, context)) return 1;
    if (!Run(Systemctl({"enable", "--now", "postgresql"}), context)) return 1;
    // Non-Debian packages need the manual catalog scripts. Debian/Ubuntu
    // packages run dbconfig-common during package configuration instead.
    for (const auto& script : BuildCatalogInitScripts(os.pkg_mgr)) {
      if (!Run(BuildRunAsPostgresCmd(script), context)) return 1;
    }
  }

  if (webui && !context.dry_run()) {
    std::vector<std::string> existing_configs;
    for (const auto& path : SetupOwnedConfigPaths()) {
      if (!Run(BuildFileAbsentCheckCmd(path), context)) {
        existing_configs.push_back(path);
      }
    }
    if (!existing_configs.empty()) {
      std::cerr << BuildExistingSetupConfigError(existing_configs) << "\n";
      return 1;
    }
    admin_password = GenerateSetupSecret();
    const std::string resource
        = "Console {\n  Name = admin\n  Password = \"" + admin_password
          + "\"\n  Profile = \"webui-admin\"\n  TLS Enable = No\n}\n";
    if (!RunWithInput(Install({"-D", "-m", "0640", "/dev/stdin",
                               "/etc/bareos/bareos-dir.d/console/admin.conf"}),
                      resource, context, {admin_password})) {
      return 1;
    }
    if (!Run(Chown({"root:bareos",
                    "/etc/bareos/bareos-dir.d/console/admin.conf"}),
             context)) {
      return 1;
    }
    if (!Run(Systemctl({"restart", "bareos-dir"}), context)) return 1;
    // No bareos-webui-proxy.ini is written: the proxy's built-in defaults
    // already match this layout and are used when no file exists.
  } else if (webui) {
    const std::string admin_path
        = "/etc/bareos/bareos-dir.d/console/admin.conf";
    if (!Run(Install({"-D", "-m", "0640", "/dev/stdin", admin_path}),
             context)) {
      return 1;
    }
    std::cout << "[preview] dry run: would create initial admin console "
                 "configuration with a generated password.\n";
    if (!Run(Chown({"root:bareos", admin_path}), context)) return 1;
    if (!Run(Systemctl({"restart", "bareos-dir"}), context)) return 1;
  }
  auto enable_services = Systemctl({"enable", "--now"});
  const auto daemon_services = BuildBareosDaemonServiceNames(os.pkg_mgr);
  enable_services.arguments.insert(enable_services.arguments.end(),
                                   daemon_services.begin(),
                                   daemon_services.end());
  if (webui) enable_services.arguments.emplace_back("bareos-webui-proxy");
  if (!Run(enable_services, context)) { return 1; }
  if (webui) {
    const auto https_setup_cmds = BuildWebServerHttpsSetupCmds(os.pkg_mgr);
    for (const auto& command : https_setup_cmds) {
      if (!Run(command, context)) return 1;
    }
    if (!Run(BuildWebUiSelinuxSetupCmd(), context)) return 1;
    if (!Run(Systemctl(
                 {"enable", "--now", BuildWebServerServiceName(os.pkg_mgr)}),
             context)) {
      return 1;
    }
    if (!https_setup_cmds.empty()
        && !Run(Systemctl({"restart", BuildWebServerServiceName(os.pkg_mgr)}),
                context)) {
      return 1;
    }
  }
  auto services = BuildBareosDaemonServiceNames(os.pkg_mgr);
  if (webui) services.emplace_back("bareos-webui-proxy");
  for (const auto& service : services) {
    if (!Run(Systemctl({"is-active", service}), context)) return 1;
  }
  if (webui
      && !Run(Systemctl({"is-active", BuildWebServerServiceName(os.pkg_mgr)}),
              context)) {
    return 1;
  }
  std::cout << "\nSetup complete.\n";
  if (!webui) return 0;
  std::cout << "WebUI username: admin\n";
  if (context.dry_run()) {
    std::cout << "Dry run only: no WebUI admin password was generated.\n";
  } else if (!unattended) {
    std::cout << "Initial WebUI password: " << admin_password << "\n";
  } else {
    std::cout << "The generated admin password is stored in "
              << SetupAdminConfigPath()
              << "; it is not printed in unattended logs.\n";
  }
  return 0;
}

int RunTuiWizard(SetupContext& context)
{
  try {
    return RunWizard(context, nullptr);
  } catch (const std::exception& error) {
    std::cerr << "Setup failed: " << error.what() << "\n";
    return 1;
  }
}

int RunUnattendedSetup(SetupContext& context,
                       const UnattendedSetupOptions& options)
{
  if (options.override_repository_urls.size() > 1) {
    std::cerr << "Unattended setup accepts at most one --override-repo-url. "
                 "Repository helpers overwrite their repository definitions.\n";
    return 1;
  }
  for (const auto& url : options.override_repository_urls) {
    if (!IsValidSetupRepositoryUrl(url) || url.ends_with('/')) {
      std::cerr
          << "Use an HTTPS --override-repo-url including the distribution "
             "path, "
             "without credentials, query parameters or a trailing slash.\n";
      return 1;
    }
  }
  for (const auto& package : options.extra_packages) {
    if (!IsSafeSetupIdentifier(package) || package.starts_with('-')) {
      std::cerr << "Invalid extra package name.\n";
      return 1;
    }
    if (!options.tape.enabled && package == "bareos-storage-tape") {
      std::cerr << "--without-tape-support conflicts with extra package "
                   "bareos-storage-tape.\n";
      return 1;
    }
  }
  if (!options.tape.enabled && options.tape.allow_repositories) {
    std::cerr << "Repository consent requires tape support to be enabled.\n";
    return 1;
  }
  try {
    return RunWizard(context, &options);
  } catch (const std::exception& error) {
    std::cerr << "Setup failed: " << error.what() << "\n";
    return 1;
  }
}
