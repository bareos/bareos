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

#ifndef BAREOS_LIB_BAREOS_RESOURCE_H_
#define BAREOS_LIB_BAREOS_RESOURCE_H_

#include "include/bareos.h"

class PoolMem;
class ConfigurationParser;

#define MAX_RES_ITEMS 90 /* maximum resource items per BareosResource */

class BareosResource {
 public:
  BareosResource* next_;             /* Pointer to next resource of this type */
  char* resource_name_;              /* Resource name */
  char* description_;                /* Resource description */
  uint32_t rcode_;                   /* Resource id or type */
  int32_t refcnt_;                   /* Reference count for releasing */
  char item_present_[MAX_RES_ITEMS]; /* Set if item is present in conf file */
  char inherit_content_[MAX_RES_ITEMS]; /* Set if item has inherited content */

  BareosResource();
  BareosResource(const BareosResource& other);
  BareosResource& operator=(const BareosResource& rhs);

  virtual ~BareosResource() = default;

  virtual void ShallowCopyTo(BareosResource* static_memory) const = 0;

  virtual bool PrintConfig(PoolMem& buf,
                           const ConfigurationParser& my_config,
                           bool hide_sensitive_data = false,
                           bool verbose = false);
};

#endif /* BAREOS_LIB_BAREOS_RESOURCE_H_ */
