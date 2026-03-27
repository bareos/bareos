/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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
#include "setup_session.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Reuse the WsCodec from bareos-webui-proxy via a local copy included via
// the include path configured in CMakeLists.txt.
#include "ws_codec.h"

#include <jansson.h>

#include "command_runner.h"
#include "os_detector.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Serialize a json_t* to a std::string and decref it. */
static std::string Dump(json_t* obj)
{
  char* s = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(s ? s : "{}");
  free(s);
  return result;
}

/** Send a JSON text frame. */
static void SendJson(WsCodec& ws, json_t* obj)
{
  ws.SendText(Dump(obj));
}

/** Send {"type":"error","message":"..."} */
static void SendError(WsCodec& ws, const std::string& step,
                      const std::string& msg)
{
  json_t* obj = json_pack("{s:s, s:s, s:s}",
                           "type", "error",
                           "step", step.c_str(),
                           "message", msg.c_str());
  SendJson(ws, obj);
}

/** Get a string field from a JSON object (or "" if missing). */
static std::string JStr(json_t* obj, const char* key)
{
  json_t* v = json_object_get(obj, key);
  return (v && json_is_string(v)) ? json_string_value(v) : "";
}

/** Get a boolean field from a JSON object (or false if missing). */
static bool JBool(json_t* obj, const char* key)
{
  json_t* v = json_object_get(obj, key);
  return v && json_is_true(v);
}

// ---------------------------------------------------------------------------
// Action handlers
// ---------------------------------------------------------------------------

static void HandleDetectOs(WsCodec& ws)
{
  try {
    OsInfo info = DetectOs();
    json_t* obj = json_pack(
        "{s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
        "type",        "os_info",
        "distro",      info.distro.c_str(),
        "version",     info.version.c_str(),
        "codename",    info.codename.c_str(),
        "pretty_name", info.pretty_name.c_str(),
        "arch",        info.arch.c_str(),
        "pkg_mgr",     info.pkg_mgr.c_str());
    SendJson(ws, obj);
  } catch (const std::exception& e) {
    SendError(ws, "detect_os", e.what());
  }
}

/** Build the apt/dnf/zypper commands to add the Bareos repo. */
static std::vector<std::string> BuildAddRepoCmd(const std::string& pkg_mgr,
                                                const std::string& distro,
                                                const std::string& version,
                                                const std::string& codename,
                                                const std::string& repo_type)
{
  const std::string base = (repo_type == "subscription")
      ? "https://download.bareos.com/bareos/release/latest"
      : "https://download.bareos.org/current";

  if (pkg_mgr == "apt") {
    // Use the Bareos script or direct approach
    std::string repo_url = base + "/" + distro + "_" + version + "/";
    std::string code = codename.empty() ? version : codename;
    // Write a .list file
    return {
        "bash", "-c",
        "curl -fsSL " + repo_url + "Release.key | gpg --dearmor "
        "-o /etc/apt/keyrings/bareos.gpg && "
        "echo 'deb [signed-by=/etc/apt/keyrings/bareos.gpg] "
        + repo_url + " " + code + " main' "
        "> /etc/apt/sources.list.d/bareos.list && "
        "apt-get update -qq"
    };
  } else if (pkg_mgr == "dnf" || pkg_mgr == "yum") {
    std::string repo_url = base + "/" + distro + "_" + version + "/";
    return {
        "bash", "-c",
        pkg_mgr + " config-manager --add-repo " + repo_url
        + "bareos.repo"
    };
  } else if (pkg_mgr == "zypper") {
    std::string repo_url = base + "/" + distro + "_" + version + "/";
    return {
        "bash", "-c",
        "zypper addrepo -f " + repo_url + " bareos && zypper refresh"
    };
  }
  return {"echo", "Unsupported package manager: " + pkg_mgr};
}

