/*
 * Copyright (c) 1989, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* OpenBSD: fnmatch.c,v 1.6 1998/03/19 00:29:59 millert */

/*
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a filename or pathname to a pattern.
 */

/* Version: $Id$ */

/* Define SYS to use the system fnmatch() rather than ours */
/* #define SYS 1 */

#include "bacula.h"
#ifdef SYS
#include <fnmatch.h>
#else
#include "fnmatch.h"
#endif

#undef  EOS
#define EOS     '\0'

#define RANGE_MATCH     1
#define RANGE_NOMATCH   0
#define RANGE_ERROR     (-1)

/* Limit of recursion during matching attempts. */
#define FNM_MAX_RECUR 64

#define ISSET(x, y) ((x) & (y))
#define FOLD(c) ((flags & FNM_CASEFOLD) && B_ISUPPER(c) ? tolower(c) : (c))

static int rangematch(const char *, char, int, char **);
static int r_fnmatch(const char *, const char *, int, int);

#ifdef SYS 
int xfnmatch(const char *pattern, const char *string, int flags)
#else
int fnmatch(const char *pattern, const char *string, int flags)
#endif
{
   int e;

   e = r_fnmatch(pattern, string, flags, FNM_MAX_RECUR);
   if (e == -1) {               /* Too much recursion */
      e = FNM_NOMATCH;
   }
   return (e);
}

static 
int r_fnmatch(const char *pattern, const char *string, int flags, int recur)
{
   const char *stringstart;
   char *newp;
   char c, test;
   int e;

   if (recur-- <= 0) {
      return (-1);
   }

   stringstart = string;
   for ( ;; ) {
      switch (c = *pattern++) {
      case EOS:
         if (ISSET(flags, FNM_LEADING_DIR) && IsPathSeparator(*string))
            return (0);
         return (*string == EOS ? 0 : FNM_NOMATCH);
      case '?':
         if (*string == EOS)
            return (FNM_NOMATCH);
         if (IsPathSeparator(*string) && ISSET(flags, FNM_PATHNAME))
            return (FNM_NOMATCH);
         if (*string == '.' && ISSET(flags, FNM_PERIOD) &&
             (string == stringstart ||
              (ISSET(flags, FNM_PATHNAME) && IsPathSeparator(*(string - 1)))))
            return (FNM_NOMATCH);
         ++string;
         break;
      case '*':
         c = *pattern;
         /* Collapse multiple stars. */
         while (c == '*') {
            c = *++pattern;
         }

         if (*string == '.' && ISSET(flags, FNM_PERIOD) &&
             (string == stringstart ||
              (ISSET(flags, FNM_PATHNAME) && IsPathSeparator(*(string - 1))))) {
            return (FNM_NOMATCH);
         }

         /* Optimize for pattern with * at end or before /. */
         if (c == EOS) {
            if (ISSET(flags, FNM_PATHNAME)) {
               return (ISSET(flags, FNM_LEADING_DIR) ||
                       strchr(string, '/') == NULL ? 0 : FNM_NOMATCH);
            } else {
               return (0);
            }
         } else if (IsPathSeparator(c) && ISSET(flags, FNM_PATHNAME)) {
            if ((string = strchr(string, '/')) == NULL)
               return (FNM_NOMATCH);
            break;
         }

         /* General case, use recursion. */
         while ((test = *string) != EOS) {
            e = r_fnmatch(pattern, string, flags & ~FNM_PERIOD, recur);
            if (e != FNM_NOMATCH) { /* can be NOMATCH, -1 or MATCH */
               return (e);
            }
            if (test == '/' && ISSET(flags, FNM_PATHNAME)) {
               break;
            }
            ++string;
         }
         return (FNM_NOMATCH);
      case '[':
         if (*string == EOS)
            return (FNM_NOMATCH);
         if (IsPathSeparator(*string) && ISSET(flags, FNM_PATHNAME))
            return (FNM_NOMATCH);
         if (*string == '.' && ISSET(flags, FNM_PERIOD) &&
             (string == stringstart ||
              (ISSET(flags, FNM_PATHNAME) && IsPathSeparator(*(string - 1)))))
            return (FNM_NOMATCH);

         switch (rangematch(pattern, *string, flags, &newp)) {
         case RANGE_ERROR:
            /* not a good range, treat as normal text */
            goto normal;
         case RANGE_MATCH:
            pattern = newp;
            break;
         case RANGE_NOMATCH:
            return (FNM_NOMATCH);
         }
         ++string;
         break;

      case '\\':
         if (!ISSET(flags, FNM_NOESCAPE)) {
            if ((c = *pattern++) == EOS) {
               c = '\\';
               --pattern;
            }
         }
         /* FALLTHROUGH */
      default:
normal:
         if (FOLD(c) != FOLD(*string)) {
            return (FNM_NOMATCH);
         }
         ++string;
         break;
      }
   }
   /* NOTREACHED */
}

static int rangematch(const char *pattern, char test, int flags,
                      char **newp)
{
   int negate, ok;
   char c, c2;

   /*
    * A bracket expression starting with an unquoted circumflex
    * character produces unspecified results (IEEE 1003.2-1992,
    * 3.13.2).  This implementation treats it like '!', for
    * consistency with the regular expression syntax.
    * J.T. Conklin (conklin@ngai.kaleida.com)
    */
   if ((negate = (*pattern == '!' || *pattern == '^')))
      ++pattern;

   test = FOLD(test);

   /*
    * A right bracket shall lose its special meaning and represent
    * itself in a bracket expression if it occurs first in the list.
    * -- POSIX.2 2.8.3.2
    */
   ok = 0;
   c = *pattern++;
   do {
      if (c == '\\' && !ISSET(flags, FNM_NOESCAPE))
         c = *pattern++;
      if (c == EOS)
         return (RANGE_ERROR);
      if (c == '/' && ISSET(flags, FNM_PATHNAME))
         return (RANGE_NOMATCH);
      c = FOLD(c);
      if (*pattern == '-' && (c2 = *(pattern + 1)) != EOS && c2 != ']') {
         pattern += 2;
         if (c2 == '\\' && !ISSET(flags, FNM_NOESCAPE))
            c2 = *pattern++;
         if (c2 == EOS)
            return (RANGE_ERROR);
         c2 = FOLD(c2);
         if (c <= test && test <= c2)
            ok = 1;
      } else if (c == test)
         ok = 1;
   } while ((c = *pattern++) != ']');

   *newp = (char *) pattern;
   return (ok == negate ? RANGE_NOMATCH : RANGE_MATCH);
}

#ifdef TEST_PROGRAM
struct test {
   const char *pattern;
   const char *string;
   const int options;
   const int result; 
};

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

#define ntests ((int)(sizeof(tests)/sizeof(struct test)))

int main()
{
   bool fail = false;
   for (int i=0; i<ntests; i++) {
      if (fnmatch(tests[i].pattern, tests[i].string, tests[i].options) != tests[i].result) {
         printf("Test %d failed: pat=%s str=%s expect=%s got=%s\n",
            i+1, tests[i].pattern, tests[i].string, 
            tests[i].result==0?"matches":"no match",
            tests[i].result==0?"no match":"matches");
         fail = true;
      } else {
         printf("Test %d succeeded\n", i+1);
      }
   }
   return fail;
}
#endif /* TEST_PROGRAM */
