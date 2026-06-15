/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
#include "http_protocol.h"
#include "proxy_auth_session.h"
#include "proxy_log.h"
#include "ws_codec.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include <unistd.h>

#include <jansson.h>

// ---------------------------------------------------------------------------
// Tiny JSON helpers (avoid pulling in a large JSON library interface)
// ---------------------------------------------------------------------------

namespace {

struct JsonDeleter {
  void operator()(json_t* value) const
  {
    if (value) { json_decref(value); }
  }
};

using JsonPtr = std::unique_ptr<json_t, JsonDeleter>;

struct FreeDeleter {
  void operator()(void* ptr) const { free(ptr); }
};

using JsonDumpPtr = std::unique_ptr<char, FreeDeleter>;

JsonPtr MakeJsonObject()
{
  JsonPtr obj(json_object());
  if (!obj) { throw std::runtime_error("Failed to allocate JSON object"); }
  return obj;
}

JsonPtr MakeJsonArray()
{
  JsonPtr array(json_array());
  if (!array) { throw std::runtime_error("Failed to allocate JSON array"); }
  return array;
}

JsonPtr MakeJsonString(std::string_view value)
{
  JsonPtr string(json_stringn(value.data(), static_cast<size_t>(value.size())));
  if (!string) { throw std::runtime_error("Failed to allocate JSON string"); }
  return string;
}

void SetJsonValue(json_t* object, const char* key, JsonPtr value)
{
  if (json_object_set_new(object, key, value.release()) != 0) {
    throw std::runtime_error("Failed to build JSON object");
  }
}

void SetJsonString(json_t* object, const char* key, std::string_view value)
{
  SetJsonValue(object, key, MakeJsonString(value));
}

void AppendJsonString(json_t* array, std::string_view value)
{
  JsonPtr string = MakeJsonString(value);
  if (json_array_append_new(array, string.release()) != 0) {
    throw std::runtime_error("Failed to build JSON array");
  }
}

std::string DumpJson(json_t* value)
{
  JsonDumpPtr raw(json_dumps(value, JSON_COMPACT));
  if (!raw) { throw std::runtime_error("Failed to serialize JSON"); }
  return raw.get();
}

std::optional<std::string_view> JsonStringField(const json_t* object,
                                                const char* key)
{
  const char* value = json_string_value(json_object_get(object, key));
  return value ? std::optional<std::string_view>(value) : std::nullopt;
}

std::string_view RequireJsonStringField(const json_t* object, const char* key)
{
  const auto value = JsonStringField(object, key);
  if (!value) {
    throw std::invalid_argument("Auth message requires string field '"
                                + std::string(key) + "'");
  }
  return *value;
}

bool ParseAuthMode(std::string_view mode)
{
  if (mode == "json") { return true; }
  if (mode == "raw") { return false; }
  throw std::invalid_argument("Auth message has unsupported mode '"
                              + std::string(mode) + "'");
}

// Build a JSON string using jansson and return it as std::string.
// Caller must ensure all values are valid.
std::string JsonObject(
    std::initializer_list<std::pair<const char*, std::string_view>> fields)
{
  JsonPtr obj = MakeJsonObject();
  for (auto& [key, val] : fields) { SetJsonString(obj.get(), key, val); }
  return DumpJson(obj.get());
}

std::string JsonDirectorList(const ProxyConfig& config)
{
  JsonPtr obj = MakeJsonObject();
  JsonPtr directors = MakeJsonArray();

  SetJsonString(obj.get(), "type", "director_list");
  for (const auto& [selector, _] : config.configured_directors) {
    AppendJsonString(directors.get(), selector);
  }
  SetJsonValue(obj.get(), "directors", std::move(directors));
  return DumpJson(obj.get());
}

JsonPtr MakeSessionInfoJson(const ProxyAuthSessionRecord& session)
{
  JsonPtr obj = MakeJsonObject();
  JsonPtr directors = MakeJsonArray();

  SetJsonString(obj.get(), "director", session.current_director);
  SetJsonString(obj.get(), "currentDirector", session.current_director);

  const auto current = session.directors.find(session.current_director);
  if (current != session.directors.end()) {
    SetJsonString(obj.get(), "username", current->second.username);
  }

  for (const auto& [selector, credentials] : session.directors) {
    JsonPtr entry = MakeJsonObject();
    SetJsonString(entry.get(), "director", selector);
    SetJsonString(entry.get(), "username", credentials.username);
    if (selector == session.current_director) {
      json_object_set_new(entry.get(), "current", json_true());
    }
    json_array_append_new(directors.get(), entry.release());
  }

  SetJsonValue(obj.get(), "authenticatedDirectors", std::move(directors));
  return obj;
}

std::string JsonSessionInfo(const ProxyAuthSessionRecord& session)
{
  return DumpJson(MakeSessionInfoJson(session).get());
}

std::string JsonAuthOk(std::string_view director,
                       std::string_view mode,
                       std::string_view transport)
{
  return JsonObject({{"type", "auth_ok"},
                     {"director", director},
                     {"mode", mode},
                     {"transport", transport}});
}

std::string JsonRawResponse(std::optional<std::string_view> id,
                            std::string_view command,
                            std::string_view text,
                            std::optional<std::string_view> prompt)
{
  JsonPtr response = MakeJsonObject();
  SetJsonString(response.get(), "type", "raw_response");
  if (id) { SetJsonString(response.get(), "id", *id); }
  SetJsonString(response.get(), "command", command);
  SetJsonString(response.get(), "text", text);
  if (prompt) { SetJsonString(response.get(), "prompt", *prompt); }
  return DumpJson(response.get());
}

std::string JsonCommandState(std::optional<std::string_view> id,
                             std::string_view command,
                             std::string_view status,
                             std::optional<std::string_view> prompt,
                             std::optional<std::string_view> message)
{
  JsonPtr response = MakeJsonObject();
  SetJsonString(response.get(), "type", "command_state");
  if (id) { SetJsonString(response.get(), "id", *id); }
  SetJsonString(response.get(), "command", command);
  SetJsonString(response.get(), "status", status);
  if (prompt) { SetJsonString(response.get(), "prompt", *prompt); }
  if (message) { SetJsonString(response.get(), "message", *message); }
  return DumpJson(response.get());
}

std::string JsonCommandResponse(std::optional<std::string_view> id,
                                std::string_view command,
                                JsonPtr data)
{
  JsonPtr response = MakeJsonObject();
  SetJsonString(response.get(), "type", "response");
  if (id) { SetJsonString(response.get(), "id", *id); }
  SetJsonString(response.get(), "command", command);
  SetJsonValue(response.get(), "data", std::move(data));
  return DumpJson(response.get());
}

std::string JsonCommandError(std::optional<std::string_view> id,
                             std::string_view command,
                             std::string_view message)
{
  JsonPtr error = MakeJsonObject();
  SetJsonString(error.get(), "type", "error");
  if (id) { SetJsonString(error.get(), "id", *id); }
  SetJsonString(error.get(), "command", command);
  SetJsonString(error.get(), "message", message);
  return DumpJson(error.get());
}

DirectorTargetConfig RequireDirectorTarget(const ProxyConfig& config,
                                          std::string_view selector)
{
  if (selector.empty()) {
    throw std::runtime_error("Proxy config: no director selected");
  }

  const auto director_it = config.configured_directors.find(std::string(selector));
  if (director_it == config.configured_directors.end()) {
    throw std::runtime_error("Proxy config: director '" + std::string(selector)
                             + "' is not configured");
  }

  return director_it->second;
}

DirectorConfig BuildDirectorConfig(const ProxyConfig& config,
                                   std::string_view selector,
                                   std::string_view username,
                                   std::string_view password,
                                   bool json_mode)
{
  const auto target = RequireDirectorTarget(config, selector);

  DirectorConfig cfg;
  cfg.username = std::string(username);
  cfg.password = std::string(password);
  cfg.json_mode = json_mode;
  cfg.director_name = target.name;
  cfg.host = target.address;
  cfg.port = target.port;
  cfg.tls_psk_disable = target.tls_psk_disable;
  return cfg;
}

bool RequestUsesHttps(const HttpRequest& request)
{
  const auto forwarded_proto = request.HeaderValue("X-Forwarded-Proto");
  return forwarded_proto && *forwarded_proto == "https";
}

bool IsAuthenticationFailure(std::string_view message)
{
  std::string normalized(message);
  std::transform(
      normalized.begin(), normalized.end(), normalized.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  return normalized.find("authentication failed") != std::string::npos
         || normalized.find("authorization failed") != std::string::npos
         || normalized.find("tls-psk handshake failed") != std::string::npos;
}

std::optional<ProxyAuthSessionRecord> LookupSessionFromRequest(
    const HttpRequest& request)
{
  const auto cookie_header = request.HeaderValue("Cookie");
  if (!cookie_header) { return std::nullopt; }

  const auto session_id = FindCookieValue(*cookie_header, kProxySessionCookieName);
  if (!session_id || session_id->empty()) { return std::nullopt; }

  return ProxyAuthSessionStore::LookupSession(*session_id);
}

std::optional<ProxyAuthSessionDirectorRecord> LookupDirectorCredentials(
    const ProxyAuthSessionRecord& session,
    std::string_view director)
{
  const auto it = session.directors.find(std::string(director));
  if (it == session.directors.end()) { return std::nullopt; }
  return it->second;
}

std::optional<std::string_view> LookupSessionIdFromRequest(const HttpRequest& request)
{
  const auto cookie_header = request.HeaderValue("Cookie");
  if (!cookie_header) { return std::nullopt; }
  return FindCookieValue(*cookie_header, kProxySessionCookieName);
}

std::optional<std::string> MatchSessionDirectorPath(std::string_view target,
                                                    std::string_view suffix = {})
{
  constexpr std::string_view kPrefix = "/api/session/directors/";
  if (!target.starts_with(kPrefix)) {
    return std::nullopt;
  }

  auto remainder = target.substr(kPrefix.size());
  if (!suffix.empty()) {
    if (!remainder.ends_with(suffix)) {
      return std::nullopt;
    }
    remainder.remove_suffix(suffix.size());
  }

  if (remainder.empty() || remainder.find('/') != std::string_view::npos) {
    return std::nullopt;
  }

  return std::string(remainder);
}
}  // namespace

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

namespace {

bool IsMessagesConsoleCommand(std::string_view command)
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

}  // namespace

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

namespace {

void SendJsonResponseWithCookie(int fd,
                                std::string_view status_line,
                                std::string_view body,
                                std::optional<std::string_view> cookie = std::nullopt)
{
  if (cookie) {
    SendHttpResponse(fd, std::chrono::seconds(5), status_line, body,
                     {{"Content-Type", "application/json"},
                      {"Cache-Control", "no-store"},
                      {"Set-Cookie", *cookie}});
  } else {
    SendHttpResponse(fd, std::chrono::seconds(5), status_line, body,
                     {{"Content-Type", "application/json"},
                      {"Cache-Control", "no-store"}});
  }
}

void SendEmptyResponseWithCookie(int fd,
                                 std::string_view status_line,
                                 std::optional<std::string_view> cookie
                                 = std::nullopt)
{
  if (cookie) {
    SendHttpResponse(fd, std::chrono::seconds(5), status_line, "",
                     {{"Cache-Control", "no-store"}, {"Set-Cookie", *cookie}});
  } else {
    SendHttpResponse(fd, std::chrono::seconds(5), status_line, "",
                     {{"Cache-Control", "no-store"}});
  }
}

JsonPtr ParseJsonBody(const HttpRequest& request)
{
  json_error_t error{};
  JsonPtr body(json_loadb(request.body.data(), request.body.size(), 0, &error));
  if (!body) { throw std::runtime_error("Expected JSON request body"); }
  return body;
}

DirectorConfig ConnectSessionDirector(const ProxyConfig& config,
                                      std::string_view selector,
                                      std::string_view username,
                                      std::string_view password)
{
  DirectorConfig cfg
      = BuildDirectorConfig(config, selector, username, password, true);
  DirectorConnection connection;
  connection.Connect(cfg);
  return cfg;
}

void SendSessionMissingResponse(int fd, const HttpRequest& request)
{
  SendJsonResponseWithCookie(
      fd, "HTTP/1.1 401 Unauthorized",
      JsonObject({{"message", "No active session"}}),
      BuildExpiredProxySessionCookie(RequestUsesHttps(request)));
}

void HandleSessionLoginRequest(int fd,
                               const std::string& peer,
                               const ProxyConfig& config,
                               const HttpRequest& request,
                               std::optional<std::string_view> target_director
                               = std::nullopt)
{
  std::string requested_director;
  try {
    JsonPtr body = ParseJsonBody(request);
    const auto username = RequireJsonStringField(body.get(), "username");
    const auto password = RequireJsonStringField(body.get(), "password");
    const auto director
        = target_director.value_or(RequireJsonStringField(body.get(), "director"));
    requested_director = std::string(director);
    const DirectorConfig cfg
        = ConnectSessionDirector(config, requested_director, username, password);

    std::optional<std::string> session_id;
    if (const auto existing_session_id = LookupSessionIdFromRequest(request)) {
      session_id = std::string(*existing_session_id);
    }
    bool new_session = false;
    if (!session_id
        || !ProxyAuthSessionStore::LookupSession(*session_id)) {
      session_id = ProxyAuthSessionStore::CreateSession(
          username, password, requested_director);
      new_session = true;
    } else {
      ProxyAuthSessionStore::StoreDirectorCredentials(
          *session_id, requested_director, username, password);
    }

    ProxyAuthSessionStore::SetCurrentDirector(*session_id,
                                                         requested_director);
    const auto session = ProxyAuthSessionStore::LookupSession(*session_id);
    if (!session) {
      throw std::runtime_error("Proxy session expired; please log in again");
    }
    const auto cookie = new_session
        ? std::optional<std::string>(
              BuildProxySessionCookie(*session_id, RequestUsesHttps(request)))
        : std::nullopt;

    PROXY_LOG_INFO(peer, "stored session login for user=%s director=%s",
                   cfg.username.c_str(), requested_director.c_str());
    SendJsonResponseWithCookie(fd, "HTTP/1.1 200 OK",
                               JsonSessionInfo(*session),
                               cookie ? std::optional<std::string_view>(*cookie)
                                      : std::nullopt);
  } catch (const std::exception& ex) {
    const std::string_view message = ex.what();
    const bool authentication_failed = IsAuthenticationFailure(message);
    const std::string_view status = authentication_failed
                                        ? std::string_view(
                                              "HTTP/1.1 401 Unauthorized")
                                        : std::string_view(
                                              "HTTP/1.1 502 Bad Gateway");
    const std::string response_message
        = authentication_failed
        ? "Authentication failed"
        : requested_director.empty()
            ? "Connection error: " + std::string(message)
            : "Connection to director '" + requested_director + "' failed: "
                  + std::string(message);
    PROXY_LOG_WARN(peer, "session login failed: %s", ex.what());
    SendJsonResponseWithCookie(
        fd, status, JsonObject({{"message", response_message}}));
  }
}

void HandleSessionInfoRequest(int fd, const HttpRequest& request)
{
  const auto session = LookupSessionFromRequest(request);
  if (!session) {
    SendSessionMissingResponse(fd, request);
    return;
  }

  SendJsonResponseWithCookie(fd, "HTTP/1.1 200 OK", JsonSessionInfo(*session));
}

void HandleSessionLogoutRequest(int fd, const HttpRequest& request)
{
  if (const auto session_id = LookupSessionIdFromRequest(request)) {
    ProxyAuthSessionStore::RemoveSession(*session_id);
  }

  const auto expired_cookie
      = BuildExpiredProxySessionCookie(RequestUsesHttps(request));
  SendEmptyResponseWithCookie(fd, "HTTP/1.1 204 No Content", expired_cookie);
}

void HandleCurrentDirectorRequest(int fd, const HttpRequest& request)
{
  const auto session_id = LookupSessionIdFromRequest(request);
  if (!session_id) {
    SendSessionMissingResponse(fd, request);
    return;
  }

  try {
    JsonPtr body = ParseJsonBody(request);
    const auto director = RequireJsonStringField(body.get(), "director");
    if (!ProxyAuthSessionStore::SetCurrentDirector(*session_id,
                                                              director)) {
      SendJsonResponseWithCookie(
          fd, "HTTP/1.1 400 Bad Request",
          JsonObject({{"message", "Director is not authenticated"}}));
      return;
    }

    const auto session = ProxyAuthSessionStore::LookupSession(*session_id);
    if (!session) {
      SendSessionMissingResponse(fd, request);
      return;
    }

    SendJsonResponseWithCookie(fd, "HTTP/1.1 200 OK", JsonSessionInfo(*session));
  } catch (const std::exception&) {
    SendJsonResponseWithCookie(fd, "HTTP/1.1 400 Bad Request",
                               JsonObject({{"message", "Expected JSON request body"}}));
  }
}

void HandleDirectorLogoutRequest(int fd,
                                 const HttpRequest& request,
                                 std::string_view director)
{
  const auto session_id = LookupSessionIdFromRequest(request);
  if (!session_id) {
    SendSessionMissingResponse(fd, request);
    return;
  }

  const auto removed
      = ProxyAuthSessionStore::RemoveDirector(*session_id, director);
  if (!removed) {
    SendJsonResponseWithCookie(fd, "HTTP/1.1 404 Not Found",
                               JsonObject({{"message", "Director is not authenticated"}}));
    return;
  }

  const auto session = ProxyAuthSessionStore::LookupSession(*session_id);
  if (!session) {
    SendEmptyResponseWithCookie(
        fd, "HTTP/1.1 204 No Content",
        BuildExpiredProxySessionCookie(RequestUsesHttps(request)));
    return;
  }

  SendJsonResponseWithCookie(fd, "HTTP/1.1 200 OK", JsonSessionInfo(*session));
}

void HandleReuseCredentialsRequest(int fd,
                                   const HttpRequest& request,
                                   const ProxyConfig& config)
{
  const auto session_id = LookupSessionIdFromRequest(request);
  const auto session = LookupSessionFromRequest(request);
  if (!session_id || !session) {
    SendSessionMissingResponse(fd, request);
    return;
  }

  try {
    JsonPtr body = ParseJsonBody(request);
    const auto source_director = JsonStringField(body.get(), "sourceDirector")
        .value_or(session->current_director);
    const auto source_credentials = LookupDirectorCredentials(*session,
                                                              source_director);
    if (!source_credentials) {
      SendJsonResponseWithCookie(
          fd, "HTTP/1.1 400 Bad Request",
          JsonObject({{"message", "Source director is not authenticated"}}));
      return;
    }

    json_t* directors = json_object_get(body.get(), "directors");
    if (!json_is_array(directors)) {
      throw std::runtime_error("Expected JSON request body");
    }

    JsonPtr results = MakeJsonArray();
    size_t index = 0;
    json_t* entry = nullptr;
    json_array_foreach(directors, index, entry)
    {
      const char* director_value = json_string_value(entry);
      if (!director_value || *director_value == '\0') {
        continue;
      }

      const std::string selector(director_value);
      JsonPtr result = MakeJsonObject();
      SetJsonString(result.get(), "director", selector);

      if (session->directors.find(selector) != session->directors.end()) {
        SetJsonString(result.get(), "status", "already_authenticated");
        json_array_append_new(results.get(), result.release());
        continue;
      }

      try {
        ConnectSessionDirector(config, selector, source_credentials->username,
                               source_credentials->password);
        ProxyAuthSessionStore::StoreDirectorCredentials(
            *session_id, selector, source_credentials->username,
            source_credentials->password, false);
        SetJsonString(result.get(), "status", "authenticated");
      } catch (const std::exception& ex) {
        SetJsonString(result.get(), "status",
                      IsAuthenticationFailure(ex.what())
                          ? "authentication_failed"
                          : "connection_error");
      }

      json_array_append_new(results.get(), result.release());
    }

    JsonPtr response = MakeJsonObject();
    SetJsonValue(response.get(), "results", std::move(results));
    const auto updated_session
        = ProxyAuthSessionStore::LookupSession(*session_id);
    if (updated_session) {
      json_object_set_new(response.get(), "session",
                          MakeSessionInfoJson(*updated_session).release());
    }

    SendJsonResponseWithCookie(fd, "HTTP/1.1 200 OK", DumpJson(response.get()));
  } catch (const std::exception&) {
    SendJsonResponseWithCookie(fd, "HTTP/1.1 400 Bad Request",
                               JsonObject({{"message", "Expected JSON request body"}}));
  }
}

std::optional<std::string> FindSessionId(std::optional<std::string_view> cookie_header)
{
  if (!cookie_header) { return std::nullopt; }
  const auto session_id = FindCookieValue(*cookie_header, kProxySessionCookieName);
  if (!session_id || session_id->empty()) { return std::nullopt; }
  return std::string(*session_id);
}

}  // namespace

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

