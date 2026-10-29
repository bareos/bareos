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

#include "gtest/gtest.h"
#include "include/bareos.h"

#include "lib/lex.h"

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct LexedToken {
  int type;
  std::string text;
};

int lex_error_count = 0;

void TestLexErrorHandler(const char* file,
                         int line,
                         lexer* lc,
                         const char* msg,
                         ...)
{
  (void)file;
  (void)line;
  (void)lc;
  (void)msg;
  ++lex_error_count;
}

std::vector<LexedToken> LexInput(const std::string& filename)
{
  lex_error_count = 0;
  std::vector<LexedToken> tokens;

  lexer* lf = lex_open_file(nullptr, filename.c_str(), TestLexErrorHandler,
                            TestLexErrorHandler);
  if (lf == nullptr) { return tokens; }

  int token;
  while ((token = LexGetToken(lf, BCT_ALL)) != BCT_EOF) {
    tokens.push_back({token, lf->str ? std::string(lf->str) : std::string()});
    if (token == BCT_ERROR) { break; }
  }

  while (lf) { lf = LexCloseFile(lf); }

  return tokens;
}

int CountToken(const std::vector<LexedToken>& tokens, int type)
{
  int count = 0;
  for (const auto& t : tokens) {
    if (t.type == type) { ++count; }
  }
  return count;
}

std::vector<std::string> QuotedStrings(const std::vector<LexedToken>& tokens)
{
  std::vector<std::string> result;
  for (const auto& t : tokens) {
    if (t.type == BCT_QUOTED_STRING) { result.push_back(t.text); }
  }
  return result;
}

std::string WriteTempFile(const std::string& name, const std::string& content)
{
  std::ofstream out(name, std::ios::binary | std::ios::trunc);
  out << content;
  out.close();
  return name;
}

const char* FilesetConfig = R"(FileSet {
  Name=DB-dba
  Plugin = "mssqlvdi:database=dba:username=bareos:password=password"
}
FileSet {
  Name=DB-dbb
}
)";

}  // namespace

TEST(Lex, multiline_quoted_string)
{
  const std::string path = WriteTempFile("test_lex_multiline.conf",
                                         R"(Name = "foo"
                "bar"
)");

  auto tokens = LexInput(path);
  std::remove(path.c_str());

  EXPECT_EQ(lex_error_count, 0);
  auto strings = QuotedStrings(tokens);
  ASSERT_EQ(strings.size(), 1u);
  EXPECT_EQ(strings[0], "foobar");
}

TEST(Lex, quoted_string_before_block_end)
{
  const std::string path
      = WriteTempFile("test_lex_block_end.conf", FilesetConfig);

  auto tokens = LexInput(path);
  std::remove(path.c_str());

  EXPECT_EQ(lex_error_count, 0);
  EXPECT_EQ(CountToken(tokens, BCT_BOB), 2);
  EXPECT_EQ(CountToken(tokens, BCT_EOB), 2);

  auto strings = QuotedStrings(tokens);
  ASSERT_EQ(strings.size(), 1u);
  EXPECT_EQ(strings[0],
            "mssqlvdi:database=dba:username=bareos:password=password");
}

#if !HAVE_WIN32
TEST(Lex, pipe_does_not_lose_lines)
{
  const std::string path
      = WriteTempFile("test_lex_pipe_input.conf", FilesetConfig);

  auto tokens = LexInput("|cat " + path);
  std::remove(path.c_str());

  EXPECT_EQ(lex_error_count, 0);
  EXPECT_EQ(CountToken(tokens, BCT_BOB), 2);
  EXPECT_EQ(CountToken(tokens, BCT_EOB), 2);

  auto strings = QuotedStrings(tokens);
  ASSERT_EQ(strings.size(), 1u);
  EXPECT_EQ(strings[0],
            "mssqlvdi:database=dba:username=bareos:password=password");
}
#endif
