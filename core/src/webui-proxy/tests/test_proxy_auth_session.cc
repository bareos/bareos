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
#include <thread>

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

TEST(ProxyAuthSessionStore, AppliesConfigurableTimeouts)
{
  auto& store = ProxyAuthSessionStore::Instance();

  // Set custom timeouts: 1 minute idle, 2 hours absolute
  store.SetSessionTimeouts(1, 2);

  const auto session_id = store.CreateSession("admin", "secret", "bareos-dir");
  const auto session = store.LookupSession(session_id);
  ASSERT_TRUE(session);
  EXPECT_EQ(session->session_id, session_id);

  // Verify session is still accessible (just created)
  EXPECT_TRUE(store.LookupSession(session_id));

  // Reset to defaults for next tests
  store.SetSessionTimeouts(30, 8);
  store.RemoveSession(session_id);
}

TEST(ProxyAuthSessionStore, ExpiresIdleSessionsWithConfiguredTimeout)
{
  auto& store = ProxyAuthSessionStore::Instance();

  // Set very short timeouts for testing: 100ms idle, 1 second absolute
  store.SetSessionTimeoutsForTesting(std::chrono::milliseconds(100),
                                      std::chrono::seconds(1));

  const auto session_id = store.CreateSession("admin", "secret", "bareos-dir");
  
  // Session should be available immediately after creation
  EXPECT_TRUE(store.LookupSession(session_id));

  // Wait for idle timeout to trigger (>100ms)
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  // Session should now be expired due to idle timeout
  EXPECT_FALSE(store.LookupSession(session_id));

  // Reset to defaults for next tests
  store.SetSessionTimeouts(30, 8);
}

TEST(ProxyAuthSessionStore, ExpiresAbsoluteLifetimeSessionsWithConfiguredTimeout)
{
  auto& store = ProxyAuthSessionStore::Instance();

  // Set very short absolute lifetime: 100ms idle (won't trigger), 150ms absolute
  store.SetSessionTimeoutsForTesting(std::chrono::seconds(10),
                                      std::chrono::milliseconds(150));

  const auto session_id = store.CreateSession("admin", "secret", "bareos-dir");
  
  // Session should be available immediately after creation
  EXPECT_TRUE(store.LookupSession(session_id));

  // Wait for absolute lifetime to trigger (>150ms)
  // We repeatedly lookup to keep the idle timeout from triggering
  auto start = std::chrono::steady_clock::now();
  bool found_after_lifetime = true;
  while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(200)) {
    if (!store.LookupSession(session_id)) {
      found_after_lifetime = false;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Session should have expired due to absolute lifetime
  EXPECT_FALSE(found_after_lifetime);

  // Reset to defaults for next tests
  store.SetSessionTimeouts(30, 8);
}