/** Build the install command for a list of packages. */
static std::vector<std::string> BuildInstallCmd(
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

static void HandleRunStep(WsCodec& ws, json_t* msg)
{
  std::string step_id = JStr(msg, "id");
  bool use_sudo       = JBool(msg, "sudo");

  std::vector<std::string> cmd;

  if (step_id == "add_repo") {
    cmd = BuildAddRepoCmd(
        JStr(msg, "pkg_mgr"),
        JStr(msg, "distro"),
        JStr(msg, "version"),
        JStr(msg, "codename"),
        JStr(msg, "repo_type"));

  } else if (step_id == "install_packages") {
    std::vector<std::string> pkgs;
    json_t* jarr = json_object_get(msg, "packages");
    if (jarr && json_is_array(jarr)) {
      size_t i;
      json_t* v;
      json_array_foreach(jarr, i, v) {
        if (json_is_string(v)) pkgs.push_back(json_string_value(v));
      }
    }
    cmd = BuildInstallCmd(JStr(msg, "pkg_mgr"), pkgs);

  } else if (step_id == "setup_db") {
    std::string db_name = JStr(msg, "db_name");
    std::string db_user = JStr(msg, "db_user");
    if (db_name.empty()) db_name = "bareos";
    if (db_user.empty()) db_user = "bareos";
    cmd = {
        "bash", "-c",
        "su - postgres -c \"createuser --no-superuser --no-createrole "
        "--no-createdb " + db_user + " 2>/dev/null || true\" && "
        "su - postgres -c \"createdb --owner=" + db_user + " " + db_name
        + " 2>/dev/null || true\" && "
        "/usr/lib/bareos/scripts/create_bareos_database && "
        "/usr/lib/bareos/scripts/make_bareos_tables && "
        "/usr/lib/bareos/scripts/grant_bareos_privileges"
    };

  } else {
    SendError(ws, step_id, "Unknown step id: " + step_id);
    return;
  }

  int exit_code = 0;
  try {
    exit_code = RunCommand(
        cmd, use_sudo,
        [&ws, &step_id](const std::string& line, const std::string& stream) {
          json_t* obj = json_pack("{s:s, s:s, s:s}",
                                  "type",   "output",
                                  "line",   line.c_str(),
                                  "stream", stream.c_str());
          ws.SendText(Dump(obj));
        });
  } catch (const std::exception& e) {
    SendError(ws, step_id, e.what());
    return;
  }

  json_t* done = json_pack("{s:s, s:s, s:i}",
                            "type",      "done",
                            "step",      step_id.c_str(),
                            "exit_code", exit_code);
  SendJson(ws, done);
}

static void HandleGenerateScript(WsCodec& ws)
{
  // Emit a minimal setup script template
  std::ostringstream script;
  script << "#!/bin/bash\n"
         << "# Bareos Setup Script\n"
         << "# Generated by bareos-setup\n"
         << "set -euo pipefail\n\n"
         << "# Add Bareos repository\n"
         << "# (edit repo URL as needed)\n"
         << "# apt-get install -y bareos-director bareos-storage "
            "bareos-filedaemon\n"
         << "# /usr/lib/bareos/scripts/create_bareos_database\n"
         << "# /usr/lib/bareos/scripts/make_bareos_tables\n"
         << "# /usr/lib/bareos/scripts/grant_bareos_privileges\n";

  json_t* obj = json_pack("{s:s, s:s}",
                           "type",    "script",
                           "content", script.str().c_str());
  SendJson(ws, obj);
}

// ---------------------------------------------------------------------------
// Session entry point
// ---------------------------------------------------------------------------

void RunSetupSession(int fd)
{
  WsCodec ws(fd);
  try {
    while (!ws.IsClosed()) {
      std::string text = ws.RecvMessage();
      if (text.empty()) break;

      json_error_t err{};
      json_t* msg = json_loads(text.c_str(), 0, &err);
      if (!msg) {
        std::cerr << "Invalid JSON from browser: " << err.text << "\n";
        continue;
      }

      std::string action = JStr(msg, "action");

      if (action == "detect_os") {
        HandleDetectOs(ws);
      } else if (action == "run_step") {
        HandleRunStep(ws, msg);
      } else if (action == "generate_script") {
        HandleGenerateScript(ws);
      } else {
        SendError(ws, action, "Unknown action: " + action);
      }

      json_decref(msg);
    }
  } catch (const std::exception& e) {
    std::cerr << "Setup session error: " << e.what() << "\n";
  }
}
