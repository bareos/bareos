/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2022 Bareos GmbH & Co. KG

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
// Kern Sibbald, MMIV and MMVII
/**
 * @file
 * Doubly linked list  -- dlist
 */

#ifndef BAREOS_LIB_DLIST_H_
#define BAREOS_LIB_DLIST_H_

#include "include/bareos.h"
#include "lib/dlink.h"
#include "lib/dlist_string.h"
#include "lib/message.h"
#include "lib/message_severity.h"

#define foreach_dlist(var, list) \
    for ((var) = nullptr;                                            \
         (list ? ((var) = (list)->next(var)) : nullptr) \
         != nullptr;)

template <typename T> class dlist {
  T* head{nullptr};
  T* tail{nullptr};
  uint32_t num_items{0};

 public:
  dlist() = default;
  ~dlist() { destroy(); }
  dlist(const dlist<T>& other) = delete;
  dlist(dlist<T>&& other) = delete;
  dlist<T>& operator=(const dlist<T>& other) = delete;
  dlist<T>& operator=(dlist<T>&& other) = delete;

  void prepend(T* item)
  {
    SetNext(item, head);
    SetPrev(item, NULL);
    if (head) { SetPrev(head, item); }
    head = item;
    if (tail == NULL) { /* if empty list, */
      tail = item;      /* item is tail too */
    }
    num_items++;
  }
  void append(T* item)
  {
    SetNext(item, NULL);
    SetPrev(item, tail);
    if (tail) { SetNext(tail, item); }
    tail = item;
    if (head == NULL) { /* if empty list, */
      head = item;      /* item is head as well */
    }
    num_items++;
  }

  void SetPrev(T* item, T* prev) { item->link.prev = prev; }
  void SetNext(T* item, T* next) { item->link.next = next; }

  T* get_prev(T* item) { return item->link.prev; }
  T* get_next(T* item) { return item->link.next; }
  dlink<T>* get_link(T* item) { return &item->link; }
  void InsertBefore(T* item, T* where)
  {
    dlink<T>* where_link = get_link(where);

    SetNext(item, where);
    SetPrev(item, where_link->prev);

    if (where_link->prev) { SetNext(where_link->prev, item); }
    where_link->prev = item;
    if (head == where) { head = item; }
    num_items++;
  }

  void InsertAfter(T* item, T* where)
  {
    dlink<T>* where_link = get_link(where);

    SetNext(item, where_link->next);
    SetPrev(item, where);

    if (where_link->next) { SetPrev(where_link->next, item); }
    where_link->next = item;
    if (tail == where) { tail = item; }
    num_items++;
  }
  /*
   *  Insert an item in the list, but only if it is unique
   *  otherwise, the item is returned non inserted
   *
   * Returns: item         if item inserted
   *          other_item   if same value already exists (item not inserted)
   */
  T* binary_insert(T* item, int compare(T* item1, T* item2))
  {
    int comp;
    int low, high, cur;
    T* cur_item;

    if (num_items == 0) {
      // Dmsg0(000, "Append first.\n");
      append(item);
      return item;
    }
    if (num_items == 1) {
      comp = compare(item, first());
      if (comp < 0) {
        prepend(item);
        // Dmsg0(000, "Insert before first.\n");
        return item;
      } else if (comp > 0) {
        InsertAfter(item, first());
        // Dmsg0(000, "Insert after first.\n");
        return item;
      } else {
        // Dmsg0(000, "Same as first.\n");
        return first();
      }
    }
    /* Check against last item */
    comp = compare(item, last());
    if (comp > 0) {
      append(item);
      // Dmsg0(000, "Appended item.\n");
      return item;
    } else if (comp == 0) {
      // Dmsg0(000, "Same as last.\n");
      return last();
    }
    /* Check against first item */
    comp = compare(item, first());
    if (comp < 0) {
      prepend(item);
      // Dmsg0(000, "Inserted item before.\n");
      return item;
    } else if (comp == 0) {
      // Dmsg0(000, "Same as first.\n");
      return first();
    }
    if (num_items == 2) {
      InsertAfter(item, first());
      // Dmsg0(000, "Inserted item after.\n");
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
      // Dmsg1(000, "Compare item to %d\n", cur);
      comp = compare(item, cur_item);
      // Dmsg2(000, "Compare item to %d = %d\n", cur, comp);
      if (comp < 0) {
        high = cur;
        // Dmsg2(000, "set high; low=%d high=%d\n", low, high);
      } else if (comp > 0) {
        low = cur + 1;
        // Dmsg2(000, "set low; low=%d high=%d\n", low, high);
      } else {
        // Dmsg1(000, "Same as item %d\n", cur);
        return cur_item;
      }
    }
    if (high == cur) {
      InsertBefore(item, cur_item);
      // Dmsg1(000, "Insert before item %d\n", cur);
    } else {
      InsertAfter(item, cur_item);
      // Dmsg1(000, "Insert after item %d\n", cur);
    }
    return item;
  }
  T* binary_search(T* item, int compare(T* item1, T* item2))
  {
    int comp;
    int low, high, cur;
    T* cur_item;


    if (num_items == 0) { return NULL; }
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
      // Dmsg2(000, "Compare item to %d = %d\n", cur, comp);
      if (comp < 0) {
        high = cur;
        // Dmsg2(000, "set high; low=%d high=%d\n", low, high);
      } else if (comp > 0) {
        low = cur + 1;
        // Dmsg2(000, "set low; low=%d high=%d\n", low, high);
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
      if (comp == 0) { return cur_item; }
    }
    return NULL;
  }
  /*
   *  Insert an item in the list, regardless if it is unique
   *  or not.
   */
  void BinaryInsertMultiple(T* item, int compare(T* item1, T* item2))
  {
    T* ins_item = binary_insert(item, compare);
    /* If identical, insert after the one found */
    if (ins_item != item) { InsertAfter(item, ins_item); }
  }
  void remove(T* item)
  {
    T* xitem;
    dlink<T>* ilink = get_link(item); /* item's link */
    if (item == head) {
      head = ilink->next;
      if (head) { SetPrev(head, NULL); }
      if (item == tail) { tail = ilink->prev; }
    } else if (item == tail) {
      tail = ilink->prev;
      if (tail) { SetNext(tail, NULL); }
    } else {
      xitem = ilink->next;
      SetPrev(xitem, ilink->prev);
      xitem = ilink->prev;
      SetNext(xitem, ilink->next);
    }
    num_items--;
    if (num_items == 0) { head = tail = NULL; }
  }
  bool empty() const { return head == nullptr; }
  int size() const { return num_items; }
  T* next(T* item)
  {
    if (item == NULL) { return head; }
    return get_next(item);
  }
  T* prev(T* item)
  {
    if (item == NULL) { return tail; }
    return get_prev(item);
  }
  /* Destroy the list contents */
  void destroy()
  {
    for (T* n = head; n;) {
      T* ni = get_next(n);
      free(n);
      n = ni;
    }
    num_items = 0;
    head = tail = NULL;
  }

  T* first() const { return head; }
  T* last() const { return tail; }
};
#endif  // BAREOS_LIB_DLIST_H_
