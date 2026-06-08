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
  EXPECT_EQ(session->username, "admin");
  EXPECT_EQ(session->password, "secret");
  EXPECT_EQ(session->director, "bareos-dir");

  store.RemoveSession(session_id);
  EXPECT_FALSE(store.LookupSession(session_id));
}

TEST(ProxyAuthSessionStore, UpdatesPreferredDirector)
{
  auto& store = ProxyAuthSessionStore::Instance();
  const auto session_id = store.CreateSession("admin", "secret", "bareos-dir");

  store.UpdateDirector(session_id, "site-b");

  const auto session = store.LookupSession(session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->director, "site-b");

  store.RemoveSession(session_id);
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
