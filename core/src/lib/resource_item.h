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

struct Alias {
  std::vector<std::string> aliases;

  template <typename... Ts>
  Alias(Ts... args)
    : aliases{ std::forward<Ts>(args)...}
  {
    static_assert(sizeof...(Ts) > 0, "You need to specify at least one alias.");
  }
};

struct UsesNoEquals {};

struct Description {
  const char* text;
};

struct PlatformSpecific {};
};  // namespace config


template <typename What, typename... Args> struct occurances {
  static constexpr size_t value = []() {
    if constexpr (sizeof...(Args) == 0) {
      return 0;
    } else {
      return (std::is_same_v<What, Args> + ...);
    }
  }();
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
  std::optional<int> extra{};
  bool required{};
  std::vector<std::string> aliases{};
  bool platform_specific{};
  bool no_equals{};
  const char* description{};

  template <typename... Types> ResourceItemFlags(Types&&... values)
  {
    static_assert(
        (is_present<Types, config::DefaultValue, config::IntroducedIn,
                    config::DeprecatedSince, config::Code, config::Required,
                    config::Alias, config::Description,
                    config::PlatformSpecific, config::UsesNoEquals>::value
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
    static_assert(occurances<config::Alias, Types...>::value <= 1,
                  "flag may only be specified once");
    static_assert(occurances<config::Description, Types...>::value <= 1,
                  "flag may only be specified once");
    static_assert(occurances<config::PlatformSpecific, Types...>::value <= 1,
                  "flag may only be specified once");
    static_assert(occurances<config::UsesNoEquals, Types...>::value <= 1,
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
    if (auto* alias = get_if<config::Alias>(tup)) {
      aliases = std::move(alias->aliases);
    }
    if (auto* _ = get_if<config::UsesNoEquals>(tup)) { no_equals = true; }
    if (auto* _ = get_if<config::PlatformSpecific>(tup)) {
      platform_specific = true;
    }
    if (auto* desc = get_if<config::Description>(tup)) {
      description = desc->text;
    }
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
               ResourceItemFlags&& resource_flags)
      : name{name_}
      , type{type_}
      , offset{offset_}
      , allocated_resource{allocated_resource_}
      , code{resource_flags.extra.value_or(0)}
      , aliases{std::move(resource_flags.aliases)}
      , required{resource_flags.required}
      , deprecated{resource_flags.deprecated_since.has_value()}
      , platform_specific{resource_flags.platform_specific}
      , no_equal{resource_flags.no_equals}
      , default_value{resource_flags.default_value}
      , introduced_in{resource_flags.introduced_in}
      , deprecated_since{resource_flags.deprecated_since}
      , description{resource_flags.description}
  {
  }

  ResourceItem() = default;

  const char* name{}; /* Resource name i.e. Director, ... */
  int type{};
  std::size_t offset{};
  BareosResource** allocated_resource{};
  int32_t code{}; /* Item code/additional info */
  std::vector<std::string> aliases{};
  bool required{};
  bool deprecated{};
  bool platform_specific{};
  bool no_equal{};
  const char* default_value{}; /* Default value */

  std::optional<config::Version> introduced_in{};
  std::optional<config::Version> deprecated_since{};

  /* short description of the directive, in plain text,
   * used for the documentation.
   * Full sentence.
   * Every new directive should have a description. */
  const char* description{};

  void SetPresent() const { (*allocated_resource)->SetMemberPresent(name); }

  bool IsPresent() const
  {
    return (*allocated_resource)->IsMemberPresent(name);
  }

  bool is_required() const { return required; }
  bool is_platform_specific() const { return platform_specific; }
  bool is_deprecated() const { return deprecated; }
  bool has_no_eq() const { return no_equal; }
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
