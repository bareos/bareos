/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2006 Free Software Foundation Europe e.V.
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
// Eric Bollengier March 2007
/**
 * @file
 * BAREOS BareosRegex Structure definition for FileDaemon
 */

#ifndef BAREOS_LIB_BREG_H_
#define BAREOS_LIB_BREG_H_

#if __has_include(<regex.h>)
#  include <regex.h>
#else
#  include "lib/bregex.h"
#endif

/* Usage:
 *
 * #include "lib/breg.h"
 *
 * BareosRegex *breg = NewBregexp("!/prod!/test!");
 * char *filename = breg->replace("/prod/data.dat");
 *   or
 * char *filename = breg->result;
 * FreeBregexp(breg);
 */

#define BREG_NREGS 11

// Structure for BareosRegex ressource
class BareosRegex {
 public:
  BareosRegex() = default;
  ~BareosRegex() = default;

  POOLMEM* result = nullptr; /**< match result */
  bool success = false;      /**< match is ok */

  char* replace(const char* fname); /**< return this.result */

  /* private */
  POOLMEM* expr = nullptr;       /**< search epression */
  POOLMEM* subst = nullptr;      /**< substitution */
  regex_t preg{};                /**< regex_t result of regcomp() */
  regmatch_t regs[BREG_NREGS]{}; /**< contains match */
  char* eor = nullptr;           /**< end of regexp in expr */

  char* ReturnFname(const char* fname, int len); /**< return fname as result */
  char* EditSubst(const char* fname, regmatch_t pmatch[]);
  int ComputeDestLen(const char* fname, regmatch_t pmatch[]);
  bool ExtractRegexp(const char* motif);
};

/* create new BareosRegex and compile regex_t */
BareosRegex* NewBregexp(const char* motif);
template <typename T> class alist;

/* launch each bregexp on filename */
int RunBregexp(alist<BareosRegex*>* bregexps, const char* fname);

/* free BareosRegex (and all POOLMEM) */
void FreeBregexp(BareosRegex* script);

/* fill an alist with BareosRegex from where */
alist<BareosRegex*>* get_bregexps(const char* where);

/* apply every regexps from the alist */
bool ApplyBregexps(const char* fname,
                   alist<BareosRegex*>* bregexps,
                   char** result);

/* foreach_alist free RunScript */
void FreeBregexps(alist<BareosRegex*>* bregexps); /* you have to free alist */

/* get regexp size */
int BregexpGetBuildWhereSize(char* strip_prefix,
                             char* add_prefix,
                             char* add_suffix);

/* get a bregexp string from user arguments
 * you must allocate it with BregexpGetBuildWhereSize();
 */
char* bregexp_build_where(char* dest,
                          int str_size,
                          char* strip_prefix,
                          char* add_prefix,
                          char* add_suffix);

/* escape a string to regexp format (sep and \)
 * dest must be long enough (dest = 2*src + 1)
 */
char* bregexp_escape_string(char* dest, const char* src, const char sep);

#endif  // BAREOS_LIB_BREG_H_
