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

#include <array>
#include <cctype>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <termios.h>
#include <unistd.h>

#include "command_runner.h"
#include "os_detector.h"
#include "setup_steps.h"

// ---------------------------------------------------------------------------
// ANSI helpers
// ---------------------------------------------------------------------------
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

static void PrintHeader(const std::string& title)
{
  std::cout << "\n"
            << ansi::bold << ansi::blue
            << "══════════════════════════════════════════════\n"
            << "  " << title << "\n"
            << "══════════════════════════════════════════════" << ansi::reset
            << "\n\n";
}

static void PrintStep(int n, int total, const std::string& label)
{
  std::cout << ansi::dim << "[" << n << "/" << total << "] " << ansi::reset
            << ansi::bold << label << ansi::reset << "\n\n";
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

// ---------------------------------------------------------------------------
// Input helpers
// ---------------------------------------------------------------------------

/** Prompt and read a line; use default_val if the user just presses Enter. */
static std::string Prompt(const std::string& question,
                          const std::string& default_val = "")
{
  if (default_val.empty())
    std::cout << ansi::cyan << "  " << question << ": " << ansi::reset;
  else
    std::cout << ansi::cyan << "  " << question << " [" << default_val
              << "]: " << ansi::reset;

  std::string line;
  if (!std::getline(std::cin, line)) return default_val;
  if (line.empty()) return default_val;
  return line;
}

/** Prompt for a yes/no question. Returns true for yes. */
static bool PromptYN(const std::string& question, bool default_yes = true)
{
  std::string hint = default_yes ? "[Y/n]" : "[y/N]";
  std::cout << ansi::cyan << "  " << question << " " << hint << ": "
            << ansi::reset;
  std::string line;
  if (!std::getline(std::cin, line)) return default_yes;
  if (line.empty()) return default_yes;
  char c = static_cast<char>(std::tolower(line[0]));
  return c == 'y';
}

/** Read a password without echo. */
static std::string PromptPassword(const std::string& question)
{
  std::cout << ansi::cyan << "  " << question << ": " << ansi::reset
            << std::flush;

  // Disable echo
  struct termios oldt{}, newt{};
  bool tty = (tcgetattr(STDIN_FILENO, &oldt) == 0);
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

/** Generate a random password. */
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

/** Press enter to continue. */
static void PressEnter()
{
  std::cout << ansi::dim << "  Press Enter to continue..." << ansi::reset;
  std::string dummy;
  std::getline(std::cin, dummy);
}

// ---------------------------------------------------------------------------
// Command execution
// ---------------------------------------------------------------------------

static int RunStep(const std::vector<std::string>& cmd,
                   bool use_sudo,
                   bool dry_run)
{
  if (dry_run) {
    std::cout << ansi::yellow << "  [dry-run] ";
    if (use_sudo) std::cout << "sudo ";
    for (size_t i = 0; i < cmd.size(); ++i) {
      if (i > 0) std::cout << ' ';
      if (cmd[i].find(' ') != std::string::npos)
        std::cout << '\'' << cmd[i] << '\'';
      else
        std::cout << cmd[i];
    }
    std::cout << ansi::reset << "\n";
    return 0;
  }

  int exit_code = RunCommand(
      cmd, use_sudo, [](const std::string& line, const std::string& stream) {
        if (stream == "stderr")
          std::cout << ansi::dim << "  " << line << ansi::reset << "\n";
        else
          std::cout << "  " << line << "\n";
      });
  return exit_code;
}

// ---------------------------------------------------------------------------
// Wizard state
// ---------------------------------------------------------------------------

struct WizardState {
  OsInfo os_info;

  // Components
  bool comp_director = true;
  bool comp_storage = true;
  bool comp_filedaemon = true;
  bool comp_webui = true;

  // Repository
  std::string repo_type = "subscription";
  std::string repo_login;
  std::string repo_password;

  // Admin user
  std::string admin_username = "admin";
  std::string admin_password;

  // DB
  bool setup_db = true;
};

// ---------------------------------------------------------------------------
// Wizard steps
// ---------------------------------------------------------------------------

static void StepWelcome()
{
  PrintHeader("Welcome to Bareos Setup");
  std::cout << "  This wizard will guide you through the installation and\n"
            << "  initial configuration of Bareos.\n\n"
            << "  Steps:\n"
            << "    1. Detect operating system\n"
            << "    2. Select components\n"
            << "    3. Configure repository\n"
            << "    4. Install packages\n"
            << "    5. Initialize database\n"
            << "    6. Create admin user\n"
            << "    7. Summary\n\n";
  PressEnter();
}

static bool StepDetectOs(WizardState& state)
{
  PrintHeader("OS Detection");
  PrintStep(1, 7, "Detecting operating system");

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

  if (state.os_info.pkg_mgr == "unknown")
    PrintWarn("Package manager not recognized — commands may need adjustment.");

  PressEnter();
  return true;
}

static void StepComponents(WizardState& state)
{
  PrintHeader("Select Components");
  PrintStep(2, 7, "Choose which Bareos components to install");

  struct Comp {
    bool& flag;
    const char* label;
    const char* desc;
  };
  std::array<Comp, 4> comps = {{
      {state.comp_director, "Director",
       "Controls backup and restore operations"},
      {state.comp_storage, "Storage Daemon",
       "Manages physical media and storage volumes"},
      {state.comp_filedaemon, "File Daemon",
       "Client agent on machines to be backed up"},
      {state.comp_webui, "WebUI", "Web-based management interface"},
  }};

  for (auto& c : comps) {
    c.flag = PromptYN(std::string(c.label) + " (" + c.desc + ")", c.flag);
  }
  std::cout << "\n";
}

static bool StepRepo(WizardState& state)
{
  PrintHeader("Configure Repository");
  PrintStep(3, 7, "Choose the Bareos package repository");

  std::cout << "  Repository type:\n"
            << "    1) Subscription (requires Bareos subscription)\n"
            << "    2) Community (free)\n\n";

  std::string choice = Prompt("Choice", "1");
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
            << CapFirst(state.os_info.distro) << "_" << state.os_info.version
            << "/" << ansi::reset << "\n\n";

  return true;
}

static bool StepAddRepo(WizardState& state, bool dry_run)
{
  std::cout << "\n";
  PrintOk("Adding repository...\n");

  auto cmd
      = BuildAddRepoCmd(state.os_info.distro, state.os_info.version,
                        state.repo_type, state.repo_login, state.repo_password);

  int rc = RunStep(cmd, true, dry_run);
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
  PrintStep(4, 7, "Installing selected Bareos packages");

  // Build package list
  static const struct {
    bool WizardState::* flag;
    const char* pkg;
  } pkg_map[] = {
      {&WizardState::comp_director, "bareos-director"},
      {&WizardState::comp_director, "bareos-bconsole"},
      {&WizardState::comp_storage, "bareos-storage"},
      {&WizardState::comp_filedaemon, "bareos-filedaemon"},
      {&WizardState::comp_webui, "bareos-webui"},
  };

  std::vector<std::string> packages;
  for (const auto& m : pkg_map)
    if (state.*m.flag) packages.push_back(m.pkg);

  if (packages.empty()) {
    PrintWarn("No components selected — skipping installation.");
    return true;
  }

  std::cout << "  Packages: ";
  for (const auto& p : packages) std::cout << p << " ";
  std::cout << "\n\n";

  auto cmd = BuildInstallCmd(state.os_info.pkg_mgr, packages);
  int rc = RunStep(cmd, true, dry_run);
  if (rc != 0) {
    PrintErr("Installation failed (exit code " + std::to_string(rc) + ").");
    return false;
  }
  PrintOk("Packages installed successfully.");
  PressEnter();
  return true;
}

static bool StepDatabase(WizardState& state, bool dry_run)
{
  PrintHeader("Database Configuration");
  PrintStep(5, 7, "Initialize Bareos catalog database");

  state.setup_db = PromptYN(
      "Run create_bareos_database / make_bareos_tables / "
      "grant_bareos_privileges",
      true);

  if (!state.setup_db) {
    PrintWarn("Skipping database initialization.");
    return true;
  }

  std::cout << "\n";
  auto cmd = BuildDbCmd();
  int rc = RunStep(cmd, true, dry_run);
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
  PrintStep(6, 7, "Create Bareos WebUI admin user");

  state.admin_username = Prompt("Username", "admin");

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
    std::string confirm = PromptPassword("Confirm password");
    if (confirm != state.admin_password) {
      PrintWarn("Passwords do not match. Try again.");
      continue;
    }
    break;
  }

  auto cmd = BuildAdminUserCmd(state.admin_username, state.admin_password);
  int rc = RunStep(cmd, true, dry_run);
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
  PrintStep(7, 7, "Setup complete");

  PrintOk("Operating System : " + state.os_info.pretty_name);

  std::string comps;
  if (state.comp_director) comps += "Director ";
  if (state.comp_storage) comps += "Storage ";
  if (state.comp_filedaemon) comps += "FileDaemon ";
  if (state.comp_webui) comps += "WebUI";
  PrintOk("Components       : " + comps);

  PrintOk("Repository       : " + state.repo_type);
  if (state.setup_db) PrintOk("Database         : initialized");
  PrintOk("Admin user       : " + state.admin_username);

  if (state.comp_webui)
    std::cout << "\n  Open the WebUI at: " << ansi::cyan
              << "http://localhost:9100" << ansi::reset << "\n"
              << "  Log in with user: " << ansi::bold << state.admin_username
              << ansi::reset << "\n\n";
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int RunTuiWizard(bool dry_run)
{
  if (dry_run)
    std::cout << ansi::yellow << "[dry-run mode — no commands will be executed]"
              << ansi::reset << "\n";

  WizardState state;

  StepWelcome();

  if (!StepDetectOs(state)) return 1;
  StepComponents(state);
  if (!StepRepo(state)) return 1;
  if (!StepAddRepo(state, dry_run)) return 1;
  if (!StepInstall(state, dry_run)) return 1;
  if (!StepDatabase(state, dry_run)) return 1;
  if (!StepAdminUser(state, dry_run)) return 1;
  StepSummary(state);

  return 0;
}
