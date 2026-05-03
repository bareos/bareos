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
#include "http_api.h"

#include "proxy_session.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include <jansson.h>

namespace {

std::string Trim(std::string value)
{
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(),
              std::find_if_not(value.begin(), value.end(), is_space));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(),
              value.end());
  return value;
}

std::string ToLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  return value;
}

std::string JsonObject(
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

std::optional<std::string> GetHeader(const HttpRequest& request,
                                     std::string_view key)
{
  const auto it = request.headers.find(ToLower(std::string(key)));
  if (it == request.headers.end()) { return std::nullopt; }
  return it->second;
}

std::optional<std::string> GetBearerToken(const HttpRequest& request)
{
  const auto authorization = GetHeader(request, "authorization");
  if (!authorization) { return std::nullopt; }
  static constexpr std::string_view prefix = "bearer ";
  const std::string lower = ToLower(*authorization);
  if (lower.rfind(prefix, 0) != 0) { return std::nullopt; }
  return authorization->substr(prefix.size());
}

std::string JsonIdentity(const AuthIdentity& identity)
{
  json_t* obj = json_object();
  json_object_set_new(obj, "provider", json_string(identity.provider.c_str()));
  json_object_set_new(obj, "subject", json_string(identity.subject.c_str()));
  json_object_set_new(obj, "username", json_string(identity.username.c_str()));
  json_object_set_new(obj, "email", json_string(identity.email.c_str()));

  json_t* groups = json_array();
  for (const auto& group : identity.groups) {
    json_array_append_new(groups, json_string(group.c_str()));
  }
  json_object_set_new(obj, "groups", groups);

  json_t* roles = json_array();
  for (const auto& role : identity.roles) {
    json_array_append_new(roles, json_string(role.c_str()));
  }
  json_object_set_new(obj, "roles", roles);

  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(raw);
  free(raw);
  return result;
}

HttpResponse JsonResponse(int status_code,
                          std::string reason,
                          std::string body)
{
  HttpResponse response;
  response.status_code = status_code;
  response.reason = std::move(reason);
  response.body = std::move(body);
  response.headers.emplace("content-type", "application/json");
  return response;
}

std::string ProvidersJson(const ProxyConfig& config)
{
  json_t* obj = json_object();
  json_t* providers = json_array();

  if (!config.local_auth_users.empty()) {
    json_t* local = json_object();
    json_object_set_new(local, "id", json_string("local"));
    json_object_set_new(local, "kind", json_string("password"));
    json_array_append_new(providers, local);
  }
  if (!config.token_auth_entries.empty()) {
    json_t* token = json_object();
    json_object_set_new(token, "id", json_string("token"));
    json_object_set_new(token, "kind", json_string("bearer"));
    json_array_append_new(providers, token);
  }

  json_object_set_new(obj, "providers", providers);
  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(raw);
  free(raw);
  return result;
}

HttpResponse HandleProvidersRequest(const ProxyConfig& config)
{
  return JsonResponse(200, "OK", ProvidersJson(config));
}

HttpResponse HandleLoginRequest(const ProxyConfig& config,
                                const std::shared_ptr<ProxySessionStore>& store,
                                const HttpRequest& request)
{
  if (!store) {
    throw std::runtime_error("Proxy auth: session support is not available");
  }

  json_error_t jerr{};
  json_t* payload = json_loads(request.body.c_str(), 0, &jerr);
  if (!payload || !json_is_object(payload)) {
    if (payload) { json_decref(payload); }
    return JsonResponse(
        400, "Bad Request",
        JsonObject({{"type", "error"}, {"message", "Expected JSON object"}}));
  }

  auto joptional = [&](const char* key) -> std::optional<std::string> {
    const char* v = json_string_value(json_object_get(payload, key));
    return v ? std::optional<std::string>(v) : std::nullopt;
  };
  auto jrequired = [&](const char* key) -> std::optional<std::string> {
    const char* v = json_string_value(json_object_get(payload, key));
    return (v && *v) ? std::optional<std::string>(v) : std::nullopt;
  };

  try {
    const auto auth = ResolveProxyAuthRequest(
        config, store, jrequired("session_token"), jrequired("access_token"),
        jrequired("username"), jrequired("password"), joptional("director"),
        std::nullopt, std::nullopt, true);

    ProxySession session;
    if (auth.reused_existing_session) {
      const auto refreshed
          = store->RefreshSession(auth.session_token, auth.preferred_director_id);
      if (!refreshed) {
        throw std::runtime_error("Proxy auth: unknown or expired session");
      }
      session = *refreshed;
    } else {
      AuthResult result;
      result.identity = auth.identity;
      result.expires_at = auth.expires_at;
      session = store->CreateSession(result, auth.director_config.username,
                                     auth.director_config.password,
                                     auth.preferred_director_id);
    }

    json_t* obj = json_object();
    json_object_set_new(obj, "session_token",
                        json_string(session.token.c_str()));
    json_object_set_new(obj, "director",
                        json_string(session.preferred_director_id.c_str()));
    json_object_set_new(
        obj, "director_username",
        json_string(session.director_username.c_str()));
    json_t* identity = json_loads(JsonIdentity(session.identity).c_str(), 0, nullptr);
    json_object_set_new(obj, "identity", identity);
    json_object_set_new(obj, "expires_at", json_integer(session.expires_at));

    char* raw = json_dumps(obj, JSON_COMPACT);
    json_decref(obj);
    HttpResponse response = JsonResponse(200, "OK", raw);
    free(raw);
    json_decref(payload);
    return response;
  } catch (const std::exception& ex) {
    json_decref(payload);
    return JsonResponse(
        401, "Unauthorized",
        JsonObject({{"type", "auth_error"}, {"message", ex.what()}}));
  }
}

HttpResponse HandleSessionRequest(
    const std::shared_ptr<ProxySessionStore>& store, const HttpRequest& request)
{
  const auto token = GetBearerToken(request);
  if (!token) {
    return JsonResponse(
        401, "Unauthorized",
        JsonObject({{"type", "auth_error"},
                    {"message", "Missing Bearer session token"}}));
  }

  const auto session = store ? store->GetSession(*token) : std::nullopt;
  if (!session) {
    return JsonResponse(
        401, "Unauthorized",
        JsonObject({{"type", "auth_error"},
                    {"message", "Unknown or expired session"}}));
  }

  json_t* obj = json_object();
  json_object_set_new(obj, "session_token", json_string(session->token.c_str()));
  json_object_set_new(
      obj, "director", json_string(session->preferred_director_id.c_str()));
  json_object_set_new(
      obj, "director_username", json_string(session->director_username.c_str()));
  json_t* identity = json_loads(JsonIdentity(session->identity).c_str(), 0, nullptr);
  json_object_set_new(obj, "identity", identity);
  json_object_set_new(obj, "created_at", json_integer(session->created_at));
  json_object_set_new(obj, "expires_at", json_integer(session->expires_at));

  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  HttpResponse response = JsonResponse(200, "OK", raw);
  free(raw);
  return response;
}

HttpResponse HandleLogoutRequest(const std::shared_ptr<ProxySessionStore>& store,
                                 const HttpRequest& request)
{
  const auto token = GetBearerToken(request);
  if (!token) {
    return JsonResponse(
        401, "Unauthorized",
        JsonObject({{"type", "auth_error"},
                    {"message", "Missing Bearer session token"}}));
  }

  const bool removed = store && store->RemoveSession(*token);
  if (!removed) {
    return JsonResponse(
        401, "Unauthorized",
        JsonObject({{"type", "auth_error"},
                    {"message", "Unknown or expired session"}}));
  }

  HttpResponse response;
  response.status_code = 204;
  response.reason = "No Content";
  return response;
}

}  // namespace

