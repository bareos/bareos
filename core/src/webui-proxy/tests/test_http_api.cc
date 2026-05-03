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

#include "../http_api.h"
#include "../local_auth.h"
#include "../token_auth.h"

#include <gtest/gtest.h>

#include <jansson.h>

namespace {

HttpRequest MakeRequest(std::string method,
                        std::string path,
                        std::string body = "",
                        std::map<std::string, std::string> headers = {})
{
  HttpRequest request;
  request.method = std::move(method);
  request.target = path;
  request.path = path;
  request.body = std::move(body);
  request.headers = std::move(headers);
  return request;
}

ProxyConfig DemoConfig()
{
  ProxyConfig cfg;
  cfg.allowed_directors.emplace(
      "bareos-dir",
      DirectorTargetConfig{.id = "bareos-dir",
                           .host = "127.0.0.1",
                           .port = 9101,
                           .name = "bareos-dir",
                           .tls_psk_disable = true,
                           .tls_psk_require = false});

  LocalAuthUser user;
  user.id = "demo-admin";
  user.username = "demo-admin";
  user.password_hash = CreateLocalAuthPasswordHash(
      "secret", "00112233445566778899aabbccddeeff", 600000);
  user.groups = {"backup-admins"};
  cfg.local_auth_users.push_back(std::move(user));

  TokenAuthEntry token;
  token.id = "ci-bot";
  token.token_hash = CreateTokenAuthHash("demo-token");
  token.subject = "ci-bot";
  token.roles = {"operator"};
  cfg.token_auth_entries.push_back(std::move(token));

  IdentityMappingRule local_rule;
  local_rule.id = "local-admin";
  local_rule.provider = "local";
  local_rule.username = "demo-admin";
  local_rule.director_username = "admin-notls";
  local_rule.director_password = "secret";
  local_rule.preferred_director_id = "bareos-dir";
  cfg.identity_mappings.push_back(std::move(local_rule));

  IdentityMappingRule token_rule;
  token_rule.id = "token-bot";
  token_rule.provider = "token";
  token_rule.subject = "ci-bot";
  token_rule.director_username = "limited-operator";
  token_rule.director_password = "secret";
  token_rule.preferred_director_id = "bareos-dir";
  cfg.identity_mappings.push_back(std::move(token_rule));

  return cfg;
}

json_t* ParseJson(const std::string& body)
{
  json_error_t err{};
  json_t* obj = json_loads(body.c_str(), 0, &err);
  EXPECT_NE(obj, nullptr) << err.text;
  return obj;
}

}  // namespace

TEST(HttpApi, ListsConfiguredProviders)
{
  const auto response
      = HandleHttpApiRequest(DemoConfig(), nullptr,
                             MakeRequest("GET", "/api/v1/auth/providers"));

  EXPECT_EQ(response.status_code, 200);
  json_t* body = ParseJson(response.body);
  json_t* providers = json_object_get(body, "providers");
  ASSERT_TRUE(json_is_array(providers));
  EXPECT_EQ(json_array_size(providers), 2U);
  json_decref(body);
}

TEST(HttpApi, LogsInLocalUserAndReturnsSession)
{
  const auto store = std::make_shared<ProxySessionStore>(std::chrono::hours(8));
  const auto response = HandleHttpApiRequest(
      DemoConfig(), store,
      MakeRequest("POST", "/api/v1/auth/login",
                  R"json({"username":"demo-admin","password":"secret","director":"bareos-dir"})json",
                  {{"content-type", "application/json"}}));

  EXPECT_EQ(response.status_code, 200);
  json_t* body = ParseJson(response.body);
  const char* director_username
      = json_string_value(json_object_get(body, "director_username"));
  EXPECT_STREQ(director_username, "admin-notls");
  const char* session_token
      = json_string_value(json_object_get(body, "session_token"));
  ASSERT_NE(session_token, nullptr);
  EXPECT_TRUE(store->GetSession(session_token).has_value());
  json_decref(body);
}

TEST(HttpApi, InspectsAndLogsOutSession)
{
  const auto store = std::make_shared<ProxySessionStore>(std::chrono::hours(8));
  AuthResult result;
  result.identity.provider = "token";
  result.identity.subject = "ci-bot";
  const auto session
      = store->CreateSession(result, "limited-operator", "secret", "bareos-dir");

  const auto session_response = HandleHttpApiRequest(
      DemoConfig(), store,
      MakeRequest("GET", "/api/v1/auth/session", "",
                  {{"authorization", "Bearer " + session.token}}));
  EXPECT_EQ(session_response.status_code, 200);

  const auto logout_response = HandleHttpApiRequest(
      DemoConfig(), store,
      MakeRequest("POST", "/api/v1/auth/logout", "",
                  {{"authorization", "Bearer " + session.token}}));
  EXPECT_EQ(logout_response.status_code, 204);
  EXPECT_FALSE(store->GetSession(session.token).has_value());
}
