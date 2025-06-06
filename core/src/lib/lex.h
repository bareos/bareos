/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, MM
/**
 * @file
 * Lexical scanning of configuration files, used by parsers.
 */

#ifndef BAREOS_LIB_LEX_H_
#define BAREOS_LIB_LEX_H_

#include "include/bareos.h"

/* Lex get_char() return values */
#define L_EOF (-1)
#define L_EOL (-2)

/* Internal tokens */
#define BCT_NONE 100

/* Tokens returned by get_token() */
#define BCT_EOF 101
#define BCT_NUMBER 102
#define BCT_IPADDR 103
#define BCT_IDENTIFIER 104
#define BCT_UNQUOTED_STRING 105
#define BCT_QUOTED_STRING 106
#define BCT_BOB 108 /* begin block */
#define BCT_EOB 109 /* end of block */
#define BCT_EQUALS 110
#define BCT_COMMA 111
#define BCT_EOL 112
#define BCT_ERROR 200
#define BCT_UTF8_BOM 201  /* File starts with a UTF-8 BOM*/
#define BCT_UTF16_BOM 202 /* File starts with a UTF-16LE BOM*/

/**
 * The following will be returned only if
 * the appropriate expect flag has been set
 */
#define BCT_SKIP_EOL 113     /* scan through EOLs */
#define BCT_PINT16 114       /* 16 bit positive integer */
#define BCT_PINT32 115       /* 32 bit positive integer */
#define BCT_PINT32_RANGE 116 /* 32 bit positive integer range */
#define BCT_INT16 117        /* 16 bit integer */
#define BCT_INT32 118        /* 32 bit integer */
#define BCT_INT64 119        /* 64 bit integer */
#define BCT_NAME 120         /* name max 128 chars */
#define BCT_STRING 121       /* string */
#define BCT_PINT64_RANGE 122 /* positive integer range */
#define BCT_PINT64 123       /* positive integer range */

#define BCT_ALL 0 /* no expectations */

/* Lexical state */
enum lex_state
{
  lex_none,
  lex_comment,
  lex_number,
  lex_ip_addr,
  lex_identifier,
  lex_string,
  lex_quoted_string,
  lex_include_quoted_string,
  lex_include,
  lex_utf8_bom,    /* we are parsing out a utf8 byte order mark */
  lex_utf16_le_bom /* we are parsing out a utf-16 (little endian) byte order
                      mark */
};

class Bpipe; /* forward reference */

#include <bitset>

/* Lexical context */
struct lexer {
  enum options : size_t
  {
    /* Lex scan options */
    NoIdent = 0,     /* parse identifiers as simple strings */
    ForceString = 1, /* parse identifiers and numbers as strings */
    NoExtern = 2,    /* ignore includes */
  };

  lexer* next;            /* pointer to next lexical context */
  std::bitset<3> options; /* scan options */
  char* fname;            /* filename */
  FILE* fd;               /* file descriptor */
  POOLMEM* line;          /* input line */
  POOLMEM* str;           /* string being scanned */
  int str_len;            /* length of string */
  int str_max_len;        /* maximum length of string */
  int line_no;            /* file line number */
  int col_no;             /* char position on line */
  int begin_line_no;      /* line no of beginning of string */
  enum lex_state state;   /* lex_state variable */
  int ch;                 /* last char/L_VAL returned by get_char */
  int token;
  union {
    uint16_t pint16_val;
    uint32_t pint32_val;
    uint64_t pint64_val;
    int16_t int16_val;
    int32_t int32_val;
    int64_t int64_val;
  } u;
  union {
    uint16_t pint16_val;
    uint32_t pint32_val;
    uint64_t pint64_val;
  } u2;

  using error_handler = PRINTF_LIKE(4, 5) void(const char* file,
                                               int line,
                                               lexer* lc,
                                               const char* msg,
                                               ...);
  using warning_handler = PRINTF_LIKE(4, 5) void(const char* file,
                                                 int line,
                                                 lexer* lc,
                                                 const char* msg,
                                                 ...);


  error_handler* scan_error;
  warning_handler* scan_warning;
  int err_type; /* message level for scan_error (M_..) */
  int error_counter;
  void* caller_ctx; /* caller private data */
  Bpipe* bpipe;     /* set if we are piping */
};

// Lexical scanning errors in parsing conf files
#define scan_err0(lc, msg) (lc)->scan_error(__FILE__, __LINE__, (lc), msg)
#define scan_err1(lc, msg, a1) \
  (lc)->scan_error(__FILE__, __LINE__, (lc), msg, a1)
#define scan_err2(lc, msg, a1, a2) \
  (lc)->scan_error(__FILE__, __LINE__, (lc), msg, a1, a2)
#define scan_err3(lc, msg, a1, a2, a3) \
  (lc)->scan_error(__FILE__, __LINE__, (lc), msg, a1, a2, a3)
#define scan_err4(lc, msg, a1, a2, a3, a4) \
  (lc)->scan_error(__FILE__, __LINE__, (lc), msg, a1, a2, a3, a4)
#define scan_err5(lc, msg, a1, a2, a3, a4, a5) \
  (lc)->scan_error(__FILE__, __LINE__, (lc), msg, a1, a2, a3, a4, a5)
#define scan_err6(lc, msg, a1, a2, a3, a4, a5, a6) \
  (lc)->scan_error(__FILE__, __LINE__, (lc), msg, a1, a2, a3, a4, a5, a6)

// Lexical scanning warnings in parsing conf files
#define scan_warn0(lc, msg) (lc)->scan_warning(__FILE__, __LINE__, (lc), msg)
#define scan_warn1(lc, msg, a1) \
  (lc)->scan_warning(__FILE__, __LINE__, (lc), msg, a1)
#define scan_warn2(lc, msg, a1, a2) \
  (lc)->scan_warning(__FILE__, __LINE__, (lc), msg, a1, a2)
#define scan_warn3(lc, msg, a1, a2, a3) \
  (lc)->scan_warning(__FILE__, __LINE__, (lc), msg, a1, a2, a3)
#define scan_warn4(lc, msg, a1, a2, a3, a4) \
  (lc)->scan_warning(__FILE__, __LINE__, (lc), msg, a1, a2, a3, a4)
#define scan_warn5(lc, msg, a1, a2, a3, a4, a5) \
  (lc)->scan_warning(__FILE__, __LINE__, (lc), msg, a1, a2, a3, a4, a5)
#define scan_warn6(lc, msg, a1, a2, a3, a4, a5, a6) \
  (lc)->scan_warning(__FILE__, __LINE__, (lc), msg, a1, a2, a3, a4, a5, a6)

void ScanToEol(lexer* lc);
int ScanToNextNotEol(lexer* lc);

lexer* LexCloseFile(lexer* lf);
lexer* lex_open_file(lexer* lf,
                     const char* fname,
                     lexer::error_handler* ScanError,
                     lexer::warning_handler* scan_warning);
int LexGetChar(lexer* lf);
void LexUngetChar(lexer* lf);
const char* lex_tok_to_str(int token);
int LexGetToken(lexer* lf, int expect);
void LexSetDefaultErrorHandler(lexer* lf);
void LexSetDefaultWarningHandler(lexer* lf);
void LexSetErrorHandlerErrorType(lexer* lf, int err_type);

#endif  // BAREOS_LIB_LEX_H_
