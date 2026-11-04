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
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

TEST(ProxyConfig, AllowsSharedDirectorNamesAcrossSelectors)
{
  ProxyConfig cfg = LoadProxyConfigFromString(
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
)ini");

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

TEST(ProxyConfig, LoadsServerSidePamConsoleAndQuotedSecret)
{
  const auto config = LoadProxyConfigFromString(R"ini(
[listen]
[pam-target]
pam_console_name = "PamConsole"
pam_console_password = "secret#;with\"quote\\slash" # trailing comment
[ordinary-target]
address = localhost
)ini");
  const auto& pam = config.configured_directors.at("pam-target").pam_console;
  ASSERT_TRUE(pam);
  EXPECT_EQ(pam->name, "PamConsole");
  EXPECT_EQ(pam->password, "secret#;with\"quote\\slash");
  EXPECT_FALSE(config.configured_directors.at("ordinary-target").pam_console);
}

TEST(ProxyConfig, RejectsIncompleteOrUnencryptedPamConsoleConfiguration)
{
  for (const std::string settings :
       {"pam_console_name = PamConsole\n",
        "pam_console_password = private-secret\n",
        "pam_console_name = \"\"\npam_console_password = private-secret\n",
        "pam_console_name = PamConsole\npam_console_password = \"\"\n",
        "pam_console_name = PamConsole\npam_console_password = private-secret\n"
        "tls_psk_disable = yes\n"}) {
    try {
      LoadProxyConfigFromString("[listen]\n[pam-target]\n" + settings);
      ADD_FAILURE() << "expected invalid PAM configuration to be rejected";
    } catch (const ProxyConfigParseError& error) {
      const std::string message = error.what();
      EXPECT_NE(message.find("[pam-target]"), std::string::npos);
      EXPECT_EQ(message.find("private-secret"), std::string::npos);
    }
  }
}

TEST(ProxyConfig, RejectsDuplicateSectionNames)
{
  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[bareos-dir]
address = prod.example.test

[bareos-dir]
address = dr.example.test
)ini"),
               ProxyConfigDuplicateSectionError);
}

TEST(ProxyConfig, RejectsEmptySectionNames)
{
  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[]
address = prod.example.test
)ini"),
               ProxyConfigParseError);
}

TEST(ProxyConfig, RejectsMissingDirectorSections)
{
  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
address = 127.0.0.1
)ini"),
               ProxyConfigMissingDirectorSectionError);
}

TEST(ProxyConfig, RejectsMissingListenSection)
{
  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listem]
address = localhost
port = 9204

[bareos-dir]
address = localhost
tls_psk_disable = no
)ini"),
               ProxyConfigMissingListenSectionError);
}

TEST(ProxyConfig, RejectsNonYesNoBooleanValues)
{
  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
address = 127.0.0.1
port = 18765

[bareos-dir]
address = prod.example.test
tls_psk_disable = true
)ini"),
               ProxyConfigInvalidBooleanError);
}

