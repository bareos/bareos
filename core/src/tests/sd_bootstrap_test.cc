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

#include "gtest/gtest.h"

#include <vector>

#include "stored/sd_bootstrap.h"

namespace {

void ParseArgs(CLI::App& app, const std::vector<std::string>& args)
{
  std::vector<char*> argv;
  argv.reserve(args.size());
  for (const auto& arg : args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }

  app.parse(static_cast<int>(argv.size()), argv.data());
}

}  // namespace

TEST(SdBootstrap, DiscoveryModeRequiresBootstrapParameters)
{
  CLI::App app;
  InitCLIApp(app, "test app");

  storagedaemon::BootstrapModeOptions options;
  storagedaemon::AddBootstrapOptions(app, options);

  std::vector<std::string> args{"bareos-sd", "--discovery"};
  EXPECT_NO_THROW(ParseArgs(app, args));
  EXPECT_EQ(
      storagedaemon::ValidateBootstrapOptions(options),
      std::optional<std::string>{
          "--discovery requires --bootstrap-url, --bootstrap-token, and "
          "--bootstrap-session."});
}

TEST(SdBootstrap, BootstrapParametersRequireDiscoveryMode)
{
  CLI::App app;
  InitCLIApp(app, "test app");

  storagedaemon::BootstrapModeOptions options;
  storagedaemon::AddBootstrapOptions(app, options);

  std::vector<std::string> args{"bareos-sd",
                                "--bootstrap-url",
                                "http://127.0.0.1:9103"};
  EXPECT_NO_THROW(ParseArgs(app, args));
  EXPECT_EQ(
      storagedaemon::ValidateBootstrapOptions(options),
      std::optional<std::string>{
          "--bootstrap-url, --bootstrap-token, and --bootstrap-session "
          "require --discovery."});
}

TEST(SdBootstrap, DiscoveryModeAcceptsCompleteBootstrapConfiguration)
{
  CLI::App app;
  InitCLIApp(app, "test app");

  storagedaemon::BootstrapModeOptions options;
  storagedaemon::AddBootstrapOptions(app, options);

  std::vector<std::string> args{"bareos-sd",
                                "--discovery",
                                "--bootstrap-url",
                                "http://127.0.0.1:9103",
                                "--bootstrap-token",
                                "secret-token",
                                "--bootstrap-session",
                                "session-123"};
  EXPECT_NO_THROW(ParseArgs(app, args));
  EXPECT_EQ(storagedaemon::ValidateBootstrapOptions(options), std::nullopt);
  EXPECT_TRUE(options.enabled);
  EXPECT_EQ(options.bootstrap_url, "http://127.0.0.1:9103");
  EXPECT_EQ(options.bootstrap_token, "secret-token");
  EXPECT_EQ(options.bootstrap_session, "session-123");
}
