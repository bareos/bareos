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

#include "../local_auth.h"

#include <gtest/gtest.h>

TEST(LocalAuth, VerifiesKnownPbkdf2Sha256Hashes)
{
  const auto hash
      = CreateLocalAuthPasswordHash("secret", "00112233445566778899aabbccddeeff",
                                    600000);

  EXPECT_TRUE(VerifyLocalAuthPassword("secret", hash));
  EXPECT_FALSE(VerifyLocalAuthPassword("wrong", hash));
}

TEST(LocalAuth, RejectsInvalidHashFormats)
{
  EXPECT_FALSE(VerifyLocalAuthPassword("secret", ""));
  EXPECT_FALSE(VerifyLocalAuthPassword("secret", "md5$123$abcd$ef"));
  EXPECT_FALSE(VerifyLocalAuthPassword("secret",
                                       "pbkdf2-sha256$abc$0011$deadbeef"));
}

TEST(LocalAuth, AuthenticatesConfiguredUsersIntoLocalIdentity)
{
  LocalAuthUser user;
  user.id = "demo-admin";
  user.username = "demo-admin";
  user.password_hash = CreateLocalAuthPasswordHash(
      "secret", "0102030405060708090a0b0c0d0e0f10", 600000);
  user.subject = "demo-admin-subject";
  user.email = "demo@example.test";
  user.groups = {"backup-admins"};
  user.roles = {"operator"};

  const auto result = AuthenticateLocalUser({user}, "demo-admin", "secret");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->identity.provider, "local");
  EXPECT_EQ(result->identity.subject, "demo-admin-subject");
  EXPECT_EQ(result->identity.username, "demo-admin");
  EXPECT_EQ(result->identity.email, "demo@example.test");
  EXPECT_EQ(result->identity.groups, std::vector<std::string>({"backup-admins"}));
  EXPECT_EQ(result->identity.roles, std::vector<std::string>({"operator"}));
}

TEST(LocalAuth, RejectsUnknownOrInvalidLocalCredentials)
{
  LocalAuthUser user;
  user.username = "demo-admin";
  user.password_hash = CreateLocalAuthPasswordHash(
      "secret", "0102030405060708090a0b0c0d0e0f10", 600000);

  EXPECT_FALSE(AuthenticateLocalUser({user}, "other", "secret").has_value());
  EXPECT_FALSE(AuthenticateLocalUser({user}, "demo-admin", "wrong").has_value());
}
