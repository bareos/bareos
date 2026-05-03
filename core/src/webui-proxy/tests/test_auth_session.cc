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

#include "../auth_session.h"

#include <gtest/gtest.h>

TEST(AuthSession, StoresNormalizedIdentityAndDirectorCredentials)
{
  std::time_t now = 100;
  ProxySessionStore store(std::chrono::hours(8), [&]() { return now; });

  AuthResult auth_result;
  auth_result.identity.provider = "password";
  auth_result.identity.subject = "admin";
  auth_result.identity.username = "admin";

  const auto created
      = store.CreateSession(auth_result, "admin", "secret", "prod");
  const auto restored = store.GetSession(created.token);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->identity.provider, "password");
  EXPECT_EQ(restored->identity.username, "admin");
  EXPECT_EQ(restored->audit_metadata.provider, "password");
  EXPECT_EQ(restored->audit_metadata.subject, "admin");
  EXPECT_EQ(restored->audit_metadata.username, "admin");
  EXPECT_EQ(restored->audit_metadata.mapped_director_username, "admin");
  EXPECT_EQ(restored->audit_metadata.proxy_session_token, created.token);
  EXPECT_EQ(restored->director_username, "admin");
  EXPECT_EQ(restored->director_password, "secret");
  EXPECT_EQ(restored->preferred_director_id, "prod");
  EXPECT_EQ(restored->created_at, 100);
  EXPECT_EQ(restored->expires_at, 100 + 8 * 60 * 60);
}

TEST(AuthSession, PreservesExplicitAuditMetadataOnSessionCreation)
{
  std::time_t now = 100;
  ProxySessionStore store(std::chrono::hours(8), [&]() { return now; });

  AuthResult auth_result;
  auth_result.identity.provider = "oidc";
  auth_result.identity.subject = "248289761001";
  auth_result.identity.username = "demo-admin";
  auth_result.identity.email = "demo@example.test";
  auth_result.audit_metadata
      = ProxyAuditMetadata{.provider = "oidc",
                           .subject = "248289761001",
                           .username = "demo-admin",
                           .email = "demo@example.test",
                           .mapped_director_username = "admin-notls",
                           .proxy_session_token = ""};

  const auto created
      = store.CreateSession(auth_result, "admin-notls", "secret", "prod");

  EXPECT_EQ(created.audit_metadata.provider, "oidc");
  EXPECT_EQ(created.audit_metadata.subject, "248289761001");
  EXPECT_EQ(created.audit_metadata.email, "demo@example.test");
  EXPECT_EQ(created.audit_metadata.mapped_director_username, "admin-notls");
  EXPECT_EQ(created.audit_metadata.proxy_session_token, created.token);
}

TEST(AuthSession, RefreshesExpiryAndPreferredDirector)
{
  std::time_t now = 100;
  ProxySessionStore store(std::chrono::hours(8), [&]() { return now; });

  AuthResult auth_result;
  auth_result.identity.provider = "password";
  auth_result.identity.subject = "admin";
  auth_result.identity.username = "admin";

  const auto created
      = store.CreateSession(auth_result, "admin", "secret", "prod");
  now += 60;
  const auto refreshed = store.RefreshSession(created.token, std::string("dr"));

  ASSERT_TRUE(refreshed.has_value());
  EXPECT_EQ(refreshed->preferred_director_id, "dr");
  EXPECT_EQ(refreshed->expires_at, now + 8 * 60 * 60);
}

TEST(AuthSession, ExpiresSessions)
{
  std::time_t now = 100;
  ProxySessionStore store(std::chrono::seconds(5), [&]() { return now; });

  AuthResult auth_result;
  auth_result.identity.provider = "password";
  auth_result.identity.subject = "admin";
  auth_result.identity.username = "admin";

  const auto created
      = store.CreateSession(auth_result, "admin", "secret", "prod");
  now += 6;

  EXPECT_FALSE(store.GetSession(created.token).has_value());
  EXPECT_FALSE(store.RefreshSession(created.token).has_value());
}
