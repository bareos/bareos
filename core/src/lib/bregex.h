/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2010 Free Software Foundation Europe e.V.
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
 * regexpr.h
 *
 * Author: Tatu Ylonen <ylo@ngs.fi>
 *
 * Copyright (c) 1991 Tatu Ylonen, Espoo, Finland
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies.  This
 * software is provided "as is" without express or implied warranty.
 *
 * Created: Thu Sep 26 17:15:36 1991 ylo
 * Last modified: Mon Nov  4 15:49:46 1991 ylo
 *
 * Modified to work with C++ for use in BAREOS,
 * Kern Sibbald April, 2006
 */
/**
 * @file
 * regexpr.h
 * regular expression header file
 *
 */

#ifndef BAREOS_LIB_BREGEX_H_
#define BAREOS_LIB_BREGEX_H_
#ifdef __cplusplus
extern "C" {
#endif

#ifndef REGEXPR_H
#define REGEXPR_H

/* If we pull in this header, make sure we only get our own library
 *  bregex.c
 */
#define regex_t               b_regex_t
#define regmatch_t            b_regmatch_t
#define re_syntax             b_re_syntax
#define re_syntax_table       b_re_syntax_table
#define ReCompileInitialize b_re_compile_initialize
#define re_set_syntax         b_re_set_syntax
#define re_compile_pattern    b_re_compile_pattern
#define ReMatch              b_re_match
#define ReSearch             b_re_search
#define ReCompileFastmap    b_re_compile_fastmap
#define re_comp               b_re_comp
#define re_exec               b_re_exec
#define regcomp               b_regcomp
#define regexec               b_regexec
#define regerror              b_regerror
#define regfree               b_regfree


#define RE_NREGS        100  /* number of registers available */

#define regoff_t int

typedef struct {
   regoff_t rm_so;
   regoff_t rm_eo;
} regmatch_t;


#define REG_EXTENDED (1<<1)
#define REG_ICASE    (1<<2)
#define REG_NOSUB    (1<<3)
#define REG_NEWLINE  (1<<4)
#define REG_NOTBOL   (1<<5)

#define REG_NOMATCH -1

struct regex_t
{
   unsigned char *buffer;          /* compiled pattern */
   int allocated;         /* allocated size of compiled pattern */
   int used;              /* actual length of compiled pattern */
   unsigned char *fastmap;         /* fastmap[ch] is true if ch can start pattern */
   unsigned char *translate;       /* translation to apply during compilation/matching */
   unsigned char fastmap_accurate; /* true if fastmap is valid */
   unsigned char can_be_null;      /* true if can match empty string */
   unsigned char uses_registers;   /* registers are used and need to be initialized */
   int num_registers;     /* number of registers used */
   unsigned char anchor;           /* anchor: 0=none 1=begline 2=begbuf */
   char *errmsg;
   int cflags;                     /* compilation flags */
   POOLMEM *lcase;                 /* used by REG_ICASE */
};


typedef struct re_registers
{
   int start[RE_NREGS];  /* start offset of region */
   int end[RE_NREGS];    /* end offset of region */
} *regexp_registers_t;

/* bit definitions for syntax */
#define RE_NO_BK_PARENS         1    /* no quoting for parentheses */
#define RE_NO_BK_VBAR           2    /* no quoting for vertical bar */
#define RE_BK_PLUS_QM           4    /* quoting needed for + and ? */
#define RE_TIGHT_VBAR           8    /* | binds tighter than ^ and $ */
#define RE_NEWLINE_OR           16   /* treat newline as or */
#define RE_CONTEXT_INDEP_OPS    32   /* ^$?*+ are special in all contexts */
#define RE_ANSI_HEX             64   /* ansi sequences (\n etc) and \xhh */
#define RE_NO_GNU_EXTENSIONS   128   /* no gnu extensions */

/* definitions for some common regexp styles */
#define RE_SYNTAX_AWK   (RE_NO_BK_PARENS|RE_NO_BK_VBAR|RE_CONTEXT_INDEP_OPS)
#define RE_SYNTAX_EGREP (RE_SYNTAX_AWK|RE_NEWLINE_OR)
#define RE_SYNTAX_GREP  (RE_BK_PLUS_QM|RE_NEWLINE_OR)
#define RE_SYNTAX_EMACS 0

#define Sword       1
#define Swhitespace 2
#define Sdigit      4
#define Soctaldigit 8
#define Shexdigit   16

/* Rename all exported symbols to avoid conflicts with similarly named
   symbols in some systems' standard C libraries... */


extern int re_syntax;
/* This is the actual syntax mask.  It was added so that Python could do
 * syntax-dependent munging of patterns before compilation. */

extern unsigned char re_syntax_table[256];

void ReCompileInitialize(void);

int re_set_syntax(int syntax);
/* This sets the syntax to use and returns the previous syntax.  The
 * syntax is specified by a bit mask of the above defined bits. */

const char *re_compile_pattern(regex_t *compiled, unsigned char *regex);
/* This compiles the regexp (given in regex and length in regex_size).
 * This returns NULL if the regexp compiled successfully, and an error
 * message if an error was encountered.  The buffer field must be
 * initialized to a memory area allocated by malloc (or to NULL) before
 * use, and the allocated field must be set to its length (or 0 if
 * buffer is NULL).  Also, the translate field must be set to point to a
 * valid translation table, or NULL if it is not used. */

int ReMatch(regex_t *compiled, unsigned char *string, int size, int pos,
             regexp_registers_t old_regs);
/* This tries to match the regexp against the string.  This returns the
 * length of the matched portion, or -1 if the pattern could not be
 * matched and -2 if an error (such as failure stack overflow) is
 * encountered. */

int ReSearch(regex_t *compiled, unsigned char *string, int size, int startpos,
              int range, regexp_registers_t regs);
/* This searches for a substring matching the regexp.  This returns the
 * first index at which a match is found.  range specifies at how many
 * positions to try matching; positive values indicate searching
 * forwards, and negative values indicate searching backwards.  mstop
 * specifies the offset beyond which a match must not go.  This returns
 * -1 if no match is found, and -2 if an error (such as failure stack
 * overflow) is encountered. */

void ReCompileFastmap(regex_t *compiled);
/* This computes the fastmap for the regexp.  For this to have any effect,
 * the calling program must have initialized the fastmap field to point
 * to an array of 256 characters. */


DLL_IMP_EXP int regcomp(regex_t *preg, const char *regex, int cflags);
DLL_IMP_EXP int regexec(regex_t *preg, const char *string, size_t nmatch,
            regmatch_t pmatch[], int eflags);
DLL_IMP_EXP size_t regerror(int errcode, regex_t *preg, char *errbuf,
                size_t errbuf_size);
DLL_IMP_EXP void regfree(regex_t *preg);

#endif /* REGEXPR_H */



#ifdef __cplusplus
}
#endif
#endif /* !BAREOS_LIB_BREGEX_H_ */
