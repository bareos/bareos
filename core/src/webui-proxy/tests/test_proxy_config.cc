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

#include "../proxy_config.h"

#include <gtest/gtest.h>

TEST(ProxyConfig, ParsesAllowedDirectorsFromIni)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"ini(
[listen]
ws_host = 127.0.0.1
ws_port = 18765
session_ttl = 600

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir

[director:dr]
host = dr.example.test
port = 29101
director_name = dr-dir
tls_psk_disable = true
)ini",
      cfg);

  EXPECT_EQ(cfg.bind_host, "127.0.0.1");
  EXPECT_EQ(cfg.port, 18765);
  EXPECT_EQ(cfg.session_ttl, 600);
  ASSERT_EQ(cfg.allowed_directors.size(), 2U);
  EXPECT_EQ(cfg.allowed_directors.at("prod").id, "prod");
  EXPECT_EQ(cfg.allowed_directors.at("prod").host, "prod.example.test");
  EXPECT_EQ(cfg.allowed_directors.at("prod").port, 19101);
  EXPECT_EQ(cfg.allowed_directors.at("prod").name, "bareos-dir");
  EXPECT_EQ(cfg.allowed_directors.at("dr").id, "dr");
  EXPECT_EQ(cfg.allowed_directors.at("dr").host, "dr.example.test");
  EXPECT_EQ(cfg.allowed_directors.at("dr").port, 29101);
  EXPECT_EQ(cfg.allowed_directors.at("dr").name, "dr-dir");
  EXPECT_TRUE(cfg.allowed_directors.at("dr").tls_psk_disable);
  EXPECT_FALSE(cfg.allowed_directors.at("dr").tls_psk_require);
}

TEST(ProxyConfig, ParsesIdentityMappingsFromIni)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"ini(
[listen]
ws_host = 127.0.0.1

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir

[identity-map:admin]
provider = password
username = admin-notls
groups = backup-admins, linux
roles = operator
director_username = webui-admin
director_password = mapped-secret
preferred_director = prod
)ini",
      cfg);

  ASSERT_EQ(cfg.identity_mappings.size(), 1U);
  const auto& rule = cfg.identity_mappings.front();
  EXPECT_EQ(rule.id, "admin");
  ASSERT_TRUE(rule.provider.has_value());
  EXPECT_EQ(*rule.provider, "password");
  ASSERT_TRUE(rule.username.has_value());
  EXPECT_EQ(*rule.username, "admin-notls");
  EXPECT_EQ(rule.groups, std::vector<std::string>({"backup-admins", "linux"}));
  EXPECT_EQ(rule.roles, std::vector<std::string>({"operator"}));
  EXPECT_EQ(rule.director_username, "webui-admin");
  EXPECT_EQ(rule.director_password, "mapped-secret");
  ASSERT_TRUE(rule.preferred_director_id.has_value());
  EXPECT_EQ(*rule.preferred_director_id, "prod");
}

TEST(ProxyConfig, RejectsIdentityMappingsWithoutDirectorCredentials)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
ws_host = 127.0.0.1

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir

[identity-map:admin]
provider = password
username = admin-notls
director_username = webui-admin
)ini",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, ParsesLocalAuthUsersFromIni)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"ini(
[listen]
ws_host = 127.0.0.1

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir

[auth-user:demo-admin]
username = demo-admin
password_hash = pbkdf2-sha256$600000$00112233445566778899aabbccddeeff$5fdcc31c2fc8cbef8a0f8317d421ebcf54f7514c0f2ec08e3f771a4312f3b9cf
subject = demo-admin-subject
email = demo@example.test
groups = backup-admins, linux
roles = operator
)ini",
      cfg);

  ASSERT_EQ(cfg.local_auth_users.size(), 1U);
  const auto& user = cfg.local_auth_users.front();
  EXPECT_EQ(user.id, "demo-admin");
  EXPECT_EQ(user.username, "demo-admin");
  EXPECT_EQ(user.subject, "demo-admin-subject");
  EXPECT_EQ(user.email, "demo@example.test");
  EXPECT_EQ(user.groups, std::vector<std::string>({"backup-admins", "linux"}));
  EXPECT_EQ(user.roles, std::vector<std::string>({"operator"}));
}

TEST(ProxyConfig, RejectsLocalAuthUsersWithoutPasswordHash)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
ws_host = 127.0.0.1

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir

[auth-user:demo-admin]
username = demo-admin
)ini",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, ParsesTokenAuthEntriesFromIni)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"ini(
[listen]
ws_host = 127.0.0.1

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir

[auth-token:ci-bot]
token_hash = sha256$94313d1c406f7af5469a1e7145dfce63a8d46d6e2523d39114f6bc9e864eb518
subject = ci-bot
username = ci-bot
email = ci@example.test
groups = automation, linux
roles = operator
expires_at = 4102444800
)ini",
      cfg);

  ASSERT_EQ(cfg.token_auth_entries.size(), 1U);
  const auto& token = cfg.token_auth_entries.front();
  EXPECT_EQ(token.id, "ci-bot");
  EXPECT_EQ(token.token_hash,
            "sha256$"
            "94313d1c406f7af5469a1e7145dfce63a8d46d6e2523d39114f6bc9e864eb518");
  EXPECT_EQ(token.subject, "ci-bot");
  EXPECT_EQ(token.username, "ci-bot");
  EXPECT_EQ(token.email, "ci@example.test");
  EXPECT_EQ(token.groups, std::vector<std::string>({"automation", "linux"}));
  EXPECT_EQ(token.roles, std::vector<std::string>({"operator"}));
  ASSERT_TRUE(token.expires_at.has_value());
  EXPECT_EQ(*token.expires_at, 4102444800);
}

TEST(ProxyConfig, RejectsTokenAuthEntriesWithoutTokenHash)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
ws_host = 127.0.0.1

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir

[auth-token:ci-bot]
subject = ci-bot
)ini",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, RejectsLegacyDirectorSection)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[director]
host = legacy.example.test
port = 19101
director_name = legacy-dir

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir
)ini",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, RejectsMissingDirectorSections)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
ws_host = 127.0.0.1
)ini",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, RequiresDirectorSelectionWhenMultipleConfigured)
{
  ProxyConfig cfg;
  cfg.allowed_directors.emplace(
      "prod",
      DirectorTargetConfig{
          .host = "prod.example.test", .port = 19101, .name = "bareos-dir"});
  cfg.allowed_directors.emplace(
      "dr", DirectorTargetConfig{
                .host = "dr.example.test", .port = 29101, .name = "dr-dir"});

  EXPECT_THROW(
      ResolveDirectorTarget(cfg, std::nullopt, std::nullopt, std::nullopt),
      std::runtime_error);
}

TEST(ProxyConfig, ResolvesAllowedDirectorById)
{
  ProxyConfig cfg;
  cfg.allowed_directors.emplace(
      "prod",
      DirectorTargetConfig{
          .host = "prod.example.test", .port = 19101, .name = "bareos-dir"});

  const auto target = ResolveDirectorTarget(cfg, std::string("prod"),
                                            std::nullopt, std::nullopt);

  EXPECT_EQ(target.host, "prod.example.test");
  EXPECT_EQ(target.port, 19101);
  EXPECT_EQ(target.name, "bareos-dir");
}

TEST(ProxyConfig, ListsAllowedDirectorIds)
{
  ProxyConfig cfg;
  cfg.allowed_directors.emplace(
      "prod",
      DirectorTargetConfig{
          .host = "prod.example.test", .port = 19101, .name = "bareos-dir"});
  cfg.allowed_directors.emplace(
      "dr", DirectorTargetConfig{
                .host = "dr.example.test", .port = 29101, .name = "dr-dir"});

  EXPECT_EQ(GetAllowedDirectorIds(cfg),
            std::vector<std::string>({"dr", "prod"}));
}

TEST(ProxyConfig, ResolvesSingleConfiguredDirectorWithoutDefault)
{
  ProxyConfig cfg;
  cfg.allowed_directors.emplace(
      "custom-dir",
      DirectorTargetConfig{
          .host = "dir.example.test", .port = 19101, .name = "custom-dir"});

  const auto target
      = ResolveDirectorTarget(cfg, std::nullopt, std::nullopt, std::nullopt);

  EXPECT_EQ(target.host, "dir.example.test");
  EXPECT_EQ(target.port, 19101);
  EXPECT_EQ(target.name, "custom-dir");
}

TEST(ProxyConfig, RejectsBrowserHostPortOverrides)
{
  ProxyConfig cfg;

  EXPECT_THROW(ResolveDirectorTarget(cfg, std::nullopt,
                                     std::string("dir.example.test"), 19101),
               std::runtime_error);
}

TEST(ProxyConfig, RejectsUnexpectedDirectorWithoutAllowlist)
{
  ProxyConfig cfg;
  cfg.allowed_directors.emplace(
      "custom-dir",
      DirectorTargetConfig{
          .host = "dir.example.test", .port = 19101, .name = "custom-dir"});

  EXPECT_THROW(ResolveDirectorTarget(cfg, std::string("other-dir"),
                                     std::nullopt, std::nullopt),
               std::runtime_error);
}