HttpRequest ParseHttpRequest(const std::string& request_head, std::string body)
{
  std::istringstream input(request_head);
  std::string line;
  HttpRequest request;

  if (!std::getline(input, line)) {
    throw std::runtime_error("HTTP: missing request line");
  }
  if (!line.empty() && line.back() == '\r') { line.pop_back(); }
  std::istringstream request_line(line);
  if (!(request_line >> request.method >> request.target)) {
    throw std::runtime_error("HTTP: invalid request line");
  }

  const auto query_pos = request.target.find('?');
  request.path = request.target.substr(0, query_pos);
  request.body = std::move(body);

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') { line.pop_back(); }
    if (line.empty()) { break; }
    const auto colon = line.find(':');
    if (colon == std::string::npos) {
      throw std::runtime_error("HTTP: invalid header");
    }
    request.headers.emplace(ToLower(Trim(line.substr(0, colon))),
                            Trim(line.substr(colon + 1)));
  }

  return request;
}

bool IsWebSocketUpgradeRequest(const HttpRequest& request)
{
  const auto upgrade = GetHeader(request, "upgrade");
  const auto connection = GetHeader(request, "connection");
  if (!upgrade || !connection) { return false; }
  return ToLower(*upgrade) == "websocket"
         && ToLower(*connection).find("upgrade") != std::string::npos;
}

HttpResponse HandleHttpApiRequest(
    const ProxyConfig& config,
    const std::shared_ptr<ProxySessionStore>& session_store,
    const HttpRequest& request)
{
  if (request.method == "GET" && request.path == "/api/v1/auth/providers") {
    return HandleProvidersRequest(config);
  }
  if (request.method == "POST" && request.path == "/api/v1/auth/login") {
    return HandleLoginRequest(config, session_store, request);
  }
  if (request.method == "GET" && request.path == "/api/v1/auth/session") {
    return HandleSessionRequest(session_store, request);
  }
  if (request.method == "POST" && request.path == "/api/v1/auth/logout") {
    return HandleLogoutRequest(session_store, request);
  }

  return JsonResponse(
      404, "Not Found",
      JsonObject({{"type", "error"}, {"message", "Unknown HTTP endpoint"}}));
}

std::string BuildHttpResponse(const HttpResponse& response)
{
  std::ostringstream output;
  output << "HTTP/1.1 " << response.status_code << ' ' << response.reason
         << "\r\n";
  for (const auto& [key, value] : response.headers) {
    output << key << ": " << value << "\r\n";
  }
  output << "content-length: " << response.body.size() << "\r\n";
  output << "connection: close\r\n\r\n";
  output << response.body;
  return output.str();
}
