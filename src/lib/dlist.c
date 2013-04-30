/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *  Bacula doubly linked list routines.
 *
 *    dlist is a doubly linked list with the links being in the
 *       list data item.
 *
 *   Kern Sibbald, July MMIII
 *
 */

#include "bacula.h"

/* ===================================================================
 *    dlist
 */

/*
 * Append an item to the list
 */
void dlist::append(void *item)
{
   set_next(item, NULL);
   set_prev(item, tail);
   if (tail) {
      set_next(tail, item);
   }
   tail = item;
   if (head == NULL) {                /* if empty list, */
      head = item;                    /* item is head as well */
   }
   num_items++;
}

/*
 * Append an item to the list
 */
void dlist::prepend(void *item)
{
   set_next(item, head);
   set_prev(item, NULL);
   if (head) {
      set_prev(head, item);
   }
   head = item;
   if (tail == NULL) {                /* if empty list, */
      tail = item;                    /* item is tail too */
   }
   num_items++;
}

void dlist::insert_before(void *item, void *where)
{
   dlink *where_link = get_link(where);

   set_next(item, where);
   set_prev(item, where_link->prev);

   if (where_link->prev) {
      set_next(where_link->prev, item);
   }
   where_link->prev = item;
   if (head == where) {
      head = item;
   }
   num_items++;
}

void dlist::insert_after(void *item, void *where)
{
   dlink *where_link = get_link(where);

   set_next(item, where_link->next);
   set_prev(item, where);

   if (where_link->next) {
      set_prev(where_link->next, item);
   }
   where_link->next = item;
   if (tail == where) {
      tail = item;
   }
   num_items++;
}

/*
 *  Insert an item in the list, but only if it is unique
 *  otherwise, the item is returned non inserted
 *
 * Returns: item         if item inserted
 *          other_item   if same value already exists (item not inserted)
 */
void *dlist::binary_insert(void *item, int compare(void *item1, void *item2))
{
   int comp;
   int low, high, cur;
   void *cur_item;

   if (num_items == 0) {
    //Dmsg0(000, "Append first.\n");
      append(item);
      return item;
   }
   if (num_items == 1) {
      comp = compare(item, first());
      if (comp < 0) {
         prepend(item);
       //Dmsg0(000, "Insert before first.\n");
         return item;
      } else if (comp > 0) {
         insert_after(item, first());
       //Dmsg0(000, "Insert after first.\n");
         return item;
      } else {
       //Dmsg0(000, "Same as first.\n");
         return first();
      }
   }
   /* Check against last item */
   comp = compare(item, last());
   if (comp > 0) {
      append(item);
    //Dmsg0(000, "Appended item.\n");
      return item;
   } else if (comp == 0) {
    //Dmsg0(000, "Same as last.\n");
      return last();
   }
   /* Check against first item */
   comp = compare(item, first());
   if (comp < 0) {
      prepend(item);
    //Dmsg0(000, "Inserted item before.\n");
      return item;
   } else if (comp == 0) {
    //Dmsg0(000, "Same as first.\n");
      return first();
   }
   if (num_items == 2) {
      insert_after(item, first());
    //Dmsg0(000, "Inserted item after.\n");
      return item;
   }
   low = 1;
   high = num_items;
   cur = 1;
   cur_item = first();
   while (low < high) {
      int nxt;
      nxt = (low + high) / 2;
      while (nxt > cur) {
         cur_item = next(cur_item);
         cur++;
      }
      while (nxt < cur) {
         cur_item = prev(cur_item);
         cur--;
      }
    //Dmsg1(000, "Compare item to %d\n", cur);
      comp = compare(item, cur_item);
    //Dmsg2(000, "Compare item to %d = %d\n", cur, comp);
      if (comp < 0) {
         high = cur;
       //Dmsg2(000, "set high; low=%d high=%d\n", low, high);
      } else if (comp > 0) {
         low = cur + 1;
       //Dmsg2(000, "set low; low=%d high=%d\n", low, high);
      } else {
       //Dmsg1(000, "Same as item %d\n", cur);
         return cur_item;
      }
   }
   if (high == cur) {
      insert_before(item, cur_item);
    //Dmsg1(000, "Insert before item %d\n", cur);
   } else {
      insert_after(item, cur_item);
    //Dmsg1(000, "Insert after item %d\n", cur);
   }
   return item;
}

/*
 *  Insert an item in the list, regardless if it is unique
 *  or not.
 */
