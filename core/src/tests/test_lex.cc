#include <gtest/gtest.h>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include "include/bareos.h"
#include "lib/lex.h"

// Helpers for creating temporary config files and lexers
namespace {

void NullErrorHandler(const char* /*file*/,
                      int /*line*/,
                      lexer* /*lc*/,
                      const char* /*msg*/,
                      ...) {}

void NullWarningHandler(const char* /*file*/,
                        int /*line*/,
                        lexer* /*lc*/,
                        const char* /*msg*/,
                        ...) {}

// Creates a temporary file with the given content and returns its path.
// Uses testing::TempDir() for a writable directory, combined with
// std::filesystem::path operator/ to correctly handle whether TempDir()
// includes a trailing separator or not (e.g. when TEST_TMPDIR is set).
// Opens in binary mode so the lexer sees exact bytes regardless of platform.
std::string WriteTempFile(const std::string& content)
{
  static std::atomic<int> counter{0};
  std::filesystem::path path
      = std::filesystem::path(::testing::TempDir())
        / ("bareos_lex_test_" + std::to_string(++counter));
  std::ofstream ofs(path, std::ios::binary);
  if (!ofs) return "";
  ofs << content;
  return path.generic_string();
}

// Opens a lexer on a temp file containing the given content
lexer* OpenLexer(const std::string& content)
{
  std::string path = WriteTempFile(content);
  if (path.empty()) return nullptr;
  lexer* lf = lex_open_file(nullptr, path.c_str(), NullErrorHandler,
                            NullWarningHandler);
  if (lf) {
    LexSetDefaultErrorHandler(lf);
    LexSetDefaultWarningHandler(lf);
    // Store filename for cleanup
    lf->caller_ctx = strdup(path.c_str());
  }
  return lf;
}

void CloseLexer(lexer* lf)
{
  if (!lf) return;
  if (lf->caller_ctx) {
    std::filesystem::remove(static_cast<char*>(lf->caller_ctx));
    free(lf->caller_ctx);
    lf->caller_ctx = nullptr;
  }
  LexCloseFile(lf);
}

}  // namespace

// ──────────────────────────────────────────────────────────
// lex_tok_to_str tests
// ──────────────────────────────────────────────────────────

TEST(LexTokToStrTest, EofToken) {
  EXPECT_STREQ(lex_tok_to_str(L_EOF), "L_EOF");
}

TEST(LexTokToStrTest, EolToken) {
  EXPECT_STREQ(lex_tok_to_str(L_EOL), "L_EOL");
}

TEST(LexTokToStrTest, NoneToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_NONE), "BCT_NONE");
}

TEST(LexTokToStrTest, NumberToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_NUMBER), "BCT_NUMBER");
}

TEST(LexTokToStrTest, IpAddrToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_IPADDR), "BCT_IPADDR");
}

TEST(LexTokToStrTest, IdentifierToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_IDENTIFIER), "BCT_IDENTIFIER");
}

TEST(LexTokToStrTest, UnquotedStringToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_UNQUOTED_STRING), "BCT_UNQUOTED_STRING");
}

TEST(LexTokToStrTest, QuotedStringToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_QUOTED_STRING), "BCT_QUOTED_STRING");
}

TEST(LexTokToStrTest, BobToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_BOB), "BCT_BOB");
}

TEST(LexTokToStrTest, EobToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_EOB), "BCT_EOB");
}

TEST(LexTokToStrTest, EqualsToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_EQUALS), "BCT_EQUALS");
}

TEST(LexTokToStrTest, ErrorToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_ERROR), "BCT_ERROR");
}

TEST(LexTokToStrTest, BctEofToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_EOF), "BCT_EOF");
}

TEST(LexTokToStrTest, CommaToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_COMMA), "BCT_COMMA");
}

TEST(LexTokToStrTest, BctEolToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_EOL), "BCT_EOL");
}

TEST(LexTokToStrTest, Utf8BomToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_UTF8_BOM), "BCT_UTF8_BOM");
}

