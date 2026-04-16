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
#include "tui_wizard.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <termios.h>
#include <unistd.h>

#include "os_detector.h"
#include "setup_steps.h"

namespace ansi {
static const char* const reset = "\033[0m";
static const char* const bold = "\033[1m";
static const char* const dim = "\033[2m";
static const char* const blue = "\033[34m";
static const char* const cyan = "\033[36m";
static const char* const green = "\033[32m";
static const char* const yellow = "\033[33m";
static const char* const red = "\033[31m";
}  // namespace ansi

static constexpr int kTotalSteps = 9;

static void PrintHeader(const std::string& title)
{
  std::cout << "\n"
            << ansi::bold << ansi::blue
            << "══════════════════════════════════════════════\n"
            << "  " << title << "\n"
            << "══════════════════════════════════════════════" << ansi::reset
            << "\n\n";
}

static void PrintStep(int n, const std::string& label)
{
  std::cout << ansi::dim << "[" << n << "/" << kTotalSteps << "] "
            << ansi::reset << ansi::bold << label << ansi::reset << "\n\n";
}

static void PrintOk(const std::string& msg)
{
  std::cout << ansi::green << "  ✓ " << ansi::reset << msg << "\n";
}

static void PrintWarn(const std::string& msg)
{
  std::cout << ansi::yellow << "  ⚠ " << ansi::reset << msg << "\n";
}

static void PrintErr(const std::string& msg)
{
  std::cout << ansi::red << "  ✗ " << ansi::reset << msg << "\n";
}

static std::string Prompt(const std::string& question,
                          const std::string& default_val = "")
{
  if (default_val.empty()) {
    std::cout << ansi::cyan << "  " << question << ": " << ansi::reset;
  } else {
    std::cout << ansi::cyan << "  " << question << " [" << default_val
              << "]: " << ansi::reset;
  }

  std::string line;
  if (!std::getline(std::cin, line)) return default_val;
  return line.empty() ? default_val : line;
}

static bool PromptYN(const std::string& question, bool default_yes = true)
{
  const std::string hint = default_yes ? "[Y/n]" : "[y/N]";
  std::cout << ansi::cyan << "  " << question << " " << hint << ": "
            << ansi::reset;
  std::string line;
  if (!std::getline(std::cin, line)) return default_yes;
  if (line.empty()) return default_yes;
  return static_cast<char>(std::tolower(line[0])) == 'y';
}

