/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
#include <unordered_set>
#include <string_view>

class PoolMem;
class ConfigurationParser;
class OutputFormatterResource;
struct ResourceItem;

#define MAX_RES_ITEMS 95 /* maximum resource items per BareosResource */

class BareosResource {
 public:
  BareosResource* next_{nullptr}; /* Pointer to next resource of this type */
  char* resource_name_{nullptr};  /* Resource name */
  char* description_{nullptr};    /* Resource description */
  uint32_t rcode_{0};             /* Resource id or type */
  int32_t refcnt_{0};             /* Reference count for releasing */
  std::string rcode_str_{};
  std::unordered_set<std::string_view>
      item_present_{}; /* set of resource member names where values were
                          written */
  char inherit_content_[MAX_RES_ITEMS]{
      0}; /* Set if item has inherited content */
  bool internal_{false};

  BareosResource() = default;
  BareosResource(const BareosResource& other);
  BareosResource& operator=(const BareosResource& rhs);

  virtual ~BareosResource() = default;
  virtual bool PrintConfig(OutputFormatterResource& send,
                           const ConfigurationParser& my_config,
                           bool hide_sensitive_data = false,
                           bool verbose = false);

  virtual bool Validate();
  virtual void PrintResourceItem(const ResourceItem& item,
                                 const ConfigurationParser& my_config,
                                 OutputFormatterResource& send,
                                 bool hide_sensitive_data,
                                 bool inherited,
                                 bool verbose);

  bool IsMemberPresent(std::string_view member_name) const
  {
    return item_present_.find(member_name) != item_present_.end();
  }
  void SetMemberPresent(std::string_view member_name)
  {
    item_present_.insert(member_name);
  }
  bool UnsetMemberPresent(std::string_view member_name)
  {
    return item_present_.erase(member_name) > 0;
  }
};

const char* GetResourceName(const void* resource);

#endif  // BAREOS_LIB_BAREOS_RESOURCE_H_