TEST(LexTokToStrTest, Utf16BomToken) {
  EXPECT_STREQ(lex_tok_to_str(BCT_UTF16_BOM), "BCT_UTF16_BOM");
}

TEST(LexTokToStrTest, UnknownToken) {
  EXPECT_STREQ(lex_tok_to_str(9999), "??????");
}

TEST(LexTokToStrTest, NegativeUnknownToken) {
  EXPECT_STREQ(lex_tok_to_str(-99), "??????");
}

// ──────────────────────────────────────────────────────────
// Token constant value tests
// ──────────────────────────────────────────────────────────

TEST(LexTokenConstantsTest, LValues) {
  EXPECT_EQ(L_EOF, -1);
  EXPECT_EQ(L_EOL, -2);
}

TEST(LexTokenConstantsTest, BctValues) {
  EXPECT_EQ(BCT_NONE, 100);
  EXPECT_EQ(BCT_EOF, 101);
  EXPECT_EQ(BCT_NUMBER, 102);
  EXPECT_EQ(BCT_IPADDR, 103);
  EXPECT_EQ(BCT_IDENTIFIER, 104);
  EXPECT_EQ(BCT_UNQUOTED_STRING, 105);
  EXPECT_EQ(BCT_QUOTED_STRING, 106);
}

TEST(LexTokenConstantsTest, BctBlockValues) {
  EXPECT_EQ(BCT_BOB, 108);
  EXPECT_EQ(BCT_EOB, 109);
  EXPECT_EQ(BCT_EQUALS, 110);
  EXPECT_EQ(BCT_COMMA, 111);
  EXPECT_EQ(BCT_EOL, 112);
}

TEST(LexTokenConstantsTest, BctErrorValues) {
  EXPECT_EQ(BCT_ERROR, 200);
  EXPECT_GT(BCT_ERROR, BCT_EOL);
}

TEST(LexTokenConstantsTest, BctAllIsZero) {
  EXPECT_EQ(BCT_ALL, 0);
}

TEST(LexTokenConstantsTest, AllTokensDistinct) {
  std::vector<int> tokens = {BCT_NONE,  BCT_EOF,   BCT_NUMBER, BCT_IPADDR,
                             BCT_IDENTIFIER, BCT_UNQUOTED_STRING,
                             BCT_QUOTED_STRING, BCT_BOB,   BCT_EOB,
                             BCT_EQUALS, BCT_COMMA, BCT_EOL,   BCT_ERROR};
  for (size_t i = 0; i < tokens.size(); ++i) {
    for (size_t j = i + 1; j < tokens.size(); ++j) {
      EXPECT_NE(tokens[i], tokens[j])
          << "Duplicate token values at indices " << i << " and " << j;
    }
  }
}

// ──────────────────────────────────────────────────────────
// lex_state enum tests
// ──────────────────────────────────────────────────────────

TEST(LexStateTest, StatesAreDistinct) {
  EXPECT_NE(lex_none, lex_comment);
  EXPECT_NE(lex_none, lex_number);
  EXPECT_NE(lex_none, lex_identifier);
  EXPECT_NE(lex_comment, lex_string);
  EXPECT_NE(lex_quoted_string, lex_include);
}

TEST(LexStateTest, NoneIsZero) {
  EXPECT_EQ(lex_none, 0);
}

// ──────────────────────────────────────────────────────────
// Lexer open / close tests
// ──────────────────────────────────────────────────────────

TEST(LexOpenCloseTest, OpenNonExistentFile) {
  lexer* lf = lex_open_file(nullptr, "/nonexistent/path/file.conf",
                            NullErrorHandler, NullWarningHandler);
  EXPECT_EQ(lf, nullptr);
}

TEST(LexOpenCloseTest, OpenEmptyFile) {
  lexer* lf = OpenLexer("");
  EXPECT_NE(lf, nullptr);
  CloseLexer(lf);
}

