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
#include "proxy_log.h"
#include "ws_codec.h"

#include <cstdio>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <unistd.h>

#include <jansson.h>

// ---------------------------------------------------------------------------
// Tiny JSON helpers (avoid pulling in a large JSON library interface)
// ---------------------------------------------------------------------------

// Build a JSON string using jansson and return it as std::string.
// Caller must ensure all values are valid.
static std::string JsonObject(
    std::initializer_list<std::pair<const char*, std::string_view>> fields)
{
  json_t* obj = json_object();
  for (auto& [key, val] : fields) {
    json_object_set_new(
        obj, key, json_stringn(val.data(), static_cast<size_t>(val.size())));
  }
  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(raw);
  free(raw);
  return result;
}

static std::string JsonDirectorList(const ProxyConfig& config)
{
  json_t* obj = json_object();
  json_t* directors = json_array();

  json_object_set_new(obj, "type", json_string("director_list"));
  for (const auto& id : GetAllowedDirectorIds(config)) {
    json_array_append_new(directors, json_string(id.c_str()));
  }
  json_object_set_new(obj, "directors", directors);

  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(raw);
  free(raw);
  return result;
}

std::string NormalizeRawConsoleCommand(std::string_view command)
{
  const auto first = command.find_first_not_of(" \r\n");
  if (first == std::string_view::npos) { return {}; }
  command.remove_prefix(first);

  while (!command.empty()) {
    const char last = command.back();
    if (last != ' ' && last != '\r' && last != '\n') {
      return std::string(command);
    }
    command.remove_suffix(1);
  }

  return {};
}

bool IsExpectedConsoleExitCommand(bool at_main_prompt, std::string_view command)
{
  if (!at_main_prompt) { return false; }

  return command == "quit" || command == "exit" || command == ".quit"
         || command == ".exit";
}

static bool IsMessagesConsoleCommand(std::string_view command)
{
  constexpr std::string_view kMessagesCommand = "messages";

  const auto separator = command.find_first_of(" \t");
  const auto token = command.substr(0, separator);
  if (token.size() < 4 || token.size() > kMessagesCommand.size()) {
    return false;
  }

  return std::equal(token.begin(), token.end(), kMessagesCommand.begin(),
                    [](char lhs, char rhs) {
                      return std::tolower(static_cast<unsigned char>(lhs))
                             == std::tolower(static_cast<unsigned char>(rhs));
                    });
}

bool ShouldSuppressRawConsoleChunk(std::string_view command,
                                   std::string_view chunk)
{
  return FilterRawConsoleChunk(command, chunk).empty();
}

std::string FilterRawConsoleChunk(std::string_view command,
                                  std::string_view chunk)
{
  if (!IsMessagesConsoleCommand(command)) { return std::string(chunk); }

  constexpr std::string_view kSuppressedLine = "You have messages.";
  std::string filtered;
  filtered.reserve(chunk.size());

  size_t pos = 0;
  while (pos < chunk.size()) {
    const auto line_end = chunk.find('\n', pos);
    const auto line_stop
        = line_end == std::string_view::npos ? chunk.size() : line_end;
    auto line = chunk.substr(pos, line_stop - pos);
    if (!line.empty() && line.back() == '\r') { line.remove_suffix(1); }

    if (line != kSuppressedLine) {
      filtered.append(line);
      if (line_end != std::string_view::npos) { filtered.push_back('\n'); }
    }

    if (line_end == std::string_view::npos) { break; }
    pos = line_end + 1;
  }

  return filtered;
}

// ---------------------------------------------------------------------------
// Session implementation
// ---------------------------------------------------------------------------