  HttpRequest request;
  try {
    request = ReadHttpRequest(fd);
  } catch (const std::exception& ex) {
    PROXY_LOG_WARN(peer, "read request failed: %s", ex.what());
    return;
  }

  if (request.method == "POST" && request.target == "/api/session/login") {
    HandleSessionLoginRequest(fd, peer, config, request);
    return;
  }
  if (request.method == "POST"
      && request.target == "/api/session/current-director") {
    HandleCurrentDirectorRequest(fd, request);
    return;
  }
  if (request.method == "POST" && request.target == "/api/session/reuse") {
    HandleReuseCredentialsRequest(fd, request, config);
    return;
  }
  if (request.method == "GET" && request.target == "/api/session") {
    HandleSessionInfoRequest(fd, request);
    return;
  }
  if (request.method == "POST" && request.target == "/api/session/logout") {
    HandleSessionLogoutRequest(fd, request);
    return;
  }
  if (request.method == "POST") {
    if (const auto director
        = MatchSessionDirectorPath(request.target, "/login")) {
      HandleSessionLoginRequest(fd, peer, config, request, *director);
      return;
    }
  }
  if (request.method == "DELETE") {
    if (const auto director = MatchSessionDirectorPath(request.target)) {
      HandleDirectorLogoutRequest(fd, request, *director);
      return;
    }
  }

