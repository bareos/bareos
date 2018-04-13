/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2005-2007 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, MMV
 */
/**
 * @file
 * red-black binary tree routines -- rblist.h
 */

#define M_ABORT 1

/**
 * There is a lot of extra casting here to work around the fact
 * that some compilers (Sun and Visual C++) do not accept
 * (bnode *) as an lvalue on the left side of an equal.
 *
 * Loop var through each member of list
 */
#ifdef HAVE_TYPEOF
#define foreach_rblist(var, tree) \
   for (((var)=(typeof(var))(tree)->first()); (var); ((var)=(typeof(var))(tree)->next(var)))
#else
#define foreach_rblist(var, tree) \
   for ((*((void **)&(var))=(void*)((tree)->first())); (var); (*((void **)&(var))=(void*)((tree)->next(var))))
#endif

struct rblink {
   void *parent;
   void *left;
   void *right;
   bool red;
};

class DLL_IMP_EXP rblist : public SmartAlloc {
   void *head;
   int16_t loffset;
   uint32_t num_items;
   bool down;
   void left_rotate(void *item);
   void right_rotate(void *item);
public:
   rblist(void *item, rblink *link);
   rblist(void);
   ~rblist(void) { destroy(); }
   void init(void *item, rblink *link);
   void set_parent(void *item, void *parent);
   void set_left(void *item, void *left);
   void set_right(void *item, void *right);
   void set_red(void *item, bool red);
   void *parent(const void *item) const;
   void *left(const void *item) const;
   void *right(const void *item) const;
   bool red(const void *item) const;
   void *insert(void *item, int compare(void *item1, void *item2));
   void *search(void *item, int compare(void *item1, void *item2));
   void *first(void);
   void *next(void *item);
   void *any(void *item);
   void remove(void *item);
   bool empty(void) const;
   int size(void) const;
   void destroy(void);
};


/**
 * This allows us to do explicit initialization,
 *   allowing us to mix C++ classes inside malloc'ed
 *   C structures. Define before called in constructor.
 */
inline void rblist::init(void *item, rblink *link)
{
   head = NULL;
   loffset = (int)((char *)link - (char *)item);
   if (loffset < 0 || loffset > 5000) {
      Emsg0(M_ABORT, 0, "Improper rblist initialization.\n");
   }
   num_items = 0;
}

inline rblist::rblist(void *item, rblink *link)
{
   init(item, link);
}

/* Constructor with link at head of item */
inline rblist::rblist(void): head(0), loffset(0), num_items(0)
{
}

inline void rblist::set_parent(void *item, void *parent)
{
   ((rblink *)(((char *)item)+loffset))->parent = parent;
}

inline void rblist::set_left(void *item, void *left)
{
   ((rblink *)(((char *)item)+loffset))->left = left;
}

inline void rblist::set_right(void *item, void *right)
{
   ((rblink *)(((char *)item)+loffset))->right = right;
}

inline void rblist::set_red(void *item, bool red)
{
   ((rblink *)(((char *)item)+loffset))->red = red;
}

inline bool rblist::empty(void) const
{
   return head == NULL;
}

inline int rblist::size() const
{
   return num_items;
}

inline void *rblist::parent(const void *item) const
{
   return ((rblink *)(((char *)item)+loffset))->parent;
}

inline void *rblist::left(const void *item) const
{
   return ((rblink *)(((char *)item)+loffset))->left;
}

inline void *rblist::right(const void *item) const
{
   return ((rblink *)(((char *)item)+loffset))->right;
}

inline bool rblist::red(const void *item) const
{
   return ((rblink *)(((char *)item)+loffset))->red;
}
