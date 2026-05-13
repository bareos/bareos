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

TEST(ProxyConfig, RejectsUnknownListenKeysWithConsistentError)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
unexpected = 127.0.0.1

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir
)ini",
        cfg);
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("line 3"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("unknown key in [listen]"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsUnknownDirectorKeysWithConsistentError)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
ws_host = 127.0.0.1

[director:prod]
unexpected = prod.example.test
port = 19101
director_name = bareos-dir
)ini",
        cfg);
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("line 6"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("unknown key in [director]"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsNonAsciiValuesWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "ws_host = 127.0.0.1\n"
        "ws_port = 18765\n"
        "\n"
        "[director:prod]\n"
        "host = prod.example.test\n"
        "port = 19101\n"
        "director_name = bar\xC3\xA4os-dir\n",
        cfg);
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("line 8"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("printable ASCII"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsNonAsciiSectionNamesWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "ws_host = 127.0.0.1\n"
        "ws_port = 18765\n"
        "\n"
        "[director:pr\xC3\xB6"
        "d]\n"
        "host = prod.example.test\n"
        "port = 19101\n"
        "director_name = bareos-dir\n",
        cfg);
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("line 5"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("printable ASCII"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsNonAsciiKeysWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "ws_h\xC3\xB6"
        "st = 127.0.0.1\n"
        "ws_port = 18765\n"
        "\n"
        "[director:prod]\n"
        "host = prod.example.test\n"
        "port = 19101\n"
        "director_name = bareos-dir\n",
        cfg);
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("line 2"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("printable ASCII"),
              std::string::npos);
  }
}

TEST(ProxyConfig, ParsesEscapedQuotedValues)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"ini(
[listen]
ws_host = "127.0.0.1"
ws_port = 18765

[director:prod]
host = "prod\"backup\\node"
port = 19101
director_name = "bareos-dir"
)ini",
      cfg);

  ASSERT_EQ(cfg.allowed_directors.size(), 1U);
  EXPECT_EQ(cfg.bind_host, "127.0.0.1");
  EXPECT_EQ(cfg.allowed_directors.at("prod").host, "prod\"backup\\node");
  EXPECT_EQ(cfg.allowed_directors.at("prod").name, "bareos-dir");
}

TEST(ProxyConfig, RejectsUnmatchedQuotedValuesWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
ws_host = "127.0.0.1
ws_port = 18765

[director:prod]
host = prod.example.test
port = 19101
director_name = bareos-dir
)ini",
        cfg);
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("line 3"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("matching quotes"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsInvalidQuotedEscapesWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
ws_host = 127.0.0.1
ws_port = 18765

[director:prod]
host = "prod\backup"
port = 19101
director_name = bareos-dir
)ini",
        cfg);
    FAIL() << "expected runtime_error";
  } catch (const std::runtime_error& error) {
    EXPECT_NE(std::string(error.what()).find("line 7"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("invalid escape"),
              std::string::npos);
  }
}
