/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2011 Free Software Foundation Europe e.V.

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
/*
 * BAREOS red-black binary tree routines.
 *
 * rblist is a binary tree with the links being in the data item.
 *
 * Developed in part from ideas obtained from several online University courses.
 *
 * Kern Sibbald, November MMV
 */

#include "include/bareos.h"

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
   SetLeft(item, NULL);
   SetRight(item, NULL);
   SetParent(item, NULL);
   SetRed(item, false);
   /* Handle empty tree */
   if (num_items == 0) {
      head = item;
      num_items++;
      return item;
   }
   x = last;
   /* Not found, so insert it on appropriate side of tree */
   if (comp < 0) {
      SetLeft(last, item);
   } else {
      SetRight(last, item);
   }
   SetRed(last, true);
   SetParent(item, last);
   num_items++;

   /* Now we must walk up the tree balancing it */
   x = last;
   while (x != head && red(parent(x))) {
      if (parent(x) == left(parent(parent(x)))) {
         /* Look at the right side of our grandparent */
         y = right(parent(parent(x)));
         if (y && red(y)) {
            /* our parent must be black */
            SetRed(parent(x), false);
            SetRed(y, false);
            SetRed(parent(parent(x)), true);
            x = parent(parent(x));       /* move up to grandpa */
         } else {
            if (x == right(parent(x))) { /* right side of parent? */
               x = parent(x);
               LeftRotate(x);
            }
            /* make parent black too */
            SetRed(parent(x), false);
            SetRed(parent(parent(x)), true);
            RightRotate(parent(parent(x)));
         }
      } else {
         /* Look at left side of our grandparent */
         y = left(parent(parent(x)));
         if (y && red(y)) {
            SetRed(parent(x), false);
            SetRed(y, false);
            SetRed(parent(parent(x)), true);
            x = parent(parent(x));       /* move up to grandpa */
         } else {
            if (x == left(parent(x))) {
               x = parent(x);
               RightRotate(x);
            }
            /* make parent black too */
            SetRed(parent(x), false);
            SetRed(parent(parent(x)), true);
            LeftRotate(parent(parent(x)));
         }
      }
   }
   /* Make sure the head is always black */
   SetRed(head, false);
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
void rblist::LeftRotate(void *item)
{
   void *y;
   void *x;

   x = item;
   y = right(x);
   SetRight(x, left(y));
   if (left(y)) {
      SetParent(left(y), x);
   }
   SetParent(y, parent(x));
   /* if no parent then we have a new head */
   if (!parent(x)) {
      head = y;
   } else if (x == left(parent(x))) {
      SetLeft(parent(x), y);
   } else {
      SetRight(parent(x), y);
   }
   SetLeft(y, x);
   SetParent(x, y);
}

void rblist::RightRotate(void *item)
{
   void *x, *y;

   y = item;
   x = left(y);
   SetLeft(y, right(x));
   if (right(x)) {
      SetParent(right(x), y);
   }
   SetParent(x, parent(y));
   /* if no parent then we have a new head */
   if (!parent(y)) {
      head = x;
   } else if (y == left(parent(y))) {
      SetLeft(parent(y), x);
   } else {
      SetRight(parent(y), x);
   }
   SetRight(x, y);
   SetParent(y, x);
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
            SetLeft(parent(x), NULL);
         } else if (x == right(parent(x))) {
            SetRight(parent(x), NULL);
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