TEST(ProxyConfig, ReportsLineNumberInParseErrors)
{
  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
address = 127.0.0.1
invalid

[bareos-dir]
address = prod.example.test
)ini");
    FAIL() << "expected ProxyConfigParseError";
  } catch (const ProxyConfigParseError& error) {
    EXPECT_NE(std::string(error.what()).find("line 4"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("invalid line"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsUnknownListenKeysWithConsistentError)
{
  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
unexpected = 127.0.0.1

[bareos-dir]
address = prod.example.test
)ini");
    FAIL() << "expected ProxyConfigUnknownKeyError";
  } catch (const ProxyConfigUnknownKeyError& error) {
    EXPECT_NE(std::string(error.what()).find("line 3"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("unknown key in [listen]"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsUnknownDirectorKeysWithConsistentError)
{
  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
address = 127.0.0.1

[site-b]
unexpected = prod.example.test
)ini");
    FAIL() << "expected ProxyConfigUnknownKeyError";
  } catch (const ProxyConfigUnknownKeyError& error) {
    EXPECT_NE(std::string(error.what()).find("line 6"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("unknown key in [site-b]"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsNonAsciiValuesWithLineNumber)
{
  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "address = 127.0.0.1\n"
        "port = 18765\n"
        "\n"
        "[bareos-dir]\n"
        "address = prod.example.test\n"
        "director_name = bar\xC3\xA4os-dir\n");
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
  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "address = 127.0.0.1\n"
        "port = 18765\n"
        "\n"
        "[pr\xC3\xB6"
        "d]\n"
        "address = prod.example.test\n");
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
  try {
    LoadProxyConfigFromString(
        "[listen]\n"
        "addr\xC3\xA9"
        "ss = 127.0.0.1\n"
        "port = 18765\n"
        "\n"
        "[bareos-dir]\n"
        "address = prod.example.test\n");
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
  EXPECT_THROW(LoadProxyConfigFromString(
                   R"ini(
[listen]
address = 127.0.0.1
port = invalid

[bareos-dir]
address = prod.example.test
)ini"),
               ProxyConfigInvalidIntegerError);
}

TEST(ProxyConfig, ParsesEscapedQuotedValues)
{
  ProxyConfig cfg = LoadProxyConfigFromString(
      R"ini(
[listen]
address = "127.0.0.1"
port = 18765

[bareos-dir]
address = "prod\"backup\\node"
director_name = "bareos-dir"
)ini");

  ASSERT_EQ(cfg.configured_directors.size(), 1U);
  EXPECT_EQ(cfg.bind_address, "127.0.0.1");
  EXPECT_EQ(cfg.configured_directors.at("bareos-dir").address,
            "prod\"backup\\node");
  EXPECT_EQ(cfg.configured_directors.at("bareos-dir").name, "bareos-dir");
}

TEST(ProxyConfig, RejectsUnmatchedQuotedValuesWithLineNumber)
{
  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
address = "127.0.0.1
port = 18765

[bareos-dir]
address = prod.example.test
)ini");
    FAIL() << "expected ProxyConfigQuotedValueError";
  } catch (const ProxyConfigQuotedValueError& error) {
    EXPECT_NE(std::string(error.what()).find("line 3"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("matching quotes"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsInvalidQuotedEscapesWithLineNumber)
{
  try {
    LoadProxyConfigFromString(
        R"ini(
[listen]
address = 127.0.0.1
port = 18765

[bareos-dir]
address = "prod\backup"
)ini");
    FAIL() << "expected ProxyConfigQuotedValueError";
  } catch (const ProxyConfigQuotedValueError& error) {
    EXPECT_NE(std::string(error.what()).find("line 7"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("invalid escape"),
              std::string::npos);
  }
}

TEST(ProxyConfig, RejectsMissingFilesWithTypedError)
{
  EXPECT_THROW(LoadProxyConfigFile("/definitely/missing/proxy.ini"),
               ProxyConfigFileError);
}

TEST(ProxyConfig, EnforcesFileAndDirectoryPermissions)
{
  auto pattern = (std::filesystem::temp_directory_path()
                  / "bareos-proxy-permissions-XXXXXX")
                     .string();
  ASSERT_NE(mkdtemp(pattern.data()), nullptr);
  const std::filesystem::path directory(pattern);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() { std::filesystem::remove_all(path); }
  } cleanup{directory};
  ASSERT_EQ(chown(directory.c_str(), -1, getegid()), 0);
  const auto file = directory / "proxy.ini";
  {
    std::ofstream output(file);
    output << "[listen]\n[pam]\npam_console_name = PamConsole\n"
              "pam_console_password = private-secret\n";
  }
  ASSERT_EQ(chown(file.c_str(), -1, getegid()), 0);
  for (const mode_t mode : {0600, 0640, 0400}) {
    ASSERT_EQ(chmod(file.c_str(), mode), 0);
    EXPECT_NO_THROW(LoadProxyConfigFile(file.string()));
  }
  for (const mode_t mode :
       {0644, 0660, 0604, 0700, 0670, 04640, 02640, 01640}) {
    ASSERT_EQ(chmod(file.c_str(), mode), 0);
    try {
      LoadProxyConfigFile(file.string());
      ADD_FAILURE() << "unsafe file permissions accepted";
    } catch (const ProxyConfigFileError& error) {
      EXPECT_NE(std::string(error.what()).find("unsafe configuration file"),
                std::string::npos);
      EXPECT_NE(std::string(error.what()).find(file.string()),
                std::string::npos);
      EXPECT_NE(std::string(error.what()).find("mode "), std::string::npos);
      EXPECT_NE(std::string(error.what()).find("uid="), std::string::npos);
      EXPECT_NE(std::string(error.what()).find("gid="), std::string::npos);
      EXPECT_NE(std::string(error.what()).find("chmod 0640"),
                std::string::npos);
      EXPECT_EQ(std::string(error.what()).find("private-secret"),
                std::string::npos);
    }
  }
  ASSERT_EQ(chmod(file.c_str(), 0640), 0);
  for (const mode_t mode : {0700, 0750}) {
    ASSERT_EQ(chmod(directory.c_str(), mode), 0);
    EXPECT_NO_THROW(LoadProxyConfigFile(file.string()));
  }
  for (const mode_t mode : {0755, 0770, 0701, 01750}) {
    ASSERT_EQ(chmod(directory.c_str(), mode), 0);
    try {
      LoadProxyConfigFile(file.string());
      ADD_FAILURE() << "unsafe directory permissions accepted";
    } catch (const ProxyConfigFileError& error) {
      const std::string message = error.what();
      EXPECT_NE(message.find("unsafe configuration directory"),
                std::string::npos);
      EXPECT_NE(message.find(directory.string()), std::string::npos);
      EXPECT_NE(message.find("chmod 0750"), std::string::npos);
      EXPECT_EQ(message.find("private-secret"), std::string::npos);
    }
  }
  ASSERT_EQ(chmod(directory.c_str(), 0700), 0);
  const auto link = directory / "linked.ini";
  std::filesystem::create_symlink(file, link);
  EXPECT_THROW(LoadProxyConfigFile(link.string()), ProxyConfigFileError);
  if (geteuid() != 0) {
    ASSERT_EQ(chmod(file.c_str(), 0000), 0);
    try {
      LoadProxyConfigFile(file.string());
      ADD_FAILURE() << "unreadable file accepted";
    } catch (const ProxyConfigFileError& error) {
      const std::string message = error.what();
      EXPECT_NE(message.find(file.string()), std::string::npos);
      EXPECT_NE(message.find("service file read permissions"),
                std::string::npos);
      EXPECT_EQ(message.find("private-secret"), std::string::npos);
    }
    ASSERT_EQ(chmod(file.c_str(), 0640), 0);
    ASSERT_EQ(chmod(directory.c_str(), 0000), 0);
    EXPECT_THROW(LoadProxyConfigFile(file.string()), ProxyConfigFileError);
    ASSERT_EQ(chmod(directory.c_str(), 0700), 0);
  }
  if (geteuid() == 0) {
    ASSERT_EQ(chown(file.c_str(), 1, getegid()), 0);
    EXPECT_THROW(LoadProxyConfigFile(file.string()), ProxyConfigFileError);
    ASSERT_EQ(chown(file.c_str(), 0, getegid()), 0);
    ASSERT_EQ(chown(directory.c_str(), 1, getegid()), 0);
    EXPECT_THROW(LoadProxyConfigFile(file.string()), ProxyConfigFileError);
    ASSERT_EQ(chown(directory.c_str(), 0, getegid()), 0);
    ASSERT_EQ(chown(file.c_str(), 0, getegid() + 1), 0);
    EXPECT_THROW(LoadProxyConfigFile(file.string()), ProxyConfigFileError);
    ASSERT_EQ(chown(file.c_str(), 0, getegid()), 0);
  }
  {
    std::ofstream output(file);
    output << "[listen]\n[ordinary]\naddress = localhost\n";
  }
  ASSERT_EQ(chmod(file.c_str(), 0644), 0);
  ASSERT_EQ(chmod(directory.c_str(), 0755), 0);
  EXPECT_THROW(LoadProxyConfigFile(file.string()), ProxyConfigFileError);
  ASSERT_EQ(chmod(file.c_str(), 0640), 0);
  EXPECT_THROW(LoadProxyConfigFile(file.string()), ProxyConfigFileError);
  ASSERT_EQ(chmod(directory.c_str(), 0750), 0);
  EXPECT_NO_THROW(LoadProxyConfigFile(file.string()));
}

TEST(ProxyConfig, ParsesMaxUnauthenticatedConnections)
{
  ProxyConfig cfg = LoadProxyConfigFromString(
      R"ini(
[listen]
address = 127.0.0.1
port = 18765
max_unauthenticated_connections = 250

[bareos-dir]
address = prod.example.test
)ini");

  EXPECT_EQ(cfg.max_unauthenticated_connections, 250);
}

TEST(ProxyConfig, DefaultMaxUnauthenticatedConnections)
{
  ProxyConfig cfg = LoadProxyConfigFromString(
      R"ini(
[listen]
address = 127.0.0.1

[bareos-dir]
address = prod.example.test
)ini");

  EXPECT_EQ(cfg.max_unauthenticated_connections, 100);
}

TEST(ProxyConfig, BuiltInDefaultsMatchTemplateDefaults)
{
  ProxyConfig cfg = LoadBuiltInDefaultProxyConfig();

  EXPECT_EQ(cfg.bind_address, "localhost");
  EXPECT_EQ(cfg.port, 9104);
  EXPECT_EQ(cfg.session_idle_timeout_minutes, 30);
  EXPECT_EQ(cfg.session_absolute_lifetime_hours, 8);
  EXPECT_EQ(cfg.max_unauthenticated_connections, 100);
  ASSERT_EQ(cfg.configured_directors.size(), 1U);
  ASSERT_TRUE(cfg.configured_directors.contains("bareos-dir"));

  const auto& director_cfg = cfg.configured_directors.at("bareos-dir");
  EXPECT_EQ(director_cfg.address, "localhost");
  EXPECT_EQ(director_cfg.port, 9101);
  EXPECT_EQ(director_cfg.name, "bareos-dir");
  EXPECT_FALSE(director_cfg.tls_psk_disable);
}
