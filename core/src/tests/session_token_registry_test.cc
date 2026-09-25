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

#include "gtest/gtest.h"
#include "stored/session_token_registry.h"

#include <string>
#include <thread>
#include <vector>

using namespace storagedaemon;

// A wiped authentication key must never be usable as a token.
static_assert(!IsUsableSessionToken(""));
static_assert(!IsUsableSessionToken(std::string_view{}));

static_assert(IsUsableSessionToken("k"));
static_assert(IsUsableSessionToken("a-real-looking-session-key"));

// Only the emptiness of the token decides, not its content.
static_assert(IsUsableSessionToken(std::string_view{"\0", 1}));

namespace {
using Registry = SessionTokenRegistry<int>;
}  // namespace

TEST(SessionTokenRegistry, FindsRegisteredToken)
{
  Registry registry;

  ASSERT_TRUE(registry.Add("key-a", 1));
  ASSERT_TRUE(registry.Add("key-b", 2));
  EXPECT_EQ(registry.size(), 2U);

  int found = 0;
  EXPECT_TRUE(registry.Visit("key-b", [&found](int value) { found = value; }));
  EXPECT_EQ(found, 2);
}

TEST(SessionTokenRegistry, UnknownTokenIsNotFound)
{
  Registry registry;

  ASSERT_TRUE(registry.Add("key-a", 1));

  bool visited = false;
  EXPECT_FALSE(registry.Visit("key-x", [&visited](int) { visited = true; }));
  EXPECT_FALSE(visited);
}

/* A job whose key was wiped is still registered under its real key only.
 * Presenting an empty key must not bind to any job. */
TEST(SessionTokenRegistry, EmptyTokenNeverMatches)
{
  Registry registry;

  ASSERT_TRUE(registry.Add("key-a", 1));

  bool visited = false;
  EXPECT_FALSE(registry.Visit("", [&visited](int) { visited = true; }));
  EXPECT_FALSE(visited);

  // An empty token can also not be registered in the first place.
  EXPECT_FALSE(registry.Add("", 99));
  EXPECT_EQ(registry.size(), 1U);
}

TEST(SessionTokenRegistry, DuplicateTokenIsRejected)
{
  Registry registry;

  ASSERT_TRUE(registry.Add("key-a", 1));
  EXPECT_FALSE(registry.Add("key-a", 2));

  // The original entry must survive a rejected registration.
  int found = 0;
  EXPECT_TRUE(registry.Visit("key-a", [&found](int value) { found = value; }));
  EXPECT_EQ(found, 1);
}

TEST(SessionTokenRegistry, RemovedTokenIsNoLongerUsable)
{
  Registry registry;

  ASSERT_TRUE(registry.Add("key-a", 1));
  EXPECT_TRUE(registry.Remove("key-a"));
  EXPECT_EQ(registry.size(), 0U);

  bool visited = false;
  EXPECT_FALSE(registry.Visit("key-a", [&visited](int) { visited = true; }));
  EXPECT_FALSE(visited);

  // Removing again reports that there was nothing to remove.
  EXPECT_FALSE(registry.Remove("key-a"));
  EXPECT_FALSE(registry.Remove(""));
}

// Rotating a key must invalidate the previous one.
TEST(SessionTokenRegistry, RotatedTokenReplacesPrevious)
{
  Registry registry;

  ASSERT_TRUE(registry.Add("old-key", 1));
  ASSERT_TRUE(registry.Remove("old-key"));
  ASSERT_TRUE(registry.Add("new-key", 1));

  EXPECT_FALSE(registry.Visit("old-key", [](int) {}));
  EXPECT_TRUE(registry.Visit("new-key", [](int) {}));
  EXPECT_EQ(registry.size(), 1U);
}

TEST(SessionTokenRegistry, ConcurrentUseKeepsRegistryConsistent)
{
  Registry registry;
  constexpr int kThreads = 8;
  constexpr int kPerThread = 100;

  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&registry, t] {
      for (int i = 0; i < kPerThread; ++i) {
        std::string token
            = "t" + std::to_string(t) + "-" + std::to_string(i);
        EXPECT_TRUE(registry.Add(token, t));
        EXPECT_TRUE(registry.Visit(token, [t](int value) {
          EXPECT_EQ(value, t);
        }));
        EXPECT_TRUE(registry.Remove(token));
      }
    });
  }
  for (auto& thread : threads) { thread.join(); }

  EXPECT_EQ(registry.size(), 0U);
}
