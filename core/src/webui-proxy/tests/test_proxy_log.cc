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

#include <cctype>
#include <unistd.h>

#include <fstream>
#include <iterator>

#include <gtest/gtest.h>

namespace {

bool IsAllDigits(std::string_view text)
{
  return !text.empty()
         && std::all_of(text.begin(), text.end(),
                        [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

bool HasIso8601LocalTimestampFormat(std::string_view text)
{
  return text.size() == 32 && IsAllDigits(text.substr(0, 4)) && text[4] == '-'
         && IsAllDigits(text.substr(5, 2)) && text[7] == '-'
         && IsAllDigits(text.substr(8, 2)) && text[10] == 'T'
         && IsAllDigits(text.substr(11, 2)) && text[13] == ':'
         && IsAllDigits(text.substr(14, 2)) && text[16] == ':'
         && IsAllDigits(text.substr(17, 2)) && text[19] == '.'
         && IsAllDigits(text.substr(20, 6))
         && (text[26] == '+' || text[26] == '-')
         && IsAllDigits(text.substr(27, 2)) && text[29] == ':'
         && IsAllDigits(text.substr(30, 2));
}

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

void WriteProxyLogForTest(ProxyLogLevel level,
                          std::string_view peer,
                          std::string_view message,
                          libbareos::source_location loc
                          = libbareos::source_location::current())
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

  EXPECT_TRUE(HasIso8601LocalTimestampFormat(formatted));
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
  ASSERT_GE(output.size(), 32u);
  EXPECT_TRUE(HasIso8601LocalTimestampFormat(output.substr(0, 32)));

  constexpr std::string_view kPrefix = " [proxy] [INFO] [test_proxy_log.cc:";
  constexpr std::string_view kSuffix = "] [127.0.0.1:12345] session started\n";

  const auto prefix_pos = output.find(kPrefix, 32);
  ASSERT_EQ(prefix_pos, 32u);

  const auto line_number_pos = prefix_pos + kPrefix.size();
  const auto suffix_pos = output.find(kSuffix, line_number_pos);
  ASSERT_NE(suffix_pos, std::string::npos);
  EXPECT_TRUE(IsAllDigits(std::string_view(output).substr(
      line_number_pos, suffix_pos - line_number_pos)));
  EXPECT_EQ(suffix_pos + kSuffix.size(), output.size());
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

TEST_F(ProxyLogFileTest, TreatsLiteralPercentTextAsMessageWithoutArgs)
{
  ProxyLoggerConfig cfg;
  cfg.log_file = path_;
  cfg.log_to_stderr = false;
  ConfigureProxyLogger(cfg);

  ProxyLogFormat(ProxyLogLevel::Info, "", libbareos::source_location::current(),
                 "100% literal %s text");

  const std::string output = ReadLogFile();
  EXPECT_NE(output.find("100% literal %s text"), std::string::npos);
}
