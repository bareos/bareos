/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2015 Bareos GmbH & Co. KG

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
/* originally was Kern Sibbald, December MMII */
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for unittest framework cmocka
 *
 * Philipp Storz, April 2015
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

extern "C" {
#include <cmocka.h>
}

#include "bareos.h"
#include "../lib/protos.h"
#include "protos.h"


void d_msg(const char*, int, int, const char*, ...) {}

void test_edit(void **state) {
   (void) state;
   char *str[] = {"3", "3n", "3 hours", "3.5 day", "3 week", "3 m", "3 q", "3 years"};
   char *resultstr[] = {"3 secs", "3 mins ", "3 hours ", "3 days 12 hours ", "21 days ", "3 months ", "9 months 3 days ", "3 years "};
   utime_t val;
   char buf[100];
   char outval[100];

   for (int i=0; i<8; i++) {
      strcpy(buf, str[i]);
      if (!duration_to_utime(buf, &val)) {
         printf("Error return from duration_to_utime for in=%s\n", str[i]);
         continue;
      }
      edit_utime(val, outval, sizeof(outval));
      assert_string_equal(outval, resultstr[i] );
   }
}



struct test {
   const char *pattern;
   const char *string;
   const int options;
   const int result;
};
//define FULL_TEST
/*
 * Note, some of these tests were duplicated from a patch file I found
 *  in an email, so I am unsure what the license is.  Since this code is
 *  never turned on in any release, it probably doesn't matter at all.
 * If by some chance someone finds this to be a problem please let
 *  me know.
 */
static struct test tests[] = {
/*1*/  {"x", "x", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"x", "x/y", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"x", "x/y/z", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"*", "x", FNM_PATHNAME | FNM_LEADING_DIR, 0},
/*5*/  {"*", "x/y", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"*", "x/y/z", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"*x", "x", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"*x", "x/y", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"*x", "x/y/z", FNM_PATHNAME | FNM_LEADING_DIR, 0},
/*10*/ {"x*", "x", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"x*", "x/y", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"x*", "x/y/z", FNM_PATHNAME | FNM_LEADING_DIR, 0},
       {"a*b/*", "abbb/.x", FNM_PATHNAME|FNM_PERIOD, FNM_NOMATCH},
       {"a*b/*", "abbb/xy", FNM_PATHNAME|FNM_PERIOD, 0},
/*15*/ {"[A-[]", "A", 0, 0},
       {"[A-[]", "a", 0, FNM_NOMATCH},
       {"[a-{]", "A", 0, FNM_NOMATCH},
       {"[a-{]", "a", 0, 0},
       {"[A-[]", "A", FNM_CASEFOLD, FNM_NOMATCH},
/*20*/ {"[A-[]", "a", FNM_CASEFOLD, FNM_NOMATCH},
       {"[a-{]", "A", FNM_CASEFOLD, 0},
       {"[a-{]", "a", FNM_CASEFOLD, 0},
       { "*LIB*", "lib", FNM_PERIOD, FNM_NOMATCH },
       { "*LIB*", "lib", FNM_CASEFOLD, 0},
/*25*/ { "a[/]b", "a/b", 0, 0},
       { "a[/]b", "a/b", FNM_PATHNAME, FNM_NOMATCH },
       { "[a-z]/[a-z]", "a/b", 0, 0 },
       { "a/b", "*", FNM_PATHNAME, FNM_NOMATCH },
       { "*", "a/b", FNM_PATHNAME, FNM_NOMATCH },
       { "*[/]b", "a/b", FNM_PATHNAME, FNM_NOMATCH },
/*30*/ { "\\[/b", "[/b", 0, 0 },
       { "?\?/b", "aa/b", 0, 0 },
       { "???b", "aa/b", 0, 0 },
       { "???b", "aa/b", FNM_PATHNAME, FNM_NOMATCH },
       { "?a/b", ".a/b", FNM_PATHNAME|FNM_PERIOD, FNM_NOMATCH },
/*35*/ { "a/?b", "a/.b", FNM_PATHNAME|FNM_PERIOD, FNM_NOMATCH },
       { "*a/b", ".a/b", FNM_PATHNAME|FNM_PERIOD, FNM_NOMATCH },
       { "a/*b", "a/.b", FNM_PATHNAME|FNM_PERIOD, FNM_NOMATCH },
       { "[.]a/b", ".a/b", FNM_PATHNAME|FNM_PERIOD, FNM_NOMATCH },
       { "a/[.]b", "a/.b", FNM_PATHNAME|FNM_PERIOD, FNM_NOMATCH },
/*40*/ { "*/?", "a/b", FNM_PATHNAME|FNM_PERIOD, 0 },
       { "?/*", "a/b", FNM_PATHNAME|FNM_PERIOD, 0 },
       { ".*/?", ".a/b", FNM_PATHNAME|FNM_PERIOD, 0 },
       { "*/.?", "a/.b", FNM_PATHNAME|FNM_PERIOD, 0 },
       { "*/*", "a/.b", FNM_PATHNAME|FNM_PERIOD, FNM_NOMATCH },
/*45*/ { "*[.]/b", "a./b", FNM_PATHNAME|FNM_PERIOD, 0 },
       { "a?b", "a.b", FNM_PATHNAME|FNM_PERIOD, 0 },
       { "a*b", "a.b", FNM_PATHNAME|FNM_PERIOD, 0 },
       { "a[.]b", "a.b", FNM_PATHNAME|FNM_PERIOD, 0 },
/*49*/ { "*a*", "a/b", FNM_PATHNAME|FNM_LEADING_DIR, 0 },
       { "[/b", "[/b", 0, 0},
#ifdef FULL_TEST
       /* This test takes a *long* time */
       {"a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p*q*r*s*t*u*v*w*x*y*z*",
          "aaaabbbbccccddddeeeeffffgggghhhhiiiijjjjkkkkllllmmmm"
          "nnnnooooppppqqqqrrrrssssttttuuuuvvvvwwwwxxxxyyyy", 0, FNM_NOMATCH},
#endif

       /* Keep dummy last to avoid compiler warnings */
       {"dummy", "dummy", 0, 0}

};



static void test_fnmatch(void **state) {
      (void) state;

#define ntests ((int)(sizeof(tests)/sizeof(struct test)))

   bool fail = false;
   for (int i=0; i<ntests; i++) {
      assert_return_code(0, fnmatch(tests[i].pattern, tests[i].string, tests[i].options) != tests[i].result);
   }
}