void dlist::binary_insert_multiple(void *item, int compare(void *item1, void *item2))
{
   void *ins_item = binary_insert(item, compare);
   /* If identical, insert after the one found */
   if (ins_item != item) {
      insert_after(item, ins_item);
   }
}

/*
 * Search for item
 */
void *dlist::binary_search(void *item, int compare(void *item1, void *item2))
{
   int comp;
   int low, high, cur;
   void *cur_item;


   if (num_items == 0) {
      return NULL;
   }
   cur_item = first();
   if (num_items == 1) {
      comp = compare(item, cur_item);
      if (comp == 0) {
         return cur_item;
      } else {
         return NULL;
      }
   }
   low = 1;
   high = num_items;
   cur = 1;
   cur_item = first();
   while (low < high) {
      int nxt;
      nxt = (low + high) / 2;
      /* Now get cur pointing to nxt */
      while (nxt > cur) {
         cur_item = next(cur_item);
         cur++;
      }
      while (nxt < cur) {
         cur_item = prev(cur_item);
         cur--;
      }
      comp = compare(item, cur_item);
      //Dmsg2(000, "Compare item to %d = %d\n", cur, comp);
      if (comp < 0) {
         high = cur;
         //Dmsg2(000, "set high; low=%d high=%d\n", low, high);
      } else if (comp > 0) {
         low = cur + 1;
         //Dmsg2(000, "set low; low=%d high=%d\n", low, high);
      } else {
         return cur_item;
      }
   }
   /*
    * low == high can only happen if low just 
    *   got incremented from cur, and we have
    *   not yet tested cur+1
    */
   if (low == high) {
      cur_item = next(cur_item);
      comp = compare(item, cur_item);
      if (comp == 0) {
         return cur_item;
      }
   }
   return NULL;
}


void dlist::remove(void *item)
{
   void *xitem;
   dlink *ilink = get_link(item);   /* item's link */
   if (item == head) {
      head = ilink->next;
      if (head) {
         set_prev(head, NULL);
      }
      if (item == tail) {
         tail = ilink->prev;
      }
   } else if (item == tail) {
      tail = ilink->prev;
      if (tail) {
         set_next(tail, NULL);
      }
   } else {
      xitem = ilink->next;
      set_prev(xitem, ilink->prev);
      xitem = ilink->prev;
      set_next(xitem, ilink->next);
   }
   num_items--;
   if (num_items == 0) {
      head = tail = NULL;
   }
}

void *dlist::next(void *item)
{
   if (item == NULL) {
      return head;
   }
   return get_next(item);
}

void *dlist::prev(void *item)
{
   if (item == NULL) {
      return tail;
   }
   return get_prev(item);
}


/* Destroy the list contents */
void dlist::destroy()
{
   for (void *n=head; n; ) {
      void *ni = get_next(n);
      free(n);
      n = ni;
   }
   num_items = 0;
   head = tail = NULL;
}

/* String helpers for dlist usage */

dlistString *new_dlistString(const char *str)
{
   return new_dlistString(str, strlen(str));
}

dlistString *new_dlistString(const char *str, int len)
{
   dlistString *node;
   node = (dlistString *)malloc(sizeof(dlink) + len +1);
   bstrncpy(node->c_str(), str, len + 1);
   return node;
}

#ifdef TEST_PROGRAM

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
 //Dmsg3(000, "compare=%d: %s to %s\n", comp, jcr1->buf, jcr2->buf);
   return comp;
}

