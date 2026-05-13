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

#include "../proxy_config.h"

#include <gtest/gtest.h>

#include <string>

TEST(ProxyConfig, ParsesAllowedDirectorsFromIni)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"ini(
[listen]
ws_host = 127.0.0.1
ws_port = 18765

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir

[director:dr]
host = dr.example.test
port = 29101
director_name = dr-dir
tls_psk_disable = yes
)ini",
      cfg);

  EXPECT_EQ(cfg.bind_host, "127.0.0.1");
  EXPECT_EQ(cfg.port, 18765);
  ASSERT_EQ(cfg.allowed_directors.size(), 2U);
  EXPECT_EQ(cfg.allowed_directors.at("prod").host, "prod.example.test");
  EXPECT_EQ(cfg.allowed_directors.at("prod").port, 19101);
  EXPECT_EQ(cfg.allowed_directors.at("prod").name, "bareos-dir");
  EXPECT_EQ(cfg.allowed_directors.at("dr").host, "dr.example.test");
  EXPECT_EQ(cfg.allowed_directors.at("dr").port, 29101);
  EXPECT_EQ(cfg.allowed_directors.at("dr").name, "dr-dir");
  EXPECT_TRUE(cfg.allowed_directors.at("dr").tls_psk_disable);
}

TEST(ProxyConfig, RejectsDirectorSectionWithoutId)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[director]
host = unscoped.example.test
port = 19101
director_name = unscoped-dir

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir
)ini",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, RejectsMissingDirectorSections)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
ws_host = 127.0.0.1
)ini",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, RejectsNonYesNoBooleanValues)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
ws_host = 127.0.0.1
ws_port = 18765

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir
tls_psk_disable = true
)ini",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, ReportsLineNumberInParseErrors)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
ws_host = 127.0.0.1
invalid

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir
)ini",
        cfg);
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("line 4"), std::string::npos);
  }
}
