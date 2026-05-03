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

#include "../token_auth.h"

#include <gtest/gtest.h>

TEST(TokenAuth, VerifiesKnownSha256TokenHashes)
{
  const auto hash = CreateTokenAuthHash("demo-token");

  EXPECT_TRUE(VerifyTokenAuthToken("demo-token", hash));
  EXPECT_FALSE(VerifyTokenAuthToken("wrong-token", hash));
}

TEST(TokenAuth, RejectsInvalidTokenHashFormats)
{
  EXPECT_FALSE(VerifyTokenAuthToken("demo-token", ""));
  EXPECT_FALSE(VerifyTokenAuthToken("demo-token", "sha1$deadbeef"));
  EXPECT_FALSE(VerifyTokenAuthToken("demo-token", "sha256$not-hex"));
}

TEST(TokenAuth, AuthenticatesConfiguredTokensIntoTokenIdentity)
{
  TokenAuthEntry token;
  token.id = "ci-bot";
  token.token_hash = CreateTokenAuthHash("demo-token");
  token.subject = "ci-bot";
  token.username = "ci-bot";
  token.email = "ci@example.test";
  token.groups = {"automation"};
  token.roles = {"operator"};
  token.expires_at = 4102444800;

  const auto result = AuthenticateToken({token}, "demo-token", 1700000000);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->identity.provider, "token");
  EXPECT_EQ(result->identity.subject, "ci-bot");
  EXPECT_EQ(result->identity.username, "ci-bot");
  EXPECT_EQ(result->identity.email, "ci@example.test");
  EXPECT_EQ(result->identity.groups, std::vector<std::string>({"automation"}));
  EXPECT_EQ(result->identity.roles, std::vector<std::string>({"operator"}));
  ASSERT_TRUE(result->expires_at.has_value());
  EXPECT_EQ(*result->expires_at, 4102444800);
}

TEST(TokenAuth, RejectsUnknownOrExpiredTokens)
{
  TokenAuthEntry token;
  token.id = "ci-bot";
  token.token_hash = CreateTokenAuthHash("demo-token");
  token.expires_at = 1700000000;

  EXPECT_FALSE(AuthenticateToken({token}, "other-token", 1699999999).has_value());
  EXPECT_FALSE(AuthenticateToken({token}, "demo-token", 1700000000).has_value());
}
