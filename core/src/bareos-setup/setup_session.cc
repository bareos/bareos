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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <jansson.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string_view>

#include "command_runner.h"
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
};

SetupProgress& Progress()
{
  static SetupProgress progress;
  return progress;
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
            const std::string& line,
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

std::filesystem::path RuntimeFile(const std::string& prefix)
{
  const char* runtime = std::getenv("XDG_RUNTIME_DIR");
  std::filesystem::path directory
      = runtime && *runtime ? runtime : ".bareos-setup-runtime";
  if (!std::filesystem::exists(directory)) {
    std::filesystem::create_directories(directory);
    ::chmod(directory.c_str(), 0700);
  }
  std::string pattern = (directory / (prefix + "-XXXXXX")).string();
  std::vector<char> name(pattern.begin(), pattern.end());
  name.push_back('\0');
  const int fd = ::mkstemp(name.data());
  if (fd < 0) throw std::runtime_error("Unable to create a private setup file");
  ::close(fd);
  ::chmod(name.data(), 0600);
  return name.data();
}

std::vector<std::string> SecretsFor(json_t* message)
{
  std::vector<std::string> secrets;
  const auto login = StringField(message, "repository_login");
  const auto password = StringField(message, "repository_password");
  if (!login.empty()) secrets.push_back(login);
  if (!password.empty()) secrets.push_back(password);
  std::lock_guard lock(Progress().mutex);
  if (!Progress().admin_password.empty())
    secrets.push_back(Progress().admin_password);
  return secrets;
}

int Run(const std::vector<std::string>& command,
        WsCodec& ws,
        const std::vector<std::string>& secrets = {})
{
  return RunCommand(
      command, true,
      [&ws, &secrets](const std::string& line, const std::string&) {
        Output(ws, line, secrets);
      });
}

int InstallRepository(WsCodec& ws, json_t* message)
{
  const auto distro = StringField(message, "distro");
  const auto version = StringField(message, "version");
  const auto repo_type = StringField(message, "repository");
  const auto login = StringField(message, "repository_login");
  const auto password = StringField(message, "repository_password");
  const auto os = DetectOs();
  if (distro != os.distro || version != os.version
      || !IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    throw std::runtime_error("This Linux distribution is not supported.");
  }
  if (repo_type != "community" && repo_type != "subscription") {
    throw std::runtime_error("Select a supported Bareos repository.");
  }
  if (repo_type == "subscription" && (login.empty() || password.empty())) {
    throw std::runtime_error("Subscription credentials are required.");
  }

  auto command
      = BuildAddRepoCmd(os.distro, os.version, repo_type, login, password);
  const auto script = RuntimeFile("repository");
  command.insert(command.end() - 1, {"--output", script.string()});
  const auto secrets = SecretsFor(message);
  const int download = Run(command, ws, secrets);
  if (download != 0) {
    std::filesystem::remove(script);
    return download;
  }
  Output(ws, "Installing the approved Bareos repository.");
  const int result = Run({"bash", script.string()}, ws, secrets);
  std::filesystem::remove(script);
  return result;
}

int InstallPackages(WsCodec& ws)
{
  const auto os = DetectOs();
  if (!IsSupportedSetupPlatform(os.distro, os.pkg_mgr)) {
    throw std::runtime_error("This Linux distribution is not supported.");
  }

  Output(ws, "Installing the fixed Bareos package set.");
  return Run(BuildInstallCmd(os.pkg_mgr, BuildDefaultPackageList()), ws);
}

int ConfigureStorage(WsCodec& ws, json_t* message)
{
  const auto path = StringField(message, "storage_path");
  if (path.empty() || path == "/var/lib/bareos/storage") {
    Output(ws, "Keeping package-provided disk storage defaults.");
    return 0;
  }
  if (!IsSafeStoragePath(path)) {
    throw std::runtime_error("Storage path must be an absolute safe path.");
  }
  return Run({"install", "-d", "-m", "0750", path}, ws);
}

int InitializeCatalog(WsCodec& ws)
{
  // Package scripts are idempotent, but do not run them when the catalog
  // marker is already present.  The marker is created only by this wizard.
  const std::filesystem::path marker = "/var/lib/bareos/.catalog-initialized";
  if (std::filesystem::exists(marker)) {
    Output(ws, "The Bareos catalog is already initialized.");
    return 0;
  }
  for (const auto& command :
       {std::vector<std::string>{
            "/usr/lib/bareos/scripts/create_bareos_database"},
        std::vector<std::string>{"/usr/lib/bareos/scripts/make_bareos_tables"},
        std::vector<std::string>{
            "/usr/lib/bareos/scripts/grant_bareos_privileges"}}) {
    if (Run(command, ws) != 0) return 1;
  }
  const int marker_result
      = Run({"install", "-D", "-m", "0640", "/dev/null", marker.string()}, ws);
  return marker_result;
}

int CreateAdmin(WsCodec& ws)
{
  constexpr std::string_view username = "admin";
  const std::string password = GenerateSetupSecret();
  const std::string resource =
      "Console {\n"
      "  Name = admin\n"
      "  Password = \"" + password
      + "\"\n"
        "  Profile = \"webui-admin\"\n"
      "  TLS Enable = yes\n"
        "}\n";
  const std::string path = "/etc/bareos/bareos-dir.d/console/admin.conf";
  const int write_result = RunCommandWithInput(
      {"install", "-D", "-m", "0640", "/dev/stdin", path}, resource, true,
      [&ws, &password](const std::string& line, const std::string&) {
        Output(ws, line, {password});
      });
  if (write_result != 0) return write_result;
  if (Run({"chown", "root:bareos", path}, ws, {password}) != 0) return 1;
  if (Run({"systemctl", "reload", "bareos-dir"}, ws, {password}) != 0) return 1;
  {
    std::lock_guard lock(Progress().mutex);
    Progress().admin_password = password;
  }
  Send(ws, json_pack("{s:s,s:s,s:s}", "type", "admin_credentials", "username",
                     username.data(), "password", password.c_str()));
  return 0;
}

int ConfigureProxy(WsCodec& ws)
{
  const std::string config
      = "[listen]\n"
        "address = 127.0.0.1\n"
        "port = 9104\n"
        "\n"
        "[bareos-dir]\n"
        "address = 127.0.0.1\n"
        "port = 9101\n"
        "director_name = bareos-dir\n"
        "tls_psk_disable = no\n";
  const std::string path = "/etc/bareos-webui-proxy/bareos-webui-proxy.ini";
  if (RunCommandWithInput({"install", "-D", "-m", "0640", "/dev/stdin", path},
                          config, true,
                          [&ws](const std::string& line, const std::string&) {
                            Output(ws, line);
                          })
      != 0) {
    return 1;
  }
  if (Run({"systemctl", "enable", "--now", "bareos-webui-proxy"}, ws) != 0) {
    return 1;
  }
  return 0;
}

int RunSmokeTest(WsCodec& ws)
{
  for (const auto& command :
       {std::vector<std::string>{"bareos-dir", "-t"},
        std::vector<std::string>{"bareos-sd", "-t"},
        std::vector<std::string>{"bareos-fd", "-t"},
        std::vector<std::string>{"systemctl", "is-active", "bareos-dir"},
        std::vector<std::string>{"systemctl", "is-active", "bareos-sd"},
        std::vector<std::string>{"systemctl", "is-active", "bareos-fd"},
        std::vector<std::string>{"systemctl", "is-active",
                                 "bareos-webui-proxy"}}) {
    if (Run(command, ws) != 0) return 1;
  }
  Output(ws, "Backup and restore smoke test prerequisites verified.");
  return 0;
}

int RunStep(WsCodec& ws, const std::string& step, json_t* message)
{
  if (step == "repository") return InstallRepository(ws, message);
  if (step == "packages") return InstallPackages(ws);
  if (step == "storage") return ConfigureStorage(ws, message);
  if (step == "catalog") return InitializeCatalog(ws);
  if (step == "admin") return CreateAdmin(ws);
  if (step == "proxy") return ConfigureProxy(ws);
  if (step == "smoke_test") return RunSmokeTest(ws);
  throw std::runtime_error("Unknown setup step.");
}

void Handle(WsCodec& ws, json_t* message)
{
  const auto action = StringField(message, "action");
  if (action == "state") {
    const auto os = DetectOs();
    std::lock_guard lock(Progress().mutex);
    json_t* completed = json_array();
    for (const auto& step : Progress().completed) {
      json_array_append_new(completed, json_string(step.c_str()));
    }
    Send(ws,
         json_pack("{s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:o,s:b,s:s}", "type", "state",
                   "distro", os.distro.c_str(), "version", os.version.c_str(),
                   "package_manager", os.pkg_mgr.c_str(), "pretty_name",
                   os.pretty_name.c_str(), "arch", os.arch.c_str(), "codename",
                   os.codename.c_str(), "completed", completed, "finished",
                   Progress().finished, "setup_version", BAREOS_FULL_VERSION));
    return;
  }
  if (action == "script") {
    const std::string script
        = "#!/bin/sh\nset -eu\n"
          "# Preview only: credentials and subscription details are omitted.\n"
          "# Use bareos-setup to execute approved package-manager commands.\n"
          "install -d /var/lib/bareos/storage\n"
          "systemctl enable --now bareos-dir bareos-sd bareos-fd "
          "bareos-webui-proxy\n";
    Send(ws,
         json_pack("{s:s,s:s}", "type", "script", "content", script.c_str()));
    return;
  }
  if (action == "rollback") {
    const std::string admin = "/etc/bareos/bareos-dir.d/console/admin.conf";
    Run({"rm", "-f", admin}, ws);
    std::lock_guard lock(Progress().mutex);
    Progress().completed.clear();
    Progress().failed_step.clear();
    Progress().finished = false;
    Progress().admin_password.clear();
    Send(ws, json_pack("{s:s}", "type", "rollback_complete"));
    return;
  }
  if (action != "run") {
    Error(ws, action, "Unknown setup action.");
    return;
  }

  const auto step = StringField(message, "step");
  static const std::set<std::string> allowed{
      "repository", "packages", "storage",   "catalog",
      "admin",      "proxy",    "smoke_test"};
  if (!allowed.contains(step)) {
    Error(ws, step, "Unknown setup step.");
    return;
  }
  {
    std::lock_guard lock(Progress().mutex);
    if (Progress().completed.contains(step)) {
      Send(ws, json_pack("{s:s,s:s,s:i}", "type", "done", "step", step.c_str(),
                         "exit_code", 0));
      return;
    }
  }
  int result = 1;
  try {
    result = RunStep(ws, step, message);
  } catch (const std::exception& error) {
    Error(ws, step, error.what());
    return;
  }
  if (result == 0) {
    std::lock_guard lock(Progress().mutex);
    Progress().completed.insert(step);
    if (step == "smoke_test") Progress().finished = true;
  } else {
    std::lock_guard lock(Progress().mutex);
    Progress().failed_step = step;
  }
  Send(ws, json_pack("{s:s,s:s,s:i}", "type", "done", "step", step.c_str(),
                     "exit_code", result));
}

}  // namespace

void RunSetupSession(int fd, bool /*dry_run*/)
{
  WsCodec ws(fd);
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
      Handle(ws, message);
      json_decref(message);
    }
  } catch (const std::exception& error) {
    std::cerr << "Setup session stopped: " << error.what() << "\n";
  }
}
