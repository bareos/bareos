/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2015      Bareos GmbH & Co. KG

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
/* originally was Kern Sibbald, November MMV */
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for unittest framework cmocka
 *
 * Philipp Storz, April 2015
 */
#include "gtest/gtest.h"
#include "bareos.h"
#include "../lib/protos.h"
#include "protos.h"


TEST(bsnprintf, bsnprintf) {
#define FP_OUTPUT
#ifndef LONG_STRING
#define LONG_STRING 1024
   char buf1[LONG_STRING];
   char buf2[LONG_STRING];
#endif
#ifdef FP_OUTPUT
   const char *fp_fmt[] = {
      "%-1.5f",
      "%1.5f",
      "%123.9f",
      "%10.5f",
      "% 10.5f",
      "%+22.9f",
      "%+4.9f",
      "%01.3f",
      "%4f",
      "%3.1f",
      "%3.2f",
      "%.1f",
      "%.0f",
      NULL
   };
   double fp_nums[] = { 0, -1.5, 134.21, 91340.2, 341.1234, 0203.9, 0.96, 0.996,
      0.9996, 1.996, 4.136, 6442452944.1234, 23365.5
   };
#endif
   const char *int_fmt[] = {
      "%-1.5d",
      "%1.5d",
      "%123.9d",
      "%5.5d",
      "%10.5d",
      "% 10.5d",
      "%+22.33d",
      "%01.3d",
      "%4d",
      "%-1.5ld",
      "%1.5ld",
      "%123.9ld",
      "%5.5ld",
      "%10.5ld",
      "% 10.5ld",
      "%+22.33ld",
      "%01.3ld",
      "%4ld",
      NULL
   };
   long int_nums[] = { -1, 134, 91340, 341, 0203, 0 };

   const char *ll_fmt[] = {
      "%-1.8lld",
      "%1.8lld",
      "%123.9lld",
      "%5.8lld",
      "%10.5lld",
      "% 10.8lld",
      "%+22.33lld",
      "%01.3lld",
      "%4lld",
      NULL
   };
   int64_t ll_nums[] = { -1976, 789134567890LL, 91340, 34123, 0203, 0 };

   const char *s_fmt[] = {
      "%-1.8s",
      "%1.8s",
      "%123.9s",
      "%5.8s",
      "%10.5s",
      "% 10.3s",
      "%+22.1s",
      "%01.3s",
      "%s",
      "%10s",
      "%3s",
      "%3.0s",
      "%3.s",
      NULL
   };
   const char *s_nums[] = { "abc", "def", "ghi", "123", "4567", "a", "bb", "ccccccc", NULL};

   const char *ls_fmt[] = {
      "%-1.8ls",
      "%1.8ls",
      "%123.9ls",
      "%5.8ls",
      "%10.5ls",
      "% 10.3ls",
      "%+22.1ls",
      "%01.3ls",
      "%ls",
      "%10ls",
      "%3ls",
      "%3.0ls",
      "%3.ls",
      NULL
   };
   const wchar_t *ls_nums[] = { L"abc", L"def", L"ghi", L"123", L"4567", L"a", L"bb", L"ccccccc", NULL};



   int x, y;
   int fail = 0;
   int num = 0;

#ifdef FP_OUTPUT
   for (x = 0; fp_fmt[x] != NULL; x++)
      for (y = 0; fp_nums[y] != 0; y++) {
         bsnprintf(buf1, sizeof(buf1), fp_fmt[x], fp_nums[y]);
         sprintf(buf2, fp_fmt[x], fp_nums[y]);
         EXPECT_STREQ ( buf1, buf2);
      }
#endif
   for (x = 0; int_fmt[x] != NULL; x++)
      for (y = 0; int_nums[y] != 0; y++) {
         int pcount, bcount;
         bcount = bsnprintf(buf1, sizeof(buf1), int_fmt[x], int_nums[y]);
         pcount = sprintf(buf2, int_fmt[x], int_nums[y]);

         EXPECT_EQ(bcount, pcount);
         EXPECT_STREQ(buf1, buf2);
      }

   for (x = 0; ll_fmt[x] != NULL; x++) {
      for (y = 0; ll_nums[y] != 0; y++) {
         int pcount, bcount;
         bcount = bsnprintf(buf1, sizeof(buf1), ll_fmt[x], ll_nums[y]);
         pcount = sprintf(buf2, ll_fmt[x], ll_nums[y]);

         EXPECT_EQ(bcount, pcount);
         EXPECT_STREQ(buf1, buf2);
      }
   }

   for (x = 0; s_fmt[x] != NULL; x++) {
      for (y = 0; s_nums[y] != 0; y++) {
         int pcount, bcount;
         bcount = bsnprintf(buf1, sizeof(buf1), s_fmt[x], s_nums[y]);
         pcount = sprintf(buf2, s_fmt[x], s_nums[y]);

         EXPECT_EQ(bcount, pcount);
         EXPECT_STREQ(buf1, buf2);
      }
   }

   for (x = 0; ls_fmt[x] != NULL; x++) {
      for (y = 0; ls_nums[y] != 0; y++) {
         int pcount, bcount;
         bcount = bsnprintf(buf1, sizeof(buf1), ls_fmt[x], ls_nums[y]);
         pcount = sprintf(buf2, ls_fmt[x], ls_nums[y]);

         EXPECT_EQ(bcount, pcount);
         EXPECT_STREQ(buf1, buf2);
      }
   }
}