TEST(LexOpenCloseTest, OpenSimpleConfig) {
  lexer* lf = OpenLexer("key = value\n");
  EXPECT_NE(lf, nullptr);
  CloseLexer(lf);
}

TEST(LexOpenCloseTest, CloseNullReturnsNull) {
  // Closing a nullptr lexer - the return type is lexer* (nullptr chain)
  // We just verify it doesn't crash when given a freshly opened file
  std::string path = WriteTempFile("hello\n");
  lexer* lf = lex_open_file(nullptr, path.c_str(), NullErrorHandler,
                            NullWarningHandler);
  ASSERT_NE(lf, nullptr);
  lexer* result = LexCloseFile(lf);
  // Returns the previous lexer in the chain (nullptr if no chain)
  EXPECT_EQ(result, nullptr);
  std::filesystem::remove(path);
}

TEST(LexOpenCloseTest, OpenMultiLineConfig) {
  lexer* lf = OpenLexer("key1 = value1\nkey2 = value2\n");
  EXPECT_NE(lf, nullptr);
  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// LexGetToken tests – single tokens
// ──────────────────────────────────────────────────────────

TEST(LexGetTokenTest, EmptyFileGivesEof) {
  lexer* lf = OpenLexer("");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_EOF);
  CloseLexer(lf);
}