void RunProxySession(int fd, const std::string& peer, const ProxyConfig& config)
{
  // RAII socket close
  struct FdGuard {
    int fd;
    ~FdGuard() { ::close(fd); }
  } guard{fd};

  PROXY_LOG_INFO(peer, "connected");

  auto ws = [&]() -> std::optional<WsCodec> {
    try {
      return WsCodec::Accept(fd);
    } catch (const std::exception& ex) {
      PROXY_LOG_WARN(peer, "WS handshake failed: %s", ex.what());
      return std::nullopt;
    }
  }();
  if (!ws) { return; }

  // ── Step 1: receive auth message ─────────────────────────────────────────
  std::string raw_auth;
  try {
    raw_auth = ws->RecvMessage();
  } catch (const std::exception& ex) {
    PROXY_LOG_WARN(peer, "recv auth: %s", ex.what());
    return;
  }

  if (raw_auth.empty()) {
    PROXY_LOG_INFO(peer, "WS closed before auth");
    return;
  }

  json_error_t jerr{};
  json_t* auth_msg = json_loads(raw_auth.c_str(), 0, &jerr);
  if (!auth_msg) {
    ws->SendText(
        JsonObject({{"type", "error"}, {"message", "Expected JSON auth"}}));
    return;
  }

  auto jstr = [&](const char* key) -> std::optional<std::string_view> {
    const char* v = json_string_value(json_object_get(auth_msg, key));
    return v ? std::optional<std::string_view>(v) : std::nullopt;
  };
  auto joptional_int = [&](const char* key) -> std::optional<int> {
    json_t* v = json_object_get(auth_msg, key);
    if (!v) { return std::nullopt; }
    return json_is_integer(v)
               ? std::optional<int>(static_cast<int>(json_integer_value(v)))
               : std::nullopt;
  };

  const auto type = jstr("type");
  if (type && *type == "list_directors") {
    ws->SendText(JsonDirectorList(config));
    json_decref(auth_msg);
    return;
  }
  if (!type || *type != "auth") {
    ws->SendText(JsonObject(
        {{"type", "error"},
         {"message",
          "First message must be type=auth or type=list_directors"}}));
    json_decref(auth_msg);
    return;
  }

  DirectorConfig cfg;
  cfg.username = jstr("username").value_or("admin");
  cfg.password = jstr("password").value_or("");
  const std::string mode(jstr("mode").value_or("json"));
  cfg.json_mode = (mode != "raw");

  try {
    const auto target = ResolveDirectorTarget(
        config,
        [&]() -> std::optional<std::string> {
          const auto director = jstr("director");
          return director ? std::optional<std::string>(*director)
                          : std::nullopt;
        }(),
        [&]() -> std::optional<std::string> {
          const auto host = jstr("host");
          return host ? std::optional<std::string>(*host) : std::nullopt;
        }(),
        joptional_int("port"));
    cfg.director_name = target.name;
    cfg.host = target.host;
    cfg.port = target.port;
    cfg.tls_psk_disable = target.tls_psk_disable;
    cfg.tls_psk_require = target.tls_psk_require;
  } catch (const std::exception& ex) {
    ws->SendText(JsonObject({{"type", "auth_error"}, {"message", ex.what()}}));
    json_decref(auth_msg);
    return;
  }

  json_decref(auth_msg);

  PROXY_LOG_INFO(peer, "auth: user=%s director=%s host=%s:%d mode=%s",
                 cfg.username.c_str(), cfg.director_name.c_str(),
                 cfg.host.c_str(), cfg.port, mode.c_str());

  // ── Step 2: connect and authenticate to director ─────────────────────────
  DirectorConnection director;
  try {
    director.Connect(cfg);
  } catch (const std::exception& ex) {
    const std::string_view msg = ex.what();
    PROXY_LOG_WARN(peer, "auth failed: %s", ex.what());
    try {
      if (msg.find("authentication failed") != std::string::npos
          || msg.find("Authorization failed") != std::string::npos) {
        ws->SendText(JsonObject(
            {{"type", "auth_error"},
             {"message", std::string("Authentication failed: ") + ex.what()}}));
      } else {
        ws->SendText(JsonObject(
            {{"type", "auth_error"},
             {"message", std::string("Connection error: ") + ex.what()}}));
      }
    } catch (...) {
      // Client disconnected before we could report the failure.
    }
    return;
  }

  const char* director_transport
      = director.UsesTlsPsk() ? "TLS-PSK" : "cleartext";

  PROXY_LOG_INFO(peer, "director transport: %s", director_transport);

  // Build auth_ok response
  {
    json_t* ok = json_object();
    json_object_set_new(ok, "type", json_string("auth_ok"));
    json_object_set_new(ok, "director", json_string(cfg.director_name.c_str()));
    json_object_set_new(ok, "mode", json_string(mode.c_str()));
    json_object_set_new(ok, "transport", json_string(director_transport));
    char* ok_str = json_dumps(ok, JSON_COMPACT);
    json_decref(ok);
    try {
      ws->SendText(ok_str);
    } catch (...) {
      free(ok_str);
      return;  // Client disconnected right after authentication succeeded.
    }
    free(ok_str);
  }

  PROXY_LOG_INFO(peer, "authenticated as %s on %s (mode=%s, transport=%s)",
                 cfg.username.c_str(), cfg.director_name.c_str(), mode.c_str(),
                 director_transport);

  // ── Step 3: command loop ─────────────────────────────────────────────────
  auto current_prompt = DirectorPrompt::Main;
  while (!ws->IsClosed()) {
    std::string raw_msg;
    try {
      raw_msg = ws->RecvMessage();
    } catch (const std::exception& ex) {
      PROXY_LOG_WARN(peer, "recv: %s", ex.what());
      break;
    }

    if (raw_msg.empty()) { break; }  // clean WebSocket close

    json_error_t jerr2{};
    json_t* req = json_loads(raw_msg.c_str(), 0, &jerr2);
    if (!req) {
      ws->SendText(
          JsonObject({{"type", "error"}, {"message", "Invalid JSON"}}));
      continue;
    }

    const char* req_type = json_string_value(json_object_get(req, "type"));
    if (req_type && std::string_view(req_type) == "ping") {
      json_decref(req);
      ws->SendText(JsonObject({{"type", "pong"}}));
      continue;
    }
    if (!req_type || std::string_view(req_type) != "command") {
      ws->SendText(JsonObject(
          {{"type", "error"}, {"message", "Expected type=command"}}));
      json_decref(req);
      continue;
    }

    const char* cmd_raw = json_string_value(json_object_get(req, "command"));
    const char* id_raw = json_string_value(json_object_get(req, "id"));
    const bool stream_raw = json_is_true(json_object_get(req, "stream"));
    const std::string_view command_raw = cmd_raw ? cmd_raw : "";
    std::string req_id = id_raw ? id_raw : "";

    std::string command = NormalizeRawConsoleCommand(command_raw);

    json_decref(req);

    if (command.empty()) { continue; }

    PROXY_LOG_INFO(peer, "command [id=%s]: %s", req_id.c_str(),
                   command.c_str());

    auto prompt_to_string = [](DirectorPrompt prompt) {
      switch (prompt) {
        case DirectorPrompt::Select:
          return "select";
        case DirectorPrompt::Sub:
          return "sub";
        case DirectorPrompt::Other:
          return "other";
        default:
          return "main";
      }
    };
    auto send_raw_response
        = [&](const std::string& text, const char* prompt_str) {
            json_t* resp = json_object();
            json_object_set_new(resp, "type", json_string("raw_response"));
            if (!req_id.empty()) {
              json_object_set_new(resp, "id", json_string(req_id.c_str()));
            }
            json_object_set_new(resp, "command", json_string(command.c_str()));
            json_object_set_new(resp, "text", json_string(text.c_str()));
            if (prompt_str) {
              json_object_set_new(resp, "prompt", json_string(prompt_str));
            }
            char* resp_str = json_dumps(resp, JSON_COMPACT);
            json_decref(resp);
            ws->SendText(resp_str);
            free(resp_str);
          };
    auto is_terminal_prompt = [](DirectorPrompt prompt) {
      return prompt == DirectorPrompt::Main || prompt == DirectorPrompt::Other;
    };
    auto send_command_state
        = [&](const char* status, const char* prompt_str = nullptr,
              const char* message = nullptr) {
            json_t* resp = json_object();
            json_object_set_new(resp, "type", json_string("command_state"));
            if (!req_id.empty()) {
              json_object_set_new(resp, "id", json_string(req_id.c_str()));
            }
            json_object_set_new(resp, "command", json_string(command.c_str()));
            json_object_set_new(resp, "status", json_string(status));
            if (prompt_str) {
              json_object_set_new(resp, "prompt", json_string(prompt_str));
            }
            if (message) {
              json_object_set_new(resp, "message", json_string(message));
            }
            char* resp_str = json_dumps(resp, JSON_COMPACT);
            json_decref(resp);
            ws->SendText(resp_str);
            free(resp_str);
          };

    try {
      if (!cfg.json_mode) { send_command_state("running"); }

      const bool should_close_console_session
          = !cfg.json_mode
            && IsExpectedConsoleExitCommand(
                current_prompt == DirectorPrompt::Main, command);
      CallResult result = director.Call(
          command, (!cfg.json_mode && stream_raw)
                       ? std::function<void(std::string_view)>(
                             [&](std::string_view chunk) {
                               auto filtered
                                   = FilterRawConsoleChunk(command, chunk);
                               if (filtered.empty()) { return; }
                               send_raw_response(filtered, "more");
                             })
                       : std::function<void(std::string_view)>());

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
        ws->SendText(resp_str);
        free(resp_str);
      } else {
        // Raw mode: {"type":"raw_response","id":...,"command":...,"text":"...",
        //            "prompt":"main"|"sub"|"select"|"other"|"more"}
        current_prompt = result.prompt;
        const char* prompt_str = prompt_to_string(result.prompt);
        send_command_state(is_terminal_prompt(result.prompt)
                               ? "completed"
                               : "waiting_for_input",
                           prompt_str);
        if (stream_raw) {
          send_raw_response("", prompt_str);
        } else {
          result.text = FilterRawConsoleChunk(command, result.text);
          send_raw_response(result.text, prompt_str);
        }
        if (should_close_console_session) { break; }
      }
    } catch (const std::exception& ex) {
      if (IsExpectedConsoleExitCommand(
              !cfg.json_mode && current_prompt == DirectorPrompt::Main,
              command)) {
        break;
      }
      if (!cfg.json_mode) {
        try {
          send_command_state("failed", nullptr, ex.what());
        } catch (...) {
          break;
        }
      }
      json_t* err = json_object();
      json_object_set_new(err, "type", json_string("error"));
      if (!req_id.empty()) {
        json_object_set_new(err, "id", json_string(req_id.c_str()));
      }
      json_object_set_new(err, "command", json_string(command.c_str()));
      json_object_set_new(err, "message", json_string(ex.what()));
      char* err_str = json_dumps(err, JSON_COMPACT);
      json_decref(err);
      PROXY_LOG_ERROR(peer, "director error: %s", ex.what());
      try {
        ws->SendText(err_str);
      } catch (...) {
        free(err_str);
        break;  // Client disconnected — end session.
      }
      free(err_str);
    }
  }

  PROXY_LOG_INFO(peer, "session ended");
}
