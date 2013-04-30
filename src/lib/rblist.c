/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2011 Free Software Foundation Europe e.V.

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
 *  Bacula red-black binary tree routines.
 *
 *    rblist is a binary tree with the links being in the data item.
 *
 *   Developped in part from ideas obtained from several online University
 *    courses. 
 *
 *   Kern Sibbald, November MMV
 *
 */

#include "bacula.h"

/* ===================================================================
 *    rblist
 */

/*
 *  Insert an item in the tree, but only if it is unique
 *   otherwise, the item is returned non inserted
 *  The big trick is keeping the tree balanced after the 
 *   insert. We use a parent pointer to make it simpler and 
 *   to avoid recursion.
 *
 * Returns: item         if item inserted
 *          other_item   if same value already exists (item not inserted)
 */
void *rblist::insert(void *item, int compare(void *item1, void *item2))
{
   void *x, *y;
   void *last = NULL;        /* last leaf if not found */
   void *found = NULL;
   int comp = 0;

   /* Search */
   x = head;
   while (x && !found) {
      last = x;
      comp = compare(item, x);
      if (comp < 0) {
         x = left(x);
      } else if (comp > 0) {
         x = right(x);
      } else {
         found = x;
      }
   }

   if (found) {                    /* found? */
      return found;                /* yes, return item found */
   }
   set_left(item, NULL);
   set_right(item, NULL);
   set_parent(item, NULL);
   set_red(item, false);
   /* Handle empty tree */
   if (num_items == 0) {
      head = item;
      num_items++;
      return item;
   }
   x = last;
   /* Not found, so insert it on appropriate side of tree */
   if (comp < 0) {
      set_left(last, item);
   } else {
      set_right(last, item);
   }
   set_red(last, true);
   set_parent(item, last);
   num_items++;

   /* Now we must walk up the tree balancing it */
   x = last;
   while (x != head && red(parent(x))) {
      if (parent(x) == left(parent(parent(x)))) {
         /* Look at the right side of our grandparent */
         y = right(parent(parent(x)));
         if (y && red(y)) {
            /* our parent must be black */
            set_red(parent(x), false);
            set_red(y, false);
            set_red(parent(parent(x)), true);
            x = parent(parent(x));       /* move up to grandpa */
         } else {
            if (x == right(parent(x))) { /* right side of parent? */
               x = parent(x);
               left_rotate(x);
            }
            /* make parent black too */
            set_red(parent(x), false);
            set_red(parent(parent(x)), true);
            right_rotate(parent(parent(x)));
         }
      } else {
         /* Look at left side of our grandparent */
         y = left(parent(parent(x)));
         if (y && red(y)) {
            set_red(parent(x), false);
            set_red(y, false);
            set_red(parent(parent(x)), true);
            x = parent(parent(x));       /* move up to grandpa */
         } else {
            if (x == left(parent(x))) {
               x = parent(x);
               right_rotate(x);
            }
            /* make parent black too */
            set_red(parent(x), false);
            set_red(parent(parent(x)), true);
            left_rotate(parent(parent(x)));
         }
      }
   }
   /* Make sure the head is always black */
   set_red(head, false);
   return item;
}

/*
 * Search for item
 */
void *rblist::search(void *item, int compare(void *item1, void *item2))
{
   void *found = NULL;
   void *x;
   int comp;

   x = head;
   while (x) {
      comp = compare(item, x);
      if (comp < 0) {
         x = left(x);
      } else if (comp > 0) {
         x = right(x);
      } else {
         found = x;
         break;
      }
   }
   return found;
}

/* 
 * Get first item (i.e. lowest value) 
 */
void *rblist::first(void)
{
   void *x;

   x = head;
   down = true;
   while (x) {
      if (left(x)) {
         x = left(x);
         continue;
      }
      return x;
   }
   /* Tree is empty */
   return NULL;
}

