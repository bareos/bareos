/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2021 Bareos GmbH & Co. KG

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
/* originally was Kern Sibbald, July MMIII */
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
#include "include/make_unique.h"
#include "lib/dlist.h"

#include <memory>

struct MYJCR {
  char* buf;
  dlink<MYJCR> link;
};

/*
 * if a contructor is used without parameter,
 * dlink must be the first item in the used structure.
 * (otherwise loffset must be calculated)
 */
struct ListItem {
  dlink<ListItem> link;
  char* buf;
};
template <typename T> static int MyCompare(T* item1, T* item2)
{
  int comp;
  comp = strcmp(item1->buf, item2->buf);
  return comp;
}

// we expect, that the list is filled with strings of numbers from 0 to n.
void TestForeachDlist(dlist<ListItem>* list)
{
  ListItem* val = NULL;
  char buf[30];
  int i = 0;

  // test all available foreach loops
  foreach_dlist (val, list) {
    sprintf(buf, "%d", i);
    EXPECT_STREQ(val->buf, buf);
    i++;
  }
}

void DlistFill(dlist<ListItem>* list, int max)
{
  ListItem* jcr = NULL;
  char buf[30];
  int start = list->size();

  for (int i = 0; i < max; i++) {
    jcr = (ListItem*)malloc(sizeof(MYJCR));
    // memset not required
    // memset(jcr, 0, sizeof(MYJCR));
    sprintf(buf, "%d", start + i);
    jcr->buf = strdup(buf);
    list->append(jcr);
    // printf("%s %s %d %s %s\n", buf, jcr->buf, list->size(), list->first(),
    // list->last()); printf("%s %s %d %x %x\n", buf, jcr->buf, list->size(),
    // list->first(), list->last());
  }
}

void FreeItem(ListItem* item)
{
  if (item) {
    free(item->buf);
    free(item);
  }
}

void FreeDlist(dlist<ListItem>* list)
{
  ListItem* val = NULL;
  int number = list->size();

  for (int i = number; i > 0; i--) {
    val = (ListItem*)list->last();
    list->remove(val);
    FreeItem(val);
  }

  EXPECT_EQ(list->size(), 0);

  delete (list);
}

void test_dlist_dynamic()
{
  dlist<ListItem>* list = NULL;
  ListItem* item = NULL;

  // NULL->size() will segfault
  // EXPECT_EQ(list->size(), 0);

  // does foreach work for NULL?
  TestForeachDlist(list);

  // create empty list
  list = new dlist<ListItem>();
  EXPECT_EQ(list->size(), 0);

  // does foreach work for empty lists?
  TestForeachDlist(list);

  // fill the list
  DlistFill(list, 20);
  EXPECT_EQ(list->size(), 20);
  TestForeachDlist(list);

  // verify and remove the latest entries
  EXPECT_EQ(list->size(), 20);
  item = (ListItem*)list->last();
  list->remove(item);
  EXPECT_STREQ(item->buf, "19");
  FreeItem(item);

  EXPECT_EQ(list->size(), 19);
  item = (ListItem*)list->last();
  list->remove(item);
  EXPECT_STREQ(item->buf, "18");
  FreeItem(item);

  // added more entires
  DlistFill(list, 20);
  TestForeachDlist(list);

  EXPECT_EQ(list->size(), 38);

  FreeDlist(list);
}

