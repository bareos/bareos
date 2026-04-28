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

TEST(ProxyConfig, ParsesAllowedDirectorsFromJson)
{
  ProxyConfig cfg;

  LoadProxyConfigFromString(
      R"json({
        "ws_host": "127.0.0.1",
        "ws_port": 18765,
        "director": {
          "host": "fallback.example.test",
          "port": 19101,
          "director_name": "fallback-dir"
        },
        "default_allowed_director": "prod",
        "allowed_directors": {
          "prod": {
            "host": "prod.example.test",
            "director_name": "bareos-dir"
          },
          "dr": {
            "host": "dr.example.test",
            "port": 29101,
            "tls_psk_disable": true
          }
        }
      })json",
      cfg);

  EXPECT_EQ(cfg.bind_host, "127.0.0.1");
  EXPECT_EQ(cfg.port, 18765);
  EXPECT_EQ(cfg.default_allowed_director, "prod");
  ASSERT_EQ(cfg.allowed_directors.size(), 2U);
  EXPECT_EQ(cfg.allowed_directors.at("prod").host, "prod.example.test");
  EXPECT_EQ(cfg.allowed_directors.at("prod").port, 19101);
  EXPECT_EQ(cfg.allowed_directors.at("prod").name, "bareos-dir");
  EXPECT_EQ(cfg.allowed_directors.at("dr").host, "dr.example.test");
  EXPECT_EQ(cfg.allowed_directors.at("dr").port, 29101);
  EXPECT_EQ(cfg.allowed_directors.at("dr").name, "dr");
  EXPECT_TRUE(cfg.allowed_directors.at("dr").tls_psk_disable);
  EXPECT_FALSE(cfg.allowed_directors.at("dr").tls_psk_require);
}

TEST(ProxyConfig, RejectsUnknownDefaultAllowedDirector)
{
  ProxyConfig cfg;

  EXPECT_THROW(LoadProxyConfigFromString(
                   R"json({
                     "default_allowed_director": "prod",
                     "allowed_directors": {
                       "dr": {
                         "host": "dr.example.test"
                       }
                     }
                   })json",
                   cfg),
               std::runtime_error);
}

TEST(ProxyConfig, ResolvesAllowedDirectorById)
{
  ProxyConfig cfg;
  cfg.default_allowed_director = "prod";
  cfg.allowed_directors.emplace(
      "prod",
      DirectorTargetConfig{.host = "prod.example.test",
                           .port = 19101,
                           .name = "bareos-dir"});

  const auto target
      = ResolveDirectorTarget(cfg, std::string("prod"), std::nullopt, std::nullopt);

  EXPECT_EQ(target.host, "prod.example.test");
  EXPECT_EQ(target.port, 19101);
  EXPECT_EQ(target.name, "bareos-dir");
}

TEST(ProxyConfig, ResolvesConfiguredDirectorWithoutAllowlist)
{
  ProxyConfig cfg;
  cfg.director.host = "dir.example.test";
  cfg.director.port = 19101;
  cfg.director.name = "custom-dir";

  const auto target
      = ResolveDirectorTarget(cfg, std::string("custom-dir"), std::nullopt,
                              std::nullopt);

  EXPECT_EQ(target.host, "dir.example.test");
  EXPECT_EQ(target.port, 19101);
  EXPECT_EQ(target.name, "custom-dir");
}

TEST(ProxyConfig, RejectsBrowserHostPortOverrides)
{
  ProxyConfig cfg;

  EXPECT_THROW(ResolveDirectorTarget(cfg, std::nullopt,
                                     std::string("dir.example.test"), 19101),
               std::runtime_error);
}

TEST(ProxyConfig, RejectsUnexpectedDirectorWithoutAllowlist)
{
  ProxyConfig cfg;
  cfg.director.name = "custom-dir";

  EXPECT_THROW(ResolveDirectorTarget(cfg, std::string("other-dir"),
                                     std::nullopt, std::nullopt),
               std::runtime_error);
}
