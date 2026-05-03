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

#include "../identity_mapping.h"

#include <gtest/gtest.h>

namespace {

IdentityMappingRule MakeRule()
{
  IdentityMappingRule rule;
  return rule;
}

AuthIdentity MakeIdentity()
{
  AuthIdentity identity;
  return identity;
}

}  // namespace

TEST(IdentityMapping, MatchesProviderAndUsername)
{
  auto rule = MakeRule();
  rule.id = "admin";
  rule.provider = "password";
  rule.username = "admin-notls";
  rule.director_username = "webui-admin";
  rule.director_password = "secret";

  auto identity = MakeIdentity();
  identity.provider = "password";
  identity.subject = "admin-notls";
  identity.username = "admin-notls";

  EXPECT_TRUE(MatchesIdentityMappingRule(rule, identity));
}

TEST(IdentityMapping, RequiresConfiguredGroupsAndRoles)
{
  auto rule = MakeRule();
  rule.id = "ops";
  rule.groups = {"backup-admins", "linux"};
  rule.roles = {"operator"};
  rule.director_username = "webui-admin";
  rule.director_password = "secret";

  auto matching = MakeIdentity();
  matching.groups = {"backup-admins", "linux", "prod"};
  matching.roles = {"operator", "viewer"};

  auto missing_group = MakeIdentity();
  missing_group.groups = {"backup-admins"};
  missing_group.roles = {"operator"};

  auto missing_role = MakeIdentity();
  missing_role.groups = {"backup-admins", "linux"};
  missing_role.roles = {"viewer"};

  EXPECT_TRUE(MatchesIdentityMappingRule(rule, matching));
  EXPECT_FALSE(MatchesIdentityMappingRule(rule, missing_group));
  EXPECT_FALSE(MatchesIdentityMappingRule(rule, missing_role));
}

TEST(IdentityMapping, ResolvesFirstMatchingRule)
{
  auto first = MakeRule();
  first.id = "first";
  first.provider = "password";
  first.username = "admin";
  first.director_username = "mapped-one";
  first.director_password = "pw-one";
  first.preferred_director_id = "prod";

  auto second = MakeRule();
  second.id = "second";
  second.provider = "password";
  second.username = "admin";
  second.director_username = "mapped-two";
  second.director_password = "pw-two";

  const std::vector<IdentityMappingRule> rules{first, second};

  auto identity = MakeIdentity();
  identity.provider = "password";
  identity.username = "admin";

  const auto resolved = ResolveIdentityMapping(rules, identity);

  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->rule_id, "first");
  EXPECT_EQ(resolved->director_username, "mapped-one");
  EXPECT_EQ(resolved->director_password, "pw-one");
  ASSERT_TRUE(resolved->preferred_director_id.has_value());
  EXPECT_EQ(*resolved->preferred_director_id, "prod");
}

TEST(IdentityMapping, ReturnsNoMatchWhenRulesDoNotApply)
{
  auto rule = MakeRule();
  rule.id = "ops";
  rule.provider = "oidc";
  rule.groups = {"backup-admins"};
  rule.director_username = "webui-admin";
  rule.director_password = "secret";

  auto identity = MakeIdentity();
  identity.provider = "password";
  identity.username = "admin";

  const auto resolved = ResolveIdentityMapping(std::vector<IdentityMappingRule>{rule},
                                               identity);

  EXPECT_FALSE(resolved.has_value());
}
