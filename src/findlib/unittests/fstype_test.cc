/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2007 Free Software Foundation Europe e.V.
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
/* originally was Written by Preben 'Peppe' Guldberg, December MMIV */
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

void test_fstype(void **state) {
#ifdef HAVE_WIN32
   char *fs_to_check[] = {"/", "c:/", NULL};
#else
   char *fs_to_check[] = {"/proc", NULL};
#endif
   char fs[1000];

   char **p = fs_to_check;
   while ( *p ) {
      assert_true( fstype(*p, fs, sizeof(fs)) );
      printf("%s\t%s\n", fs, *p);
      p++;
   }
}
