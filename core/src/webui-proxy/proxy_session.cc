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
#include "identity_mapping.h"
#include "local_auth.h"
#include "token_auth.h"
#include "ws_codec.h"

#include <optional>
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

static std::string JsonAuditMetadata(const ProxyAuditMetadata& audit)
{
  json_t* obj = json_object();
  json_object_set_new(obj, "provider", json_string(audit.provider.c_str()));
  json_object_set_new(obj, "subject", json_string(audit.subject.c_str()));
  json_object_set_new(obj, "username", json_string(audit.username.c_str()));
  json_object_set_new(obj, "email", json_string(audit.email.c_str()));
  json_object_set_new(obj, "mapped_director_username",
                      json_string(audit.mapped_director_username.c_str()));
  json_object_set_new(obj, "proxy_session_token",
                      json_string(audit.proxy_session_token.c_str()));

  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(raw);
  free(raw);
  return result;
}

std::string NormalizeRawConsoleCommand(std::string command)
{
  command.erase(0, command.find_first_not_of(" \r\n"));

  while (!command.empty()) {
    const char last = command.back();
    if (last != ' ' && last != '\r' && last != '\n') { break; }
    command.pop_back();
  }

  return command;
}

ProxyAuthContext ResolveMappedIdentity(
    const ProxyConfig& config,
    const AuthResult& auth_result,
    const std::optional<std::string>& requested_director,
    const std::optional<std::string>& requested_host,
    const std::optional<int>& requested_port,
    bool json_mode)
{
  ProxyAuthContext auth;
  auth.identity = auth_result.identity;
  auth.expires_at = auth_result.expires_at;
  auth.director_config.json_mode = json_mode;

  if (!config.identity_mappings.empty()) {
    const auto mapped
        = ResolveIdentityMapping(config.identity_mappings, auth.identity);
    if (!mapped) {
      throw std::runtime_error(
          "Proxy auth: no identity mapping matched this user");
    }
    auth.director_config.username = mapped->director_username;
    auth.director_config.password = mapped->director_password;
    if (mapped->preferred_director_id && !mapped->preferred_director_id->empty()
        && !requested_director) {
      auth.preferred_director_id = *mapped->preferred_director_id;
    }
  }

  if (auth.director_config.username.empty()
      || auth.director_config.password.empty()) {
    throw std::runtime_error(
        "Proxy auth: no Director credentials resolved for this identity");
  }

  auth.audit_metadata = auth_result.audit_metadata.value_or(
      BuildProxyAuditMetadata(auth.identity, auth.director_config.username));

  const auto target = ResolveDirectorTarget(
      config,
      requested_director
          ? requested_director
          : (!auth.preferred_director_id.empty()
                 ? std::optional<std::string>(auth.preferred_director_id)
                 : std::nullopt),
      requested_host, requested_port);
  auth.preferred_director_id = target.id;
  auth.director_config.director_name = target.name;
  auth.director_config.host = target.host;
  auth.director_config.port = target.port;
  auth.director_config.tls_psk_disable = target.tls_psk_disable;
  auth.director_config.tls_psk_require = target.tls_psk_require;
  auth.director_config.audit_metadata = auth.audit_metadata;

  return auth;
}

