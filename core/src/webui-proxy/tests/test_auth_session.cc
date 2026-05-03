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

  const auto created = store.CreateSession(auth_result, "admin", "secret", "prod");
  const auto restored = store.GetSession(created.token);

  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->identity.provider, "password");
  EXPECT_EQ(restored->identity.username, "admin");
  EXPECT_EQ(restored->director_username, "admin");
  EXPECT_EQ(restored->director_password, "secret");
  EXPECT_EQ(restored->preferred_director_id, "prod");
  EXPECT_EQ(restored->created_at, 100);
  EXPECT_EQ(restored->expires_at, 100 + 8 * 60 * 60);
}

TEST(AuthSession, RefreshesExpiryAndPreferredDirector)
{
  std::time_t now = 100;
  ProxySessionStore store(std::chrono::hours(8), [&]() { return now; });

  AuthResult auth_result;
  auth_result.identity.provider = "password";
  auth_result.identity.subject = "admin";
  auth_result.identity.username = "admin";

  const auto created = store.CreateSession(auth_result, "admin", "secret", "prod");
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

  const auto created = store.CreateSession(auth_result, "admin", "secret", "prod");
  now += 6;

  EXPECT_FALSE(store.GetSession(created.token).has_value());
  EXPECT_FALSE(store.RefreshSession(created.token).has_value());
}
