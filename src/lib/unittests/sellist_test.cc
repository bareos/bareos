/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2015      Bareos GmbH & Co.KG

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
/* originally was Kern Sibbald, January  MMXII */
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


void test_sellist(void **state) {
   (void) state; /* unused */

   char *str1 =           "1,4,5,7,12,12321,1231,99999";
   uint32_t resarray1[] = {1,4,5,7,12,12321,1231,99999};
   char *str2           = "1,2,3,4,5";
   uint32_t resarray2[] = {1,2,3,4,5};
   const char *msg;
   sellist sl;
   int i;
   int c;

   sl.set_string(str1, true);

   msg = sl.get_errmsg();

   //printf("%s", msg);

   assert_null(msg);
   c = 0;
   while ((i=sl.next()) >= 0) {
      assert_int_equal(i, resarray1[c]);
      //printf("%d\t", i);
      //printf("%d\n", resarray1[c]);
      c++;
   }

   msg = sl.get_errmsg();

   assert_null(msg);

   sl.set_string(str2, true);
   c = 0;
   while ((i=sl.next()) >= 0) {
      assert_int_equal(i, resarray2[c]);
      //printf("%d\t", i);
      //printf("%d\n", resarray2[c]);
      c++;
   }

   msg = sl.get_errmsg();

   assert_null(msg);
}
