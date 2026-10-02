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
/**
 * @file
 * Command runner: fork/exec a command and stream stdout/stderr lines
 * to a callback as they arrive.
 */
#ifndef BAREOS_BAREOS_SETUP_COMMAND_RUNNER_H_
#define BAREOS_BAREOS_SETUP_COMMAND_RUNNER_H_

#include <array>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "os_detector.h"

/** Called for each output line.  stream is "stdout" or "stderr". */
using OutputCallback
    = std::function<void(std::string_view line, std::string_view stream)>;

enum class SetupTool
{
  Bash,
  Curl,
  Install,
  Chown,
  Systemctl,
  Su,
  Sh,
  AptGet,
  Dnf,
  Yum,
  Zypper,
  Rm,
  PostgresqlSetup,
  A2enmod,
  A2ensite,
  A2enflag,
  Echo,
  Sudo,
  OpenSSL,
  Chmod,
  Cat,
  Getenforce,
  Setsebool,
  XdgOpen,
  Open,
  SensibleBrowser,
  Count
};

/** A command can only name an executable through its registered wrapper. */
struct SetupCommand {
  SetupTool tool;
  std::vector<std::string> arguments;

  std::vector<std::string> Argv() const;
  bool operator==(const SetupCommand&) const = default;
};

using CommandLogCallback
    = std::function<void(const SetupCommand&, bool dry_run, bool has_input)>;
using PreviewCallback = std::function<void(bool has_input)>;

/**
 * Own the execution mode and resolved executable paths for one setup run.
 * Dry runs log commands without executing them; Remove() is a no-op in that
 * mode. Missing executables are looked up again later so tools installed by
 * the package-install step can be resolved when first used.
 */
class SetupContext {
 public:
  explicit SetupContext(bool dry_run = false);

  bool dry_run() const;
  bool IsToolAvailable(SetupTool tool) const;
  std::vector<std::string> MissingRequiredTools(PackageManager pkg_mgr) const;
  std::vector<std::string> MissingPostInstallTools(
      PackageManager pkg_mgr) const;

  int Run(const SetupCommand& command,
          bool run_as_root,
          OutputCallback output,
          CommandLogCallback log_command = {},
          PreviewCallback preview = {}) const;
  int RunWithInput(const SetupCommand& command,
                   const std::string& input,
                   bool run_as_root,
                   OutputCallback output,
                   CommandLogCallback log_command = {},
                   PreviewCallback preview = {}) const;

  void Remove(const std::filesystem::path& path) const;

 private:
  std::optional<std::string> ToolPath(SetupTool tool) const;
  int RunImpl(const SetupCommand& command,
              const std::string* input,
              bool run_as_root,
              OutputCallback output,
              CommandLogCallback log_command,
              PreviewCallback preview) const;

  bool dry_run_;
  mutable std::mutex tool_paths_mutex_;
  mutable std::array<std::optional<std::string>,
                     static_cast<size_t>(SetupTool::Count)>
      tool_paths_{};
};

SetupCommand Bash(std::vector<std::string> arguments);
SetupCommand Curl(std::vector<std::string> arguments);
SetupCommand Install(std::vector<std::string> arguments);
SetupCommand Chown(std::vector<std::string> arguments);
SetupCommand Systemctl(std::vector<std::string> arguments);
SetupCommand Su(std::vector<std::string> arguments);
SetupCommand Sh(std::vector<std::string> arguments);
SetupCommand AptGet(std::vector<std::string> arguments);
SetupCommand Dnf(std::vector<std::string> arguments);
SetupCommand Yum(std::vector<std::string> arguments);
SetupCommand Zypper(std::vector<std::string> arguments);
SetupCommand Rm(std::vector<std::string> arguments);
SetupCommand PostgresqlSetup(std::vector<std::string> arguments);
SetupCommand A2enmod(std::vector<std::string> arguments);
SetupCommand A2ensite(std::vector<std::string> arguments);
SetupCommand A2enflag(std::vector<std::string> arguments);
SetupCommand Echo(std::vector<std::string> arguments);
SetupCommand XdgOpen(std::vector<std::string> arguments);
SetupCommand Open(std::vector<std::string> arguments);
SetupCommand SensibleBrowser(std::vector<std::string> arguments);

/** True if this process is currently running as root (effective UID 0). */
bool IsRoot();

#endif  // BAREOS_BAREOS_SETUP_COMMAND_RUNNER_H_
