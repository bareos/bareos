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

#include "../oidc_auth.h"

#include <gtest/gtest.h>

TEST(OidcAuth, CreatesAndConsumesPendingAuthorizationState)
{
  std::time_t now = 100;
  OidcPendingAuthStore store(std::chrono::minutes(10), [&]() { return now; });

  const auto pending = store.CreatePendingAuth("example");

  EXPECT_EQ(pending.provider_id, "example");
  EXPECT_FALSE(pending.state.empty());
  EXPECT_FALSE(pending.nonce.empty());
  EXPECT_FALSE(pending.code_verifier.empty());

  const auto restored = store.ConsumePendingAuth(pending.state);
  ASSERT_TRUE(restored.has_value());
  EXPECT_EQ(restored->state, pending.state);
  EXPECT_FALSE(store.ConsumePendingAuth(pending.state).has_value());
}

TEST(OidcAuth, ExpiresPendingAuthorizationState)
{
  std::time_t now = 100;
  OidcPendingAuthStore store(std::chrono::seconds(5), [&]() { return now; });

  const auto pending = store.CreatePendingAuth("example");
  now += 6;

  EXPECT_FALSE(store.ConsumePendingAuth(pending.state).has_value());
}

TEST(OidcAuth, BuildsAuthorizationUrlWithPkceAndNonce)
{
  OidcAuthProvider provider;
  provider.id = "example";
  provider.authorization_endpoint = "https://id.example.test/oauth2/authorize";
  provider.client_id = "bareos-webui";
  provider.redirect_uri
      = "https://webui.example.test/api/v1/auth/oidc/example/callback";
  provider.scopes = {"openid", "profile", "email"};

  OidcPendingAuth pending;
  pending.provider_id = "example";
  pending.state = "state-token";
  pending.nonce = "nonce-token";
  pending.code_verifier = "code-verifier";

  const auto url = BuildOidcAuthorizationUrl(provider, pending);

  EXPECT_NE(url.find("response_type=code"), std::string::npos);
  EXPECT_NE(url.find("client_id=bareos-webui"), std::string::npos);
  EXPECT_NE(url.find("scope=openid%20profile%20email"), std::string::npos);
  EXPECT_NE(url.find("state=state-token"), std::string::npos);
  EXPECT_NE(url.find("nonce=nonce-token"), std::string::npos);
  EXPECT_NE(url.find("code_challenge_method=S256"), std::string::npos);
  EXPECT_NE(url.find("code_challenge="), std::string::npos);
}
