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
/* originally was Kern Sibbald, July MMIII */
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



struct MYJCR {
   char *buf;
   dlink link;
};

static int my_compare(void *item1, void *item2)
{
   MYJCR *jcr1, *jcr2;
   int comp;
   jcr1 = (MYJCR *)item1;
   jcr2 = (MYJCR *)item2;
   comp = strcmp(jcr1->buf, jcr2->buf);
   return comp;
}

void test_dlist(void **state) {
   (void) state;
   char buf[30];
   dlist *jcr_chain;
   MYJCR *jcr = NULL;
   MYJCR *jcr1;
   MYJCR *save_jcr = NULL;
   MYJCR *next_jcr;
   int count;
   int index = 0;

   jcr_chain = (dlist *)malloc(sizeof(dlist));
   jcr_chain->init(jcr, &jcr->link);

   for (int i=0; i<20; i++) {
      sprintf(buf, "%d", i);
      jcr = (MYJCR *)malloc(sizeof(MYJCR));
      jcr->buf = bstrdup(buf);
      jcr_chain->prepend(jcr);
      if (i == 10) {
         save_jcr = jcr;
      }
   }

   next_jcr = (MYJCR *)jcr_chain->next(save_jcr);
   assert_int_equal(atoi(next_jcr->buf), 9);
   jcr1 = (MYJCR *)malloc(sizeof(MYJCR));
   jcr1->buf = save_jcr->buf;
   jcr_chain->remove(save_jcr);
   free(save_jcr);
   jcr_chain->insert_before(jcr1, next_jcr);

   index = 19;
   foreach_dlist(jcr, jcr_chain) {
      assert_int_equal(index, atoi(jcr->buf));
      index --;
      free(jcr->buf);
   }
   jcr_chain->destroy();
   free(jcr_chain);

   /* The following may seem a bit odd, but we create a chaing
    *  of jcr objects.  Within a jcr object, there is a buf
    *  that points to a malloced string containing data
    */
   jcr_chain = New(dlist(jcr, &jcr->link));
   for (int i=0; i<20; i++) {
      sprintf(buf, "%d", i);
      jcr = (MYJCR *)malloc(sizeof(MYJCR));
      jcr->buf = bstrdup(buf);
      jcr_chain->append(jcr);
      if (i == 10) {
         save_jcr = jcr;
      }
   }

   next_jcr = (MYJCR *)jcr_chain->next(save_jcr);
   assert_int_equal(11, atoi(next_jcr->buf));
   jcr = (MYJCR *)malloc(sizeof(MYJCR));
   jcr->buf = save_jcr->buf;
   jcr_chain->remove(save_jcr);
   free(save_jcr);
   jcr_chain->insert_before(jcr, next_jcr);

   index= 0;
   foreach_dlist (jcr, jcr_chain) {
      assert_int_equal(index, atoi(jcr->buf));
      index ++;
      free(jcr->buf);
   }

   delete jcr_chain;


   /* Now do a binary insert for the list */
   jcr_chain = New(dlist(jcr, &jcr->link));
#define CNT 6
   strcpy(buf, "ZZZ");
   count = 0;
   for (int i=0; i<CNT; i++) {
      for (int j=0; j<CNT; j++) {
         for (int k=0; k<CNT; k++) {
            count++;
            jcr = (MYJCR *)malloc(sizeof(MYJCR));
            jcr->buf = bstrdup(buf);
            jcr1 = (MYJCR *)jcr_chain->binary_insert(jcr, my_compare);
            assert_ptr_equal(jcr, jcr1);
            buf[1]--;
         }
         buf[1] = 'Z';
         buf[2]--;
      }
      buf[2] = 'Z';
      buf[0]--;
   }

   jcr = (MYJCR *)malloc(sizeof(MYJCR));
   strcpy(buf, "a");
   jcr->buf = bstrdup(buf);
   assert_null(jcr_chain->binary_search(jcr, my_compare));
   free(jcr->buf);
   strcpy(buf, "ZZZZZZZZZZZZZZZZ");
   jcr->buf = bstrdup(buf);
   assert_null(jcr_chain->binary_search(jcr, my_compare));
   free(jcr->buf);
   free(jcr);


   foreach_dlist (jcr, jcr_chain) {
      if (!jcr_chain->binary_search(jcr, my_compare)) {
         printf("Dlist binary_search item not found = %s\n", jcr->buf);
         exit (1);
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
   dlist chain;
   chain.append(new_dlistString("This is a long test line"));
#define CNT 6
   strcpy(buf, "ZZZ");
   count = 0;
   for (int i=0; i<CNT; i++) {
      for (int j=0; j<CNT; j++) {
         for (int k=0; k<CNT; k++) {
            count++;
            chain.append(new_dlistString(buf));
            buf[1]--;
         }
         buf[1] = 'Z';
         buf[2]--;
      }
      buf[2] = 'Z';
      buf[0]--;
   }
   dlistString *node;
   foreach_dlist(node, &chain) {
   }
   chain.destroy();

   sm_dump(false);    /* unit test */

}