TEST(dlist, dlist)
{
  char buf[30];
  dlist<MYJCR>* jcr_chain;
  MYJCR* jcr = NULL;
  MYJCR* jcr1;
  MYJCR* save_jcr = NULL;
  MYJCR* next_jcr;
  int count;
  int index = 0;

  jcr_chain = (dlist<MYJCR>*)malloc(sizeof(dlist<MYJCR>));
  jcr_chain->init(jcr, &jcr->link);

  for (int i = 0; i < 20; i++) {
    sprintf(buf, "%d", i);
    jcr = (MYJCR*)malloc(sizeof(MYJCR));
    jcr->buf = strdup(buf);
    jcr_chain->prepend(jcr);
    if (i == 10) { save_jcr = jcr; }
  }

  next_jcr = (MYJCR*)jcr_chain->next(save_jcr);
  EXPECT_EQ(atoi(next_jcr->buf), 9);
  jcr1 = (MYJCR*)malloc(sizeof(MYJCR));
  jcr1->buf = save_jcr->buf;
  jcr_chain->remove(save_jcr);
  free(save_jcr);
  jcr_chain->InsertBefore(jcr1, next_jcr);

  index = 19;
  foreach_dlist (jcr, jcr_chain) {
    EXPECT_EQ(index, atoi(jcr->buf));
    index--;
    free(jcr->buf);
  }
  jcr_chain->destroy();
  free(jcr_chain);

  /* The following may seem a bit odd, but we create a chaing
   *  of jcr objects.  Within a jcr object, there is a buf
   *  that points to a malloced string containing data
   */
  jcr_chain = new dlist<MYJCR>(jcr, &jcr->link);
  for (int i = 0; i < 20; i++) {
    sprintf(buf, "%d", i);
    jcr = (MYJCR*)malloc(sizeof(MYJCR));
    jcr->buf = strdup(buf);
    jcr_chain->append(jcr);
    if (i == 10) { save_jcr = jcr; }
  }

  next_jcr = (MYJCR*)jcr_chain->next(save_jcr);
  EXPECT_EQ(11, atoi(next_jcr->buf));
  jcr = (MYJCR*)malloc(sizeof(MYJCR));
  jcr->buf = save_jcr->buf;
  jcr_chain->remove(save_jcr);
  free(save_jcr);
  jcr_chain->InsertBefore(jcr, next_jcr);

  index = 0;
  foreach_dlist (jcr, jcr_chain) {
    EXPECT_EQ(index, atoi(jcr->buf));
    index++;
    free(jcr->buf);
  }

  delete jcr_chain;


  /* Now do a binary insert for the list */
  jcr_chain = new dlist<MYJCR>(jcr, &jcr->link);
#define CNT 6
  strcpy(buf, "ZZZ");
  count = 0;
  for (int i = 0; i < CNT; i++) {
    for (int j = 0; j < CNT; j++) {
      for (int k = 0; k < CNT; k++) {
        count++;
        jcr = (MYJCR*)malloc(sizeof(MYJCR));
        jcr->buf = strdup(buf);
        jcr1 = jcr_chain->binary_insert(jcr, MyCompare<MYJCR>);
        EXPECT_EQ(jcr, jcr1);
        buf[1]--;
      }
      buf[1] = 'Z';
      buf[2]--;
    }
    buf[2] = 'Z';
    buf[0]--;
  }

  jcr = (MYJCR*)malloc(sizeof(MYJCR));
  strcpy(buf, "a");
  jcr->buf = strdup(buf);
  ASSERT_EQ(NULL, (jcr_chain->binary_search(jcr, MyCompare)));
  free(jcr->buf);
  strcpy(buf, "ZZZZZZZZZZZZZZZZ");
  jcr->buf = strdup(buf);
  ASSERT_EQ(NULL, (jcr_chain->binary_search(jcr, MyCompare)));
  free(jcr->buf);
  free(jcr);


  foreach_dlist (jcr, jcr_chain) {
    if (!jcr_chain->binary_search(jcr, MyCompare)) {
      printf("Dlist binary_search item not found = %s\n", jcr->buf);
      exit(1);
    }
  }
  foreach_dlist (jcr, jcr_chain) {
    free(jcr->buf);
    jcr->buf = NULL;
  }
  delete jcr_chain;

  /* Finally, do a test using the dlistString string helper, which
   *  allocates a dlist node and stores the string directly in
   *  it.
   */
  auto chain = std::make_unique<dlist<dlistString>>();
  chain->append(new_dlistString("This is a long test line"));
#define CNT 6
  strcpy(buf, "ZZZ");
  count = 0;
  for (int i = 0; i < CNT; i++) {
    for (int j = 0; j < CNT; j++) {
      for (int k = 0; k < CNT; k++) {
        count++;
        chain->append(new_dlistString(buf));
        buf[1]--;
      }
      buf[1] = 'Z';
      buf[2]--;
    }
    buf[2] = 'Z';
    buf[0]--;
  }
  dlistString* node;
  foreach_dlist (node, chain) {
  }
  chain->destroy();

  test_dlist_dynamic();
}
