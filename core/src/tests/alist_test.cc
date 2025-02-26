/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2024 Bareos GmbH & Co. KG

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
 * and adapted for gtest
 *
 * Philipp Storz, November 2017
 */

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "lib/alist.h"

struct FileSet {
  FileSet() = default;
  ~FileSet() = default;

  alist<const char*> mylist;
};

// helper functions

void AlistFill(alist<const char*>* list, int max)
{
  char buf[30];
  int start = 0;

  if (list) { start = list->size(); }

  // fill list with strings of numbers from 0 to n.
  for (int i = 0; i < max; i++) {
    sprintf(buf, "%d", start + i);
    list->append(strdup(buf));
  }

  // verify that max elements have been added
  EXPECT_EQ(list->size(), start + max);
}

// we expect, that the list is filled with strings of numbers from 0 to n.
void TestForeachAlist(alist<const char*>* list)
{
  char buf[30];
  int i = 0;

  // test all available foreach loops

  ASSERT_TRUE(list);

  for (auto* str : *list) {
    sprintf(buf, "%d", i);
    EXPECT_STREQ(str, buf);
    i++;
  }

  const char* str;
  foreach_alist_index (i, str, list) {
    sprintf(buf, "%d", i);
    EXPECT_STREQ(str, buf);
  }

  foreach_alist_rindex (i, str, list) {
    sprintf(buf, "%d", i);
    EXPECT_STREQ(str, buf);
  }
}

// Tests

void test_alist_init_destroy()
{
  FileSet* fileset;
  fileset = new FileSet;
  fileset->mylist.init();

  AlistFill(&(fileset->mylist), 20);
  for (int i = 0; i < fileset->mylist.size(); i++) {
    EXPECT_EQ(i, atoi((char*)fileset->mylist[i]));
  }
  fileset->mylist.destroy();
  delete fileset;
}


void test_alist_dynamic()
{
  alist<const char*>* list = nullptr;
  char* buf;

  // NULL->size() will segfault
  // EXPECT_EQ(list->size(), 0);

  // does foreach work for NULL? -> not anymore (macro has been removed)
  // TestForeachAlist(list);

  // create empty list, which is prepared for a number of entires
  list = new alist<const char*>(10);
  EXPECT_EQ(list->size(), 0);

  // does foreach work for empty lists?
  TestForeachAlist(list);

  // fill the list
  AlistFill(list, 20);
  TestForeachAlist(list);

  // verify and remove the latest entries
  EXPECT_EQ(list->size(), 20);
  buf = (char*)list->pop();
  EXPECT_STREQ(buf, "19");
  free(buf);

  EXPECT_EQ(list->size(), 19);
  buf = (char*)list->pop();
  EXPECT_STREQ(buf, "18");
  free(buf);

  // added more entires
  AlistFill(list, 20);
  TestForeachAlist(list);

  EXPECT_EQ(list->size(), 38);

  delete (list);
}


// main entry point

TEST(alist, alist)
{
  test_alist_init_destroy();
  test_alist_dynamic();
}
