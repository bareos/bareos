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

#include "../proxy_session.h"
#include "../token_auth.h"

#include <gtest/gtest.h>

TEST(ProxySession, TrimsWhitespaceAroundCommands)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("  list jobs \r\n"), "list jobs");
}

TEST(ProxySession, PreservesTrailingTabForCompletion)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("  list cl\t \r\n"), "list cl\t");
}

TEST(ProxySession, PreservesStandaloneTabCompletion)
{
  EXPECT_EQ(NormalizeRawConsoleCommand("\t"), "\t");
}

TEST(ProxySession, ResolvesAccessTokenIntoMappedDirectorCredentials)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      "[listen]\n"
      "ws_host = 127.0.0.1\n"
      "\n"
      "[director:bareos-dir]\n"
      "host = dir.example.test\n"
      "port = 19101\n"
      "director_name = bareos-dir\n"
      "tls_psk_disable = true\n"
      "\n"
      "[auth-token:ci-bot]\n"
      "token_hash = " + CreateTokenAuthHash("demo-token") + "\n"
      "subject = ci-bot\n"
      "groups = automation\n"
      "roles = operator\n"
      "expires_at = 4102444800\n"
      "\n"
      "[identity-map:ci-bot]\n"
      "provider = token\n"
      "subject = ci-bot\n"
      "director_username = limited-operator\n"
      "director_password = secret\n"
      "preferred_director = bareos-dir\n",
      cfg);

  const auto auth = ResolveProxyAuthRequest(
      cfg, nullptr, std::nullopt, std::string("demo-token"), std::nullopt,
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, true);

  EXPECT_EQ(auth.identity.provider, "token");
  EXPECT_EQ(auth.identity.subject, "ci-bot");
  EXPECT_EQ(auth.identity.groups, std::vector<std::string>({"automation"}));
  ASSERT_TRUE(auth.audit_metadata.has_value());
  EXPECT_EQ(auth.audit_metadata->provider, "token");
  EXPECT_EQ(auth.audit_metadata->subject, "ci-bot");
  EXPECT_EQ(auth.audit_metadata->mapped_director_username, "limited-operator");
  ASSERT_TRUE(auth.expires_at.has_value());
  EXPECT_EQ(*auth.expires_at, 4102444800);
  EXPECT_EQ(auth.director_config.username, "limited-operator");
  EXPECT_EQ(auth.director_config.password, "secret");
  ASSERT_TRUE(auth.director_config.audit_metadata.has_value());
  EXPECT_EQ(auth.director_config.audit_metadata->provider, "token");
  EXPECT_EQ(auth.director_config.audit_metadata->subject, "ci-bot");
  EXPECT_EQ(auth.director_config.audit_metadata->mapped_director_username,
            "limited-operator");
  EXPECT_EQ(auth.director_config.host, "dir.example.test");
  EXPECT_EQ(auth.director_config.port, 19101);
  EXPECT_EQ(auth.director_config.director_name, "bareos-dir");
  EXPECT_TRUE(auth.director_config.tls_psk_disable);
  EXPECT_FALSE(auth.director_config.tls_psk_require);
  EXPECT_EQ(auth.preferred_director_id, "bareos-dir");
}

TEST(ProxySession, RejectsInvalidAccessTokenBeforeDirectorConnect)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      "[listen]\n"
      "ws_host = 127.0.0.1\n"
      "\n"
      "[director:bareos-dir]\n"
      "host = dir.example.test\n"
      "port = 19101\n"
      "director_name = bareos-dir\n"
      "\n"
      "[auth-token:ci-bot]\n"
      "token_hash = " + CreateTokenAuthHash("demo-token") + "\n"
      "subject = ci-bot\n"
      "\n"
      "[identity-map:ci-bot]\n"
      "provider = token\n"
      "subject = ci-bot\n"
      "director_username = limited-operator\n"
      "director_password = secret\n",
      cfg);

  EXPECT_THROW(ResolveProxyAuthRequest(cfg, nullptr, std::nullopt,
                                       std::string("wrong-token"), std::nullopt,
                                       std::nullopt, std::nullopt, std::nullopt,
                                       std::nullopt, true),
               std::runtime_error);
}
