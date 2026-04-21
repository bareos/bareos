/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
#include <optional>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <string_view>

class PoolMem;
class ConfigurationParser;
class OutputFormatterResource;
struct ResourceItem;

#define MAX_RES_ITEMS 95 /* maximum resource items per BareosResource */

class BareosResource {
 public:
  struct SourceLocation {
    std::string file{};
    int line{0};
  };

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
  std::optional<SourceLocation> definition_source_{};
  std::unordered_map<std::string, SourceLocation> item_sources_{};

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
  void SetMemberPresent(std::string_view member_name,
                        std::string_view source_file = {},
                        int source_line = 0)
  {
    item_present_.insert(member_name);
    if (!source_file.empty() && source_line > 0) {
      item_sources_[std::string(member_name)]
          = SourceLocation{std::string(source_file), source_line};
    }
  }
  bool UnsetMemberPresent(std::string_view member_name)
  {
    item_sources_.erase(std::string(member_name));
    return item_present_.erase(member_name) > 0;
  }
  void SetDefinitionSource(std::string_view source_file, int source_line)
  {
    if (!source_file.empty() && source_line > 0) {
      definition_source_
          = SourceLocation{std::string(source_file), source_line};
    }
  }
  const std::optional<SourceLocation>& GetDefinitionSource() const
  {
    return definition_source_;
  }
  const SourceLocation* GetMemberSource(std::string_view member_name) const
  {
    auto it = item_sources_.find(std::string(member_name));
    return it == item_sources_.end() ? nullptr : &it->second;
  }
};

const char* GetResourceName(const void* resource);

#endif  // BAREOS_LIB_BAREOS_RESOURCE_H_