int main()
{
   char buf[30];
   dlist *jcr_chain;
   MYJCR *jcr = NULL;
   MYJCR *jcr1;
   MYJCR *save_jcr = NULL;
   MYJCR *next_jcr;
   int count;

   jcr_chain = (dlist *)malloc(sizeof(dlist));
   jcr_chain->init(jcr, &jcr->link);

   printf("Prepend 20 items 0-19\n");
   for (int i=0; i<20; i++) {
      sprintf(buf, "This is dlist item %d", i);
      jcr = (MYJCR *)malloc(sizeof(MYJCR));
      jcr->buf = bstrdup(buf);
      jcr_chain->prepend(jcr);
      if (i == 10) {
         save_jcr = jcr;
      }
   }

   next_jcr = (MYJCR *)jcr_chain->next(save_jcr);
   printf("11th item=%s\n", next_jcr->buf);
   jcr1 = (MYJCR *)malloc(sizeof(MYJCR));
   jcr1->buf = save_jcr->buf;
   printf("Remove 10th item\n");
   jcr_chain->remove(save_jcr);
   free(save_jcr);
   printf("Re-insert 10th item\n");
   jcr_chain->insert_before(jcr1, next_jcr);

   printf("Print remaining list.\n");
   foreach_dlist(jcr, jcr_chain) {
      printf("Dlist item = %s\n", jcr->buf);
      free(jcr->buf);
   }
   jcr_chain->destroy();
   free(jcr_chain);

   /* The following may seem a bit odd, but we create a chaing
    *  of jcr objects.  Within a jcr object, there is a buf
    *  that points to a malloced string containing data   
    */
   jcr_chain = New(dlist(jcr, &jcr->link));
   printf("append 20 items 0-19\n");
   for (int i=0; i<20; i++) {
      sprintf(buf, "This is dlist item %d", i);
      jcr = (MYJCR *)malloc(sizeof(MYJCR));
      jcr->buf = bstrdup(buf);
      jcr_chain->append(jcr);
      if (i == 10) {
         save_jcr = jcr;
      }
   }

   next_jcr = (MYJCR *)jcr_chain->next(save_jcr);
   printf("11th item=%s\n", next_jcr->buf);
   jcr = (MYJCR *)malloc(sizeof(MYJCR));
   jcr->buf = save_jcr->buf;
   printf("Remove 10th item\n");
   jcr_chain->remove(save_jcr);
   free(save_jcr);
   printf("Re-insert 10th item\n");
   jcr_chain->insert_before(jcr, next_jcr);

   printf("Print remaining list.\n");
   foreach_dlist (jcr, jcr_chain) {
      printf("Dlist item = %s\n", jcr->buf);
      free(jcr->buf);
   }

   delete jcr_chain;


   /* Now do a binary insert for the list */
   jcr_chain = New(dlist(jcr, &jcr->link));
#define CNT 26
   printf("append %d items\n", CNT*CNT*CNT);
   strcpy(buf, "ZZZ");
   count = 0;
   for (int i=0; i<CNT; i++) {
      for (int j=0; j<CNT; j++) {
         for (int k=0; k<CNT; k++) {
            count++;
            if ((count & 0x3FF) == 0) {
               Dmsg1(000, "At %d\n", count);
            }
            jcr = (MYJCR *)malloc(sizeof(MYJCR));
            jcr->buf = bstrdup(buf);
            jcr1 = (MYJCR *)jcr_chain->binary_insert(jcr, my_compare);
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

   jcr = (MYJCR *)malloc(sizeof(MYJCR));
   strcpy(buf, "a");
   jcr->buf = bstrdup(buf);
   if (jcr_chain->binary_search(jcr, my_compare)) {
      printf("One less failed!!!!\n");
   } else {
      printf("One less: OK\n");
   }
   free(jcr->buf);
   strcpy(buf, "ZZZZZZZZZZZZZZZZ");
   jcr->buf = bstrdup(buf);
   if (jcr_chain->binary_search(jcr, my_compare)) {
      printf("One greater failed!!!!\n");
   } else {
      printf("One greater: OK\n");
   }
   free(jcr->buf);
   free(jcr);


   printf("Find each of %d items in list.\n", count);
   foreach_dlist (jcr, jcr_chain) {
      if (!jcr_chain->binary_search(jcr, my_compare)) {
         printf("Dlist binary_search item not found = %s\n", jcr->buf);
      }
   }
   printf("Free each of %d items in list.\n", count);
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
#define CNT 26
   printf("append %d dlistString items\n", CNT*CNT*CNT);
   strcpy(buf, "ZZZ");
   count = 0;
   for (int i=0; i<CNT; i++) {
      for (int j=0; j<CNT; j++) {
         for (int k=0; k<CNT; k++) {
            count++;
            if ((count & 0x3FF) == 0) {
               Dmsg1(000, "At %d\n", count);
            }
            chain.append(new_dlistString(buf));
            buf[1]--;
         }
         buf[1] = 'Z';
         buf[2]--;
      }
      buf[2] = 'Z';
      buf[0]--;
   }
   printf("dlistString items appended, walking chain\n");
   dlistString *node;
   foreach_dlist(node, &chain) {
      printf("%s\n", node->c_str());
   }
   printf("destroy dlistString chain\n");
   chain.destroy();

   sm_dump(false);    /* unit test */

}
#endif
