/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
/*
 * Lexical scanner for BAREOS configuration file
 *
 * Kern Sibbald, 2000
 */

#include "include/bareos.h"
#include "lex.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "lib/berrno.h"
#include "lib/bpipe.h"
#include <glob.h>
#include <fstream>

extern int debug_level;

/* Debug level for this source file */
static const int debuglevel = 5000;

std::string_view read_line_containing(const lexer* lf, std::size_t offset)
{
  auto block = std::lower_bound(lf->line_map.begin(), lf->line_map.end(),
                                offset, [](auto& val, std::size_t x) {
                                  return val.byte_start + val.length <= x;
                                });

  if (block == lf->line_map.end()) { return ""; }

  auto& data = lf->file_contents[block->file_index].content;

  std::size_t file_offset = offset - block->byte_start + block->file_start;

  std::size_t line_start = data.rfind('\n', file_offset);
  std::size_t line_end = data.find('\n', file_offset);

  if (line_start == data.npos) {
    line_start = 0;
  } else if (line_start < data.size()) {
    line_start += 1; /* skip new line */
  }

  if (line_end == data.npos) { line_end = data.size(); }
  // else if (line_end > 0) { line_end -= 1; }

  return std::string_view{data}.substr(line_start, line_end - line_start);
}

std::string read_span(const lexer* lf,
                      std::size_t bytes_start,
                      std::size_t bytes_end)
{
  auto block = std::lower_bound(lf->line_map.begin(), lf->line_map.end(),
                                bytes_start, [](auto& val, std::size_t x) {
                                  return val.byte_start + val.length <= x;
                                });

  if (block == lf->line_map.end()) { return ""; }

  std::size_t current = bytes_start;

  std::string result;

  while (current < bytes_end) {
    ASSERT(block->byte_start <= current);
    ASSERT(current < block->byte_start + block->length);

    std::size_t end = block->byte_start + block->length;
    if (end > bytes_end) { end = bytes_end; }

    auto& data = lf->file_contents[block->file_index].content;

    std::size_t content_start = current - block->byte_start + block->file_start;
    std::size_t content_end = end - block->byte_start + block->file_start;


    result += std::string_view{data}.substr(content_start,
                                            content_end - content_start);

    block++;
    current = end;
  }


  return result;
}

static inline void NewErrorMsg(lexer* lf, const char* x)
{
  auto cause = read_line_containing(lf, lf->token_start);

  Emsg0(M_CONFIG_ERROR, 0, "Error: %s in line:\n%.*s\n", x, (int)cause.size(),
        cause.data());
}

/*
 * Scan to "logical" end of line. I.e. end of line,
 *   or semicolon, but stop on BCT_EOB (same as end of
 *   line except it is not eaten).
 */
void ScanToEol(lexer* lc)
{
  int token;

  Dmsg0(debuglevel, "start scan to eof\n");
  while ((token = LexGetToken(lc, BCT_ALL)) != BCT_EOL) {
    if (token == BCT_EOB) {
      LexUngetChar(lc);
      return;
    }
  }
}

// Get next token, but skip EOL
int ScanToNextNotEol(lexer* lc)
{
  int token;

  do {
    token = LexGetToken(lc, BCT_ALL);
  } while (token == BCT_EOL);

  return token;
}

// Format a scanner error message
PRINTF_LIKE(4, 5)
static void s_err(const char* file, int line, lexer* lc, const char* msg, ...)
{
  va_list ap;
  PoolMem buf(PM_NAME), more(PM_NAME);

  va_start(ap, msg);
  buf.Bvsprintf(msg, ap);
  va_end(ap);

  if (lc->err_type == 0) { /* M_ERROR_TERM by default */
    lc->err_type = M_ERROR_TERM;
  }

  if (lc->files.empty()) {
    e_msg(file, line, lc->err_type, 0, T_("Config error: %s\n"), buf.c_str());

    lc->error_counter += 1;
    return;
  }

  auto& current = lc->files.back();
  auto err_content = read_span(lc, lc->token_start, lc->bytes_read);
  if (!err_content.empty()) {
    e_msg(file, line, lc->err_type, 0,
          T_("Config error: %s\n"
             "            : line %d, col %d of file %s\n%.*s\n"),
          buf.c_str(), current.line_no, current.col_no,
          lc->file_contents[current.file_index].fname.c_str(),
          (int)err_content.size(), err_content.data());
  } else {
    e_msg(file, line, lc->err_type, 0, T_("Config error: %s\n"), buf.c_str());
  }

  lc->error_counter++;
}

