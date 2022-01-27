/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_DLIST_STRING_H_
#define BAREOS_LIB_DLIST_STRING_H_

#include "lib/dlink.h"

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

  dlink<char> link;

 private:
  char str_[1];
  /* !!! Don't put anything after this as this space is used
   *     to hold the string in inline
   */
};

extern dlistString* new_dlistString(const char* str, int len);
extern dlistString* new_dlistString(const char* str);

#endif  // BAREOS_LIB_DLIST_STRING_H_
