/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2017 Bareos GmbH & Co. KG

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
 * and adapted for gtest
 *
 * Philipp Storz, November 2017
 */

#include "include/bareos.h"
#include "gtest/gtest.h"
#include "../lib/protos.h"
#include "protos.h"


struct RbListJobControlRecord {
   char *buf;
};

static int rblist_compare(void *item1, void *item2)
{
   RbListJobControlRecord *jcr1, *jcr2;
   int comp;
   jcr1 = (RbListJobControlRecord *)item1;
   jcr2 = (RbListJobControlRecord *)item2;
   comp = strcmp(jcr1->buf, jcr2->buf);
   return comp;
}

TEST(rblist,rblist){
   char buf[30];
   rblist *jcr_chain;
   RbListJobControlRecord *jcr = NULL;
   RbListJobControlRecord *jcr1;


   /* Now do a binary insert for the tree */
   jcr_chain = New(rblist());
#define RB_CNT 1
   printf("append %d items\n", RB_CNT*RB_CNT*RB_CNT);
   strcpy(buf, "ZZZ");
   int count = 0;
   for (int i=0; i<RB_CNT; i++) {
      for (int j=0; j<RB_CNT; j++) {
         for (int k=0; k<RB_CNT; k++) {
            count++;
            if ((count & 0x3FF) == 0) {
               Dmsg1(000, "At %d\n", count);
            }
            jcr = (RbListJobControlRecord *)malloc(sizeof(RbListJobControlRecord));
            memset(jcr, 0, sizeof(RbListJobControlRecord));
            jcr->buf = bstrdup(buf);
            jcr1 = (RbListJobControlRecord *)jcr_chain->insert((void *)jcr, rblist_compare);
            if (jcr != jcr1) {
               Dmsg2(000, "Insert of %s vs %s failed.\n", jcr->buf, jcr1->buf);
            }
            buf[1]--;
         }
         buf[1] = 'Z';
         buf[2]--;
      }
      buf[2] = 'Z';
      buf[0]--;
   }
   printf("%d items appended\n", RB_CNT*RB_CNT*RB_CNT);
   printf("num_items=%d\n", jcr_chain->size());

// test stop in the next line...
   jcr = (RbListJobControlRecord *)malloc(sizeof(RbListJobControlRecord));
   memset(jcr, 0, sizeof(RbListJobControlRecord));

   jcr->buf = bstrdup("a");
   if ((jcr1=(RbListJobControlRecord *)jcr_chain->search((void *)jcr, rblist_compare))) {
      printf("One less failed!!!! Got: %s\n", jcr1->buf);
   } else {
      printf("One less: OK\n");
   }
   free(jcr->buf);

   jcr->buf = bstrdup("ZZZZZZZZZZZZZZZZ");
   if ((jcr1=(RbListJobControlRecord *)jcr_chain->search((void *)jcr, rblist_compare))) {
      printf("One greater failed!!!! Got:%s\n", jcr1->buf);
   } else {
      printf("One greater: OK\n");
   }
   free(jcr->buf);

   jcr->buf = bstrdup("AAA");
   if ((jcr1=(RbListJobControlRecord *)jcr_chain->search((void *)jcr, rblist_compare))) {
      printf("Search for AAA got %s\n", jcr1->buf);
   } else {
      printf("Search for AAA not found\n");
   }
   free(jcr->buf);

   jcr->buf = bstrdup("ZZZ");
   if ((jcr1 = (RbListJobControlRecord *)jcr_chain->search((void *)jcr, rblist_compare))) {
      printf("Search for ZZZ got %s\n", jcr1->buf);
   } else {
      printf("Search for ZZZ not found\n");
   }
   free(jcr->buf);
   free(jcr);


   printf("Find each of %d items in tree.\n", count);
   for (jcr=(RbListJobControlRecord *)jcr_chain->first(); jcr; (jcr=(RbListJobControlRecord *)jcr_chain->next((void *)jcr)) ) {
      if (!jcr_chain->search((void *)jcr, rblist_compare)) {
         printf("rblist binary_search item not found = %s\n", jcr->buf);
      }
   }
   printf("Free each of %d items in tree.\n", count);
   for (jcr=(RbListJobControlRecord *)jcr_chain->first(); jcr; (jcr=(RbListJobControlRecord *)jcr_chain->next((void *)jcr)) ) {
      free(jcr->buf);
      jcr->buf = NULL;
   }
   printf("num_items=%d\n", jcr_chain->size());
   delete jcr_chain;


   sm_dump(true);      /* unit test */

}