// Format a scanner warning message
PRINTF_LIKE(4, 5)
static void s_warn(const char* file, int line, lexer* lc, const char* msg, ...)
{
  va_list ap;
  PoolMem buf(PM_NAME), more(PM_NAME);

  va_start(ap, msg);
  buf.Bvsprintf(msg, ap);
  va_end(ap);

  auto& current = lc->files.back();

  if (current.line_no > current.begin_line_no) {
    Mmsg(more, T_("Problem probably begins at line %d.\n"),
         current.begin_line_no);
  } else {
    PmStrcpy(more, "");
  }

  if (current.line_no > 0) {
    p_msg(file, line, 0,
          T_("Config warning: %s\n"
             "            : line %d, col %d of file %s\n%s\n%s"),
          buf.c_str(), current.line_no, current.col_no,
          lc->file_contents[current.file_index].fname.c_str(),
          current.line.c_str(), more.c_str());
  } else {
    p_msg(file, line, 0, T_("Config warning: %s\n"), buf.c_str());
  }
}

void LexSetDefaultErrorHandler(lexer* lf) { lf->scan_error = s_err; }

void LexSetDefaultWarningHandler(lexer* lf) { lf->scan_warning = s_warn; }

// Set err_type used in error_handler
void LexSetErrorHandlerErrorType(lexer* lf, int err_type)
{
  lf->err_type = err_type;
}

/*
 * Free the current file, and retrieve the contents of the previous packet if
 * any.
 */
lexer* LexCloseFile(lexer* lf)
{
  ASSERT(!lf->files.empty());
  auto& current = lf->files.back();

  Dmsg1(debuglevel, "Close lex file: %s\n",
        lf->file_contents[current.file_index].fname.c_str());

  lf->files.pop_back();

  if (lf->files.empty()) {
    delete lf;
    return nullptr;
  }

  return lf;
}

static inline std::string lex_read_file(FILE* fd)
{
  std::string s;
  constexpr std::size_t buf_size = 4096;
  auto buf = std::make_unique<char[]>(buf_size);

  for (;;) {
    size_t bytes_read = fread(buf.get(), 1, buf_size, fd);

    if (bytes_read == 0) {
      ASSERT(!ferror(fd));
      ASSERT(feof(fd));
      break;
    }

    s += std::string_view{buf.get(), static_cast<std::size_t>(bytes_read)};
  }
  return s;
}

// Add lex structure for an included config file.
static inline lexer* lex_add(lexer* lf,
                             const char* filename,
                             FILE* fd,
                             Bpipe* bpipe,
                             lexer::error_handler* scan_error,
                             lexer::warning_handler* scan_warning)
{
  if (!scan_error) { scan_error = s_err; }
  if (!scan_warning) { scan_warning = s_warn; }

  if (!lf) {
    lf = new lexer;
    lf->scan_error = scan_error;
    lf->scan_warning = scan_warning;

    lf->state = lex_none;
    lf->ch_ = L_EOL;
  }

  auto& contents = lf->file_contents.emplace_back();
  auto& current = lf->files.emplace_back();
  contents.fname = filename ? filename : "";
  contents.content = lex_read_file(fd);
  if (bpipe) {
    contents.generated = true;
    CloseBpipe(bpipe);
  } else {
    contents.generated = false;
    fclose(fd);
  }

  current.file_index = &contents - &lf->file_contents[0];

  return lf;
}

#ifdef HAVE_GLOB
static inline bool IsWildcardString(const char* string)
{
  return (strchr(string, '*') || strchr(string, '?'));
}
#endif

/*
 * Open a new configuration file. We push the
 * state of the current file (lf) so that we
 * can do includes.  This is a bit of a hammer.
 * Instead of passing back the pointer to the
 * new packet, I simply replace the contents
 * of the caller's packet with the new packet,
 * and link the contents of the old packet into
 * the next field.
 */
