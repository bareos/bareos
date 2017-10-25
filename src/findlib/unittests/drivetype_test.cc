/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2007 Free Software Foundation Europe e.V.
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
/* originally was  Written by Robert Nelson, June 2006 */
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

#include <stdio.h>
#include "bareos.h"
#include "findlib/find.h"

void test_drivetype(void **state) {
#ifdef HAVE_WIN32
   char *driveletters_to_check[] = {"/" , "c:\\", "d:\\", NULL};

   char **p = driveletters_to_check;

   char dt[1000];

   while (*p) {
      assert_true(drivetype(*p, dt, sizeof(dt)));
      printf("%s\t%s\n", dt, *p);
      p++;
   }
#else
   printf ("drivetype only makes sense on windows, doing nothing on UNIX/Linux\n");
#endif
}
