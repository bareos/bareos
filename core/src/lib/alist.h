/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, June MMIII
 */
/**
 * @file
 * alist header file
 */
#ifndef BAREOS_LIB_ALIST_H_
#define BAREOS_LIB_ALIST_H_


/**
 * There is a lot of extra casting here to work around the fact
 * that some compilers (Sun and Visual C++) do not accept
 * (void *) as an lvalue on the left side of an equal.
 *
 * Loop var through each member of list
 * Loop var through each member of list using an increasing index.
 * Loop var through each member of list using an decreasing index.
 */
#ifdef HAVE_TYPEOF
#define foreach_alist(var, list)                                    \
  for ((var) = list ? (typeof((var)))(list)->first() : NULL; (var); \
       (var) = (typeof(var))(list)->next())

#define foreach_alist_index(inx, var, list)                                  \
  for ((inx) = 0; list ? ((var) = (typeof((var)))(list)->get((inx))) : NULL; \
       (inx)++)

#define foreach_alist_rindex(inx, var, list)    \
  for (list ? (inx) = ((list)->size() - 1) : 0; \
       list ? ((var) = (typeof((var)))(list)->get((inx))) : NULL; (inx)--)

#else
#define foreach_alist(var, list)                                            \
  for (list ? (*((void**)&(var)) = (void*)((list)->first())) : NULL; (var); \
       (*((void**)&(var)) = (void*)((list)->next())))


#define foreach_alist_index(inx, var, list)                               \
  for ((inx) = 0;                                                         \
       list ? ((*((void**)&(var)) = (void*)((list)->get((inx))))) : NULL; \
       (inx)++)

#define foreach_alist_rindex(inx, var, list)                              \
  for (list ? (inx) = ((list)->size() - 1) : 0;                           \
       list ? ((*((void**)&(var)) = (void*)((list)->get((inx))))) : NULL; \
       (inx)--)

#endif

/**
 * Second arg of init
 */
enum
{
  owned_by_alist = true,
  not_owned_by_alist = false
};

/**
 * Array list -- much like a simplified STL vector
 *               array of pointers to inserted items
 */
class alist {
  void** items = nullptr;
  int num_items = 0;
  int max_items = 0;
  int num_grow = 0;
  int cur_item = 0;
  bool own_items = false;
  void GrowList(void);

 public:
  alist(int num = 1, bool own = true);
  ~alist();
  void init(int num = 1, bool own = true);
  void append(void* item);
  void prepend(void* item);
  void* remove(int index);
  void* get(int index);
  bool empty() const;
  void* prev();
  void* next();
  void* first();
  void* last();
  void* operator[](int index) const;
  int current() const { return cur_item; }
  int size() const;
  void destroy();
  void grow(int num);

  /*
   * Use it as a stack, pushing and poping from the end
   */
  void push(void* item) { append(item); }
  void* pop() { return remove(num_items - 1); }
};

/**
 * Define index operator []
 */
inline void* alist::operator[](int index) const
{
  if (index < 0 || index >= num_items) { return NULL; }
  return items[index];
}

inline bool alist::empty() const { return num_items == 0; }

/**
 * This allows us to do explicit initialization,
 *   allowing us to mix C++ classes inside malloc'ed
 *   C structures. Define before called in constructor.
 */
inline void alist::init(int num, bool own)
{
  items = nullptr;
  num_items = 0;
  max_items = 0;
  num_grow = num;
  own_items = own;
  cur_item = 0;
}

/* Constructor */
inline alist::alist(int num, bool own) { init(num, own); }

/* Destructor */
inline alist::~alist() { destroy(); }


/* Current size of list */
inline int alist::size() const
{
  /*
   * Check for null pointer, which allows test
   *  on size to succeed even if nothing put in
   *  alist.
   */
  return num_items;
}

/* How much to grow by each time */
inline void alist::grow(int num) { num_grow = num; }

#endif /* BAREOS_LIB_ALIST_H_ */