lexer* lex_open_file(lexer* lf,
                     const char* filename,
                     lexer::error_handler* ScanError,
                     lexer::warning_handler* scan_warning)
{
  FILE* fd;
  Bpipe* bpipe = NULL;
  char* bpipe_filename = NULL;

  if (filename[0] == '|') {
    bpipe_filename = strdup(filename);
    if ((bpipe = OpenBpipe(bpipe_filename + 1, 0, "rb")) == NULL) {
      free(bpipe_filename);
      return NULL;
    }
    free(bpipe_filename);
    fd = bpipe->rfd;
    return lex_add(lf, filename, fd, bpipe, ScanError, scan_warning);
  } else {
#ifdef HAVE_GLOB
    int globrc;
    glob_t fileglob;
    char* filename_expanded = NULL;

    Dmsg1(500, "Trying glob match with %s\n", filename);

    /* Flag GLOB_NOMAGIC is a GNU extension, therefore manually check if string
     * is a wildcard string. */

    // Clear fileglob at least required for mingw version of glob()
    memset(&fileglob, 0, sizeof(fileglob));
    globrc = glob(filename, 0, NULL, &fileglob);

    if ((globrc == GLOB_NOMATCH) && (IsWildcardString(filename))) {
      /* fname is a wildcard string, but no matching files have been found.
       * Ignore this include statement and continue. */
      Dmsg1(500, "glob => nothing found for wildcard %s\n", filename);
      return lf;
    } else if (globrc != 0) {
      // glob() error has occurred. Giving up.
      Dmsg1(500, "glob => error\n");
      return NULL;
    }

    Dmsg2(100, "glob %s: %" PRIuz " files\n", filename, fileglob.gl_pathc);
    for (size_t i = 0; i < fileglob.gl_pathc; i++) {
      filename_expanded = fileglob.gl_pathv[i];
      if ((fd = fopen(filename_expanded, "rb")) == NULL) {
        globfree(&fileglob);
        return NULL;
      }
      lf = lex_add(lf, filename_expanded, fd, bpipe, ScanError, scan_warning);
    }
    globfree(&fileglob);
#else
    Dmsg1(500, "Trying open file %s\n", filename);
    if ((fd = fopen(filename, "rb")) == NULL) { return NULL; }
    lf = lex_add(lf, filename, fd, bpipe, ScanError, scan_warning);
#endif
    return lf;
  }
}

void UpdateLineMap(lexer* lf)
{
  auto& current = lf->current();
  auto& map = lf->line_map;

  bool insert_new = true;

  if (!map.empty()) {
    auto& last = map.back();

    if (last.file_index == current.file_index
        && last.file_start + last.length == current.bytes) {
      last.length += 1;
      insert_new = false;
    }
  }

  if (insert_new) {
    map.push_back(
        line_entry{current.file_index, lf->bytes_read, current.bytes, 1});
  }

  lf->bytes_read += 1;
}

void LineMapRemoveLastChar(lexer* lf)
{
  if (lf->bytes_read == 0) { return; }
  ASSERT(!lf->line_map.empty());

  lf->line_map.back().length -= 1;
  if (lf->line_map.back().length == 0) { lf->line_map.pop_back(); }

  lf->bytes_read -= 1;
}

static inline bool prepare_next_line(lexer* lf, lex_file_state& state)
{
  auto& contents = lf->file_contents[state.file_index];
  auto& str = contents.content;

  if (str.size() <= state.current_offset) { return false; }

  auto found = str.find('\n', state.current_offset);

  std::string_view line = [&] {
    if (found == std::string::npos) {
      return std::string_view{str}.substr(state.current_offset);
    } else {
      return std::string_view{str}.substr(state.current_offset,
                                          found - state.current_offset);
    }
  }();

  state.current_offset += line.size() + 1;

  state.line.assign(line);

  return true;
}

/*
 * Get the next character from the input.
 *  Returns the character or
 *    L_EOF if end of file
 *    L_EOL if end of line
 */
