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

TEST(ProxyConfig, AllowsSharedDirectorNamesAcrossSelectors)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"ini(
[listen]
address = 127.0.0.1
port = 18765

[bareos-dir]
address = prod.example.test

[site-b]
address = dr.example.test
port = 29101
director_name = bareos-dir
tls_psk_disable = yes
)ini",
      cfg);

  EXPECT_EQ(cfg.bind_address, "127.0.0.1");
  EXPECT_EQ(cfg.port, 18765);
  ASSERT_EQ(cfg.configured_directors.size(), 2U);
  EXPECT_EQ(cfg.configured_directors.at("bareos-dir").address,
            "prod.example.test");
  EXPECT_EQ(cfg.configured_directors.at("bareos-dir").port, 9101);
  EXPECT_EQ(cfg.configured_directors.at("bareos-dir").name, "bareos-dir");
  EXPECT_EQ(cfg.configured_directors.at("site-b").address, "dr.example.test");
  EXPECT_EQ(cfg.configured_directors.at("site-b").port, 29101);
  EXPECT_EQ(cfg.configured_directors.at("site-b").name, "bareos-dir");
  EXPECT_TRUE(cfg.configured_directors.at("site-b").tls_psk_disable);
}

TEST(ProxyConfig, RejectsDuplicateSectionNames)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[bareos-dir]
address = prod.example.test

[bareos-dir]
address = dr.example.test
)ini",
                   cfg),
               ProxyConfigDuplicateSectionError);
}

TEST(ProxyConfig, RejectsEmptySectionNames)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[]
address = prod.example.test
)ini",
                   cfg),
               ProxyConfigParseError);
}

TEST(ProxyConfig, RejectsMissingDirectorSections)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
address = 127.0.0.1
)ini",
                   cfg),
               ProxyConfigMissingDirectorSectionError);
}

TEST(ProxyConfig, RejectsNonYesNoBooleanValues)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
address = 127.0.0.1
port = 18765

[bareos-dir]
address = prod.example.test
tls_psk_disable = true
)ini",
                   cfg),
               ProxyConfigInvalidBooleanError);
}

TEST(ProxyConfig, ReportsLineNumberInParseErrors)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
address = 127.0.0.1
invalid

[bareos-dir]
address = prod.example.test
)ini",
        cfg);
    FAIL() << "expected ProxyConfigParseError";
  } catch (const ProxyConfigParseError& error) {
    EXPECT_NE(std::string(error.what()).find("line 4"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("invalid line"),
              std::string::npos);
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

[bareos-dir]
address = prod.example.test
)ini",
        cfg);
    FAIL() << "expected ProxyConfigUnknownKeyError";
  } catch (const ProxyConfigUnknownKeyError& error) {
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
address = 127.0.0.1

[site-b]
unexpected = prod.example.test
)ini",
        cfg);
    FAIL() << "expected ProxyConfigUnknownKeyError";
  } catch (const ProxyConfigUnknownKeyError& error) {
    EXPECT_NE(std::string(error.what()).find("line 6"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("unknown key in [site-b]"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsNonAsciiValuesWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "address = 127.0.0.1\n"
        "port = 18765\n"
        "\n"
        "[bareos-dir]\n"
        "address = prod.example.test\n"
        "director_name = bar\xC3\xA4os-dir\n",
        cfg);
    FAIL() << "expected ProxyConfigInvalidCharacterError";
  } catch (const ProxyConfigInvalidCharacterError& error) {
    EXPECT_NE(std::string(error.what()).find("line 7"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("printable ASCII"),
              std::string::npos);
    EXPECT_NE(std::string(error.what()).find("value"), std::string::npos);
  }
}

TEST(ProxyConfig, RejectsNonAsciiSectionNamesWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "address = 127.0.0.1\n"
        "port = 18765\n"
        "\n"
        "[pr\xC3\xB6"
        "d]\n"
        "address = prod.example.test\n",
        cfg);
    FAIL() << "expected ProxyConfigInvalidCharacterError";
  } catch (const ProxyConfigInvalidCharacterError& error) {
    EXPECT_NE(std::string(error.what()).find("line 5"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("printable ASCII"),
              std::string::npos);
    EXPECT_NE(std::string(error.what()).find("section name"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsNonAsciiKeysWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "addr\xC3\xA9"
        "ss = 127.0.0.1\n"
        "port = 18765\n"
        "\n"
        "[bareos-dir]\n"
        "address = prod.example.test\n",
        cfg);
    FAIL() << "expected ProxyConfigInvalidCharacterError";
  } catch (const ProxyConfigInvalidCharacterError& error) {
    EXPECT_NE(std::string(error.what()).find("line 2"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("printable ASCII"),
              std::string::npos);
    EXPECT_NE(std::string(error.what()).find("key"), std::string::npos);
  }
}

TEST(ProxyConfig, RejectsNonIntegerValues)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
address = 127.0.0.1
port = invalid

[bareos-dir]
address = prod.example.test
)ini",
                   cfg),
               ProxyConfigInvalidIntegerError);
}

TEST(ProxyConfig, ParsesEscapedQuotedValues)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"ini(
[listen]
address = "127.0.0.1"
port = 18765

[bareos-dir]
address = "prod\"backup\\node"
director_name = "bareos-dir"
)ini",
      cfg);

  ASSERT_EQ(cfg.configured_directors.size(), 1U);
  EXPECT_EQ(cfg.bind_address, "127.0.0.1");
  EXPECT_EQ(cfg.configured_directors.at("bareos-dir").address,
            "prod\"backup\\node");
  EXPECT_EQ(cfg.configured_directors.at("bareos-dir").name, "bareos-dir");
}

TEST(ProxyConfig, RejectsUnmatchedQuotedValuesWithLineNumber)
{
  ProxyConfig cfg;

  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
address = "127.0.0.1
port = 18765

[bareos-dir]
address = prod.example.test
)ini",
        cfg);
    FAIL() << "expected ProxyConfigQuotedValueError";
  } catch (const ProxyConfigQuotedValueError& error) {
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
address = 127.0.0.1
port = 18765

[bareos-dir]
address = "prod\backup"
)ini",
        cfg);
    FAIL() << "expected ProxyConfigQuotedValueError";
  } catch (const ProxyConfigQuotedValueError& error) {
    EXPECT_NE(std::string(error.what()).find("line 7"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("invalid escape"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsMissingFilesWithTypedError)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFile("/definitely/missing/proxy.ini", cfg),
               ProxyConfigFileError);
}
