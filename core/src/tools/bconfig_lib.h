/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_TOOLS_BCONFIG_LIB_H_
#define BAREOS_TOOLS_BCONFIG_LIB_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "lib/bareos_resource.h"
#include "lib/parse_conf.h"

namespace bconfig {

enum class Component
{
  kDirector,
  kStorage,
  kClient,
#ifdef BCONFIG_HAVE_CONSOLE
  kConsole,
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
  kTrayMonitor,
#endif
};

struct ParseMessages {
  std::vector<std::string> errors{};
  std::vector<std::string> warnings{};
};

struct DirectiveSchemaEntry {
  std::string name{};
  std::string datatype{};
  std::optional<std::string> default_value{};
  bool required{false};
  bool deprecated{false};
  std::vector<std::string> aliases{};
  std::optional<std::string> description{};
};

struct ResourceSchemaEntry {
  std::string name{};
  std::vector<DirectiveSchemaEntry> directives{};
};

struct RelationEntry {
  std::string directive{};
  std::string target_type{};
  std::string target_name{};
  std::optional<BareosResource::SourceLocation> source{};
};

struct DirectiveUseEntry {
  std::string name{};
  std::optional<BareosResource::SourceLocation> source{};
};

struct ResourceInspectionEntry {
  std::string type{};
  std::string name{};
  std::optional<BareosResource::SourceLocation> source{};
  bool internal{false};
  std::vector<DirectiveUseEntry> directives{};
  std::vector<RelationEntry> relations{};
};

struct LoadedConfig {
  Component component;
  std::unique_ptr<ConfigurationParser> parser{};
  ParseMessages messages{};
  bool parse_ok{false};
};

std::optional<Component> ParseComponent(std::string_view component_name);
const char* ComponentToString(Component component);
LoadedConfig LoadConfig(Component component,
                        const std::string& path,
                        bool parse = true);
std::vector<ResourceSchemaEntry> CollectSchema(const ConfigurationParser& config);
std::vector<ResourceInspectionEntry> CollectResources(
    ConfigurationParser& config);

}  // namespace bconfig

#endif  // BAREOS_TOOLS_BCONFIG_LIB_H_
