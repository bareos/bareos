/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.
*/
#include "setup_session.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#include <utility>

#include <jansson.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string_view>

#include "command_runner.h"
#include "http_server.h"
#include "os_detector.h"
#include "setup_steps.h"
#include "ws_codec.h"

namespace {

struct SetupProgress {
  std::mutex mutex;
  std::set<std::string> completed;
  std::string failed_step;
  bool finished = false;
  std::string admin_password;
  bool admin_config_created = false;
  // Repository OS path resolved by the repository step; may be a manual
  // choice on distributions the wizard does not recognise.
  std::string repo_os_path;
  std::optional<TapeSupportOptions> tape;
};

struct SessionContext {
  SetupContext& setup;
  bool peer_is_loopback = true;
};

const std::string kAdminConfigPath = SetupAdminConfigPath();

std::mutex& TerminalCredentialPromptMutex()
{
  static std::mutex mutex;
  return mutex;
}

SetupProgress& Progress()
{
  static SetupProgress progress;
  return progress;
}

std::mutex& SetupOperationMutex()
{
  static std::mutex mutex;
  return mutex;
}

std::string Dump(json_t* value)
{
  char* text = json_dumps(value, JSON_COMPACT);
  json_decref(value);
  std::string result(text ? text : "{}");
  free(text);
  return result;
}

void Send(WsCodec& ws, json_t* value) { ws.SendText(Dump(value)); }

void Error(WsCodec& ws, const std::string& step, const std::string& message)
{
  Send(ws, json_pack("{s:s,s:s,s:s}", "type", "error", "step", step.c_str(),
                     "message", message.c_str()));
}

void Output(WsCodec& ws,
            std::string_view line,
            const std::vector<std::string>& secrets = {})
{
  const auto safe = RedactSetupSecrets(line, secrets);
  Send(ws, json_pack("{s:s,s:s}", "type", "output", "line", safe.c_str()));
}

std::string StringField(json_t* object, const char* key)
{
  const auto* value = json_object_get(object, key);
  return json_is_string(value) ? json_string_value(value) : "";
}

std::string LocalHostname()
{
  std::array<char, 256> hostname{};
  if (::gethostname(hostname.data(), hostname.size()) != 0) return {};
  hostname.back() = '\0';
  return hostname.data();
}

std::string ReadTerminalLine(int tty_fd, bool hide_input)
{
  termios old_term{};
  bool restore_term = false;
  if (hide_input && tcgetattr(tty_fd, &old_term) == 0) {
    termios new_term = old_term;
    new_term.c_lflag &= ~static_cast<tcflag_t>(ECHO);
    if (tcsetattr(tty_fd, TCSAFLUSH, &new_term) == 0) restore_term = true;
  }

  std::string value;
  char ch = '\0';
  while (read(tty_fd, &ch, 1) == 1) {
    if (ch == '\n' || ch == '\r') break;
    value += ch;
  }

  if (restore_term) {
    tcsetattr(tty_fd, TCSAFLUSH, &old_term);
    [[maybe_unused]] auto _ = write(tty_fd, "\n", 1);
  }
  return value;
}

std::pair<std::string, std::string> PromptRemoteSubscriptionCredentials()
{
  std::lock_guard lock(TerminalCredentialPromptMutex());
  const int tty_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
  if (tty_fd < 0) {
    throw std::runtime_error(
        "Subscription credentials are required. Re-run bareos-setup from an "
        "interactive terminal, use a local loopback browser session, or choose "
        "the community repository.");
  }

  constexpr std::string_view intro
      = "\nRemote browser selected the Bareos Subscription repository.\n"
        "Enter credentials here; they are not shown or sent in the browser.\n";
  [[maybe_unused]] auto _ = write(tty_fd, intro.data(), intro.size());
  constexpr std::string_view login_prompt = "Subscription login: ";
  [[maybe_unused]] auto _login
      = write(tty_fd, login_prompt.data(), login_prompt.size());
  std::string login = ReadTerminalLine(tty_fd, false);
  constexpr std::string_view password_prompt = "Subscription password: ";
  [[maybe_unused]] auto _password
      = write(tty_fd, password_prompt.data(), password_prompt.size());
  std::string password = ReadTerminalLine(tty_fd, true);
  close(tty_fd);

  if (login.empty() || password.empty()) {
    throw std::runtime_error("Subscription login and password are required.");
  }
  return {std::move(login), std::move(password)};
}

std::vector<std::string> SecretsFor(
    json_t* message,
    const std::vector<std::string>& additional_secrets = {})
{
  std::vector<std::string> secrets = additional_secrets;
  const auto login = StringField(message, "repository_login");
  const auto password = StringField(message, "repository_password");
  if (!login.empty()) secrets.push_back(login);
  if (!password.empty()) secrets.push_back(password);
  std::lock_guard lock(Progress().mutex);
  if (!Progress().admin_password.empty())
    secrets.push_back(Progress().admin_password);
  return secrets;
}

int Run(const SetupCommand& command,
        WsCodec& ws,
        const SessionContext& context,
        const std::vector<std::string>& secrets = {})
{
  return context.setup.Run(
      command, true,
      [&ws, &secrets](std::string_view line, std::string_view) {
        Output(ws, line, secrets);
      },
      [&ws, &secrets](const SetupCommand& logged_command, bool, bool) {
        Output(ws, "$ " + JoinCommandForDisplay(logged_command), secrets);
      },
      [&ws](bool) { Output(ws, "[preview] dry run: command not executed."); });
}

int RunWithInput(const SetupCommand& command,
                 const std::string& input,
                 WsCodec& ws,
                 const SessionContext& context,
                 const std::vector<std::string>& secrets = {})
{
  return context.setup.RunWithInput(
      command, input, true,
      [&ws, &secrets](std::string_view line, std::string_view) {
        Output(ws, line, secrets);
      },
      [&ws, &secrets](const SetupCommand& logged_command, bool, bool) {
        Output(ws, "$ " + JoinCommandForDisplay(logged_command), secrets);
      },
      [&ws](bool) {
        Output(ws, "[preview] dry run: command not executed; stdin redacted.");
      });
}

std::string DiscoverSubscriptionRelease(WsCodec& ws,
                                        const SessionContext& context,
                                        const std::vector<std::string>& secrets,
                                        const std::string& curl_config)
{
  const auto command = BuildSubscriptionReleaseIndexCmd(true);
  std::string index;
  const int result = context.setup.RunWithInput(
      command, curl_config, true,
      [&index](std::string_view line, std::string_view) {
        index += line;
        index += '\n';
      },
      [&ws, &secrets](const SetupCommand& logged_command, bool dry_run, bool) {
        if (dry_run) {
          Output(ws, "[preview] " + JoinCommandForDisplay(logged_command),
                 secrets);
        } else {
          Output(ws, "$ " + JoinCommandForDisplay(logged_command), secrets);
        }
      });
  if (context.setup.dry_run()) return "newest-release";
  if (result != 0) {
    throw std::runtime_error(
        "Unable to retrieve the Bareos Subscription release index.");
  }
  const auto release = ParseLatestSubscriptionRelease(index);
  if (release.empty()) {
    throw std::runtime_error(
        "The Bareos Subscription release index contains no valid release.");
  }
  Output(ws, "Using Bareos Subscription release " + release + ".");
  return release;
}

void EnsureNoExistingSetupConfigs(WsCodec& ws,
                                  const SessionContext& context,
                                  const std::vector<std::string>& paths)
{
  std::vector<std::string> existing;
  for (const auto& path : paths) {
    if (Run(BuildFileAbsentCheckCmd(path), ws, context) != 0) {
      existing.push_back(path);
    }
  }
  if (existing.empty()) return;

  throw std::runtime_error(BuildExistingSetupConfigError(existing));
}

/**
 * Determine which Bareos repository path to install from.
 *
 * On a recognised distribution the path is derived from the detected ID and
 * version and a manual choice is refused, so a supported system can never be
 * pointed at a mismatched repository. On an unrecognised distribution the
 * manual choice is required, because nothing else can supply the path.
 */
std::string ResolveRepoOsPath(const OsInfo& os, const std::string& requested)
{
  if (!IsSupportedPackageManager(os.pkg_mgr)) {
    throw std::runtime_error(
        "No supported package manager (apt, dnf, yum or zypper) was found. "
        "Bareos cannot be installed on this system.");
  }

  if (IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    const auto detected = BuildRepoOsPath(os.distro, os.version);
    if (!requested.empty() && requested != detected) {
      throw std::runtime_error(
          "This distribution was recognised, so its Bareos repository cannot "
          "be selected manually.");
    }
    return detected;
  }

  if (requested.empty()) {
    throw std::runtime_error(
        "This Linux distribution was not recognised. Select a compatible "
        "Bareos repository manually to continue.");
  }
  if (!IsValidRepoOsPath(requested)) {
    throw std::runtime_error("Invalid Bareos repository selection.");
  }
  return requested;
}

int InstallRepository(WsCodec& ws,
                      json_t* message,
                      const SessionContext& context)
{
  const auto distro = StringField(message, "distro");
  const auto version = StringField(message, "version");
  const auto repo_type = StringField(message, "repository");
  const auto requested_repo_os_path = StringField(message, "repo_os_path");
  std::string login = StringField(message, "repository_login");
  std::string password = StringField(message, "repository_password");
  const auto os = DetectOs();
  if (distro != os.distro || version != os.version) {
    throw std::runtime_error(
        "The detected platform changed. Reload the setup wizard.");
  }
  const auto repo_os_path = ResolveRepoOsPath(os, requested_repo_os_path);
  if (repo_type != "community" && repo_type != "subscription") {
    throw std::runtime_error("Select a supported Bareos repository.");
  }
  if (repo_type == "subscription" && !context.peer_is_loopback
      && (!login.empty() || !password.empty())) {
    throw std::runtime_error(
        "Remote browser sessions must not send subscription credentials. "
        "They are requested securely on the bareos-setup terminal.");
  }
  if (repo_type == "subscription" && !context.setup.dry_run()
      && context.peer_is_loopback && (login.empty() || password.empty())) {
    throw std::runtime_error("Subscription credentials are required.");
  }
  if (repo_type == "subscription" && !context.peer_is_loopback) {
    if (context.setup.dry_run()) {
      Output(ws,
             "Dry run: subscription credentials would be requested on the "
             "bareos-setup terminal, not in this browser.");
    } else {
      Output(ws,
             "Subscription credentials are required; prompt is shown on the "
             "bareos-setup terminal.");
      std::tie(login, password) = PromptRemoteSubscriptionCredentials();
    }
  }

  std::vector<std::string> secrets;
  if (repo_type == "subscription" && !context.setup.dry_run())
    secrets = SecretsFor(message, {login, password});
  else
    secrets = SecretsFor(message);
  if (repo_type == "community") {
    Output(ws, "Checking connectivity to the Bareos download server...");
    const auto network_check = BuildNetworkCheckCmd(repo_type);
    if (!network_check) {
      throw std::logic_error("Community repository check command is missing");
    }
    const int reachable = Run(*network_check, ws, context, secrets);
    if (reachable != 0) { return reachable; }
  } else {
    Output(ws,
           "Downloading the subscription repository script. This validates "
           "the credentials and download connectivity.");
  }

  const bool use_curl_config = repo_type == "subscription";
  const std::string curl_config
      = context.setup.dry_run() ? "" : BuildCurlUserConfig(login, password);
  const std::string release
      = use_curl_config
            ? DiscoverSubscriptionRelease(ws, context, secrets, curl_config)
            : "";
  // Only a manually chosen repository can be wrong here; a detected one is
  // validated implicitly by the download below.
  const bool manual_repo_choice = !requested_repo_os_path.empty();
  if (manual_repo_choice) {
    Output(ws, "Verifying that the " + repo_os_path
                   + " Bareos repository is available.");
    const auto probe_cmd = BuildRepoPathProbeCmd(repo_os_path, repo_type,
                                                 use_curl_config, release);
    const int probe = use_curl_config ? RunWithInput(probe_cmd, curl_config, ws,
                                                     context, secrets)
                                      : Run(probe_cmd, ws, context, secrets);
    if (probe != 0) {
      throw std::runtime_error(
          "The Bareos repository \"" + repo_os_path
          + "\" could not be reached. Select a repository that matches this "
            "system"
          + (use_curl_config ? " and check your subscription credentials."
                             : "."));
    }
  }

  auto command = BuildAddRepoCmdForPath(repo_os_path, repo_type,
                                        use_curl_config, release);
  const std::filesystem::path script
      = context.setup.dry_run()
            ? std::filesystem::path{"bareos-setup-repository.sh"}
            : context.setup.CreateTemporaryFile("bareos-setup-repository");
  command.arguments.insert(command.arguments.end() - 1,
                           {"--output", script.string()});
  const int download = use_curl_config ? RunWithInput(command, curl_config, ws,
                                                      context, secrets)
                                       : Run(command, ws, context, secrets);
  if (download != 0) {
    context.setup.Remove(script);
    return download;
  }
  Output(ws, "Installing the approved Bareos repository.");
  const int result = Run(Bash({script.string()}), ws, context, secrets);
  context.setup.Remove(script);
  if (result != 0) return result;
  {
    std::lock_guard lock(Progress().mutex);
    Progress().repo_os_path = repo_os_path;
  }
  const auto update_cmd = BuildPackageCacheUpdateCmd(os.pkg_mgr);
  if (update_cmd) {
    Output(ws, "Refreshing package metadata.");
    return Run(*update_cmd, ws, context);
  }
  return 0;
}

TapeSupportOptions TapeOptions(json_t* message)
{
  TapeSupportOptions options;
  for (const auto* key : {"tape_support", "allow_tape_repositories"}) {
    const auto* value = json_object_get(message, key);
    if (value && !json_is_boolean(value)) {
      throw std::runtime_error(std::string(key) + " must be a boolean.");
    }
  }
  if (const auto* value = json_object_get(message, "tape_support")) {
    options.enabled = json_is_true(value);
  }
  options.allow_repositories
      = json_is_true(json_object_get(message, "allow_tape_repositories"));
  if (!options.enabled && options.allow_repositories) {
    throw std::runtime_error("Repository consent requires tape support.");
  }
  return options;
}

int InstallPackages(WsCodec& ws, json_t* message, const SessionContext& context)
{
  const auto os = DetectOs();
  auto tape = TapeOptions(message);
  if (!IsSupportedPackageManager(os.pkg_mgr)) {
    throw std::runtime_error(
        "No supported package manager (apt, dnf, yum or zypper) was found. "
        "Bareos cannot be installed on this system.");
  }

  if (os.pkg_mgr == PackageManager::Apt) {
    Output(ws, "Installing PostgreSQL package.");
    if (Run(BuildInstallCmd(os.pkg_mgr, {"postgresql"}), ws, context) != 0) {
      return 1;
    }
    if (Run(Systemctl({"enable", "--now", "postgresql"}), ws, context) != 0) {
      return 1;
    }
  }

  auto packages = os.pkg_mgr == PackageManager::Apt
                      ? BuildPackageListWithoutPostgresServer(os.pkg_mgr)
                      : BuildDefaultPackageList(os.pkg_mgr);
  if (PrepareTapeSupport(
          context.setup, os, tape,
          [&](std::string_view line, std::string_view) { Output(ws, line); })
      != 0) {
    return 1;
  }
  if (!tape.enabled) std::erase(packages, "bareos-storage-tape");

  Output(ws, "Installing the fixed Bareos package set.");
  return Run(BuildInstallCmd(os.pkg_mgr, packages), ws, context);
}

int InitializeCatalog(WsCodec& ws, const SessionContext& context)
{
  const auto os = DetectOs();
  // Package scripts are idempotent, but do not run them when the catalog
  // marker is already present.  The marker is created only by this wizard.
  const std::filesystem::path marker = "/var/lib/bareos/.catalog-initialized";
  // Use the non-throwing overload: if existence can't be determined (e.g.
  // a permission error), treat it the same as "not present" rather than
  // letting a filesystem_error exception propagate out of this step.
  std::error_code marker_error;
  if (context.setup.dry_run()
      || !std::filesystem::exists(marker, marker_error)) {
    const auto init_cmd = BuildPostgresInitCmd(context.setup);
    if (init_cmd && Run(*init_cmd, ws, context) != 0) return 1;
    if (Run(Systemctl({"enable", "--now", "postgresql"}), ws, context) != 0) {
      return 1;
    }
    // Non-Debian packages need the manual catalog scripts. Debian/Ubuntu
    // packages run dbconfig-common during package configuration instead.
    for (const auto& script : BuildCatalogInitScripts(os.pkg_mgr)) {
      if (Run(BuildRunAsPostgresCmd(script), ws, context) != 0) return 1;
    }
    const int marker_result
        = Run(Install({"-D", "-m", "0640", "/dev/null", marker.string()}), ws,
              context);
    if (marker_result != 0) return marker_result;
  } else {
    Output(ws, "The Bareos catalog is already initialized.");
  }
  // The daemons need a working, privilege-granted catalog before they can
  // start successfully, so enable/start them here (idempotent) rather than
  // right after package installation.
  auto enable_daemons = Systemctl({"enable", "--now"});
  const auto daemon_services = BuildBareosDaemonServiceNames(os.pkg_mgr);
  enable_daemons.arguments.insert(enable_daemons.arguments.end(),
                                  daemon_services.begin(),
                                  daemon_services.end());
  return Run(enable_daemons, ws, context);
}

int CreateAdmin(WsCodec& ws, const SessionContext& context)
{
  constexpr std::string_view username = "admin";
  if (!context.setup.dry_run()) {
    EnsureNoExistingSetupConfigs(ws, context, {kAdminConfigPath});
  }
  if (context.setup.dry_run()) {
    const auto write_command = Install(
        {"-D", "-m", "0640", "/dev/stdin", std::string{kAdminConfigPath}});
    context.setup.RunWithInput(
        write_command, "", true, [](std::string_view, std::string_view) {},
        [&ws](const SetupCommand& logged_command, bool, bool) {
          Output(ws, "$ " + JoinCommandForDisplay(logged_command));
        },
        [&ws](bool) {
          Output(ws,
                 "[preview] dry run: would create initial admin console "
                 "configuration with a generated password.");
        });
    if (Run(Chown({"root:bareos", std::string{kAdminConfigPath}}), ws, context)
        != 0) {
      return 1;
    }
    if (Run(Systemctl({"restart", "bareos-dir"}), ws, context) != 0) {
      return 1;
    }
    Send(ws, json_pack("{s:s,s:s,s:b}", "type", "admin_credentials", "username",
                       username.data(), "dry_run", true));
    return 0;
  }

  const std::string password = GenerateSetupSecret();
  const std::string resource =
      "Console {\n"
      "  Name = admin\n"
      "  Password = \"" + password
      + "\"\n"
        "  Profile = \"webui-admin\"\n"
      "  TLS Enable = No\n"
        "}\n";
  const std::string path{kAdminConfigPath};
  const auto write_command = Install({"-D", "-m", "0640", "/dev/stdin", path});
  const int write_result = context.setup.RunWithInput(
      write_command, resource, true,
      [&ws, &password](std::string_view line, std::string_view) {
        Output(ws, line, {password});
      },
      [&ws, &password](const SetupCommand& logged_command, bool, bool) {
        Output(ws, "$ " + JoinCommandForDisplay(logged_command), {password});
      },
      [&ws](bool) {
        Output(ws, "[preview] dry run: command not executed; stdin redacted.");
      });
  if (write_result != 0) return write_result;
  {
    std::lock_guard lock(Progress().mutex);
    Progress().admin_config_created = true;
  }
  if (Run(Chown({"root:bareos", path}), ws, context, {password}) != 0) {
    return 1;
  }
  if (Run(Systemctl({"restart", "bareos-dir"}), ws, context, {password}) != 0) {
    return 1;
  }
  {
    std::lock_guard lock(Progress().mutex);
    Progress().admin_password = password;
  }
  if (context.peer_is_loopback) {
    Send(ws, json_pack("{s:s,s:s,s:s}", "type", "admin_credentials", "username",
                       username.data(), "password", password.c_str()));
  } else {
    std::cerr << "\nBareos WebUI admin credentials:\n"
              << "  Username: " << username << "\n"
              << "  Password: " << password << "\n\n";
    Send(ws, json_pack("{s:s,s:s,s:b}", "type", "admin_credentials", "username",
                       username.data(), "password_printed_to_terminal", true));
  }
  return 0;
}

int ConfigureProxy(WsCodec& ws, const SessionContext& context)
{
  const auto os = DetectOs();
  if (!context.setup.dry_run()) {
    const auto missing = context.setup.MissingPostInstallTools(os.pkg_mgr);
    if (!missing.empty()) {
      std::ostringstream message;
      message << "Required post-install tool(s) not found in PATH:";
      for (const auto& tool : missing) message << " " << tool;
      Output(ws, message.str());
      return 1;
    }
  }
  // No configuration file is written: the built-in defaults of
  // bareos-webui-proxy already describe exactly the layout this setup
  // creates, and the service falls back to them when no file exists. Any
  // configuration an administrator placed there is therefore left alone.
  if (Run(Systemctl({"enable", "--now", "bareos-webui-proxy"}), ws, context)
      != 0) {
    return 1;
  }
  const auto https_setup_cmds = BuildWebServerHttpsSetupCmds(os.pkg_mgr);
  for (const auto& command : https_setup_cmds) {
    if (Run(command, ws, context) != 0) return 1;
  }
  if (Run(BuildWebUiSelinuxSetupCmd(), ws, context) != 0) return 1;
  if (Run(Systemctl({"enable", "--now", BuildWebServerServiceName(os.pkg_mgr)}),
          ws, context)
      != 0) {
    return 1;
  }
  if (!https_setup_cmds.empty()
      && Run(Systemctl({"restart", BuildWebServerServiceName(os.pkg_mgr)}), ws,
             context)
             != 0) {
    return 1;
  }
  return 0;
}

int RunSmokeTest(WsCodec& ws, const SessionContext& context)
{
  const auto os = DetectOs();
  // Previously this ran "bareos-dir -t"/"bareos-sd -t" directly as
  // root to validate the config before checking daemon status. But
  // bareos-dir/bareos-sd connect to the catalog via PostgreSQL peer
  // authentication, which requires the OS user to match the systemd
  // unit's own User= setting -- running "-t" as root instead fails with
  // "Peer authentication failed for user \"bareos\"".
  // Since InitializeCatalog() already did "systemctl enable --now" for
  // these daemons, a successful "systemctl is-active" here already
  // proves each daemon started up correctly (including a valid config
  // and catalog connection) under systemd's own user, so the separate
  // "-t" invocation is both redundant and the source of this bug --
  // simply drop it and rely on is-active alone.
  auto services = BuildBareosDaemonServiceNames(os.pkg_mgr);
  services.emplace_back("bareos-webui-proxy");
  for (const auto& service : services) {
    if (Run(Systemctl({"is-active", service}), ws, context) != 0) return 1;
  }
  if (Run(Systemctl({"is-active", BuildWebServerServiceName(os.pkg_mgr)}), ws,
          context)
      != 0) {
    return 1;
  }
  Output(ws, "Required Bareos services are active.");
  return 0;
}

int RunStep(WsCodec& ws,
            const std::string& step,
            json_t* message,
            const SessionContext& context)
{
  if (step == "repository") return InstallRepository(ws, message, context);
  if (step == "packages") return InstallPackages(ws, message, context);
  if (step == "catalog") return InitializeCatalog(ws, context);
  if (step == "admin") return CreateAdmin(ws, context);
  if (step == "proxy") return ConfigureProxy(ws, context);
  if (step == "smoke_test") return RunSmokeTest(ws, context);
  throw std::runtime_error("Unknown setup step.");
}

void Handle(WsCodec& ws, json_t* message, const SessionContext& context)
{
  const auto action = StringField(message, "action");
  if (action == "state") {
    const auto os = DetectOs();
    std::lock_guard lock(Progress().mutex);
    json_t* completed = json_array();
    for (const auto& step : Progress().completed) {
      json_array_append_new(completed, json_string(step.c_str()));
    }
    const auto hostname = LocalHostname();
    const bool pkg_mgr_supported = IsSupportedPackageManager(os.pkg_mgr);
    const bool platform_supported
        = IsSupportedSetupPlatform(os.distro, os.pkg_mgr);
    json_t* known_paths = json_array();
    for (const auto& path : KnownRepoOsPaths()) {
      json_array_append_new(known_paths, json_string(path.c_str()));
    }
    json_t* suggested_paths = json_array();
    if (!platform_supported) {
      for (const auto& path : SuggestRepoOsPaths(os)) {
        json_array_append_new(suggested_paths, json_string(path.c_str()));
      }
    }
    const std::string repo_os_path
        = platform_supported ? BuildRepoOsPath(os.distro, os.version)
                             : std::string{};
    Send(
        ws,
        json_pack(
            "{s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:o,s:b,s:s,s:b,s:b,s:b,s:b,"
            "s:b,s:b,s:s,s:o,s:o,s:b,s:b}",
            "type", "state", "distro", os.distro.c_str(), "version",
            os.version.c_str(), "package_manager",
            PackageManagerName(os.pkg_mgr), "pretty_name",
            os.pretty_name.c_str(), "arch", os.arch.c_str(), "codename",
            os.codename.c_str(), "hostname", hostname.c_str(), "completed",
            completed, "finished", Progress().finished, "setup_version",
            BAREOS_FULL_VERSION, "peer_is_loopback", context.peer_is_loopback,
            "dry_run", context.setup.dry_run(),
            "subscription_credentials_in_browser",
            context.peer_is_loopback && !context.setup.dry_run(),
            "subscription_credentials_on_terminal",
            !context.peer_is_loopback && !context.setup.dry_run(),
            "platform_supported", platform_supported,
            "package_manager_supported", pkg_mgr_supported, "repo_os_path",
            repo_os_path.c_str(), "known_repo_os_paths", known_paths,
            "suggested_repo_os_paths", suggested_paths, "tape_support",
            Progress().tape.value_or(TapeSupportOptions{}).enabled,
            "allow_tape_repositories",
            Progress().tape.value_or(TapeSupportOptions{}).allow_repositories));
    return;
  }
  if (action == "close") {
    Send(ws, json_pack("{s:s}", "type", "closed"));
    ws.SendClose();
    RequestHttpServerShutdown();
    return;
  }
  if (action == "rollback") {
    std::lock_guard operation_lock(SetupOperationMutex());
    bool remove_admin = false;
    {
      std::lock_guard lock(Progress().mutex);
      remove_admin = Progress().admin_config_created;
    }
    if (context.setup.dry_run()) {
      Output(ws, "[preview] dry run: rollback effects not executed.");
    } else {
      if (remove_admin) {
        Run(Rm({"-f", std::string{kAdminConfigPath}}), ws, context);
      }
    }
    std::lock_guard lock(Progress().mutex);
    if (!context.setup.dry_run()) {
      Progress().completed.clear();
      Progress().failed_step.clear();
      Progress().finished = false;
      Progress().admin_password.clear();
      Progress().admin_config_created = false;
      Progress().tape.reset();
    }
    Send(ws, json_pack("{s:s}", "type", "rollback_complete"));
    return;
  }
  if (action != "run") {
    Error(ws, action, "Unknown setup action.");
    return;
  }

  const auto step = StringField(message, "step");
  static const std::set<std::string> allowed{
      "repository", "packages", "catalog", "admin", "proxy", "smoke_test"};
  if (!allowed.contains(step)) {
    Error(ws, step, "Unknown setup step.");
    return;
  }
  std::lock_guard operation_lock(SetupOperationMutex());
  {
    std::lock_guard lock(Progress().mutex);
    if (!context.setup.dry_run() && Progress().completed.contains(step)) {
      if (step == "packages") {
        try {
          const auto options = TapeOptions(message);
          if (!Progress().tape || options.enabled != Progress().tape->enabled
              || options.allow_repositories
                     != Progress().tape->allow_repositories) {
            Error(ws, step,
                  "Package choices cannot change after installation.");
            return;
          }
        } catch (const std::exception& error) {
          Error(ws, step, error.what());
          return;
        }
      }
      Send(ws, json_pack("{s:s,s:s,s:i}", "type", "done", "step", step.c_str(),
                         "exit_code", 0));
      return;
    }
  }
  int result = 1;
  try {
    result = RunStep(ws, step, message, context);
  } catch (const std::exception& error) {
    Error(ws, step, error.what());
    return;
  }
  if (result == 0 && !context.setup.dry_run()) {
    std::lock_guard lock(Progress().mutex);
    Progress().completed.insert(step);
    if (step == "packages") Progress().tape = TapeOptions(message);
    if (step == "smoke_test") Progress().finished = true;
  } else if (result != 0 && !context.setup.dry_run()) {
    std::lock_guard lock(Progress().mutex);
    Progress().failed_step = step;
  }
  Send(ws, json_pack("{s:s,s:s,s:i}", "type", "done", "step", step.c_str(),
                     "exit_code", result));
}

}  // namespace

