/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
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
// Kern Sibbald, June MMIII
/* @file
 * alist header file
 */
#ifndef BAREOS_LIB_ALIST_H_
#define BAREOS_LIB_ALIST_H_


/* There is a lot of extra casting here to work around the fact
 * that some compilers (Sun and Visual C++) do not accept
 * (void *) as an lvalue on the left side of an equal.
 *
 * Loop var through each member of list
 * Loop var through each member of list using an increasing index.
 * Loop var through each member of list using an decreasing index.
 */
#ifdef HAVE_TYPEOF
#  define foreach_alist(var, list)                                 \
    for ((var) = list ? (typeof((var)))(list)->first() : 0; (var); \
         (var) = (typeof(var))(list)->next())

#  define foreach_alist_null(var, list)                                  \
    for ((var) = list ? (typeof((var)))(list)->first() : nullptr; (var); \
         (var) = (typeof(var))(list)->next())

#  define foreach_alist_index(inx, var, list)                                 \
    for ((inx) = 0;                                                           \
         (list != nullptr) ? ((var) = (typeof((var)))(list)->get((inx))) : 0; \
         (inx)++)

#  define foreach_alist_rindex(inx, var, list)                                \
    for ((list != nullptr) ? (inx) = ((list)->size() - 1) : 0;                \
         (list != nullptr) ? ((var) = (typeof((var)))(list)->get((inx))) : 0; \
         (inx)--)

#else
#  define foreach_alist(var, list)                                          \
    for ((void)(list ? (*((void**)&(var)) = (void*)((list)->first())) : 0); \
         (var); (*((void**)&(var)) = (void*)((list)->next())))

#  define foreach_alist_null(var, list)                               \
    for ((void)(list ? (*((void**)&(var)) = (void*)((list)->first())) \
                     : nullptr);                                      \
         (var); (*((void**)&(var)) = (void*)((list)->next())))

#  define foreach_alist_index(inx, var, list)                                 \
    for ((inx) = 0; (list != nullptr)                                         \
                        ? ((*((void**)&(var)) = (void*)((list)->get((inx))))) \
                        : 0;                                                  \
         (inx)++)

#  define foreach_alist_rindex(inx, var, list)                     \
    for ((list != nullptr) ? (inx) = ((list)->size() - 1) : 0;     \
         (list != nullptr)                                         \
             ? ((*((void**)&(var)) = (void*)((list)->get((inx))))) \
             : 0;                                                  \
         (inx)--)
#endif


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
  T* items = nullptr;
  int num_items = 0;
  int max_items = 0;
  int num_grow = 0;
  int cur_item = 0;
  bool own_items = false;
  void GrowList(void);

 public:
  // Ueb disable non pointer initialization
  alist(int num = 1, bool own = true);
  ~alist();
  void init(int num = 1, bool own = true);
  void append(T item);
  void prepend(T item);
  T remove(int index);
  T get(int index);
  bool empty() const;
  T prev();
  T next();
  T first();
  T last();
  T operator[](int index) const;
  int current() const { return cur_item; }
  int size() const;
  void destroy();
  void grow(int num);

  std::list<std::string> to_std_list_string();

  // Use it as a stack, pushing and popping from the end
  void push(T item) { append(item); }
  T pop() { return remove(num_items - 1); }
};

// Define index operator []
template <typename T> inline T alist<T>::operator[](int index) const
{
  if (index < 0 || index >= num_items) { return nullptr; }
  return items[index];
}

template <typename T> inline bool alist<T>::empty() const
{
  return num_items == 0;
}

/* This allows us to do explicit initialization,
 *   allowing us to mix C++ classes inside malloc'ed
 *   C structures. Define before called in constructor.
 */
template <typename T> inline void alist<T>::init(int num, bool own)
{
  items = nullptr;
  num_items = 0;
  max_items = 0;
  num_grow = num;
  own_items = own;
  cur_item = 0;
}

template <typename T> inline alist<T>::alist(int num, bool own)
{
  init(num, own);
}

template <typename T> inline alist<T>::~alist() { destroy(); }

template <typename T> inline int alist<T>::size() const
{
  /* Check for null pointer, which allows test
   *  on size to succeed even if nothing put in
   *  alist.
   */
  return num_items;
}

/* How much to grow by each time */
template <typename T> inline void alist<T>::grow(int num) { num_grow = num; }

/* Private grow list function. Used to insure that
 *   at least one more "slot" is available.
 */
template <typename T> void alist<T>::GrowList()
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

template <typename T> T alist<T>::first()
{
  cur_item = 1;
  if (num_items == 0) {
    return NULL;
  } else {
    return items[0];
  }
}

template <typename T> T alist<T>::last()
{
  if (num_items == 0) {
    return NULL;
  } else {
    cur_item = num_items;
    return items[num_items - 1];
  }
}

template <typename T> T alist<T>::next()
{
  if (cur_item >= num_items) {
    return NULL;
  } else {
    return items[cur_item++];
  }
}

template <typename T> T alist<T>::prev()
{
  if (cur_item <= 1) {
    return NULL;
  } else {
    return items[--cur_item];
  }
}

// prepend an item to the list -- i.e. add to beginning
template <typename T> void alist<T>::prepend(T item)
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


// Append an item to the list
template <typename T> void alist<T>::append(T item)
{
  GrowList();
  items[num_items++] = item;
}

template <typename T>
/* Remove an item from the list */
T alist<T>::remove(int index)
{
  T item;
  if (index < 0 || index >= num_items) { return NULL; }
  item = items[index];
  num_items--;
  for (int i = index; i < num_items; i++) { items[i] = items[i + 1]; }
  return item;
}


/* Get the index item -- we should probably allow real indexing here */
template <typename T> T alist<T>::get(int index)
{
  if (index < 0 || index >= num_items) { return NULL; }
  return items[index];
}

/* Destroy the list and its contents */
template <typename T> void alist<T>::destroy()
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

#endif  // BAREOS_LIB_ALIST_H_
