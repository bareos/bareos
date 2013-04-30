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
 *  Bacula array list routines
 *
 *    alist is a simple malloc'ed array of pointers.  For the moment,
 *      it simply malloc's a bigger array controlled by num_grow.
 *      Default is to realloc the pointer array for each new member.
 *
 *   Kern Sibbald, June MMIII
 *
 */

#include "bacula.h"

/*
 * Private grow list function. Used to insure that
 *   at least one more "slot" is available.
 */
void alist::grow_list()
{
   if (items == NULL) {
      if (num_grow == 0) {
         num_grow = 1;                /* default if not initialized */
      }
      items = (void **)malloc(num_grow * sizeof(void *));
      max_items = num_grow;
   } else if (num_items == max_items) {
      max_items += num_grow;
      items = (void **)realloc(items, max_items * sizeof(void *));
   }
}

void *alist::first()
{
   cur_item = 1;
   if (num_items == 0) {
      return NULL;
   } else {
      return items[0];
   }
}

void *alist::last()
{
   if (num_items == 0) {
      return NULL;
   } else {
      cur_item = num_items;
      return items[num_items-1];
   }
}

void *alist::next()
{
   if (cur_item >= num_items) {
      return NULL;
   } else {
      return items[cur_item++];
   }
}

void *alist::prev()
{
   if (cur_item <= 1) {
      return NULL;
   } else {
      return items[--cur_item];
   }
}

/*
 * prepend an item to the list -- i.e. add to beginning
 */
void alist::prepend(void *item) {
   grow_list();
   if (num_items == 0) {
      items[num_items++] = item;
      return;
   }
   for (int i=num_items; i > 0; i--) {
      items[i] = items[i-1];
   }
   items[0] = item;
   num_items++;
}


/*
 * Append an item to the list
 */
void alist::append(void *item) {
   grow_list();
   items[num_items++] = item;
}

/* Remove an item from the list */
void * alist::remove(int index)
{
   void *item;
   if (index < 0 || index >= num_items) {
      return NULL;
   }
   item = items[index];
   num_items--;
   for (int i=index; i < num_items; i++) {
      items[i] = items[i+1];
   }
   return item;
}


/* Get the index item -- we should probably allow real indexing here */
void * alist::get(int index)
{
   if (index < 0 || index >= num_items) {
      return NULL;
   }
   return items[index];
}

/* Destroy the list and its contents */
void alist::destroy()
{
   if (items) {
      if (own_items) {
         for (int i=0; i<num_items; i++) {
            free(items[i]);
            items[i] = NULL;
         }
      }
      free(items);
      items = NULL;
   }
}

#ifdef TEST_PROGRAM


struct FILESET {
   alist mylist;
};

int main()
{
   FILESET *fileset;
   char buf[30];
   alist *mlist;

   fileset = (FILESET *)malloc(sizeof(FILESET));
   memset(fileset, 0, sizeof(FILESET));
   fileset->mylist.init();

   printf("Manual allocation/destruction of list:\n");

   for (int i=0; i<20; i++) {
      sprintf(buf, "This is item %d", i);
      fileset->mylist.append(bstrdup(buf));
   }
   for (int i=0; i< fileset->mylist.size(); i++) {
      printf("Item %d = %s\n", i, (char *)fileset->mylist[i]);
   }
   fileset->mylist.destroy();
   free(fileset);

   printf("Allocation/destruction using new delete\n");
   mlist = new alist(10);

   for (int i=0; i<20; i++) {
      sprintf(buf, "This is item %d", i);
      mlist->append(bstrdup(buf));
   }
   for (int i=0; i< mlist->size(); i++) {
      printf("Item %d = %s\n", i, (char *)mlist->get(i));
   }

   delete mlist;


   sm_dump(false);       /* test program */

}
#endif