/*
 * This is a non-recursive btree walk routine that returns
 *  the items one at a time in order. I've never seen a
 *  non-recursive tree walk routine published that returns
 *  one item at a time rather than doing a callback.
 *
 * Return the next item in sorted order.  We assume first()
 *  was called once before calling this routine.
 *  We always go down as far as we can to the left, then up, and
 *  down one to the right, and again down as far as we can to the
 *  left.  etc. 
 *
 * Returns: pointer to next larger item 
 *          NULL when no more items in tree
 */
void *rblist::next(void *item)
{
   void *x;

   if (!item) {
      return first();
   }

   x = item;
   if ((down && !left(x) && right(x)) || (!down && right(x))) {
      /* Move down to right one */
      down = true;
      x = right(x);                       
      /* Then all the way down left */
      while (left(x))  {
         x = left(x);
      }
      return x;
   }

   /* We have gone down all we can, so now go up */
   for ( ;; ) {
      /* If at head, we are done */
      if (!parent(x)) {
         return NULL;
      }
      /* Move up in tree */
      down = false;
      /* if coming from right, continue up */
      if (right(parent(x)) == x) {
         x = parent(x);
         continue;
      }
      /* Coming from left, go up one -- ie. return parent */
      return parent(x);
   }
}

/*
 * Similer to next(), but visits all right nodes when
 *  coming up the tree.
 */
void *rblist::any(void *item)
{
   void *x;

   if (!item) {
      return NULL;
   }

   x = item;
   if ((down && !left(x) && right(x)) || (!down && right(x))) {
      /* Move down to right one */
      down = true;
      x = right(x);                       
      /* Then all the way down left */
      while (left(x))  {
         x = left(x);
      }
      return x;
   }

   /* We have gone down all we can, so now go up */
   for ( ;; ) {
      /* If at head, we are done */
      if (!parent(x)) {
         return NULL;
      }
      down = false;
      /* Go up one and return parent */
      return parent(x);
   }
}


/* x is item, y is below and to right, then rotated to below left */
void rblist::left_rotate(void *item)
{
   void *y;
   void *x;

   x = item;
   y = right(x);
   set_right(x, left(y));
   if (left(y)) {
      set_parent(left(y), x);
   }
   set_parent(y, parent(x));
   /* if no parent then we have a new head */
   if (!parent(x)) {
      head = y;
   } else if (x == left(parent(x))) {
      set_left(parent(x), y);
   } else {
      set_right(parent(x), y);
   }
   set_left(y, x);
   set_parent(x, y);
}

void rblist::right_rotate(void *item)
{
   void *x, *y;

   y = item;
   x = left(y);
   set_left(y, right(x));
   if (right(x)) {
      set_parent(right(x), y);
   }
   set_parent(x, parent(y));
   /* if no parent then we have a new head */
   if (!parent(y)) {
      head = x;
   } else if (y == left(parent(y))) {
      set_left(parent(y), x);
   } else {
      set_right(parent(y), x);
   }
   set_right(x, y);
   set_parent(y, x);
}


void rblist::remove(void *item)
{
}

/* Destroy the tree contents.  Not totally working */
void rblist::destroy()
{
   void *x, *y = NULL;

   x = first();
// printf("head=%p first=%p left=%p right=%p\n", head, x, left(x), right(x));
   for (  ; (y=any(x)); ) {
      /* Prune the last item */
      if (parent(x)) {
         if (x == left(parent(x))) {
            set_left(parent(x), NULL);
         } else if (x == right(parent(x))) {
            set_right(parent(x), NULL);
         }
      }
      if (!left(x) && !right(x)) {
         if (head == x) {  
            head = NULL;
         }
//          if (num_items<30) {
//             printf("free nitems=%d item=%p left=%p right=%p\n", num_items, x, left(x), right(x));
//          }   
         free((void *)x);      /* free previous node */
         num_items--;
      }
      x = y;                  /* save last node */
   }
   if (x) {
      if (x == head) {
         head = NULL;
      }
//    printf("free nitems=%d item=%p left=%p right=%p\n", num_items, x, left(x), right(x));
      free((void *)x);
      num_items--;
   }
   if (head) {
//    printf("Free head\n");
      free((void *)head);
   }
// printf("free nitems=%d\n", num_items);

   head = NULL;
}



