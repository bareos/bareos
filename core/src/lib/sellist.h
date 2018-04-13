/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2012 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, January  MMXII
 */
/**
 * @file
 *
 * Selection list. A string of integers separated by commas
 * representing items selected. Ranges of the form nn-mm
 * are also permitted.
 */

/**
 * Loop var through each member of list
 */
#define foreach_sellist(var, list) \
        for((var)=(list)->first(); (var)>=0; (var)=(list)->next() )


class sellist : public SmartAlloc {
   const char *errmsg;
   char *p, *e, *h;
   char esave, hsave;
   int64_t beg, end;
   int64_t max;
   int num_items;
   char *str;
public:
   sellist();
   ~sellist();
   bool set_string(char *string, bool scan);
   int64_t first();
   int64_t next();
   void begin();
   /* size() valid only if scan enabled on string */
   int size() const { return num_items; };
   char *get_list() { return str; };
   /* if errmsg == NULL, no error */
   const char *get_errmsg() { return errmsg; };
};

/**
 * Initialize the list structure
 */
inline sellist::sellist()
{
   num_items = 0;
   max = 99999;
   str = NULL;
   e = NULL;
   errmsg = NULL;
}

/**
 * Destroy the list
 */
inline sellist::~sellist()
{
   if (str) {
      free(str);
      str = NULL;
   }
}

/**
 * Returns first item
 *   error if returns -1 and errmsg set
 *   end of items if returns -1 and errmsg NULL
 */
inline int64_t sellist::first()
{
   begin();
   return next();
}

/**
 * Reset to walk list from beginning
 */
inline void sellist::begin()
{
   e = str;
   end = 0;
   beg = 1;
   errmsg = NULL;
}