int LexGetChar(lexer* lf)
{
  auto& current = lf->current();

  if (lf->ch_ == L_EOF) {
    NewErrorMsg(lf,
                "get_char: called after EOF."
                " You may have a open double quote without the closing double "
                "quote.\n");


    Emsg0(M_CONFIG_ERROR, 0,
          T_("get_char: called after EOF."
             " You may have a open double quote without the closing double "
             "quote.\n"));
  }

  if (lf->ch_ == L_EOL) {
    // See if we are really reading a file otherwise we have reached EndOfFile.
    if (!prepare_next_line(lf, current)) {
      if (lf->files.size() > 1) {
        LexCloseFile(lf);
        lf->ch_ = L_EOL;
        return LexGetChar(lf);
      } else {
        lf->ch_ = L_EOF;
        if (lf->files.size() > 1) { LexCloseFile(lf); }

        // auto& new_cur = lf->current();

        // if (new_cur.ch != L_EOF) {
        //   UpdateLineMap(lf);
        //   new_cur.bytes += 1;
        // }
        // this is not correct!
        return lf->ch_;
      }
    }
    current.line_no++;
    current.col_no = 0;
    Dmsg2(1000, "fget line=%d %s", current.line_no, current.line.c_str());
  }

  lf->ch_ = (uint8_t)current.line[current.col_no];
  if (lf->ch_ == 0) {
    lf->ch_ = L_EOL;
  } else {
    if (lf->ch_ == '\n') {
      lf->ch_ = L_EOL;
      current.col_no++;
    } else {
      current.col_no++;
    }
  }
  UpdateLineMap(lf);
  current.bytes += 1;
  Dmsg2(debuglevel, "LexGetChar: %c %d\n", lf->ch_, lf->ch_);

  if (lf->ch_ > 0) {
    auto findex = lf->line_map.back().file_index;
    const auto& fcontent = lf->file_contents[findex].content;
    auto current_offset = lf->bytes_read - lf->line_map.back().byte_start
                          + lf->line_map.back().file_start;
    ASSERT((char)lf->ch_ == fcontent[current_offset - 1]);
  }
  return lf->ch_;
}

void LexUngetChar(lexer* lf)
{
  auto& current = lf->current();
  if (lf->ch_ == L_EOL) {
    lf->ch_ = 0; /* End of line, force read of next one */
  } else {
    current.col_no--; /* Backup to re-read char */
  }
  LineMapRemoveLastChar(lf);
  current.bytes -= 1;
}

// Add a character to the current string
static void add_str(lexer* lf, int ch) { lf->current_str.push_back(ch); }

// Begin the string
static void BeginStr(lexer* lf, int ch)
{
  auto& current = lf->current();
  lf->current_str.clear();
  if (ch != 0) { add_str(lf, ch); }
  current.begin_line_no = current.line_no; /* save start string line no */
}

static const char* lex_state_to_str(int state)
{
  switch (state) {
    case lex_none:
      return T_("none");
    case lex_comment:
      return T_("comment");
    case lex_number:
      return T_("number");
    case lex_ip_addr:
      return T_("ip_addr");
    case lex_identifier:
      return T_("identifier");
    case lex_string:
      return T_("string");
    case lex_quoted_string:
      return T_("quoted_string");
    case lex_include:
      return T_("include");
    case lex_include_quoted_string:
      return T_("include_quoted_string");
    case lex_utf8_bom:
      return T_("UTF-8 Byte Order Mark");
    case lex_utf16_le_bom:
      return T_("UTF-16le Byte Order Mark");
    default:
      return "??????";
  }
}

/*
 * Convert a lex token to a string
 * used for debug/error printing.
 */
const char* lex_tok_to_str(int token)
{
  switch (token) {
    case L_EOF:
      return "L_EOF";
    case L_EOL:
      return "L_EOL";
    case BCT_NONE:
      return "BCT_NONE";
    case BCT_NUMBER:
      return "BCT_NUMBER";
    case BCT_IPADDR:
      return "BCT_IPADDR";
    case BCT_IDENTIFIER:
      return "BCT_IDENTIFIER";
    case BCT_UNQUOTED_STRING:
      return "BCT_UNQUOTED_STRING";
    case BCT_QUOTED_STRING:
      return "BCT_QUOTED_STRING";
    case BCT_BOB:
      return "BCT_BOB";
    case BCT_EOB:
      return "BCT_EOB";
    case BCT_EQUALS:
      return "BCT_EQUALS";
    case BCT_ERROR:
      return "BCT_ERROR";
    case BCT_EOF:
      return "BCT_EOF";
    case BCT_COMMA:
      return "BCT_COMMA";
    case BCT_EOL:
      return "BCT_EOL";
    case BCT_UTF8_BOM:
      return "BCT_UTF8_BOM";
    case BCT_UTF16_BOM:
      return "BCT_UTF16_BOM";
    default:
      return "??????";
  }
}

static uint32_t scan_pint(lexer* lf, const char* str)
{
  int64_t val = 0;

  if (!Is_a_number(str)) {
    scan_err1(lf, T_("expected a positive integer number, got: %s"), str);
  } else {
    errno = 0;
    val = str_to_int64(str);
    if (errno != 0 || val < 0) {
      scan_err1(lf, T_("expected a positive integer number, got: %s"), str);
    }
  }

  return (uint32_t)(val & 0xffffffff);
}