  if (!IsWebSocketUpgradeRequest(request)) {
    SendHttpResponse(fd, std::chrono::seconds(5), "HTTP/1.1 404 Not Found",
                     "Not Found",
                     {{"Content-Type", "text/plain; charset=utf-8"}});
    return;
  }

  auto ws = [&]() -> std::optional<WsCodec> {
    try {
      return WsCodec::Accept(fd, request.raw_headers, std::move(request.pending_input));
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
  JsonPtr auth_msg(json_loads(raw_auth.c_str(), 0, &jerr));
  if (!auth_msg) {
    ws->SendText(
        JsonObject({{"type", "error"}, {"message", "Expected JSON auth"}}));
    return;
  }

  const auto type = JsonStringField(auth_msg.get(), "type");
  if (type && *type == "list_directors") {
    ws->SendText(JsonDirectorList(config));
    return;
  }
  if (!type || *type != "auth") {
    ws->SendText(JsonObject(
        {{"type", "error"},
         {"message",
          "First message must be type=auth or type=list_directors"}}));
    return;
  }

  DirectorConfig cfg;
  const std::string mode(
      JsonStringField(auth_msg.get(), "mode").value_or("json"));
  std::optional<std::string> session_id;
  try {
    const auto username
        = std::string(RequireJsonStringField(auth_msg.get(), "username"));
    const auto password
        = std::string(RequireJsonStringField(auth_msg.get(), "password"));
    const auto requested_selector
        = std::string(RequireJsonStringField(auth_msg.get(), "director"));
    const bool json_mode = ParseAuthMode(mode);

    if (password == kProxySessionPasswordPlaceholder) {
      session_id = FindSessionId(ws->RequestHeader("Cookie"));
      if (!session_id) {
        throw std::runtime_error("Proxy session missing; please log in again");
      }

      const auto session
          = ProxyAuthSessionStore::LookupSession(*session_id);
      if (!session) {
        throw std::runtime_error("Proxy session expired; please log in again");
      }
      const auto credentials
          = LookupDirectorCredentials(*session, requested_selector);
      if (!credentials) {
        throw std::runtime_error("Proxy session has no credentials for director '"
                                 + requested_selector + "'");
      }

      cfg = BuildDirectorConfig(config, requested_selector,
                                credentials->username, credentials->password,
                                json_mode);
    } else {
      cfg = BuildDirectorConfig(config, requested_selector, username, password,
                                json_mode);
    }

    PROXY_LOG_INFO(peer, "auth: user=%s director=%s host=%s:%d mode=%s",
                   cfg.username.c_str(), cfg.director_name.c_str(),
                   cfg.host.c_str(), cfg.port, mode.c_str());

  } catch (const std::exception& ex) {
    ws->SendText(JsonObject({{"type", "auth_error"}, {"message", ex.what()}}));
    return;
  }

  // ── Step 2: connect and authenticate to director ─────────────────────────
  try {
    DirectorConnection director = DirectorConnection::Connect(cfg);

    const char* director_transport
        = director.UsesTlsPsk() ? "TLS-PSK" : "cleartext";

    PROXY_LOG_INFO(peer, "director transport: %s", director_transport);
    if (session_id) {
      ProxyAuthSessionStore::SetCurrentDirector(
          *session_id, RequireJsonStringField(auth_msg.get(), "director"));
    }

    try {
      ws->SendText(JsonAuthOk(cfg.director_name, mode, director_transport));
    } catch (...) {
      return;  // Client disconnected right after authentication succeeded.
    }

    PROXY_LOG_INFO(peer, "authenticated as %s on %s (mode=%s, transport=%s)",
                   cfg.username.c_str(), cfg.director_name.c_str(),
                   mode.c_str(), director_transport);

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
      JsonPtr req(json_loads(raw_msg.c_str(), 0, &jerr2));
      if (!req) {
        ws->SendText(
            JsonObject({{"type", "error"}, {"message", "Invalid JSON"}}));
        continue;
      }

      const auto req_type = JsonStringField(req.get(), "type");
      if (req_type && *req_type == "ping") {
        ws->SendText(JsonObject({{"type", "pong"}}));
        continue;
      }
      if (!req_type || *req_type != "command") {
        ws->SendText(JsonObject(
            {{"type", "error"}, {"message", "Expected type=command"}}));
        continue;
      }

      const auto req_id = JsonStringField(req.get(), "id");
      const bool stream_raw = json_is_true(json_object_get(req.get(), "stream"));
      const std::string_view command_raw
          = JsonStringField(req.get(), "command").value_or("");
      std::string command = NormalizeRawConsoleCommand(command_raw);

      if (command.empty()) { continue; }

      PROXY_LOG_INFO(peer, "command [id=%.*s]: %s",
                     static_cast<int>(req_id.value_or("").size()),
                     req_id.value_or("").data(), command.c_str());

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
          = [&](std::string_view text, const char* prompt_str) {
              ws->SendText(JsonRawResponse(
                  req_id, command, text,
                  prompt_str ? std::optional<std::string_view>(prompt_str)
                             : std::nullopt));
            };
      auto is_terminal_prompt = [](DirectorPrompt prompt) {
        return prompt == DirectorPrompt::Main || prompt == DirectorPrompt::Other;
      };
      auto send_command_state = [&](const char* status,
                                    const char* prompt_str = nullptr,
                                    const char* message = nullptr) {
        ws->SendText(JsonCommandState(
            req_id, command, status,
            prompt_str ? std::optional<std::string_view>(prompt_str)
                       : std::nullopt,
            message ? std::optional<std::string_view>(message) : std::nullopt));
      };

      try {
        if (!cfg.json_mode) { send_command_state("running"); }

        const bool should_close_console_session
            = !cfg.json_mode
              && IsExpectedConsoleExitCommand(
                  current_prompt == DirectorPrompt::Main, command);
        CallResult result;
        if (!cfg.json_mode && stream_raw) {
          result.prompt
              = director.CallStreamed(command, [&](std::string_view chunk) {
                  auto filtered = FilterRawConsoleChunk(command, chunk);
                  if (filtered.empty()) { return; }
                  send_raw_response(filtered, "more");
                });
        } else {
          result = director.Call(command);
        }

        if (cfg.json_mode) {
          // Parse and re-emit as
          // {"type":"response","id":...,"command":...,"data":{...}} The director
          // returns a jsonrpc envelope: {"jsonrpc":"2.0","result":{...}}. Unwrap
          // it so callers receive {"key": value} directly, matching the behaviour
          // of the python-bareos director.call() method.
          json_error_t jerr3{};
          JsonPtr data(json_loads(result.text.c_str(), 0, &jerr3));
          if (!data) { data = MakeJsonString(result.text); }

          // Unwrap jsonrpc envelope when present.
          if (json_is_object(data.get())
              && json_object_get(data.get(), "jsonrpc")) {
            json_t* inner = json_object_get(data.get(), "result");
            if (inner) {
              json_incref(inner);
              data.reset(inner);
            }
          }

          ws->SendText(JsonCommandResponse(req_id, command, std::move(data)));
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
        PROXY_LOG_ERROR(peer, "director error: %s", ex.what());
        try {
          ws->SendText(JsonCommandError(req_id, command, ex.what()));
        } catch (...) {
          break;  // Client disconnected — end session.
        }
      }
    }
  } catch (const std::exception& ex) {
    const std::string_view msg = ex.what();
    PROXY_LOG_WARN(peer, "auth failed: %s", ex.what());
    try {
      if (IsAuthenticationFailure(msg)) {
        ws->SendText(JsonObject(
            {{"type", "auth_error"}, {"message", "Authentication failed"}}));
      } else {
        ws->SendText(JsonObject(
            {{"type", "auth_error"}, {"message", "Connection error"}}));
      }
    } catch (...) {
      // Client disconnected before we could report the failure.
    }
    return;
  }

  PROXY_LOG_INFO(peer, "session ended");
}
