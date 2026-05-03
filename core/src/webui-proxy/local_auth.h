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
#ifndef BAREOS_WEBUI_PROXY_LOCAL_AUTH_H_
#define BAREOS_WEBUI_PROXY_LOCAL_AUTH_H_

#include "auth_session.h"

#include <optional>
#include <string>
#include <vector>

struct LocalAuthUser {
  std::string id;
  std::string username;
  std::string password_hash;
  std::string subject;
  std::string email;
  std::vector<std::string> groups;
  std::vector<std::string> roles;
};

std::string CreateLocalAuthPasswordHash(const std::string& password,
                                        const std::string& salt_hex,
                                        int iterations);

bool VerifyLocalAuthPassword(const std::string& password,
                             const std::string& password_hash);

std::optional<AuthResult> AuthenticateLocalUser(
    const std::vector<LocalAuthUser>& users,
    const std::string& username,
    const std::string& password);

#endif  // BAREOS_WEBUI_PROXY_LOCAL_AUTH_H_
