/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, MM
 */
/**
 * @file
 * Lexical scanning of configuration files, used by parsers.
 */

#ifndef _LEX_H
#define _LEX_H

/* Lex get_char() return values */
#define L_EOF                         (-1)
#define L_EOL                         (-2)

/* Internal tokens */
#define T_NONE                        100

/* Tokens returned by get_token() */
#define T_EOF                         101
#define T_NUMBER                      102
#define T_IPADDR                      103
#define T_IDENTIFIER                  104
#define T_UNQUOTED_STRING             105
#define T_QUOTED_STRING               106
#define T_BOB                         108  /* begin block */
#define T_EOB                         109  /* end of block */
#define T_EQUALS                      110
#define T_COMMA                       111
#define T_EOL                         112
#define T_ERROR                       200
#define T_UTF8_BOM                    201 /* File starts with a UTF-8 BOM*/
#define T_UTF16_BOM                   202 /* File starts with a UTF-16LE BOM*/

/**
 * The following will be returned only if
 * the appropriate expect flag has been set
 */
#define T_SKIP_EOL                    113  /* scan through EOLs */
#define T_PINT16                      114  /* 16 bit positive integer */
#define T_PINT32                      115  /* 32 bit positive integer */
#define T_PINT32_RANGE                116  /* 32 bit positive integer range */
#define T_INT16                       117  /* 16 bit integer */
#define T_INT32                       118  /* 32 bit integer */
#define T_INT64                       119  /* 64 bit integer */
#define T_NAME                        120  /* name max 128 chars */
#define T_STRING                      121  /* string */
#define T_PINT64_RANGE                122  /* positive integer range */
#define T_PINT64                      123  /* positive integer range */

#define T_ALL                           0  /* no expectations */

/* Lexical state */
enum lex_state {
   lex_none,
   lex_comment,
   lex_number,
   lex_ip_addr,
   lex_identifier,
   lex_string,
   lex_quoted_string,
   lex_include_quoted_string,
   lex_include,
   lex_utf8_bom,      /* we are parsing out a utf8 byte order mark */
   lex_utf16_le_bom   /* we are parsing out a utf-16 (little endian) byte order mark */
};

/* Lex scan options */
#define LOPT_NO_IDENT            0x1  /* No Identifiers -- use string */
#define LOPT_STRING              0x2  /* Force scan for string */
#define LOPT_NO_EXTERN           0x4  /* Don't follow @ command */

class BPIPE;                          /* forward reference */

/* Lexical context */
typedef struct s_lex_context {
   struct s_lex_context *next;        /* pointer to next lexical context */
   int options;                       /* scan options */
   char *fname;                       /* filename */
   FILE *fd;                          /* file descriptor */
   POOLMEM *line;                     /* input line */
   POOLMEM *str;                      /* string being scanned */
   int str_len;                       /* length of string */
   int str_max_len;                   /* maximum length of string */
   int line_no;                       /* file line number */
   int col_no;                        /* char position on line */
   int begin_line_no;                 /* line no of beginning of string */
   enum lex_state state;              /* lex_state variable */
   int ch;                            /* last char/L_VAL returned by get_char */
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
   void (*scan_error)(const char *file, int line, struct s_lex_context *lc, const char *msg, ...);
   void (*scan_warning)(const char *file, int line, struct s_lex_context *lc, const char *msg, ...);
   int err_type;                      /* message level for scan_error (M_..) */
   int error_counter;
   void *caller_ctx;                  /* caller private data */
   BPIPE *bpipe;                      /* set if we are piping */
} LEX;

typedef void (LEX_ERROR_HANDLER)(const char *file, int line, LEX *lc, const char *msg, ...);
typedef void (LEX_WARNING_HANDLER)(const char *file, int line, LEX *lc, const char *msg, ...);

/**
 * Lexical scanning errors in parsing conf files
 */
#define scan_err0(lc, msg) \
lc->scan_error(__FILE__, __LINE__, lc, msg)
#define scan_err1(lc, msg, a1) \
lc->scan_error(__FILE__, __LINE__, lc, msg, a1)
#define scan_err2(lc, msg, a1, a2) \
lc->scan_error(__FILE__, __LINE__, lc, msg, a1, a2)
#define scan_err3(lc, msg, a1, a2, a3) \
lc->scan_error(__FILE__, __LINE__, lc, msg, a1, a2, a3)
#define scan_err4(lc, msg, a1, a2, a3, a4) \
lc->scan_error(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4)
#define scan_err5(lc, msg, a1, a2, a3, a4, a5) \
lc->scan_error(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4, a5)
#define scan_err6(lc, msg, a1, a2, a3, a4, a5, a6) \
lc->scan_error(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4, a5, a6)

/**
 * Lexical scanning warnings in parsing conf files
 */
#define scan_warn0(lc, msg) \
lc->scan_warning(__FILE__, __LINE__, lc, msg)
#define scan_warn1(lc, msg, a1) \
lc->scan_warning(__FILE__, __LINE__, lc, msg, a1)
#define scan_warn2(lc, msg, a1, a2) \
lc->scan_warning(__FILE__, __LINE__, lc, msg, a1, a2)
#define scan_warn3(lc, msg, a1, a2, a3) \
lc->scan_warning(__FILE__, __LINE__, lc, msg, a1, a2, a3)
#define scan_warn4(lc, msg, a1, a2, a3, a4) \
lc->scan_warning(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4)
#define scan_warn5(lc, msg, a1, a2, a3, a4, a5) \
lc->scan_warning(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4, a5)
#define scan_warn6(lc, msg, a1, a2, a3, a4, a5, a6) \
lc->scan_warning(__FILE__, __LINE__, lc, msg, a1, a2, a3, a4, a5, a6)

DLL_IMP_EXP void scan_to_eol(LEX *lc);
DLL_IMP_EXP int scan_to_next_not_eol(LEX * lc);

#endif /* _LEX_H */
