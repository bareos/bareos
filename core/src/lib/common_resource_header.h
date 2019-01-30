/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_COMMON_RESOURCE_HEADER_
#define BAREOS_LIB_COMMON_RESOURCE_HEADER_

#define MAX_RES_ITEMS 90 /* maximum resource items per CommonResourceHeader */

/*
 * This is the universal header that is at the beginning of every resource record.
 */
class CommonResourceHeader {
 public:
  CommonResourceHeader *next;          /* Pointer to next resource of this type */
  char *name;                          /* Resource name */
  char *desc;                          /* Resource description */
  uint32_t rcode;                      /* Resource id or type */
  int32_t refcnt;                      /* Reference count for releasing */
  char item_present[MAX_RES_ITEMS];    /* Set if item is present in conf file */
  char inherit_content[MAX_RES_ITEMS]; /* Set if item has inherited content */
};

#endif /* BAREOS_LIB_COMMON_RESOURCE_HEADER_ */