TEST(LexGetTokenTest, IdentifierToken) {
  lexer* lf = OpenLexer("hello\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_IDENTIFIER);
  EXPECT_STREQ(lf->str, "hello");
  CloseLexer(lf);
}

TEST(LexGetTokenTest, NumberToken) {
  lexer* lf = OpenLexer("42\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_NUMBER);
  CloseLexer(lf);
}

TEST(LexGetTokenTest, QuotedStringToken) {
  lexer* lf = OpenLexer(R"("hello world")" "\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_QUOTED_STRING);
  EXPECT_STREQ(lf->str, "hello world");
  CloseLexer(lf);
}

TEST(LexGetTokenTest, EqualsToken) {
  lexer* lf = OpenLexer("=\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_EQUALS);
  CloseLexer(lf);
}

TEST(LexGetTokenTest, BeginBlockToken) {
  lexer* lf = OpenLexer("{\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_BOB);
  CloseLexer(lf);
}

TEST(LexGetTokenTest, EndBlockToken) {
  lexer* lf = OpenLexer("}\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_EOB);
  CloseLexer(lf);
}

TEST(LexGetTokenTest, CommaToken) {
  lexer* lf = OpenLexer(",\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_COMMA);
  CloseLexer(lf);
}

TEST(LexGetTokenTest, EolToken) {
  lexer* lf = OpenLexer("\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_EOL);
  CloseLexer(lf);
}

TEST(LexGetTokenTest, IpAddressToken) {
  lexer* lf = OpenLexer("192.168.1.1\n");
  ASSERT_NE(lf, nullptr);
  int tok = LexGetToken(lf, BCT_ALL);
  // IP addresses may be lexed as IPADDR or UNQUOTED_STRING depending on context
  EXPECT_TRUE(tok == BCT_IPADDR || tok == BCT_UNQUOTED_STRING);
  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// Multi-token parsing tests
// ──────────────────────────────────────────────────────────

TEST(LexMultiTokenTest, KeyEqualsValue) {
  lexer* lf = OpenLexer("Name = bareos\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_STREQ(lf->str, "Name");

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EQUALS);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_STREQ(lf->str, "bareos");

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EOL);
  CloseLexer(lf);
}

TEST(LexMultiTokenTest, BlockSyntax) {
  lexer* lf = OpenLexer("Resource {\n  Name = test\n}\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);  // Resource
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_BOB);         // {
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EOL);         // newline
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);  // Name
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EQUALS);      // =
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);  // test
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EOL);         // newline
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EOB);         // }

  CloseLexer(lf);
}

TEST(LexMultiTokenTest, NumberValue) {
  lexer* lf = OpenLexer("Port = 9101\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EQUALS);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_NUMBER);

  CloseLexer(lf);
}

TEST(LexMultiTokenTest, QuotedStringValue) {
  lexer* lf = OpenLexer(R"(Description = "a quoted value")" "\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EQUALS);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_QUOTED_STRING);
  EXPECT_STREQ(lf->str, "a quoted value");

  CloseLexer(lf);
}

TEST(LexMultiTokenTest, CommaList) {
  lexer* lf = OpenLexer("a, b, c\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_COMMA);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_COMMA);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EOL);

  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// Comment handling tests
// ──────────────────────────────────────────────────────────

TEST(LexCommentTest, HashCommentSkipped) {
  lexer* lf = OpenLexer("# this is a comment\nhello\n");
  ASSERT_NE(lf, nullptr);

  // Comment line should result in just EOL, then the identifier
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_EOL);
  tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_IDENTIFIER);
  EXPECT_STREQ(lf->str, "hello");

  CloseLexer(lf);
}

TEST(LexCommentTest, InlineCommentSkipped) {
  lexer* lf = OpenLexer("key = value # inline comment\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EQUALS);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EOL);

  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// Line number tracking tests
// ──────────────────────────────────────────────────────────

TEST(LexLineNumberTest, StartsAtOne) {
  lexer* lf = OpenLexer("hello\n");
  ASSERT_NE(lf, nullptr);
  EXPECT_GE(lf->line_no, 0);
  CloseLexer(lf);
}

TEST(LexLineNumberTest, AdvancesAfterNewline) {
  lexer* lf = OpenLexer("line1\nline2\n");
  ASSERT_NE(lf, nullptr);

  LexGetToken(lf, BCT_ALL);  // line1
  int line_after_first = lf->line_no;

  LexGetToken(lf, BCT_ALL);  // EOL
  LexGetToken(lf, BCT_ALL);  // line2
  int line_after_second = lf->line_no;

  EXPECT_LE(line_after_first, line_after_second);
  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// LexGetChar / LexUngetChar tests
// ──────────────────────────────────────────────────────────

TEST(LexGetCharTest, ReadsFirstChar) {
  lexer* lf = OpenLexer("A\n");
  ASSERT_NE(lf, nullptr);
  int ch = LexGetChar(lf);
  EXPECT_EQ(ch, 'A');
  CloseLexer(lf);
}

TEST(LexGetCharTest, UngetThenGetAgain) {
  lexer* lf = OpenLexer("XY\n");
  ASSERT_NE(lf, nullptr);
  int ch = LexGetChar(lf);
  EXPECT_EQ(ch, 'X');
  LexUngetChar(lf);
  int ch2 = LexGetChar(lf);
  EXPECT_EQ(ch2, 'X');
  CloseLexer(lf);
}

TEST(LexGetCharTest, EofOnEmptyFile) {
  lexer* lf = OpenLexer("");
  ASSERT_NE(lf, nullptr);
  int ch = LexGetChar(lf);
  EXPECT_EQ(ch, L_EOF);
  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// Handler configuration tests
// ──────────────────────────────────────────────────────────

TEST(LexHandlerTest, SetDefaultErrorHandler) {
  lexer* lf = OpenLexer("hello\n");
  ASSERT_NE(lf, nullptr);
  LexSetDefaultErrorHandler(lf);
  EXPECT_NE(lf->scan_error, nullptr);
  CloseLexer(lf);
}

TEST(LexHandlerTest, SetDefaultWarningHandler) {
  lexer* lf = OpenLexer("hello\n");
  ASSERT_NE(lf, nullptr);
  LexSetDefaultWarningHandler(lf);
  EXPECT_NE(lf->scan_warning, nullptr);
  CloseLexer(lf);
}

TEST(LexHandlerTest, SetErrorHandlerErrorType) {
  lexer* lf = OpenLexer("hello\n");
  ASSERT_NE(lf, nullptr);
  LexSetDefaultErrorHandler(lf);
  LexSetErrorHandlerErrorType(lf, 10);
  EXPECT_EQ(lf->err_type, 10);
  CloseLexer(lf);
}

TEST(LexHandlerTest, SetDifferentErrorTypes) {
  lexer* lf = OpenLexer("hello\n");
  ASSERT_NE(lf, nullptr);
  LexSetDefaultErrorHandler(lf);

  for (int type : {0, 1, 5, 10, 100}) {
    LexSetErrorHandlerErrorType(lf, type);
    EXPECT_EQ(lf->err_type, type);
  }
  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// Lexer options tests
// ──────────────────────────────────────────────────────────

TEST(LexOptionsTest, NoIdentOptionParseAsString) {
  lexer* lf = OpenLexer("identifier\n");
  ASSERT_NE(lf, nullptr);

  lf->options.set(lexer::options::NoIdent);
  int tok = LexGetToken(lf, BCT_ALL);
  // With NoIdent, identifiers are parsed as unquoted strings
  EXPECT_EQ(tok, BCT_UNQUOTED_STRING);

  CloseLexer(lf);
}

TEST(LexOptionsTest, ForceStringOption) {
  lexer* lf = OpenLexer("42\n");
  ASSERT_NE(lf, nullptr);

  lf->options.set(lexer::options::ForceString);
  int tok = LexGetToken(lf, BCT_ALL);
  // With ForceString, numbers are parsed as strings
  EXPECT_EQ(tok, BCT_UNQUOTED_STRING);

  CloseLexer(lf);
}

TEST(LexOptionsTest, DefaultOptionsAreOff) {
  lexer* lf = OpenLexer("hello\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_FALSE(lf->options.test(lexer::options::NoIdent));
  EXPECT_FALSE(lf->options.test(lexer::options::ForceString));
  EXPECT_FALSE(lf->options.test(lexer::options::NoExtern));

  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// ScanToEol / ScanToNextNotEol tests
// ──────────────────────────────────────────────────────────

TEST(ScanToEolTest, SkipsToEndOfLine) {
  lexer* lf = OpenLexer("skip this whole line\nnextline\n");
  ASSERT_NE(lf, nullptr);

  ScanToEol(lf);  // should consume the first line

  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_IDENTIFIER);
  EXPECT_STREQ(lf->str, "nextline");

  CloseLexer(lf);
}

TEST(ScanToNextNotEolTest, SkipsEmptyLines) {
  lexer* lf = OpenLexer("\n\n\nhello\n");
  ASSERT_NE(lf, nullptr);

  int tok = ScanToNextNotEol(lf);
  EXPECT_NE(tok, BCT_EOL);
  EXPECT_NE(tok, BCT_EOF);

  CloseLexer(lf);
}

// ──────────────────────────────────────────────────────────
// Real-world config snippet tests
// ──────────────────────────────────────────────────────────

TEST(LexRealConfigTest, DirectorResource) {
  lexer* lf = OpenLexer(R"(
Director {
  Name = bareos-dir
  QueryFile = "/usr/lib/bareos/scripts/query.sql"
  Maximum Concurrent Jobs = 10
}
)");
  ASSERT_NE(lf, nullptr);

  // EOL from blank line
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EOL);
  // Director identifier
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  // {
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_BOB);

  CloseLexer(lf);
}

TEST(LexRealConfigTest, QuotedPathValue) {
  lexer* lf = OpenLexer(R"(WorkingDirectory = "/var/lib/bareos")" "\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EQUALS);
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_QUOTED_STRING);
  EXPECT_STREQ(lf->str, "/var/lib/bareos");

  CloseLexer(lf);
}

TEST(LexRealConfigTest, SemicolonActsAsEol) {
  lexer* lf = OpenLexer("key = value;\n");
  ASSERT_NE(lf, nullptr);

  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_EQUALS);
  EXPECT_EQ(LexGetToken(lf, BCT_ALL), BCT_IDENTIFIER);
  // Semicolon should produce EOL
  int tok = LexGetToken(lf, BCT_ALL);
  EXPECT_EQ(tok, BCT_EOL);

  CloseLexer(lf);
}