static uint64_t scan_pint64(lexer* lf, const char* str)
{
  uint64_t val = 0;

  if (!Is_a_number(str)) {
    scan_err1(lf, T_("expected a positive integer number, got: %s"), str);
  } else {
    errno = 0;
    val = str_to_uint64(str);
    if (errno != 0) {
      scan_err1(lf, T_("expected a positive integer number, got: %s"), str);
    }
  }

  return val;
}

class TemporaryBuffer {
 public:
  TemporaryBuffer(FILE* fd) : buf(GetPoolMemory(PM_NAME)), fd_(fd)
  {
    pos_ = ftell(fd_);
  }
  ~TemporaryBuffer()
  {
    FreePoolMemory(buf);
    fseek(fd_, pos_, SEEK_SET);
  }
  POOLMEM* buf;

 private:
  FILE* fd_;
  long pos_;
};

static bool NextLineContinuesWithQuotes(lexer* lf)
{
  auto& current = lf->current();
  auto& contents = lf->file_contents[current.file_index].content;


  for (std::size_t offset = current.current_offset; offset < contents.size();
       ++offset) {
    switch (contents[offset]) {
      case '"': {
        return true;
      } break;
      case ' ':
        [[fallthrough]];
      case '\t': {
        continue;
      }
      default: {
        return false;
      }
    }
  }

  return false;
}

static bool CurrentLineContinuesWithQuotes(lexer* lf)
{
  auto& current = lf->current();
  int i = current.col_no;
  while (current.line[i] != '\0') {
    if (current.line[i] == '"') { return true; }
    if (current.line[i] != ' ' && current.line[i] != '\t') { return false; }
    ++i;
  };
  return false;
}

std::string read_part(const char* fname, size_t start, size_t end)
{
  ASSERT(start < end);

  std::ifstream f{fname};

  f.seekg(start);

  std::string result;
  result.resize(end - start);

  f.read(result.data(), result.size());

  ASSERT(result.size() == static_cast<size_t>(f.gcount()));

  return result;
}


/*
 *
 * Get the next token from the input
 *
 */