ProxyAuthContext ResolveProxyAuthRequest(
    const ProxyConfig& config,
    const std::shared_ptr<ProxySessionStore>& session_store,
    const std::optional<std::string>& requested_session_token,
    const std::optional<std::string>& requested_access_token,
    const std::optional<std::string>& requested_username,
    const std::optional<std::string>& requested_password,
    const std::optional<std::string>& requested_director,
    const std::optional<std::string>& requested_host,
    const std::optional<int>& requested_port,
    bool json_mode)
{
  ProxyAuthContext auth;
  auth.director_config.json_mode = json_mode;

  std::optional<ProxySession> session;
  if (requested_session_token) {
    if (!session_store) {
      throw std::runtime_error("Proxy auth: session support is not available");
    }
    session = session_store->GetSession(*requested_session_token);
    if (!session) {
      throw std::runtime_error("Proxy auth: unknown or expired session");
    }

    auth.identity = session->identity;
    auth.session_token = session->token;
    auth.preferred_director_id = session->preferred_director_id;
    auth.reused_existing_session = true;
    auth.audit_metadata = session->audit_metadata;
    auth.director_config.username = session->director_username;
    auth.director_config.password = session->director_password;
    auth.director_config.audit_metadata = session->audit_metadata;
  } else if (requested_access_token) {
    const auto token_identity
        = AuthenticateToken(config.token_auth_entries, *requested_access_token);
    if (!token_identity) {
      throw std::runtime_error("Proxy auth: invalid or expired access token");
    }
    return ResolveMappedIdentity(config, *token_identity, requested_director,
                                 requested_host, requested_port, json_mode);
  } else {
    if (!requested_username || !requested_password) {
      throw std::runtime_error(
          "Proxy auth: provide session_token, access_token, or "
          "username/password");
    }

    if (!config.local_auth_users.empty()) {
      const auto local_user = AuthenticateLocalUser(
          config.local_auth_users, *requested_username, *requested_password);
      if (!local_user) {
        throw std::runtime_error(
            "Proxy auth: invalid local username or "
            "password");
      }
      return ResolveMappedIdentity(config, *local_user, requested_director,
                                   requested_host, requested_port, json_mode);
    } else {
      auth.director_config.username = *requested_username;
      auth.director_config.password = *requested_password;
      auth.identity.provider = "password";
      auth.identity.subject = auth.director_config.username;
      auth.identity.username = auth.director_config.username;
      auth.audit_metadata = BuildProxyAuditMetadata(
          auth.identity, auth.director_config.username);
      auth.director_config.audit_metadata = auth.audit_metadata;
    }
  }

  const auto target = ResolveDirectorTarget(
      config,
      requested_director
          ? requested_director
          : (!auth.preferred_director_id.empty()
                 ? std::optional<std::string>(auth.preferred_director_id)
                 : std::nullopt),
      requested_host, requested_port);
  auth.preferred_director_id = target.id;
  auth.director_config.director_name = target.name;
  auth.director_config.host = target.host;
  auth.director_config.port = target.port;
  auth.director_config.tls_psk_disable = target.tls_psk_disable;
  auth.director_config.tls_psk_require = target.tls_psk_require;
  if (!auth.audit_metadata) {
    auth.audit_metadata
        = BuildProxyAuditMetadata(auth.identity, auth.director_config.username);
  }
  auth.director_config.audit_metadata = auth.audit_metadata;

  return auth;
}

// ---------------------------------------------------------------------------
// Session implementation
// ---------------------------------------------------------------------------

