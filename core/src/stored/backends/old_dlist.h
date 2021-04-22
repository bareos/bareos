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
/*
 * Kern Sibbald, MMIV and MMVII
 */
/**
 * @file
 * Doubly linked list  -- old_dlist
 *
 * See the end of the file for the old_dlistString class which
 * facilitates storing strings in a old_dlist.
 */

#ifndef BAREOS_LIB_OLD_DLIST_H_
#define BAREOS_LIB_OLD_DLIST_H_

#include "include/bareos.h"
#include "old_dlink.h"
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
#  define foreach_old_dlist(var, list)                               \
    for ((var) = nullptr;                                            \
         (list ? ((var) = (typeof(var))(list)->next(var)) : nullptr) \
         != nullptr;)
#else
#  define foreach_old_dlist(var, list)                                       \
    for ((var) = nullptr;                                                    \
         (list ? (*((void**)&(var)) = (void*)((list)->next(var))) : nullptr) \
         != nullptr;)
#endif

class old_dlist {
  void* head;
  void* tail;
  int16_t loffset;
  uint32_t num_items;

 public:
  old_dlist(void* item, old_dlink* link);
  old_dlist(void);
  ~old_dlist() { destroy(); }
  void init(void* item, old_dlink* link);
  void init();
  void prepend(void* item);
  void append(void* item);
  void SetPrev(void* item, void* prev);
  void SetNext(void* item, void* next);
  void* get_prev(void* item);
  void* get_next(void* item);
  old_dlink* get_link(void* item);
  void InsertBefore(void* item, void* where);
  void InsertAfter(void* item, void* where);
  void* binary_insert(void* item, int compare(void* item1, void* item2));
  void* binary_search(void* item, int compare(void* item1, void* item2));
  void BinaryInsertMultiple(void* item, int compare(void* item1, void* item2));
  void remove(void* item);
  bool empty() const;
  int size() const;
  void* next(void* item);
  void* prev(void* item);
  void destroy();
  void* first() const;
  void* last() const;
};

inline void old_dlist::init()
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
inline old_dlist::old_dlist(void* item, old_dlink* link) { init(item, link); }

/* Constructor with link at head of item */
inline old_dlist::old_dlist(void)
    : head(nullptr), tail(nullptr), loffset(0), num_items(0)
{
  return;
}

inline void old_dlist::SetPrev(void* item, void* prev)
{
  ((old_dlink*)(((char*)item) + loffset))->prev = prev;
}

inline void old_dlist::SetNext(void* item, void* next)
{
  ((old_dlink*)(((char*)item) + loffset))->next = next;
}

inline void* old_dlist::get_prev(void* item)
{
  return ((old_dlink*)(((char*)item) + loffset))->prev;
}

inline void* old_dlist::get_next(void* item)
{
  return ((old_dlink*)(((char*)item) + loffset))->next;
}


inline old_dlink* old_dlist::get_link(void* item)
{
  return (old_dlink*)(((char*)item) + loffset);
}


inline bool old_dlist::empty() const { return head == nullptr; }

inline int old_dlist::size() const { return num_items; }


inline void* old_dlist::first() const { return head; }

inline void* old_dlist::last() const { return tail; }

/**
 * C string helper routines for old_dlist
 *   The string (char *) is kept in the node
 *
 *   Kern Sibbald, February 2007
 *
 */
class old_dlistString {
 public:
  char* c_str() { return str_; }

 private:
#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-private-field"
#endif
  old_dlink link_;
#ifdef __clang__
#  pragma clang diagnostic pop
#endif
  char str_[1];
  /* !!! Don't put anything after this as this space is used
   *     to hold the string in inline
   */
};

extern old_dlistString* new_old_dlistString(const char* str, int len);
extern old_dlistString* new_old_dlistString(const char* str);

#endif  // BAREOS_LIB_OLD_DLIST_H_
