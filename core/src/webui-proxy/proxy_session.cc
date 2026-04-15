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
#include "proxy_session.h"
#include "director_connection.h"
#include "ws_codec.h"

#include <cstdio>
#include <stdexcept>
#include <string>

#include <unistd.h>

#include <jansson.h>

// ---------------------------------------------------------------------------
// Tiny JSON helpers (avoid pulling in a large JSON library interface)
// ---------------------------------------------------------------------------

// Build a JSON string using jansson and return it as std::string.
// Caller must ensure all values are valid.
static std::string JsonObject(
    std::initializer_list<std::pair<const char*, std::string>> fields)
{
  json_t* obj = json_object();
  for (auto& [key, val] : fields) {
    json_object_set_new(obj, key, json_string(val.c_str()));
  }
  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(raw);
  free(raw);
  return result;
}

// ---------------------------------------------------------------------------
// Session implementation
// ---------------------------------------------------------------------------

void RunProxySession(int fd,
                     const std::string& peer,
                     const DefaultDirectorConfig& defaults)
{
  // RAII socket close
  struct FdGuard {
    int fd;
    ~FdGuard() { ::close(fd); }
  } guard{fd};

  fprintf(stderr, "[proxy] connected: %s\n", peer.c_str());

  WsCodec ws(fd);
  try {
    ws.Handshake();
  } catch (const std::exception& ex) {
    fprintf(stderr, "[proxy] %s WS handshake failed: %s\n", peer.c_str(),
            ex.what());
    return;
  }

  // ── Step 1: receive auth message ─────────────────────────────────────────
  std::string raw_auth;
  try {
    raw_auth = ws.RecvMessage();
  } catch (const std::exception& ex) {
    fprintf(stderr, "[proxy] %s recv auth: %s\n", peer.c_str(), ex.what());
    return;
  }

  if (raw_auth.empty()) {
    fprintf(stderr, "[proxy] %s: WS closed before auth\n", peer.c_str());
    return;
  }

  json_error_t jerr{};
  json_t* auth_msg = json_loads(raw_auth.c_str(), 0, &jerr);
  if (!auth_msg) {
    ws.SendText(
        JsonObject({{"type", "error"}, {"message", "Expected JSON auth"}}));
    return;
  }

  const char* type = json_string_value(json_object_get(auth_msg, "type"));
  if (!type || std::string(type) != "auth") {
    ws.SendText(JsonObject(
        {{"type", "error"}, {"message", "First message must be type=auth"}}));
    json_decref(auth_msg);
    return;
  }

  auto jstr = [&](const char* key, const std::string& def) -> std::string {
    const char* v = json_string_value(json_object_get(auth_msg, key));
    return v ? v : def;
  };

  DirectorConfig cfg;
  cfg.username = jstr("username", "admin");
  cfg.password = jstr("password", "");
  cfg.director_name = jstr("director", defaults.name);
  cfg.host = defaults.host;
  cfg.port = defaults.port;
  std::string mode = jstr("mode", "json");
  cfg.json_mode = (mode != "raw");

  json_decref(auth_msg);

  fprintf(stderr, "[proxy] %s auth: user=%s director=%s host=%s:%d mode=%s\n",
          peer.c_str(), cfg.username.c_str(), cfg.director_name.c_str(),
          cfg.host.c_str(), cfg.port, mode.c_str());

  // ── Step 2: connect and authenticate to director ─────────────────────────
  DirectorConnection director;
  try {
    director.Connect(cfg);
  } catch (const std::exception& ex) {
    std::string msg = ex.what();
    fprintf(stderr, "[proxy] %s auth failed: %s\n", peer.c_str(), ex.what());
    try {
      if (msg.find("authentication failed") != std::string::npos
          || msg.find("Authorization failed") != std::string::npos) {
        ws.SendText(JsonObject(
            {{"type", "auth_error"},
             {"message", std::string("Authentication failed: ") + ex.what()}}));
      } else {
        ws.SendText(JsonObject(
            {{"type", "auth_error"},
             {"message", std::string("Connection error: ") + ex.what()}}));
      }
    } catch (...) {
      // Client disconnected before we could report the failure.
    }
    return;
  }

  // Build auth_ok response
  {
    json_t* ok = json_object();
    json_object_set_new(ok, "type", json_string("auth_ok"));
    json_object_set_new(ok, "director", json_string(cfg.director_name.c_str()));
    json_object_set_new(ok, "mode", json_string(mode.c_str()));
    char* ok_str = json_dumps(ok, JSON_COMPACT);
    json_decref(ok);
    try {
      ws.SendText(std::string(ok_str));
    } catch (...) {
      free(ok_str);
      return;  // Client disconnected right after authentication succeeded.
    }
    free(ok_str);
  }

  fprintf(stderr, "[proxy] %s authenticated as %s on %s (mode=%s)\n",
          peer.c_str(), cfg.username.c_str(), cfg.director_name.c_str(),
          mode.c_str());

  // ── Step 3: command loop ─────────────────────────────────────────────────
  while (!ws.IsClosed()) {
    std::string raw_msg;
    try {
      raw_msg = ws.RecvMessage();
    } catch (const std::exception& ex) {
      fprintf(stderr, "[proxy] %s recv: %s\n", peer.c_str(), ex.what());
      break;
    }

    if (raw_msg.empty()) { break; }  // clean WebSocket close

    json_error_t jerr2{};
    json_t* req = json_loads(raw_msg.c_str(), 0, &jerr2);
    if (!req) {
      ws.SendText(JsonObject({{"type", "error"}, {"message", "Invalid JSON"}}));
      continue;
    }

    const char* req_type = json_string_value(json_object_get(req, "type"));
    if (!req_type || std::string(req_type) != "command") {
      ws.SendText(JsonObject(
          {{"type", "error"}, {"message", "Expected type=command"}}));
      json_decref(req);
      continue;
    }

    const char* cmd_raw = json_string_value(json_object_get(req, "command"));
    const char* id_raw = json_string_value(json_object_get(req, "id"));
    std::string command = cmd_raw ? cmd_raw : "";
    std::string req_id = id_raw ? id_raw : "";

    // Trim leading/trailing whitespace
    auto trim = [](std::string s) {
      s.erase(0, s.find_first_not_of(" \t\r\n"));
      s.erase(s.find_last_not_of(" \t\r\n") + 1);
      return s;
    };
    command = trim(command);

    json_decref(req);

    if (command.empty()) { continue; }

    fprintf(stderr, "[proxy] %s command [id=%s]: %s\n", peer.c_str(),
            req_id.c_str(), command.c_str());

    try {
      CallResult result = director.Call(command);

      if (cfg.json_mode) {
        // Parse and re-emit as
        // {"type":"response","id":...,"command":...,"data":{...}} The director
        // returns a jsonrpc envelope: {"jsonrpc":"2.0","result":{...}}. Unwrap
        // it so callers receive {"key": value} directly, matching the behaviour
        // of the python-bareos director.call() method.
        json_error_t jerr3{};
        json_t* data = json_loads(result.text.c_str(), 0, &jerr3);
        if (!data) { data = json_string(result.text.c_str()); }

        // Unwrap jsonrpc envelope when present.
        if (json_is_object(data) && json_object_get(data, "jsonrpc")) {
          json_t* inner = json_object_get(data, "result");
          if (inner) {
            json_incref(inner);
            json_decref(data);
            data = inner;
          }
        }

        json_t* resp = json_object();
        json_object_set_new(resp, "type", json_string("response"));
        if (!req_id.empty()) {
          json_object_set_new(resp, "id", json_string(req_id.c_str()));
        }
        json_object_set_new(resp, "command", json_string(command.c_str()));
        json_object_set_new(resp, "data", data);

        char* resp_str = json_dumps(resp, JSON_COMPACT);
        json_decref(resp);
        ws.SendText(std::string(resp_str));
        free(resp_str);
      } else {
        // Raw mode: {"type":"raw_response","id":...,"command":...,"text":"...",
        //            "prompt":"main"|"sub"|"select"}
        const char* prompt_str = [&]() {
          switch (result.prompt) {
            case DirectorPrompt::Select:
              return "select";
            case DirectorPrompt::Sub:
              return "sub";
            default:
              return "main";
          }
        }();
        json_t* resp = json_object();
        json_object_set_new(resp, "type", json_string("raw_response"));
        if (!req_id.empty()) {
          json_object_set_new(resp, "id", json_string(req_id.c_str()));
        }
        json_object_set_new(resp, "command", json_string(command.c_str()));
        json_object_set_new(resp, "text", json_string(result.text.c_str()));
        json_object_set_new(resp, "prompt", json_string(prompt_str));
        char* resp_str = json_dumps(resp, JSON_COMPACT);
        json_decref(resp);
        ws.SendText(std::string(resp_str));
        free(resp_str);
      }
    } catch (const std::exception& ex) {
      json_t* err = json_object();
      json_object_set_new(err, "type", json_string("error"));
      if (!req_id.empty()) {
        json_object_set_new(err, "id", json_string(req_id.c_str()));
      }
      json_object_set_new(err, "command", json_string(command.c_str()));
      json_object_set_new(err, "message", json_string(ex.what()));
      char* err_str = json_dumps(err, JSON_COMPACT);
      json_decref(err);
      fprintf(stderr, "[proxy] %s director error: %s\n", peer.c_str(),
              ex.what());
      try {
        ws.SendText(std::string(err_str));
      } catch (...) {
        free(err_str);
        break;  // Client disconnected — end session.
      }
      free(err_str);
    }
  }

  fprintf(stderr, "[proxy] session ended: %s\n", peer.c_str());
}
