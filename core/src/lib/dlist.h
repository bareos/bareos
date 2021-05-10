/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2021 Bareos GmbH & Co. KG

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
 *
 * See the end of the file for the dlistString class which
 * facilitates storing strings in a dlist.
 */

#ifndef BAREOS_LIB_DLIST_H_
#define BAREOS_LIB_DLIST_H_

#include "include/bareos.h"
#include "lib/dlink.h"
#include "lib/message.h"

#define M_ABORT 1

/* In case you want to specifically specify the offset to the link */
#define OFFSET(item, link) (int)((char*)(link) - (char*)(item))
/**
 * There is a lot of extra casting here to work around the fact
 * that some compilers (Sun and Visual C++) do not accept
 * (void *) as an lvalue on the left side of an equal.
 *
 * Loop var through each member of list
 */
#ifdef HAVE_TYPEOF
#  define foreach_dlist(var, list)                                   \
    for ((var) = nullptr;                                            \
         (list ? ((var) = (typeof(var))(list)->next(var)) : nullptr) \
         != nullptr;)
#else
#  define foreach_dlist(var, list)                                           \
    for ((var) = nullptr;                                                    \
         (list ? (*((void**)&(var)) = (void*)((list)->next(var))) : nullptr) \
         != nullptr;)
#endif

template <typename T> class dlist {
  T* head;
  T* tail;
  int16_t loffset;
  uint32_t num_items;

 public:
  dlist(T* item, dlink<T>* link);
  dlist(void);
  ~dlist() { destroy(); }
  void init(T* item, dlink<T>* link);
  void init();
  void prepend(T* item);
  void append(T* item);
  void SetPrev(T* item, T* prev);
  void SetNext(T* item, T* next);
  T* get_prev(T* item);
  T* get_next(T* item);
  dlink<T>* get_link(T* item);
  void InsertBefore(T* item, T* where);
  void InsertAfter(T* item, T* where);
  T* binary_insert(T* item, int compare(T* item1, T* item2));
  T* binary_search(T* item, int compare(T* item1, T* item2));
  void BinaryInsertMultiple(T* item, int compare(T* item1, T* item2));
  void remove(T* item);
  bool empty() const;
  int size() const;
  T* next(T* item);
  T* prev(T* item);
  void destroy();
  T* first() const;
  T* last() const;
};

template <typename T> inline void dlist<T>::init()
{
  head = tail = nullptr;
  loffset = 0;
  num_items = 0;
}


/**
 * Constructor called with the address of a
 *   member of the list (not the list head), and
 *   the address of the link within that member.
 * If the link is at the beginning of the list member,
 *   then there is no need to specify the link address
 *   since the offset is zero.
 */
template <typename T> inline dlist<T>::dlist(T* item, dlink<T>* link)
{
  init(item, link);
}

/* Constructor with link at head of item */
template <typename T>
inline dlist<T>::dlist(void)
    : head(nullptr), tail(nullptr), loffset(0), num_items(0)
{
  return;
}

template <typename T> inline void dlist<T>::SetPrev(T* item, T* prev)
{
  ((dlink<T>*)(((char*)item) + loffset))->prev = prev;
}

template <typename T> inline void dlist<T>::SetNext(T* item, T* next)
{
  ((dlink<T>*)(((char*)item) + loffset))->next = next;
}

template <typename T> inline T* dlist<T>::get_prev(T* item)
{
  return ((dlink<T>*)(((char*)item) + loffset))->prev;
}

template <typename T> inline T* dlist<T>::get_next(T* item)
{
  return ((dlink<T>*)(((char*)item) + loffset))->next;
}


template <typename T> inline dlink<T>* dlist<T>::get_link(T* item)
{
  return (dlink<T>*)(((char*)item) + loffset);
}


template <typename T> inline bool dlist<T>::empty() const
{
  return head == nullptr;
}

template <typename T> inline int dlist<T>::size() const { return num_items; }


template <typename T> inline T* dlist<T>::first() const { return head; }

template <typename T> inline T* dlist<T>::last() const { return tail; }

/**
 * C string helper routines for dlist
 *   The string (char *) is kept in the node
 *
 *   Kern Sibbald, February 2007
 *
 */
class dlistString {
 public:
  char* c_str() { return str_; }

 private:
#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-private-field"
#endif
  dlink<char> link_;
#ifdef __clang__
#  pragma clang diagnostic pop
#endif
  char str_[1];
  /* !!! Don't put anything after this as this space is used
   *     to hold the string in inline
   */
};

extern dlistString* new_dlistString(const char* str, int len);
extern dlistString* new_dlistString(const char* str);


// Init dlist
template <typename T> void dlist<T>::init(T* item, dlink<T>* link)
{
  head = tail = NULL;
  loffset = (int)((char*)link - (char*)item);
  if (loffset < 0 || loffset > 5000) {
    Emsg0(M_ABORT, 0, "Improper dlist initialization.\n");
  }
  num_items = 0;
}

// Append an item to the list
template <typename T> void dlist<T>::append(T* item)
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

// Prepend an item to the list
template <typename T> void dlist<T>::prepend(T* item)
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

template <typename T> void dlist<T>::InsertBefore(T* item, T* where)
{
  dlink<T>* where_link = get_link(where);

  SetNext(item, where);
  SetPrev(item, where_link->prev);

  if (where_link->prev) { SetNext(where_link->prev, item); }
  where_link->prev = item;
  if (head == where) { head = item; }
  num_items++;
}

template <typename T> void dlist<T>::InsertAfter(T* item, T* where)
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
template <typename T>
T* dlist<T>::binary_insert(T* item, int compare(T* item1, T* item2))
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

/*
 *  Insert an item in the list, regardless if it is unique
 *  or not.
 */
template <typename T>
void dlist<T>::BinaryInsertMultiple(T* item, int compare(T* item1, T* item2))
{
  T* ins_item = binary_insert(item, compare);
  /* If identical, insert after the one found */
  if (ins_item != item) { InsertAfter(item, ins_item); }
}

// Search for item
template <typename T>
T* dlist<T>::binary_search(T* item, int compare(T* item1, T* item2))
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


template <typename T> void dlist<T>::remove(T* item)
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

template <typename T> T* dlist<T>::next(T* item)
{
  if (item == NULL) { return head; }
  return get_next(item);
}

template <typename T> T* dlist<T>::prev(T* item)
{
  if (item == NULL) { return tail; }
  return get_prev(item);
}


/* Destroy the list contents */
template <typename T> void dlist<T>::destroy()
{
  for (T* n = head; n;) {
    T* ni = get_next(n);
    free(n);
    n = ni;
  }
  num_items = 0;
  head = tail = NULL;
}

#endif  // BAREOS_LIB_DLIST_H_
