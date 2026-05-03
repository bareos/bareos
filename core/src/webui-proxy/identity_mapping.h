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
#ifndef BAREOS_WEBUI_PROXY_IDENTITY_MAPPING_H_
#define BAREOS_WEBUI_PROXY_IDENTITY_MAPPING_H_

#include "auth_session.h"

#include <optional>
#include <string>
#include <vector>

struct IdentityMappingRule {
  std::string id;
  std::optional<std::string> provider;
  std::optional<std::string> subject;
  std::optional<std::string> username;
  std::optional<std::string> email;
  std::vector<std::string> groups;
  std::vector<std::string> roles;
  std::string director_username;
  std::string director_password;
  std::optional<std::string> preferred_director_id;
};

struct ResolvedIdentityMapping {
  std::string rule_id;
  std::string director_username;
  std::string director_password;
  std::optional<std::string> preferred_director_id;
};

bool MatchesIdentityMappingRule(const IdentityMappingRule& rule,
                                const AuthIdentity& identity);

std::optional<ResolvedIdentityMapping> ResolveIdentityMapping(
    const std::vector<IdentityMappingRule>& rules, const AuthIdentity& identity);

#endif  // BAREOS_WEBUI_PROXY_IDENTITY_MAPPING_H_
