/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, June MMIII
/* @file
 * alist header file
 */
#ifndef BAREOS_LIB_ALIST_H_
#define BAREOS_LIB_ALIST_H_


/* Loop var through each member of list using an increasing index.
 * Loop var through each member of list using an decreasing index.
 *
 * These should also get removed.  Currently there are some situations where
 * items are removed from the list during traversal (see UnloadPlugin).
 * Since this is thread safe it can stay for now. */
#define foreach_alist_index(inx, var, list) \
  for ((inx) = 0; (list != nullptr) ? ((var) = (list)->get((inx))) : 0; (inx)++)

#define foreach_alist_rindex(inx, var, list)                 \
  for ((list != nullptr) ? (inx) = ((list)->size() - 1) : 0; \
       (list != nullptr) ? ((var) = (list)->get((inx))) : 0; (inx)--)


#include <string>
#include <list>

// Second arg of init
enum
{
  owned_by_alist = true,
  not_owned_by_alist = false
};

/* Array list -- much like a simplified STL vector
 *               array of pointers to inserted items
 */


template <typename T> class alist {
 public:
  T* begin() { return &items[0]; }

  T* end() { return &items[num_items]; }

  const T* begin() const { return &items[0]; }

  const T* end() const { return &items[num_items]; }

  // Ueb disable non pointer initialization
  alist(int num = 1, bool own = true) { init(num, own); }
  ~alist() { destroy(); }

  /* This allows us to do explicit initialization,
   *   allowing us to mix C++ classes inside malloc'ed
   *   C structures. Define before called in constructor.
   */
  void init(int num = 1, bool own = true)
  {
    items = nullptr;
    num_items = 0;
    max_items = 0;
    num_grow = num; /* todo: do not grow linearly */
    own_items = own;
  }
  void append(T item)
  {
    GrowList();
    items[num_items++] = item;
  }
  void prepend(T item)
  {
    GrowList();
    if (num_items == 0) {
      items[num_items++] = item;
      return;
    }
    for (int i = num_items; i > 0; i--) { items[i] = items[i - 1]; }
    items[0] = item;
    num_items++;
  }
  T remove(int index)
  {
    T item;
    if (index < 0 || index >= num_items) { return NULL; }
    item = items[index];
    num_items--;
    for (int i = index; i < num_items; i++) { items[i] = items[i + 1]; }
    return item;
  }
  T get(int index)
  {
    if (index < 0 || index >= num_items) { return NULL; }
    return items[index];
  }
  bool empty() const { return num_items == 0; }
  T first()
  {
    if (num_items == 0) {
      return NULL;
    } else {
      return items[0];
    }
  }
  T last()
  {
    if (num_items == 0) {
      return NULL;
    } else {
      return items[num_items - 1];
    }
  }
  T operator[](int index) const
  {
    if (index < 0 || index >= num_items) { return nullptr; }
    return items[index];
  }
  int size() const { return num_items; }
  void destroy()
  {
    if (items) {
      if (own_items) {
        for (int i = 0; i < num_items; i++) {
          free((void*)(items[i]));
          items[i] = NULL;
        }
      }
      free(items);
      items = NULL;
    }
  }

  // Use it as a stack, pushing and popping from the end
  void push(T item) { append(item); }
  T pop() { return remove(num_items - 1); }

 private:
  /* Private grow list function. Used to insure that
   *   at least one more "slot" is available.
   */
  void GrowList(void)
  {
    if (items == NULL) {
      if (num_grow == 0) { num_grow = 1; /* default if not initialized */ }
      items = (T*)malloc(num_grow * sizeof(T));
      max_items = num_grow;
    } else if (num_items == max_items) {
      max_items += num_grow;
      items = (T*)realloc(items, max_items * sizeof(T));
    }
  }

  T* items = nullptr;
  int num_items = 0;
  int max_items = 0;
  int num_grow = 0;
  bool own_items = false;
};

#endif  // BAREOS_LIB_ALIST_H_