#ifdef TEST_PROGRAM

struct MYJCR {
   void link;
   char *buf;
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
   rblist *jcr_chain;
   MYJCR *jcr = NULL;
   MYJCR *jcr1;


   /* Now do a binary insert for the tree */
   jcr_chain = New(rblist());
#define CNT 26
   printf("append %d items\n", CNT*CNT*CNT);
   strcpy(buf, "ZZZ");
   int count = 0;
   for (int i=0; i<CNT; i++) {
      for (int j=0; j<CNT; j++) {
         for (int k=0; k<CNT; k++) {
            count++;
            if ((count & 0x3FF) == 0) {
               Dmsg1(000, "At %d\n", count);
            }
            jcr = (MYJCR *)malloc(sizeof(MYJCR));
            memset(jcr, 0, sizeof(MYJCR));
            jcr->buf = bstrdup(buf);
//          printf("buf=%p %s\n", jcr, jcr->buf);
            jcr1 = (MYJCR *)jcr_chain->insert((void *)jcr, my_compare);
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
   printf("%d items appended\n", CNT*CNT*CNT);
   printf("num_items=%d\n", jcr_chain->size());

   jcr = (MYJCR *)malloc(sizeof(MYJCR));
   memset(jcr, 0, sizeof(MYJCR));

   jcr->buf = bstrdup("a");
   if ((jcr1=(MYJCR *)jcr_chain->search((void *)jcr, my_compare))) {
      printf("One less failed!!!! Got: %s\n", jcr1->buf);
   } else {
      printf("One less: OK\n");
   }
   free(jcr->buf);

   jcr->buf = bstrdup("ZZZZZZZZZZZZZZZZ");
   if ((jcr1=(MYJCR *)jcr_chain->search((void *)jcr, my_compare))) {
      printf("One greater failed!!!! Got:%s\n", jcr1->buf);
   } else {
      printf("One greater: OK\n");
   }
   free(jcr->buf);

   jcr->buf = bstrdup("AAA");
   if ((jcr1=(MYJCR *)jcr_chain->search((void *)jcr, my_compare))) {
      printf("Search for AAA got %s\n", jcr1->buf);
   } else {
      printf("Search for AAA not found\n"); 
   }
   free(jcr->buf);

   jcr->buf = bstrdup("ZZZ");
   if ((jcr1 = (MYJCR *)jcr_chain->search((void *)jcr, my_compare))) {
      printf("Search for ZZZ got %s\n", jcr1->buf);
   } else {
      printf("Search for ZZZ not found\n"); 
   }
   free(jcr->buf);
   free(jcr);


   printf("Find each of %d items in tree.\n", count);
   for (jcr=(MYJCR *)jcr_chain->first(); jcr; (jcr=(MYJCR *)jcr_chain->next((void *)jcr)) ) {
//    printf("Got: %s\n", jcr->buf);
      if (!jcr_chain->search((void *)jcr, my_compare)) {
         printf("rblist binary_search item not found = %s\n", jcr->buf);
      }
   }
   printf("Free each of %d items in tree.\n", count);
   for (jcr=(MYJCR *)jcr_chain->first(); jcr; (jcr=(MYJCR *)jcr_chain->next((void *)jcr)) ) {
//    printf("Free: %p %s\n", jcr, jcr->buf);
      free(jcr->buf);
      jcr->buf = NULL;
   }
   printf("num_items=%d\n", jcr_chain->size());
   delete jcr_chain;


   sm_dump(true);      /* unit test */

}
#endif
