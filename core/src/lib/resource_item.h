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

#include <vector>
#include <string>
#include <optional>
#include <utility>

#include "lib/parse_conf.h"

struct s_password;
template <typename T> class alist;
template <typename T> class dlist;

namespace config {
struct DefaultValue {
  const char* value;
};

struct Version {
  size_t major, minor, patch;
};

struct DeprecatedSince {
  Version version;
};

struct IntroducedIn {
  Version version;
};

struct Code {
  size_t value;
};

struct Required {};
};  // namespace config


template <typename What, typename... Args> struct occurances {
  static constexpr size_t value{(std::is_same_v<What, Args> + ...)};
};

template <typename What, typename... Args> struct is_present {
  static constexpr bool value{occurances<What, Args...>::value > 0};
};


template <typename T, typename... Ts> T* get_if(std::tuple<Ts...>& tuple)
{
  if constexpr (is_present<T, Ts...>::value) {
    return &std::get<T>(tuple);
  } else {
    return nullptr;
  }
}

struct ResourceItemFlags {
  std::optional<config::Version> introduced_in{};
  std::optional<config::Version> deprecated_since{};
  const char* default_value{};
  std::optional<size_t> extra{};
  bool required{};

  template <typename... Types> ResourceItemFlags(Types&&... values)
  {
    static_assert((is_present<Types, config::DefaultValue, config::IntroducedIn,
                              config::DeprecatedSince, config::Code,
                              config::Required>::value
                   && ...),
                  "only allowed flags may be used");

    static_assert(occurances<config::DefaultValue, Types...>::value <= 1,
                  "flag may only be specified once");
    static_assert(occurances<config::IntroducedIn, Types...>::value <= 1,
                  "flag may only be specified once");
    static_assert(occurances<config::DeprecatedSince, Types...>::value <= 1,
                  "flag may only be specified once");
    static_assert(occurances<config::Code, Types...>::value <= 1,
                  "flag may only be specified once");
    static_assert(occurances<config::Required, Types...>::value <= 1,
                  "flag may only be specified once");

    std::tuple<Types...> tup{std::forward<Types>(values)...};

    if (auto* defval = get_if<config::DefaultValue>(tup)) {
      default_value = defval->value;
    }
    if (auto* introduced = get_if<config::IntroducedIn>(tup)) {
      introduced_in = introduced->version;
    }
    if (auto* deprecated = get_if<config::DeprecatedSince>(tup)) {
      deprecated_since = deprecated->version;
    }
    if (auto* code = get_if<config::Code>(tup)) { extra = code->value; }
    if (auto* _ = get_if<config::Required>(tup)) { required = true; }
  }
};

/*
 * This is the structure that defines the record types (items) permitted within
 * each resource. It is used to define the configuration tables.
 */
struct ResourceItem {
  ResourceItem(const char* name_,
               const int type_,
               std::size_t offset_,
               BareosResource** allocated_resource_,
               const char* description_,
               ResourceItemFlags&& resource_flags)
      : name{name_}
      , type{type_}
      , offset{offset_}
      , allocated_resource{allocated_resource_}
      , description{description_}
  {
    if (resource_flags.deprecated_since) { flags |= CFG_ITEM_DEPRECATED; }
    if (resource_flags.required) { flags |= CFG_ITEM_REQUIRED; }
    default_value = resource_flags.default_value;
    code = resource_flags.extra.value_or(0);
  }

  ResourceItem(const char* name_,
               const int type_,
               std::size_t offset_,
               BareosResource** allocated_resource_,
               int32_t code_,
               uint32_t flags_,
               const char* default_value_,
               const char* versions_,
               const char* description_)
      : name{name_}
      , type{type_}
      , offset{offset_}
      , allocated_resource{allocated_resource_}
      , code{code_}
      , flags{flags_}
      , default_value{default_value_}
      , versions{versions_}
      , description{description_}
  {
  }

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

  std::vector<std::string> aliases = {};

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