int RunSetupStepForTests(int fd,
                         const std::string& step,
                         const std::string& json_message,
                         bool peer_is_loopback,
                         bool dry_run)
{
  WsCodec ws = WsCodec::FromUpgradedConnection(fd);
  SetupContext setup_context(dry_run);
  const SessionContext context{setup_context, peer_is_loopback};
  json_error_t error{};
  json_t* message = json_loads(json_message.c_str(), 0, &error);
  if (!message) message = json_object();
  const int result = RunStep(ws, step, message, context);
  json_decref(message);
  return result;
}

void RunSetupSession(int fd,
                     SetupContext& setup_context,
                     bool peer_is_loopback,
                     std::string request_headers,
                     std::string pending_input)
{
  WsCodec ws = [&] {
    if (request_headers.empty()) {
      return WsCodec::FromUpgradedConnection(fd, std::move(pending_input));
    }
    constexpr auto kSetupMaxWebSocketSize = 16 * 1024 * 1024;
    return WsCodec::Accept(fd, request_headers, std::move(pending_input),
                           std::chrono::milliseconds::zero(),
                           std::chrono::seconds(5), kSetupMaxWebSocketSize,
                           kSetupMaxWebSocketSize);
  }();
  const SessionContext context{setup_context, peer_is_loopback};
  try {
    while (!ws.IsClosed()) {
      const auto text = ws.RecvMessage();
      if (text.empty()) break;
      json_error_t error{};
      json_t* message = json_loads(text.c_str(), 0, &error);
      if (!message || !json_is_object(message)) {
        if (message) json_decref(message);
        Error(ws, "request", "Invalid JSON request.");
        continue;
      }
      Handle(ws, message, context);
      json_decref(message);
    }
  } catch (const std::exception& error) {
    std::cerr << "Setup session stopped: " << error.what() << "\n";
  }
}
