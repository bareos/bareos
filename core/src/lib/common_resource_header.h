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

#include "include/bareos.h"

#define MAX_RES_ITEMS 90 /* maximum resource items per CommonResourceHeader */

/*
 * This is the universal header that is at the beginning of every resource
 * record.
 */
class CommonResourceHeader {
 public:
  CommonResourceHeader* next_;       /* Pointer to next resource of this type */
  char* resource_name_;              /* Resource name */
  char* description_;                /* Resource description */
  uint32_t rcode;                    /* Resource id or type */
  int32_t refcnt;                    /* Reference count for releasing */
  char item_present_[MAX_RES_ITEMS]; /* Set if item is present in conf file */
  char inherit_content_[MAX_RES_ITEMS]; /* Set if item has inherited content */

  CommonResourceHeader()
      : next_(nullptr)
      , resource_name_(nullptr)
      , description_(nullptr)
      , rcode(0)
      , refcnt(0)
      , item_present_{0}
      , inherit_content_{0}
  {
    return;
  }

  virtual ~CommonResourceHeader() = default;

  CommonResourceHeader(const CommonResourceHeader& other)
      : CommonResourceHeader()
  {
    /* do not copy next_ because that is part of the resource chain */
    if (other.resource_name_) {
      resource_name_ = bstrdup(other.resource_name_);
    }
    if (other.description_) { description_ = bstrdup(other.description_); }
    rcode = other.rcode;
    refcnt = other.refcnt;
    ::memcpy(item_present_, other.item_present_, MAX_RES_ITEMS);
    ::memcpy(inherit_content_, other.inherit_content_, MAX_RES_ITEMS);
  }

  CommonResourceHeader& operator=(const CommonResourceHeader& rhs)
  {
    next_ = rhs.next_;
    resource_name_ = rhs.resource_name_;
    description_ = rhs.description_;
    rcode = rhs.rcode;
    refcnt = rhs.refcnt;
    ::memcpy(item_present_, rhs.item_present_, MAX_RES_ITEMS);
    ::memcpy(inherit_content_, rhs.inherit_content_, MAX_RES_ITEMS);
    return *this;
  }
};

#endif /* BAREOS_LIB_COMMON_RESOURCE_HEADER_ */
