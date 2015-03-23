/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
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
/* originally was Kern Sibbald, June MMIII */
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

struct FILESET {
   alist mylist;
};


void test_alist(void **state) {
   (void) state; /* unused */
   FILESET *fileset;
   char buf[30];
   alist *mlist;

   fileset = (FILESET *)malloc(sizeof(FILESET));
   memset(fileset, 0, sizeof(FILESET));
   fileset->mylist.init();


   for (int i=0; i<20; i++) {
      sprintf(buf, "%d", i);
      fileset->mylist.append(bstrdup(buf));
   }
   for (int i=0; i< fileset->mylist.size(); i++) {
      assert_int_equal(i, atoi((char *)fileset->mylist[i]));
   }
   fileset->mylist.destroy();
   free(fileset);

   mlist = New(alist(10));

   for (int i=0; i<20; i++) {
      sprintf(buf, "%d", i);
      mlist->append(bstrdup(buf));
   }
   for (int i=0; i< mlist->size(); i++) {
      assert_int_equal(i, atoi((char *)fileset->mylist[i]));
   }
   delete mlist;
   sm_dump(false);
}
