/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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

#ifdef HAVE_GLOB_H
#include <glob.h>
#endif

extern int debug_level;

/* Debug level for this source file */
static const int debuglevel = 5000;

/*
 * Scan to "logical" end of line. I.e. end of line,
 *   or semicolon, but stop on BCT_EOB (same as end of
 *   line except it is not eaten).
 */
void ScanToEol(LEX* lc)
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

/*
 * Get next token, but skip EOL
 */
int ScanToNextNotEol(LEX* lc)
{
  int token;

  do {
    token = LexGetToken(lc, BCT_ALL);
  } while (token == BCT_EOL);

  return token;
}

/*
 * Format a scanner error message
 */
static void s_err(const char* file, int line, LEX* lc, const char* msg, ...)
{
  va_list ap;
  int len, maxlen;
  PoolMem buf(PM_NAME), more(PM_NAME);

  while (1) {
    maxlen = buf.size() - 1;
    va_start(ap, msg);
    len = Bvsnprintf(buf.c_str(), maxlen, msg, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      buf.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  if (lc->err_type == 0) { /* M_ERROR_TERM by default */
    lc->err_type = M_ERROR_TERM;
  }

  if (lc->line_no > lc->begin_line_no) {
    Mmsg(more, _("Problem probably begins at line %d.\n"), lc->begin_line_no);
  } else {
    PmStrcpy(more, "");
  }

  if (lc->line_no > 0) {
    e_msg(file, line, lc->err_type, 0,
          _("Config error: %s\n"
            "            : line %d, col %d of file %s\n%s\n%s"),
          buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line,
          more.c_str());
  } else {
    e_msg(file, line, lc->err_type, 0, _("Config error: %s\n"), buf.c_str());
  }

  lc->error_counter++;
}

/*
 * Format a scanner warning message
 */
static void s_warn(const char* file, int line, LEX* lc, const char* msg, ...)
{
  va_list ap;
  int len, maxlen;
  PoolMem buf(PM_NAME), more(PM_NAME);

  while (1) {
    maxlen = buf.size() - 1;
    va_start(ap, msg);
    len = Bvsnprintf(buf.c_str(), maxlen, msg, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      buf.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  if (lc->line_no > lc->begin_line_no) {
    Mmsg(more, _("Problem probably begins at line %d.\n"), lc->begin_line_no);
  } else {
    PmStrcpy(more, "");
  }

  if (lc->line_no > 0) {
    p_msg(file, line, 0,
          _("Config warning: %s\n"
            "            : line %d, col %d of file %s\n%s\n%s"),
          buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line,
          more.c_str());
  } else {
    p_msg(file, line, 0, _("Config warning: %s\n"), buf.c_str());
  }
}

void LexSetDefaultErrorHandler(LEX* lf) { lf->ScanError = s_err; }

void LexSetDefaultWarningHandler(LEX* lf) { lf->scan_warning = s_warn; }

/*
 * Set err_type used in error_handler
 */
void LexSetErrorHandlerErrorType(LEX* lf, int err_type)
{
  LEX* lex = lf;
  while (lex) {
    lex->err_type = err_type;
    lex = lex->next;
  }
}

/*
 * Free the current file, and retrieve the contents of the previous packet if
 * any.
 */
LEX* LexCloseFile(LEX* lf)
{
  LEX* of;

  if (lf == NULL) { Emsg0(M_ABORT, 0, _("Close of NULL file\n")); }
  Dmsg1(debuglevel, "Close lex file: %s\n", lf->fname);

  of = lf->next;
  if (lf->bpipe) {
    CloseBpipe(lf->bpipe);
    lf->bpipe = NULL;
  } else {
    fclose(lf->fd);
  }
  Dmsg1(debuglevel, "Close cfg file %s\n", lf->fname);
  free(lf->fname);
  FreeMemory(lf->line);
  FreeMemory(lf->str);
  lf->line = NULL;
  if (of) {
    of->options = lf->options;              /* preserve options */
    of->error_counter += lf->error_counter; /* summarize the errors */
    memcpy(lf, of, sizeof(LEX));
    Dmsg1(debuglevel, "Restart scan of cfg file %s\n", of->fname);
  } else {
    of = lf;
    lf = NULL;
  }
  free(of);
  return lf;
}

/*
 * Add lex structure for an included config file.
 */
static inline LEX* lex_add(LEX* lf,
                           const char* filename,
                           FILE* fd,
                           Bpipe* bpipe,
                           LEX_ERROR_HANDLER* ScanError,
                           LEX_WARNING_HANDLER* scan_warning)
{
  LEX* nf;

  Dmsg1(100, "open config file: %s\n", filename);
  nf = (LEX*)malloc(sizeof(LEX));
  if (lf) {
    memcpy(nf, lf, sizeof(LEX));
    memset(lf, 0, sizeof(LEX));
    lf->next = nf;             /* if have lf, push it behind new one */
    lf->options = nf->options; /* preserve user options */
    /*
     * preserve err_type to prevent bareos exiting on 'reload'
     * if config is invalid.
     */
    lf->err_type = nf->err_type;
  } else {
    lf = nf; /* start new packet */
    memset(lf, 0, sizeof(LEX));
    LexSetErrorHandlerErrorType(lf, M_ERROR_TERM);
  }

  if (ScanError) {
    lf->ScanError = ScanError;
  } else {
    LexSetDefaultErrorHandler(lf);
  }

  if (scan_warning) {
    lf->scan_warning = scan_warning;
  } else {
    LexSetDefaultWarningHandler(lf);
  }

  lf->fd = fd;
  lf->bpipe = bpipe;
  lf->fname = strdup(filename ? filename : "");
  lf->line = GetMemory(1024);
  lf->str = GetMemory(256);
  lf->str_max_len = SizeofPoolMemory(lf->str);
  lf->state = lex_none;
  lf->ch = L_EOL;

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
LEX* lex_open_file(LEX* lf,
                   const char* filename,
                   LEX_ERROR_HANDLER* ScanError,
                   LEX_WARNING_HANDLER* scan_warning)
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

    /*
     * Flag GLOB_NOMAGIC is a GNU extension, therefore manually check if string
     * is a wildcard string.
     */

    /*
     * Clear fileglob at least required for mingw version of glob()
     */
    memset(&fileglob, 0, sizeof(fileglob));
    globrc = glob(filename, 0, NULL, &fileglob);

    if ((globrc == GLOB_NOMATCH) && (IsWildcardString(filename))) {
      /*
       * fname is a wildcard string, but no matching files have been found.
       * Ignore this include statement and continue.
       */
      return lf;
    } else if (globrc != 0) {
      /*
       * glob() error has occurred. Giving up.
       */
      return NULL;
    }

    Dmsg2(100, "glob %s: %i files\n", filename, fileglob.gl_pathc);
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
    if ((fd = fopen(filename, "rb")) == NULL) { return NULL; }
    lf = lex_add(lf, filename, fd, bpipe, ScanError, scan_warning);
#endif
    return lf;
  }
}

LEX* lex_new_buffer(LEX* lf,
                    LEX_ERROR_HANDLER* ScanError,
                    LEX_WARNING_HANDLER* scan_warning)
{
  lf = lex_add(lf, NULL, NULL, NULL, ScanError, scan_warning);
  Dmsg1(debuglevel, "Return lex=%x\n", lf);

  return lf;
}

/*
 * Get the next character from the input.
 *  Returns the character or
 *    L_EOF if end of file
 *    L_EOL if end of line
 */
int LexGetChar(LEX* lf)
{
  if (lf->ch == L_EOF) {
    Emsg0(M_ABORT, 0,
          _("get_char: called after EOF."
            " You may have a open double quote without the closing double "
            "quote.\n"));
  }

  if (lf->ch == L_EOL) {
    /*
     * See if we are really reading a file otherwise we have reached EndOfFile.
     */
    if (!lf->fd || bfgets(lf->line, lf->fd) == NULL) {
      lf->ch = L_EOF;
      if (lf->next) {
        if (lf->fd) { LexCloseFile(lf); }
      }
      return lf->ch;
    }
    lf->line_no++;
    lf->col_no = 0;
    Dmsg2(1000, "fget line=%d %s", lf->line_no, lf->line);
  }

  lf->ch = (uint8_t)lf->line[lf->col_no];
  if (lf->ch == 0) {
    lf->ch = L_EOL;
  } else {
    lf->col_no++;
  }
  Dmsg2(debuglevel, "LexGetChar: %c %d\n", lf->ch, lf->ch);

  return lf->ch;
}

void LexUngetChar(LEX* lf)
{
  if (lf->ch == L_EOL) {
    lf->ch = 0; /* End of line, force read of next one */
  } else {
    lf->col_no--; /* Backup to re-read char */
  }
}

/*
 * Add a character to the current string
 */
static void add_str(LEX* lf, int ch)
{
  /*
   * The default config string is sized to 256 bytes.
   * If we need longer config strings its increased with 256 bytes each time.
   */
  if ((lf->str_len + 3) >= lf->str_max_len) {
    lf->str = CheckPoolMemorySize(lf->str, lf->str_max_len + 256);
    lf->str_max_len = SizeofPoolMemory(lf->str);
  }

  lf->str[lf->str_len++] = ch;
  lf->str[lf->str_len] = 0;
}

/*
 * Begin the string
 */
static void BeginStr(LEX* lf, int ch)
{
  lf->str_len = 0;
  lf->str[0] = 0;
  if (ch != 0) { add_str(lf, ch); }
  lf->begin_line_no = lf->line_no; /* save start string line no */
}

static const char* lex_state_to_str(int state)
{
  switch (state) {
    case lex_none:
      return _("none");
    case lex_comment:
      return _("comment");
    case lex_number:
      return _("number");
    case lex_ip_addr:
      return _("ip_addr");
    case lex_identifier:
      return _("identifier");
    case lex_string:
      return _("string");
    case lex_quoted_string:
      return _("quoted_string");
    case lex_include:
      return _("include");
    case lex_include_quoted_string:
      return _("include_quoted_string");
    case lex_utf8_bom:
      return _("UTF-8 Byte Order Mark");
    case lex_utf16_le_bom:
      return _("UTF-16le Byte Order Mark");
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

static uint32_t scan_pint(LEX* lf, char* str)
{
  int64_t val = 0;

  if (!Is_a_number(str)) {
    scan_err1(lf, _("expected a positive integer number, got: %s"), str);
    /* NOT REACHED */
  } else {
    errno = 0;
    val = str_to_int64(str);
    if (errno != 0 || val < 0) {
      scan_err1(lf, _("expected a positive integer number, got: %s"), str);
      /* NOT REACHED */
    }
  }

  return (uint32_t)(val & 0xffffffff);
}

static uint64_t scan_pint64(LEX* lf, char* str)
{
  uint64_t val = 0;

  if (!Is_a_number(str)) {
    scan_err1(lf, _("expected a positive integer number, got: %s"), str);
    /* NOT REACHED */
  } else {
    errno = 0;
    val = str_to_uint64(str);
    if (errno != 0) {
      scan_err1(lf, _("expected a positive integer number, got: %s"), str);
      /* NOT REACHED */
    }
  }

  return val;
}

/*
 *
 * Get the next token from the input
 *
 */
int LexGetToken(LEX* lf, int expect)
{
  int ch;
  int token = BCT_NONE;
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
    switch (lf->state) {
      case lex_none:
        Dmsg2(debuglevel, "Lex state lex_none ch=%d,%x\n", ch, ch);
        if (B_ISSPACE(ch)) break;
        if (B_ISALPHA(ch)) {
          if (lf->options & LOPT_NO_IDENT || lf->options & LOPT_STRING) {
            lf->state = lex_string;
          } else {
            lf->state = lex_identifier;
          }
          BeginStr(lf, ch);
          break;
        }
        if (B_ISDIGIT(ch)) {
          if (lf->options & LOPT_STRING) {
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
          case '"':
            lf->state = lex_quoted_string;
            BeginStr(lf, 0);
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
            Dmsg0(debuglevel, "got L_EOL set token=BCT_EOL\n");
            if (expect != BCT_SKIP_EOL) { token = BCT_EOL; }
            break;
          case '@':
            /* In NO_EXTERN mode, @ is part of a string */
            if (lf->options & LOPT_NO_EXTERN) {
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
            if (lf->line_no != 1 || lf->col_no != 1) {
              lf->state = lex_string;
              BeginStr(lf, ch);
            } else {
              bom_bytes_seen = 1;
              if (ch == 0xEF) {
                lf->state = lex_utf8_bom;
              } else if (ch == 0xFF) {
                lf->state = lex_utf16_le_bom;
              } else {
                scan_err0(lf, _("This config file appears to be in an "
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
        if (ch == '\n' || ch == L_EOL || ch == '=' || ch == '}' || ch == '{' ||
            ch == '\r' || ch == ';' || ch == ',' || ch == '#' ||
            (B_ISSPACE(ch))) {
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
        } else if (ch == '\n' || ch == L_EOL || ch == '=' || ch == '}' ||
                   ch == '{' || ch == '\r' || ch == ';' || ch == ',' ||
                   ch == '"' || ch == '#') {
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
          token = BCT_QUOTED_STRING;
          /*
           * Since we may be scanning a quoted list of names,
           *  we get the next character (a comma indicates another
           *  one), then we put it back for rescanning.
           */
          LexGetChar(lf);
          LexUngetChar(lf);
          lf->state = lex_none;
          break;
        }
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
          LEX* lfori = lf;
          /* Skip the double quote when restarting parsing */
          LexGetChar(lf);

          lf->state = lex_none;
          lf = lex_open_file(lf, lf->str, lf->ScanError, lf->scan_warning);
          if (lf == NULL) {
            BErrNo be;
            scan_err2(lfori, _("Cannot open included config file %s: %s\n"),
                      lfori->str, be.bstrerror());
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


        if (B_ISSPACE(ch) || ch == '\n' || ch == L_EOL || ch == '}' ||
            ch == '{' || ch == ';' || ch == ',' || ch == '"' || ch == '#') {
          /* Keep the original LEX so we can print an error if the included file
           * can't be opened. */
          LEX* lfori = lf;

          lf->state = lex_none;
          lf = lex_open_file(lf, lf->str, lf->ScanError, lf->scan_warning);
          if (lf == NULL) {
            BErrNo be;
            scan_err2(lfori, _("Cannot open included config file %s: %s\n"),
                      lfori->str, be.bstrerror());
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
  Dmsg2(debuglevel, "lex returning: line %d token: %s\n", lf->line_no,
        lex_tok_to_str(token));
  lf->token = token;

  /*
   * Here is where we check to see if the user has set certain
   *  expectations (e.g. 32 bit integer). If so, we do type checking
   *  and possible additional scanning (e.g. for range).
   */
  switch (expect) {
    case BCT_PINT16:
      lf->u.pint16_val = (scan_pint(lf, lf->str) & 0xffff);
      lf->u2.pint16_val = lf->u.pint16_val;
      token = BCT_PINT16;
      break;

    case BCT_PINT32:
      lf->u.pint32_val = scan_pint(lf, lf->str);
      lf->u2.pint32_val = lf->u.pint32_val;
      token = BCT_PINT32;
      break;

    case BCT_PINT32_RANGE:
      if (token == BCT_NUMBER) {
        lf->u.pint32_val = scan_pint(lf, lf->str);
        lf->u2.pint32_val = lf->u.pint32_val;
        token = BCT_PINT32;
      } else {
        char* p = strchr(lf->str, '-');
        if (!p) {
          scan_err2(lf, _("expected an integer or a range, got %s: %s"),
                    lex_tok_to_str(token), lf->str);
          token = BCT_ERROR;
          break;
        }
        *p++ = 0; /* Terminate first half of range */
        lf->u.pint32_val = scan_pint(lf, lf->str);
        lf->u2.pint32_val = scan_pint(lf, p);
        token = BCT_PINT32_RANGE;
      }
      break;

    case BCT_INT16:
      if (token != BCT_NUMBER || !Is_a_number(lf->str)) {
        scan_err2(lf, _("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str);
        token = BCT_ERROR;
        break;
      }
      errno = 0;
      lf->u.int16_val = (int16_t)str_to_int64(lf->str);
      if (errno != 0) {
        scan_err2(lf, _("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str);
        token = BCT_ERROR;
      } else {
        token = BCT_INT16;
      }
      break;

    case BCT_INT32:
      if (token != BCT_NUMBER || !Is_a_number(lf->str)) {
        scan_err2(lf, _("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str);
        token = BCT_ERROR;
        break;
      }
      errno = 0;
      lf->u.int32_val = (int32_t)str_to_int64(lf->str);
      if (errno != 0) {
        scan_err2(lf, _("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str);
        token = BCT_ERROR;
      } else {
        token = BCT_INT32;
      }
      break;

    case BCT_INT64:
      Dmsg2(debuglevel, "int64=:%s: %f\n", lf->str, strtod(lf->str, NULL));
      if (token != BCT_NUMBER || !Is_a_number(lf->str)) {
        scan_err2(lf, _("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str);
        token = BCT_ERROR;
        break;
      }
      errno = 0;
      lf->u.int64_val = str_to_int64(lf->str);
      if (errno != 0) {
        scan_err2(lf, _("expected an integer number, got %s: %s"),
                  lex_tok_to_str(token), lf->str);
        token = BCT_ERROR;
      } else {
        token = BCT_INT64;
      }
      break;

    case BCT_PINT64_RANGE:
      if (token == BCT_NUMBER) {
        lf->u.pint64_val = scan_pint64(lf, lf->str);
        lf->u2.pint64_val = lf->u.pint64_val;
        token = BCT_PINT64;
      } else {
        char* p = strchr(lf->str, '-');
        if (!p) {
          scan_err2(lf, _("expected an integer or a range, got %s: %s"),
                    lex_tok_to_str(token), lf->str);
          token = BCT_ERROR;
          break;
        }
        *p++ = 0; /* Terminate first half of range */
        lf->u.pint64_val = scan_pint64(lf, lf->str);
        lf->u2.pint64_val = scan_pint64(lf, p);
        token = BCT_PINT64_RANGE;
      }
      break;

    case BCT_NAME:
      if (token != BCT_IDENTIFIER && token != BCT_UNQUOTED_STRING &&
          token != BCT_QUOTED_STRING) {
        scan_err2(lf, _("expected a name, got %s: %s"), lex_tok_to_str(token),
                  lf->str);
        token = BCT_ERROR;
      } else if (lf->str_len > MAX_RES_NAME_LENGTH) {
        scan_err3(lf, _("name %s length %d too long, max is %d\n"), lf->str,
                  lf->str_len, MAX_RES_NAME_LENGTH);
        token = BCT_ERROR;
      }
      break;

    case BCT_STRING:
      if (token != BCT_IDENTIFIER && token != BCT_UNQUOTED_STRING &&
          token != BCT_QUOTED_STRING) {
        scan_err2(lf, _("expected a string, got %s: %s"), lex_tok_to_str(token),
                  lf->str);
        token = BCT_ERROR;
      } else {
        token = BCT_STRING;
      }
      break;


    default:
      break; /* no expectation given */
  }
  lf->token = token; /* set possible new token */
  return token;
}
