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

#ifndef BAREOS_LIB_RESOURCE_ITEM_H_
#define BAREOS_LIB_RESOURCE_ITEM_H_

#include <set>
#include <string>

struct s_password;
template <typename T> class alist;
template <typename T> class dlist;

/*
 * This is the structure that defines the record types (items) permitted within
 * each resource. It is used to define the configuration tables.
 */
struct ResourceItem {
  const char* name; /* Resource name i.e. Director, ... */
  const int type;
  std::size_t offset;
  BareosResource** allocated_resource;
  int32_t code;              /* Item code/additional info */
  uint32_t flags;            /* Flags: See CFG_ITEM_* */
  const char* default_value; /* Default value */
  /* version string in format: [start_version]-[end_version]
   * start_version: directive has been introduced in this version
   * end_version:   directive is deprecated since this version */
  const char* versions;
  /* short description of the directive, in plain text,
   * used for the documentation.
   * Full sentence.
   * Every new directive should have a description. */
  const char* description;

  std::set<std::string> aliases = {};

  void SetPresent() { (*allocated_resource)->SetMemberPresent(name); }

  bool IsPresent() const
  {
    return (*allocated_resource)->IsMemberPresent(name);
  }
};

static inline void* CalculateAddressOfMemberVariable(const ResourceItem& item)
{
  char* base = reinterpret_cast<char*>(*item.allocated_resource);
  return static_cast<void*>(base + item.offset);
}

template <typename P> P GetItemVariable(const ResourceItem& item)
{
  void* p = CalculateAddressOfMemberVariable(item);
  return *(static_cast<typename std::remove_reference<P>::type*>(p));
}

template <typename P> P GetItemVariablePointer(const ResourceItem& item)
{
  void* p = CalculateAddressOfMemberVariable(item);
  return static_cast<P>(p);
}

template <typename P, typename V>
void SetItemVariable(const ResourceItem& item, const V& value)
{
  P* p = GetItemVariablePointer<P*>(item);
  *p = value;
}

template <typename P, typename V>
void SetItemVariableFreeMemory(const ResourceItem& item, const V& value)
{
  void* p = GetItemVariablePointer<void*>(item);
  P** q = (P**)p;
  if (*q) free(*q);
  (*(P**)p) = (P*)value;
}

#endif  // BAREOS_LIB_RESOURCE_ITEM_H_