static std::string PromptPassword(const std::string& question)
{
  std::cout << ansi::cyan << "  " << question << ": " << ansi::reset
            << std::flush;

  struct termios oldt{}, newt{};
  const bool tty = (tcgetattr(STDIN_FILENO, &oldt) == 0);
  if (tty) {
    newt = oldt;
    newt.c_lflag &= ~static_cast<tcflag_t>(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  }

  std::string pw;
  std::getline(std::cin, pw);

  if (tty) tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  std::cout << "\n";
  return pw;
}

static std::string GeneratePassword(size_t len = 24)
{
  static const char chars[]
      = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789!@#$%^&*";
  std::random_device rd;
  std::uniform_int_distribution<size_t> dist(0, sizeof(chars) - 2);
  std::string pw;
  pw.reserve(len);
  for (size_t i = 0; i < len; ++i) pw += chars[dist(rd)];
  return pw;
}

static void PressEnter()
{
  std::cout << ansi::dim << "  Press Enter to continue..." << ansi::reset;
  std::string dummy;
  std::getline(std::cin, dummy);
}

static int PromptInt(const std::string& question, int default_val, int min_val)
{
  while (true) {
    const auto input = Prompt(question, std::to_string(default_val));
    try {
      const int value = std::stoi(input);
      if (value >= min_val) return value;
    } catch (...) {
    }
    PrintWarn("Please enter a number >= " + std::to_string(min_val) + ".");
  }
}

struct WizardState {
  OsInfo os_info;

  std::string repo_type = "subscription";
  std::string repo_login;
  std::string repo_password;

  bool disk_enabled = true;
  bool tape_enabled = false;
  bool storage_configured = false;
  DiskStorageConfig disk_config{
      true,
      "/var/lib/bareos/storage",
      false,
      15,
  };
  TapeStorageConfig tape_config;
  std::vector<TapeChangerInfo> tape_changers;
  std::vector<TapeDriveInfo> tape_drives;

  std::string admin_username = "admin";
  std::string admin_password;
  bool setup_db = true;
};

static std::string FormatTapeDeviceLabel(const TapeDriveInfo& device)
{
  std::ostringstream label;
  label << (device.identifier.empty() ? device.display_name
                                      : device.identifier);
  if (!device.serial_number.empty()) label << " / " << device.serial_number;
  label << " (" << device.path << ")";
  return label.str();
}

static std::string FormatTapeChangerLabel(const TapeChangerInfo& changer)
{
  std::ostringstream label;
  label << (changer.identifier.empty() ? changer.display_name
                                       : changer.identifier);
  if (!changer.serial_number.empty()) label << " / " << changer.serial_number;
  label << " (" << changer.path << ")";
  return label.str();
}

static std::string FormatChangerDriveIdentifiers(const TapeChangerInfo& changer)
{
  if (changer.drive_identifiers.empty()) return "(none)";

  std::ostringstream output;
  for (size_t i = 0; i < changer.drive_identifiers.size(); ++i) {
    const auto& drive = changer.drive_identifiers[i];
    if (i > 0) output << "; ";
    output << "Elem " << drive.element_address << ": ";
    for (size_t j = 0; j < drive.device_identifiers.size(); ++j) {
      if (j > 0) output << " | ";
      output << DescribeDeviceIdentifier(drive.device_identifiers[j]);
    }
  }
  return output.str();
}

static std::string JoinDriveNumbers(const std::vector<int>& numbers)
{
  std::ostringstream joined;
  for (size_t i = 0; i < numbers.size(); ++i) {
    if (i > 0) joined << ",";
    joined << numbers[i];
  }
  return joined.str();
}

static std::vector<int> DriveNumbersForAssignment(
    const TapeAssignment& assignment,
    const std::vector<TapeDriveInfo>& drives)
{
  std::vector<int> numbers;
  for (const auto& drive_path : assignment.drive_paths) {
    for (size_t i = 0; i < drives.size(); ++i) {
      if (drives[i].path == drive_path) {
        numbers.push_back(static_cast<int>(i + 1));
        break;
      }
    }
  }
  return numbers;
}

static std::vector<int> ParseDriveNumbers(const std::string& input)
{
  std::vector<int> numbers;
  std::set<int> seen;
  std::stringstream stream(input);
  std::string token;
  while (std::getline(stream, token, ',')) {
    token = Trim(token);
    if (token.empty()) continue;
    int value = 0;
    try {
      value = std::stoi(token);
    } catch (...) {
      throw std::runtime_error(
          "Enter comma-separated drive numbers such as 1,2.");
    }
    if (value < 1 || !seen.insert(value).second) {
      throw std::runtime_error("Invalid or duplicate drive number: " + token);
    }
    numbers.push_back(value);
  }
  return numbers;
}

static bool ApplyStorageConfiguration(WizardState& state, bool dry_run)
{
  state.disk_config.enabled = state.disk_enabled;
  state.tape_config.enabled = state.tape_enabled;

  std::string error;
  if (!ValidateStorageConfig(state.disk_config, state.tape_config, error)) {
    PrintErr(error);
    return false;
  }

  const auto defaults = ReadDirectorStorageDefaults();
  const auto script = BuildConfigureStorageScript(state.disk_config,
                                                  state.tape_config, defaults);

  if (dry_run) {
    std::istringstream stream(script);
    std::string line;
    while (std::getline(stream, line)) {
      std::cout << ansi::yellow << "  [dry-run] " << line << ansi::reset
                << "\n";
    }
    state.storage_configured = true;
    return true;
  }

  int rc = 0;
  try {
    rc = RunGeneratedScript(
        script, true, [](const std::string& line, const std::string& stream) {
          if (stream == "stderr") {
            std::cout << ansi::dim << "  " << line << ansi::reset << "\n";
          } else {
            std::cout << "  " << line << "\n";
          }
        });
  } catch (const std::exception& e) {
    PrintErr(e.what());
    return false;
  }
  if (rc != 0) {
    PrintErr("Storage configuration failed (exit code " + std::to_string(rc)
             + ").");
    return false;
  }

  PrintOk("Storage configuration applied successfully.");
  state.storage_configured = true;
  return true;
}

static void StepWelcome()
{
  PrintHeader("Welcome to Bareos Setup");
  std::cout << "  This wizard follows the same setup flow as the graphical\n"
            << "  Bareos web wizard.\n\n"
            << "  Steps:\n"
            << "    1. Detect operating system\n"
            << "    2. Configure repository\n"
            << "    3. Install the fixed Bareos package set\n"
            << "    4. Choose storage targets\n"
            << "    5. Configure disk storage\n"
            << "    6. Scan and assign tape changers and drives\n"
            << "    7. Initialize database\n"
            << "    8. Create admin user\n"
            << "    9. Summary\n\n";
  PressEnter();
}

static bool StepDetectOs(WizardState& state)
{
  PrintHeader("OS Detection");
  PrintStep(1, "Detecting operating system");

  try {
    state.os_info = DetectOs();
  } catch (const std::exception& e) {
    PrintErr(std::string("Detection failed: ") + e.what());
    return false;
  }

  PrintOk("Detected: " + state.os_info.pretty_name);
  std::cout << "\n"
            << "  Version    : " << state.os_info.version << "\n"
            << "  Arch       : " << state.os_info.arch << "\n"
            << "  Pkg manager: " << state.os_info.pkg_mgr << "\n\n";
  PressEnter();
  return true;
}

static bool StepRepo(WizardState& state)
{
  PrintHeader("Configure Repository");
  PrintStep(2, "Choose the Bareos package repository");

  std::cout << "  Repository type:\n"
            << "    1) Subscription (requires Bareos subscription)\n"
            << "    2) Community (free)\n\n";

  const std::string choice = Prompt("Choice", "1");
  if (choice == "2") {
    state.repo_type = "community";
    PrintOk("Using community repository");
  } else {
    state.repo_type = "subscription";
    PrintOk("Using subscription repository");
    state.repo_login = Prompt("Subscription login");
    state.repo_password = PromptPassword("Subscription password");
    if (state.repo_login.empty()) {
      PrintWarn("No login provided — repository access may fail.");
    }
  }

  const std::string base
      = (state.repo_type == "subscription")
            ? "https://download.bareos.com/bareos/release/latest"
            : "https://download.bareos.org/current";
  std::cout << "\n  Repository URL: " << ansi::cyan << base << "/"
            << BuildRepoOsPath(state.os_info.distro, state.os_info.version)
            << "/" << ansi::reset << "\n\n";

  return true;
}

static bool StepAddRepo(WizardState& state, bool dry_run)
{
  const auto cmd
      = BuildAddRepoCmd(state.os_info.distro, state.os_info.version,
                        state.repo_type, state.repo_login, state.repo_password);

  if (dry_run) {
    std::cout << ansi::yellow << "  [dry-run] sudo ";
    for (size_t i = 0; i < cmd.size(); ++i) {
      if (i > 0) std::cout << ' ';
      if (cmd[i].find(' ') != std::string::npos) {
        std::cout << '\'' << cmd[i] << '\'';
      } else {
        std::cout << cmd[i];
      }
    }
    std::cout << ansi::reset << "\n";
    PressEnter();
    return true;
  }

  const int rc = RunCommand(
      cmd, true, [](const std::string& line, const std::string& stream) {
        if (stream == "stderr") {
          std::cout << ansi::dim << "  " << line << ansi::reset << "\n";
        } else {
          std::cout << "  " << line << "\n";
        }
      });
  if (rc != 0) {
    PrintErr("Repository setup failed (exit code " + std::to_string(rc) + ").");
    return false;
  }
  PrintOk("Repository added successfully.");
  PressEnter();
  return true;
}

static bool StepInstall(WizardState& state, bool dry_run)
{
  PrintHeader("Install Packages");
  PrintStep(3, "Installing the fixed Bareos package set");

  const auto packages = BuildDefaultPackageList();
  std::cout << "  Packages:\n";
  for (const auto& package : packages) {
    std::cout << "    - " << package << "\n";
  }
  std::cout << "\n";

  const auto cmd = BuildInstallCmd(state.os_info.pkg_mgr, packages);
  if (dry_run) {
    std::cout << ansi::yellow << "  [dry-run] sudo ";
    for (size_t i = 0; i < cmd.size(); ++i) {
      if (i > 0) std::cout << ' ';
      if (cmd[i].find(' ') != std::string::npos) {
        std::cout << '\'' << cmd[i] << '\'';
      } else {
        std::cout << cmd[i];
      }
    }
    std::cout << ansi::reset << "\n";
    PressEnter();
    return true;
  }

  const int rc = RunCommand(
      cmd, true, [](const std::string& line, const std::string& stream) {
        if (stream == "stderr") {
          std::cout << ansi::dim << "  " << line << ansi::reset << "\n";
        } else {
          std::cout << "  " << line << "\n";
        }
      });
  if (rc != 0) {
    PrintErr("Installation failed (exit code " + std::to_string(rc) + ").");
    return false;
  }
  PrintOk("Packages installed successfully.");
  PressEnter();
  return true;
}

static bool StepStorageTargets(WizardState& state)
{
  PrintHeader("Storage Targets");
  PrintStep(4, "Choose disk and tape storage targets");

  const auto inventory = DiscoverTapeStorageInventory();
  state.tape_changers = inventory.changers;
  state.tape_drives = inventory.drives;

  if (!state.tape_changers.empty()) {
    PrintOk("Detected " + std::to_string(state.tape_changers.size())
            + " tape changer(s) and " + std::to_string(state.tape_drives.size())
            + " tape drive(s).");
    if (!state.tape_enabled) {
      state.tape_enabled = true;
      PrintOk("Tape storage was preselected because a changer was found.");
    }
  } else {
    PrintWarn("No tape changers were detected during the initial scan.");
  }

  do {
    state.disk_enabled = PromptYN("Store to disk", state.disk_enabled);
    state.tape_enabled = PromptYN("Store to tape changer", state.tape_enabled);
    if (!state.disk_enabled && !state.tape_enabled) {
      PrintWarn("Select at least one storage target.");
    }
  } while (!state.disk_enabled && !state.tape_enabled);

  state.disk_config.enabled = state.disk_enabled;
  state.tape_config.enabled = state.tape_enabled;
  return true;
}

static bool StepDiskStorage(WizardState& state, bool dry_run)
{
  PrintHeader("Disk Storage");
  PrintStep(5, "Configure disk-backed storage");

  if (!state.disk_enabled) {
    PrintWarn("Disk storage was not selected.");
    PressEnter();
    return true;
  }

  state.disk_config.storage_path
      = Prompt("Disk storage path", state.disk_config.storage_path);
  state.disk_config.dedupable
      = PromptYN("Use dedupable storage", state.disk_config.dedupable);
  state.disk_config.concurrent_jobs = PromptInt(
      "Parallel backup operations", state.disk_config.concurrent_jobs, 1);

  if (state.tape_enabled) return true;

  std::cout << "\n";
  return ApplyStorageConfiguration(state, dry_run);
}

static void PrintTapeInventory(const WizardState& state)
{
  std::cout << "  Detected tape changers:\n";
  if (state.tape_changers.empty()) {
    std::cout << "    (none)\n";
  } else {
    for (size_t i = 0; i < state.tape_changers.size(); ++i) {
      const auto& changer = state.tape_changers[i];
      std::cout << "    " << (i + 1) << ") " << FormatTapeChangerLabel(changer)
                << "\n";
      std::cout << "       Drives: " << changer.status.drives
                << ", Slots: " << changer.status.slots
                << ", I/E Slots: " << changer.status.ie_slots << "\n";
      std::cout << "       Tape identifiers: "
                << FormatChangerDriveIdentifiers(changer) << "\n";
      if (!changer.status.error.empty()) {
        std::cout << "       Status: " << Trim(changer.status.error) << "\n";
      }
    }
  }

  std::cout << "\n  Detected tape drives:\n";
  if (state.tape_drives.empty()) {
    std::cout << "    (none)\n";
  } else {
    for (size_t i = 0; i < state.tape_drives.size(); ++i) {
      std::cout << "    " << (i + 1) << ") "
                << FormatTapeDeviceLabel(state.tape_drives[i]) << "\n";
    }
  }
  std::cout << "\n";
}

static TapeAssignment CurrentAssignmentFor(const WizardState& state,
                                           const std::string& changer_path)
{
  for (const auto& assignment : state.tape_config.assignments) {
    if (assignment.changer_path == changer_path) return assignment;
  }
  return {changer_path, {}};
}

static bool CollectTapeAssignments(WizardState& state)
{
  if (state.tape_changers.empty()) {
    PrintErr("No tape changers were detected.");
    return false;
  }
  if (state.tape_drives.empty()) {
    PrintErr("No tape drives were detected.");
    return false;
  }

  if (state.tape_config.assignments.empty()) {
    state.tape_config.assignments
        = SuggestTapeAssignments(state.tape_changers, state.tape_drives);
    if (!state.tape_config.assignments.empty()) {
      PrintOk("Assigned tape drives automatically for each detected library.");
    }
  }

  while (true) {
    std::vector<TapeAssignment> assignments;
    for (const auto& changer : state.tape_changers) {
      const auto current = CurrentAssignmentFor(state, changer.path);
      const auto current_numbers
          = DriveNumbersForAssignment(current, state.tape_drives);

      std::cout << "  Assign drives to " << FormatTapeChangerLabel(changer)
                << "\n";
      std::cout << "    Enter comma-separated drive numbers in the order "
                   "Bareos should use them.\n";

      const std::string input
          = Prompt("Drive numbers", JoinDriveNumbers(current_numbers));

      if (Trim(input).empty()) continue;

      try {
        const auto selected_numbers = ParseDriveNumbers(input);
        TapeAssignment assignment;
        assignment.changer_path = changer.path;
        for (const auto number : selected_numbers) {
          if (number < 1
              || number > static_cast<int>(state.tape_drives.size())) {
            throw std::runtime_error("Drive number out of range: "
                                     + std::to_string(number));
          }
          assignment.drive_paths.push_back(
              state.tape_drives[static_cast<size_t>(number - 1)].path);
        }
        assignments.push_back(std::move(assignment));
      } catch (const std::exception& e) {
        PrintWarn(e.what());
        assignments.clear();
        break;
      }
      std::cout << "\n";
    }

    if (assignments.empty()) continue;

    TapeStorageConfig tape_config;
    tape_config.enabled = true;
    tape_config.assignments = assignments;

    DiskStorageConfig disk_config = state.disk_config;
    disk_config.enabled = state.disk_enabled;

    std::string error;
    if (!ValidateStorageConfig(disk_config, tape_config, error)) {
      PrintWarn(error);
      continue;
    }

    state.tape_config = std::move(tape_config);
    return true;
  }
}

static bool StepTapeStorage(WizardState& state, bool dry_run)
{
  PrintHeader("Tape Storage");
  PrintStep(6, "Scan and assign tape changers and drives");

  if (!state.tape_enabled) {
    PrintWarn("Tape storage was not selected.");
    PressEnter();
    return true;
  }

  PrintWarn("Make sure the tape changer is attached before scanning.");
  const auto inventory = DiscoverTapeStorageInventory();
  state.tape_changers = inventory.changers;
  state.tape_drives = inventory.drives;

  PrintTapeInventory(state);
  if (!CollectTapeAssignments(state)) return false;

  std::cout << "\n";
  return ApplyStorageConfiguration(state, dry_run);
}

static bool StepDatabase(WizardState& state, bool dry_run)
{
  PrintHeader("Database Configuration");
  PrintStep(7, "Initialize the Bareos catalog database");

  state.setup_db = PromptYN(
      "Run create_bareos_database / make_bareos_tables / "
      "grant_bareos_privileges",
      true);

  if (!state.setup_db) {
    PrintWarn("Skipping database initialization.");
    return true;
  }

  const auto cmd = BuildDbCmd();
  if (dry_run) {
    std::cout << ansi::yellow << "  [dry-run] sudo ";
    for (size_t i = 0; i < cmd.size(); ++i) {
      if (i > 0) std::cout << ' ';
      if (cmd[i].find(' ') != std::string::npos) {
        std::cout << '\'' << cmd[i] << '\'';
      } else {
        std::cout << cmd[i];
      }
    }
    std::cout << ansi::reset << "\n";
    PressEnter();
    return true;
  }

  const int rc = RunCommand(
      cmd, true, [](const std::string& line, const std::string& stream) {
        if (stream == "stderr") {
          std::cout << ansi::dim << "  " << line << ansi::reset << "\n";
        } else {
          std::cout << "  " << line << "\n";
        }
      });
  if (rc != 0) {
    PrintErr("Database setup failed (exit code " + std::to_string(rc) + ").");
    return false;
  }

  PrintOk("Database initialized successfully.");
  PressEnter();
  return true;
}

static bool StepAdminUser(WizardState& state, bool dry_run)
{
  PrintHeader("Administrative User");
  PrintStep(8, "Create the Bareos WebUI admin user");

  state.admin_username = Prompt("Username", state.admin_username);
  while (true) {
    std::cout << "  (leave blank to generate a random password)\n";
    state.admin_password = PromptPassword("Password");
    if (state.admin_password.empty()) {
      state.admin_password = GeneratePassword();
      std::cout << "  Generated password: " << ansi::yellow
                << state.admin_password << ansi::reset << "\n"
                << "  " << ansi::bold << "Save this password!" << ansi::reset
                << "\n\n";
      break;
    }
    if (state.admin_password.size() < 8) {
      PrintWarn("Password must be at least 8 characters. Try again.");
      continue;
    }
    const std::string confirm = PromptPassword("Confirm password");
    if (confirm != state.admin_password) {
      PrintWarn("Passwords do not match. Try again.");
      continue;
    }
    break;
  }

  const auto cmd
      = BuildAdminUserCmd(state.admin_username, state.admin_password);
  if (dry_run) {
    std::cout << ansi::yellow << "  [dry-run] sudo ";
    for (size_t i = 0; i < cmd.size(); ++i) {
      if (i > 0) std::cout << ' ';
      if (cmd[i].find(' ') != std::string::npos) {
        std::cout << '\'' << cmd[i] << '\'';
      } else {
        std::cout << cmd[i];
      }
    }
    std::cout << ansi::reset << "\n";
    PressEnter();
    return true;
  }

  const int rc = RunCommand(
      cmd, true, [](const std::string& line, const std::string& stream) {
        if (stream == "stderr") {
          std::cout << ansi::dim << "  " << line << ansi::reset << "\n";
        } else {
          std::cout << "  " << line << "\n";
        }
      });
  if (rc != 0) {
    PrintErr("Failed to create admin user (exit code " + std::to_string(rc)
             + ").");
    return false;
  }

  PrintOk("Admin user '" + state.admin_username + "' created.");
  PressEnter();
  return true;
}

static void StepSummary(const WizardState& state)
{
  PrintHeader("Summary");
  PrintStep(9, "Setup complete");

  PrintOk("Operating System : " + state.os_info.pretty_name);
  PrintOk("Repository       : " + state.repo_type);

  std::string targets;
  if (state.disk_enabled) targets += "disk ";
  if (state.tape_enabled) targets += "tape";
  PrintOk("Storage Targets  : " + targets);

  if (state.disk_enabled) {
    PrintOk("Disk Storage     : " + state.disk_config.storage_path);
  }
  if (state.tape_enabled) {
    PrintOk("Tape Changers    : "
            + std::to_string(state.tape_config.assignments.size()));
  }
  if (state.setup_db) PrintOk("Database         : initialized");
  PrintOk("Admin user       : " + state.admin_username);

  std::cout << "\n  Open the WebUI at: " << ansi::cyan
            << "http://localhost:9100" << ansi::reset << "\n"
            << "  Log in with user: " << ansi::bold << state.admin_username
            << ansi::reset << "\n\n";
}

int RunTuiWizard(bool dry_run)
{
  if (dry_run) {
    std::cout << ansi::yellow << "[dry-run mode — no commands will be executed]"
              << ansi::reset << "\n";
  }

  WizardState state;
  StepWelcome();

  if (!StepDetectOs(state)) return 1;
  if (!StepRepo(state)) return 1;
  if (!StepAddRepo(state, dry_run)) return 1;
  if (!StepInstall(state, dry_run)) return 1;
  if (!StepStorageTargets(state)) return 1;
  if (!StepDiskStorage(state, dry_run)) return 1;
  if (!StepTapeStorage(state, dry_run)) return 1;
  if (!StepDatabase(state, dry_run)) return 1;
  if (!StepAdminUser(state, dry_run)) return 1;
  StepSummary(state);
  return 0;
}
