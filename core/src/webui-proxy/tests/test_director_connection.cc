/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

#include "../director_connection.h"

#include "ascii_control_characters.h"

#include <gtest/gtest.h>

TEST(DirectorConnection, BuildsTlsPskIdentityForConsole)
{
  const std::string identity = GetDirectorTlsPskIdentity("admin-tls");
  std::string expected = "R_CONSOLE";
  expected.push_back(AsciiControlCharacters::RecordSeparator());
  expected.append("admin-tls");

  EXPECT_EQ(identity, expected);
}

TEST(DirectorConnection, UsesMd5HexPasswordAsTlsPsk)
{
  EXPECT_EQ(GetDirectorTlsPskSecret("secret"),
            "5ebe2294ecd0e0f08eab7690d2a6ee69");
}

TEST(DirectorConfig, DoesNotRequireTlsPskByDefault)
{
  DirectorConfig cfg;

  EXPECT_FALSE(cfg.tls_psk_disable);
  EXPECT_FALSE(cfg.tls_psk_require);
}

TEST(DirectorConnection, StartsWithoutTlsPskTransport)
{
  DirectorConnection connection;

  EXPECT_FALSE(connection.UsesTlsPsk());
}