void RunProxySession(int fd,
                     const std::string& peer,
                     const ProxyConfig& config,
                     const std::shared_ptr<ProxySessionStore>& session_store,
                     const std::optional<std::string>& initial_request)
{
  // RAII socket close
  struct FdGuard {
    int fd;
    ~FdGuard() { ::close(fd); }
  } guard{fd};

  fprintf(stderr, "[proxy] connected: %s\n", peer.c_str());

  WsCodec ws(fd);
  try {
    ws.Handshake(initial_request);
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
  if (type && std::string(type) == "list_directors") {
    ws.SendText(JsonDirectorList(config));
    json_decref(auth_msg);
    return;
  }
  if (!type || std::string(type) != "auth") {
    ws.SendText(JsonObject(
        {{"type", "error"},
         {"message",
          "First message must be type=auth or type=list_directors"}}));
    json_decref(auth_msg);
    return;
  }

  auto jstr = [&](const char* key, const std::string& def) -> std::string {
    const char* v = json_string_value(json_object_get(auth_msg, key));
    return v ? v : def;
  };
  auto joptional = [&](const char* key) -> std::optional<std::string> {
    const char* v = json_string_value(json_object_get(auth_msg, key));
    return v ? std::optional<std::string>(v) : std::nullopt;
  };
  auto joptional_int = [&](const char* key) -> std::optional<int> {
    json_t* v = json_object_get(auth_msg, key);
    if (!v) { return std::nullopt; }
    return json_is_integer(v)
               ? std::optional<int>(static_cast<int>(json_integer_value(v)))
               : std::nullopt;
  };
  auto jstr_required = [&](const char* key) -> std::optional<std::string> {
    const char* v = json_string_value(json_object_get(auth_msg, key));
    if (!v || !*v) { return std::nullopt; }
    return std::string(v);
  };

  DirectorConfig cfg;
  const auto requested_session_token = jstr_required("session_token");
  const auto requested_access_token = jstr_required("access_token");
  const auto requested_username = jstr_required("username");
  const auto requested_password = jstr_required("password");
  const auto requested_director = joptional("director");
  std::string mode = jstr("mode", "json");
  const bool json_mode = (mode != "raw");
  ProxyAuthContext auth;

  try {
    auth = ResolveProxyAuthRequest(
        config, session_store, requested_session_token, requested_access_token,
        requested_username, requested_password, requested_director,
        joptional("host"), joptional_int("port"), json_mode);
    cfg = auth.director_config;
  } catch (const std::exception& ex) {
    ws.SendText(JsonObject({{"type", "auth_error"}, {"message", ex.what()}}));
    json_decref(auth_msg);
    return;
  }

  json_decref(auth_msg);

  fprintf(stderr,
          "[proxy] %s auth: user=%s director=%s target=%s host=%s:%d mode=%s\n",
          peer.c_str(), cfg.username.c_str(),
          auth.preferred_director_id.c_str(), cfg.director_name.c_str(),
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

  const char* director_transport
      = director.UsesTlsPsk() ? "TLS-PSK" : "cleartext";

  if (auth.reused_existing_session) {
    const auto refreshed = session_store->RefreshSession(
        auth.session_token, auth.preferred_director_id);
    if (refreshed) {
      auth.session_token = refreshed->token;
      auth.audit_metadata = refreshed->audit_metadata;
    }
  } else {
    AuthResult auth_result;
    auth_result.identity = auth.identity;
    auth_result.expires_at = auth.expires_at;
    auth_result.audit_metadata = auth.audit_metadata;
    const auto created = session_store->CreateSession(
        auth_result, cfg.username, cfg.password, auth.preferred_director_id);
    auth.session_token = created.token;
    auth.audit_metadata = created.audit_metadata;
  }

  fprintf(stderr, "[proxy] %s director transport: %s\n", peer.c_str(),
          director_transport);

  // Build auth_ok response
  {
    json_t* ok = json_object();
    json_object_set_new(ok, "type", json_string("auth_ok"));
    json_object_set_new(ok, "director", json_string(cfg.director_name.c_str()));
    json_object_set_new(ok, "mode", json_string(mode.c_str()));
    json_object_set_new(ok, "transport", json_string(director_transport));
    json_object_set_new(ok, "username", json_string(cfg.username.c_str()));
    json_object_set_new(ok, "session_token",
                        json_string(auth.session_token.c_str()));
    if (auth.audit_metadata) {
      json_t* audit = json_loads(
          JsonAuditMetadata(*auth.audit_metadata).c_str(), 0, nullptr);
      json_object_set_new(ok, "audit", audit);
    }
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

  fprintf(stderr,
          "[proxy] %s authenticated as %s on %s (mode=%s, transport=%s)\n",
          peer.c_str(), cfg.username.c_str(), cfg.director_name.c_str(),
          mode.c_str(), director_transport);

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
    if (req_type && std::string(req_type) == "ping") {
      json_decref(req);
      ws.SendText(JsonObject({{"type", "pong"}}));
      continue;
    }
    if (!req_type || std::string(req_type) != "command") {
      ws.SendText(JsonObject(
          {{"type", "error"}, {"message", "Expected type=command"}}));
      json_decref(req);
      continue;
    }

    const char* cmd_raw = json_string_value(json_object_get(req, "command"));
    const char* id_raw = json_string_value(json_object_get(req, "id"));
    const bool stream_raw = json_is_true(json_object_get(req, "stream"));
    std::string command = cmd_raw ? cmd_raw : "";
    std::string req_id = id_raw ? id_raw : "";

    command = NormalizeRawConsoleCommand(command);

    json_decref(req);

    if (command.empty()) { continue; }

    fprintf(stderr, "[proxy] %s command [id=%s]: %s\n", peer.c_str(),
            req_id.c_str(), command.c_str());

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
            ws.SendText(std::string(resp_str));
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
            ws.SendText(std::string(resp_str));
            free(resp_str);
          };

    try {
      if (!cfg.json_mode) { send_command_state("running"); }

      CallResult result = director.Call(
          command, (!cfg.json_mode && stream_raw)
                       ? std::function<void(std::string_view)>(
                             [&](std::string_view chunk) {
                               send_raw_response(std::string(chunk), "more");
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
        ws.SendText(std::string(resp_str));
        free(resp_str);
      } else {
        // Raw mode: {"type":"raw_response","id":...,"command":...,"text":"...",
        //            "prompt":"main"|"sub"|"select"|"other"|"more"}
        const char* prompt_str = prompt_to_string(result.prompt);
        send_command_state(is_terminal_prompt(result.prompt)
                               ? "completed"
                               : "waiting_for_input",
                           prompt_str);
        if (stream_raw) {
          send_raw_response("", prompt_str);
        } else {
          send_raw_response(result.text, prompt_str);
        }
      }
    } catch (const std::exception& ex) {
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
