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

#include "../proxy_log.h"

#include <unistd.h>

#include <fstream>
#include <iterator>
#include <regex>

#include <gtest/gtest.h>

namespace {

class ProxyLogFileTest : public ::testing::Test {
 protected:
  void SetUp() override
  {
    char path_template[] = "/tmp/bareos-webui-proxy-log-XXXXXX";
    const int fd = mkstemp(path_template);
    ASSERT_GE(fd, 0);
    close(fd);
    path_ = path_template;
  }

  void TearDown() override
  {
    ConfigureProxyLogger({});
    if (!path_.empty()) { unlink(path_.c_str()); }
  }

  [[nodiscard]] std::string ReadLogFile() const
  {
    std::ifstream input(path_);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
  }

  std::string path_;
};

void WriteProxyLogForTest(
    ProxyLogLevel level,
    std::string_view peer,
    std::string_view message,
    libbareos::source_location loc = libbareos::source_location::current())
{
  ProxyLogWrite(level, peer, message, loc);
}

}  // namespace

TEST(ProxyLog, ParsesLogLevelsCaseInsensitively)
{
  ProxyLogLevel level = ProxyLogLevel::Info;

  EXPECT_TRUE(ParseProxyLogLevel("DEBUG", level));
  EXPECT_EQ(level, ProxyLogLevel::Debug);
  EXPECT_TRUE(ParseProxyLogLevel("warning", level));
  EXPECT_EQ(level, ProxyLogLevel::Warn);
  EXPECT_TRUE(ParseProxyLogLevel("error", level));
  EXPECT_EQ(level, ProxyLogLevel::Error);
  EXPECT_FALSE(ParseProxyLogLevel("verbose", level));
}

TEST(ProxyLog, FormatsIso8601LocalTimestampWithMicroseconds)
{
  const auto timestamp = std::chrono::system_clock::now();
  const std::string formatted = FormatProxyLogTimestamp(timestamp);

  EXPECT_TRUE(std::regex_match(
      formatted,
      std::regex(
          R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}[+-]\d{2}:\d{2}$)")));
}

TEST_F(ProxyLogFileTest, WritesLogLineWithSourceLocationAndPeer)
{
  ProxyLoggerConfig cfg;
  cfg.log_file = path_;
  cfg.log_to_stderr = false;
  ConfigureProxyLogger(cfg);

  WriteProxyLogForTest(ProxyLogLevel::Info, "127.0.0.1:12345",
                       "session started");

  const std::string output = ReadLogFile();
  EXPECT_TRUE(std::regex_search(
      output,
      std::regex(
          R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{6}[+-]\d{2}:\d{2} \[proxy\] \[INFO\] \[test_proxy_log\.cc:\d+\] \[127\.0\.0\.1:12345\] session started\n$)")));
}

TEST_F(ProxyLogFileTest, HonorsMinimumLogLevel)
{
  ProxyLoggerConfig cfg;
  cfg.min_level = ProxyLogLevel::Warn;
  cfg.log_file = path_;
  cfg.log_to_stderr = false;
  ConfigureProxyLogger(cfg);

  WriteProxyLogForTest(ProxyLogLevel::Info, "", "suppressed");
  WriteProxyLogForTest(ProxyLogLevel::Error, "", "emitted");

  const std::string output = ReadLogFile();
  EXPECT_EQ(output.find("suppressed"), std::string::npos);
  EXPECT_NE(output.find("emitted"), std::string::npos);
}
