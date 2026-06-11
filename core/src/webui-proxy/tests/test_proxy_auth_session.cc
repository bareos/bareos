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

#include "../proxy_auth_session.h"

#include <gtest/gtest.h>

TEST(ProxyAuthSessionStore, CreatesLooksUpAndRemovesSessions)
{
  auto& store = ProxyAuthSessionStore::Instance();
  const auto session_id = store.CreateSession("admin", "secret", "bareos-dir");

  const auto session = store.LookupSession(session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->session_id, session_id);
  EXPECT_EQ(session->current_director, "bareos-dir");
  ASSERT_EQ(session->directors.size(), 1U);
  ASSERT_TRUE(session->directors.contains("bareos-dir"));
  EXPECT_EQ(session->directors.at("bareos-dir").username, "admin");
  EXPECT_EQ(session->directors.at("bareos-dir").password, "secret");

  store.RemoveSession(session_id);
  EXPECT_FALSE(store.LookupSession(session_id));
}

TEST(ProxyAuthSessionStore, StoresAdditionalDirectorCredentials)
{
  auto& store = ProxyAuthSessionStore::Instance();
  const auto session_id = store.CreateSession("admin", "secret", "bareos-dir");

  ASSERT_TRUE(store.StoreDirectorCredentials(session_id, "site-b", "ops", "site-secret",
                                             false));

  const auto session = store.LookupSession(session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->current_director, "bareos-dir");
  ASSERT_EQ(session->directors.size(), 2U);
  ASSERT_TRUE(session->directors.contains("site-b"));
  EXPECT_EQ(session->directors.at("site-b").username, "ops");
  EXPECT_EQ(session->directors.at("site-b").password, "site-secret");

  store.RemoveSession(session_id);
}

TEST(ProxyAuthSessionStore, SwitchesAndRemovesDirectors)
{
  auto& store = ProxyAuthSessionStore::Instance();
  const auto session_id = store.CreateSession("admin", "secret", "bareos-dir");
  ASSERT_TRUE(
      store.StoreDirectorCredentials(session_id, "site-b", "ops", "site-secret", false));

  ASSERT_TRUE(store.SetCurrentDirector(session_id, "site-b"));
  auto session = store.LookupSession(session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->current_director, "site-b");

  ASSERT_TRUE(store.RemoveDirector(session_id, "site-b"));
  session = store.LookupSession(session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->current_director, "bareos-dir");
  EXPECT_FALSE(session->directors.contains("site-b"));

  ASSERT_TRUE(store.RemoveDirector(session_id, "bareos-dir"));
  EXPECT_FALSE(store.LookupSession(session_id));
}

TEST(ProxyAuthSessionStore, BuildsCookieHeaders)
{
  const auto session_cookie = BuildProxySessionCookie("abc123", true);
  EXPECT_NE(session_cookie.find("bareos_proxy_session=abc123"), std::string::npos);
  EXPECT_NE(session_cookie.find("HttpOnly"), std::string::npos);
  EXPECT_NE(session_cookie.find("SameSite=Strict"), std::string::npos);
  EXPECT_NE(session_cookie.find("Secure"), std::string::npos);

  const auto expired_cookie = BuildExpiredProxySessionCookie(false);
  EXPECT_NE(expired_cookie.find("Max-Age=0"), std::string::npos);
  EXPECT_EQ(expired_cookie.find("Secure"), std::string::npos);
}