int LexGetToken(lexer* lf, int expect)
{
  lf->token_start = lf->bytes_read;
  int ch;
  int token = BCT_NONE;
  bool continue_string = false;
  bool esc_next = false;
  /* Unicode files, especially on Win32, may begin with a "Byte Order Mark"
     to indicate which transmission format the file is in. The codepoint for
     this mark is U+FEFF and is represented as the octets EF-BB-BF in UTF-8
     and as FF-FE in UTF-16le(little endian) and  FE-FF in UTF-16(big endian).
     We use a distinct state for UTF-8 and UTF-16le, and use bom_bytes_seen
     to tell which byte we are expecting. */
  int bom_bytes_seen = 0;

  Dmsg0(debuglevel, "enter LexGetToken\n");

  while (token == BCT_NONE) {
    ch = LexGetChar(lf);

    auto& current = lf->current();
    switch (lf->state) {
      case lex_none:
        Dmsg2(debuglevel, "Lex state lex_none ch=%d,%x\n", ch, ch);
        if (B_ISSPACE(ch)) break;
        if (B_ISALPHA(ch)) {
          if (lf->options[lexer::options::NoIdent]
              || lf->options[lexer::options::ForceString]) {
            lf->state = lex_string;
          } else {
            lf->state = lex_identifier;
          }
          BeginStr(lf, ch);
          break;
        }
        if (B_ISDIGIT(ch)) {
          if (lf->options[lexer::options::ForceString]) {
            lf->state = lex_string;
          } else {
            lf->state = lex_number;
          }
          BeginStr(lf, ch);
          break;
        }
        Dmsg0(debuglevel, "Enter lex_none switch\n");
        switch (ch) {
          case L_EOF:
            token = BCT_EOF;
            Dmsg0(debuglevel, "got L_EOF set token=T_EOF\n");
            break;
          case '#':
            lf->state = lex_comment;
            break;
          case '{':
            token = BCT_BOB;
            BeginStr(lf, ch);
            break;
          case '}':
            token = BCT_EOB;
            BeginStr(lf, ch);
            break;
          case ' ':
            if (continue_string) { continue; }
            break;
          case '"':
            lf->state = lex_quoted_string;
            if (!continue_string) { BeginStr(lf, 0); }
            break;
          case '=':
            token = BCT_EQUALS;
            BeginStr(lf, ch);
            break;
          case ',':
            token = BCT_COMMA;
            BeginStr(lf, ch);
            break;
          case ';':
            if (expect != BCT_SKIP_EOL) {
              token = BCT_EOL; /* treat ; like EOL */
            }
            break;
          case L_EOL:
            if (continue_string) {
              continue;
            } else {
              Dmsg0(debuglevel, "got L_EOL set token=BCT_EOL\n");
              if (expect != BCT_SKIP_EOL) { token = BCT_EOL; }
            }
            break;
          case '@':
            /* In NO_EXTERN mode, @ is part of a string */
            if (lf->options[lexer::options::NoExtern]) {
              lf->state = lex_string;
              BeginStr(lf, ch);
            } else {
              lf->state = lex_include;
              BeginStr(lf, 0);
            }
            break;
          case 0xEF: /* probably a UTF-8 BOM */
          case 0xFF: /* probably a UTF-16le BOM */
          case 0xFE: /* probably a UTF-16be BOM (error)*/
            if (current.line_no != 1 || current.col_no != 1) {
              lf->state = lex_string;
              BeginStr(lf, ch);
            } else {
              bom_bytes_seen = 1;
              if (ch == 0xEF) {
                lf->state = lex_utf8_bom;
              } else if (ch == 0xFF) {
                lf->state = lex_utf16_le_bom;
              } else {
                scan_err0(lf,
                          T_("This config file appears to be in an "
                             "unsupported Unicode format (UTF-16be). Please "
                             "resave as UTF-8\n"));
                return BCT_ERROR;
              }
            }
            break;
          default:
            lf->state = lex_string;
            BeginStr(lf, ch);
            break;
        }
        break;
      case lex_comment:
        Dmsg1(debuglevel, "Lex state lex_comment ch=%x\n", ch);
        if (ch == L_EOL) {
          lf->state = lex_none;
          if (expect != BCT_SKIP_EOL) { token = BCT_EOL; }
        } else if (ch == L_EOF) {
          token = BCT_ERROR;
        }
        break;
      case lex_number:
        Dmsg2(debuglevel, "Lex state lex_number ch=%x %c\n", ch, ch);
        if (ch == L_EOF) {
          token = BCT_ERROR;
          break;
        }
        /* Might want to allow trailing specifications here */
        if (B_ISDIGIT(ch)) {
          add_str(lf, ch);
          break;
        }

        /* A valid number can be terminated by the following */
        if (B_ISSPACE(ch) || ch == L_EOL || ch == ',' || ch == ';') {
          token = BCT_NUMBER;
          lf->state = lex_none;
        } else {
          lf->state = lex_string;
        }
        LexUngetChar(lf);
        break;
      case lex_ip_addr:
        if (ch == L_EOF) {
          token = BCT_ERROR;
          break;
        }
        Dmsg1(debuglevel, "Lex state lex_ip_addr ch=%x\n", ch);
        break;
      case lex_string:
        Dmsg1(debuglevel, "Lex state lex_string ch=%x\n", ch);
        if (ch == L_EOF) {
          token = BCT_ERROR;
          break;
        }
        if (ch == '\n' || ch == L_EOL || ch == '=' || ch == '}' || ch == '{'
            || ch == '\r' || ch == ';' || ch == ',' || ch == '#'
            || (B_ISSPACE(ch)) || ch == '"') {
          LexUngetChar(lf);
          token = BCT_UNQUOTED_STRING;
          lf->state = lex_none;
          break;
        }
        add_str(lf, ch);
        break;
      case lex_identifier:
        Dmsg2(debuglevel, "Lex state lex_identifier ch=%x %c\n", ch, ch);
        if (B_ISALPHA(ch)) {
          add_str(lf, ch);
          break;
        } else if (B_ISSPACE(ch)) {
          break;
        } else if (ch == '\n' || ch == L_EOL || ch == '=' || ch == '}'
                   || ch == '{' || ch == '\r' || ch == ';' || ch == ','
                   || ch == '"' || ch == '#') {
          LexUngetChar(lf);
          token = BCT_IDENTIFIER;
          lf->state = lex_none;
          break;
        } else if (ch == L_EOF) {
          token = BCT_ERROR;
          lf->state = lex_none;
          BeginStr(lf, ch);
          break;
        }
        /* Some non-alpha character => string */
        lf->state = lex_string;
        add_str(lf, ch);
        break;
      case lex_quoted_string:
        Dmsg2(debuglevel, "Lex state lex_quoted_string ch=%x %c\n", ch, ch);
        if (ch == L_EOF) {
          token = BCT_ERROR;
          break;
        }
        if (ch == L_EOL) {
          add_str(lf, '\n');
          esc_next = false;
          break;
        }
        if (esc_next) {
          add_str(lf, ch);
          esc_next = false;
          break;
        }
        if (ch == '\\') {
          esc_next = true;
          break;
        }
        if (ch == '"') {
          if (NextLineContinuesWithQuotes(lf)
              || CurrentLineContinuesWithQuotes(lf)) {
            continue_string = true;
            lf->state = lex_none;
            continue;
          } else {
            token = BCT_QUOTED_STRING;
            /* Since we may be scanning a quoted list of names,
             *  we get the next character (a comma indicates another
             *  one), then we put it back for rescanning. */
            LexGetChar(lf);
            LexUngetChar(lf);
            lf->state = lex_none;
          }
          break;
        }
        continue_string = false;
        add_str(lf, ch);
        break;
      case lex_include_quoted_string:
        if (ch == L_EOF) {
          token = BCT_ERROR;
          break;
        }
        if (esc_next) {
          add_str(lf, ch);
          esc_next = false;
          break;
        }
        if (ch == '\\') {
          esc_next = true;
          break;
        }
        if (ch == '"') {
          /* Keep the original LEX so we can print an error if the included file
           * can't be opened. */
          lexer* lfori = lf;
          /* Skip the double quote when restarting parsing */
          LexGetChar(lf);

          lf->state = lex_none;
          std::string str = lf->str();
          lf = lex_open_file(lf, lf->str(), lf->scan_error, lf->scan_warning);
          if (lf == NULL) {
            BErrNo be;
            scan_err2(lfori, T_("Cannot open included config file %s: %s\n"),
                      str.c_str(), be.bstrerror());
            return BCT_ERROR;
          }
          break;
        }
        add_str(lf, ch);
        break;
      case lex_include: /* scanning a filename */
        if (ch == L_EOF) {
          token = BCT_ERROR;
          break;
        }
        if (ch == '"') {
          lf->state = lex_include_quoted_string;
          break;
        }


        if (B_ISSPACE(ch) || ch == '\n' || ch == L_EOL || ch == '}' || ch == '{'
            || ch == ';' || ch == ',' || ch == '"' || ch == '#') {
          /* Keep the original LEX so we can print an error if the included file
           * can't be opened. */
          lexer* lfori = lf;

          lf->state = lex_none;
          lf = lex_open_file(lf, lf->str(), lf->scan_error, lf->scan_warning);
          if (lf == NULL) {
            BErrNo be;
            scan_err2(lfori, T_("Cannot open included config file %s: %s\n"),
                      lf->str(), be.bstrerror());
            return BCT_ERROR;
          }
          break;
        }
        add_str(lf, ch);
        break;
      case lex_utf8_bom:
        /* we only end up in this state if we have read an 0xEF
           as the first byte of the file, indicating we are probably
           reading a UTF-8 file */
        if (ch == 0xBB && bom_bytes_seen == 1) {
          bom_bytes_seen++;
        } else if (ch == 0xBF && bom_bytes_seen == 2) {
          token = BCT_UTF8_BOM;
          lf->state = lex_none;
        } else {
          token = BCT_ERROR;
        }
        break;
      case lex_utf16_le_bom:
        /* we only end up in this state if we have read an 0xFF
           as the first byte of the file -- indicating that we are
           probably dealing with an Intel based (little endian) UTF-16 file*/
        if (ch == 0xFE) {
          token = BCT_UTF16_BOM;
          lf->state = lex_none;
        } else {
          token = BCT_ERROR;
        }
        break;
    }
    Dmsg4(debuglevel, "ch=%d state=%s token=%s %c\n", ch,
          lex_state_to_str(lf->state), lex_tok_to_str(token), ch);
  }

  auto& current = lf->current();
  Dmsg2(debuglevel, "lex returning: line %d token: %s\n", current.line_no,
        lex_tok_to_str(token));
  lf->token = token;


  /* Here is where we check to see if the user has set certain
   *  expectations (e.g. 32 bit integer). If so, we do type checking
   *  and possible additional scanning (e.g. for range). */
  switch (expect) {
    case BCT_PINT16:
      lf->u.pint16_val = (scan_pint(lf, lf->str()) & 0xffff);
      lf->u2.pint16_val = lf->u.pint16_val;
      token = BCT_PINT16;
      break;

    case BCT_PINT32:
      lf->u.pint32_val = scan_pint(lf, lf->str());
      lf->u2.pint32_val = lf->u.pint32_val;
      token = BCT_PINT32;
      break;

    case BCT_PINT32_RANGE:
      if (token == BCT_NUMBER) {
        lf->u.pint32_val = scan_pint(lf, lf->str());
        lf->u2.pint32_val = lf->u.pint32_val;
        token = BCT_PINT32;
      } else {
        std::string_view view = lf->current_str;
        size_t pos = view.find('-');
        if (pos == view.npos) {
          scan_err2(lf, T_("expected an integer or a range, got %s: %s"),
                    lex_tok_to_str(token), lf->str());
          token = BCT_ERROR;
          break;
        }

        std::string first{view.substr(0, pos)};
        std::string second{view.substr(pos + 1)};

        lf->u.pint32_val = scan_pint(lf, first.c_str());
        lf->u2.pint32_val = scan_pint(lf, second.c_str());
        token = BCT_PINT32_RANGE;
      }
      break;

    case BCT_INT16:
      if (token != BCT_NUMBER || !Is_a_number(lf->str())) {
        scan_err2(lf, T_("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str());
        token = BCT_ERROR;
        break;
      }
      errno = 0;
      lf->u.int16_val = (int16_t)str_to_int64(lf->str());
      if (errno != 0) {
        scan_err2(lf, T_("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str());
        token = BCT_ERROR;
      } else {
        token = BCT_INT16;
      }
      break;

    case BCT_INT32:
      if (token != BCT_NUMBER || !Is_a_number(lf->str())) {
        scan_err2(lf, T_("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str());
        token = BCT_ERROR;
        break;
      }
      errno = 0;
      lf->u.int32_val = (int32_t)str_to_int64(lf->str());
      if (errno != 0) {
        scan_err2(lf, T_("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str());
        token = BCT_ERROR;
      } else {
        token = BCT_INT32;
      }
      break;

    case BCT_INT64:
      Dmsg2(debuglevel, "int64=:%s: %f\n", lf->str(), strtod(lf->str(), NULL));
      if (token != BCT_NUMBER || !Is_a_number(lf->str())) {
        scan_err2(lf, T_("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str());
        token = BCT_ERROR;
        break;
      }
      errno = 0;
      lf->u.int64_val = str_to_int64(lf->str());
      if (errno != 0) {
        scan_err2(lf, T_("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str());
        token = BCT_ERROR;
      } else {
        token = BCT_INT64;
      }
      break;

    case BCT_PINT64_RANGE:
      if (token == BCT_NUMBER) {
        lf->u.pint64_val = scan_pint64(lf, lf->str());
        lf->u2.pint64_val = lf->u.pint64_val;
        token = BCT_PINT64;
      } else {
        std::string_view view = lf->current_str;
        size_t pos = view.find('-');
        if (pos == view.npos) {
          scan_err2(lf, T_("expected an integer or a range, got %s: %s"),
                    lex_tok_to_str(token), lf->str());
          token = BCT_ERROR;
          break;
        }

        std::string first{view.substr(0, pos)};
        std::string second{view.substr(pos + 1)};

        lf->u.pint64_val = scan_pint64(lf, first.c_str());
        lf->u2.pint64_val = scan_pint64(lf, second.c_str());
        token = BCT_PINT64_RANGE;
      }
      break;

    case BCT_NAME:
      if (token != BCT_IDENTIFIER && token != BCT_UNQUOTED_STRING
          && token != BCT_QUOTED_STRING) {
        scan_err2(lf, T_("expected a name, got %s: %s"), lex_tok_to_str(token),
                  lf->str());
        token = BCT_ERROR;
      } else if (lf->str_len() > MAX_RES_NAME_LENGTH) {
        scan_err3(lf, T_("name %s length %d too long, max is %d\n"), lf->str(),
                  lf->str_len(), MAX_RES_NAME_LENGTH);
        token = BCT_ERROR;
      }
      break;

    case BCT_STRING:
      if (token != BCT_IDENTIFIER && token != BCT_UNQUOTED_STRING
          && token != BCT_QUOTED_STRING) {
        scan_err2(lf, T_("expected a string, got %s: %s"),
                  lex_tok_to_str(token), lf->str());
        token = BCT_ERROR;
      } else {
        token = BCT_STRING;
      }
      break;


    default:
      break; /* no expectation given */
  }
  lf->token = token; /* set possible new token */

#if 1
  auto token_acc = read_span(lf, lf->token_start, lf->bytes_read);

  Dmsg0(10, "'%s' - '%s'\n", token_acc.c_str(), lf->str());
#endif

  return token;
}
