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
#include "identity_mapping.h"

#include <algorithm>

namespace {

bool ContainsAll(const std::vector<std::string>& expected,
                 const std::vector<std::string>& actual)
{
  return std::all_of(expected.begin(), expected.end(), [&](const auto& value) {
    return std::find(actual.begin(), actual.end(), value) != actual.end();
  });
}

}  // namespace

bool MatchesIdentityMappingRule(const IdentityMappingRule& rule,
                                const AuthIdentity& identity)
{
  if (rule.provider && *rule.provider != identity.provider) { return false; }
  if (rule.subject && *rule.subject != identity.subject) { return false; }
  if (rule.username && *rule.username != identity.username) { return false; }
  if (rule.email && *rule.email != identity.email) { return false; }
  if (!rule.groups.empty() && !ContainsAll(rule.groups, identity.groups)) {
    return false;
  }
  if (!rule.roles.empty() && !ContainsAll(rule.roles, identity.roles)) {
    return false;
  }
  return true;
}

std::optional<ResolvedIdentityMapping> ResolveIdentityMapping(
    const std::vector<IdentityMappingRule>& rules, const AuthIdentity& identity)
{
  auto it = std::find_if(rules.begin(), rules.end(), [&](const auto& rule) {
    return MatchesIdentityMappingRule(rule, identity);
  });
  if (it == rules.end()) { return std::nullopt; }

  return ResolvedIdentityMapping{.rule_id = it->id,
                                 .director_username = it->director_username,
                                 .director_password = it->director_password,
                                 .preferred_director_id
                                 = it->preferred_director_id};
}
