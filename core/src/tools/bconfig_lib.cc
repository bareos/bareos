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

#include "tools/bconfig_lib.h"

#include <algorithm>
#include <array>
#include <cstdarg>

#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "filed/filed_conf.h"
#include "filed/filed_globals.h"
#include "include/bareos.h"
#include "lib/alist.h"
#include "lib/lex.h"
#include "lib/parse_conf.h"
#include "lib/resource_item.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"
#ifdef BCONFIG_HAVE_CONSOLE
#  include "console/console_conf.h"
#  include "console/console_globals.h"
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
#  include "qt-tray-monitor/tray_conf.h"
#endif

namespace bconfig {
namespace {

thread_local ParseMessages* active_parse_messages = nullptr;

void CaptureScannerMessage(std::vector<std::string>& messages,
                           lexer* lc,
                           const char* fmt,
                           va_list arg_ptr)
{
  char rendered[4096];
  vsnprintf(rendered, sizeof(rendered), fmt, arg_ptr);

  std::string prefix;
  if (lc && lc->fname) {
    prefix += lc->fname;
    prefix += ":";
    prefix += std::to_string(lc->line_no);
    prefix += ": ";
  }

  messages.emplace_back(prefix + rendered);
}

void CaptureConfigError(const char*, int, lexer* lc, const char* fmt, ...)
{
  if (!active_parse_messages) { return; }

  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  CaptureScannerMessage(active_parse_messages->errors, lc, fmt, arg_ptr);
  va_end(arg_ptr);
}

void CaptureConfigWarning(const char*, int, lexer* lc, const char* fmt, ...)
{
  if (!active_parse_messages) { return; }

  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  CaptureScannerMessage(active_parse_messages->warnings, lc, fmt, arg_ptr);
  va_end(arg_ptr);
}

void InitializeRuntime()
{
  static bool initialized = false;
  if (initialized) { return; }

  OSDependentInit();
#if HAVE_WIN32
  WSA_Init();
#endif
  initialized = true;
}

std::unique_ptr<ConfigurationParser> CreateParser(Component component,
                                                  const std::string& path)
{
  switch (component) {
    case Component::kDirector: {
      directordaemon::my_config = nullptr;
      directordaemon::me = nullptr;
      auto parser = std::unique_ptr<ConfigurationParser>(
          directordaemon::InitDirConfig(path.c_str(), M_ERROR));
      directordaemon::my_config = parser.get();
      return parser;
    }
    case Component::kStorage: {
      storagedaemon::my_config = nullptr;
      storagedaemon::me = nullptr;
      auto parser = std::unique_ptr<ConfigurationParser>(
          storagedaemon::InitSdConfig(path.c_str(), M_ERROR));
      storagedaemon::my_config = parser.get();
      if (parser) { parser->SetRunReadyCallback(false); }
      return parser;
    }
    case Component::kClient: {
      filedaemon::my_config = nullptr;
      filedaemon::me = nullptr;
      auto parser = std::unique_ptr<ConfigurationParser>(
          filedaemon::InitFdConfig(path.c_str(), M_ERROR));
      filedaemon::my_config = parser.get();
      return parser;
    }
#ifdef BCONFIG_HAVE_CONSOLE
    case Component::kConsole: {
      console::my_config = nullptr;
      console::me = nullptr;
      console::console_resource = nullptr;
      console::director_resource = nullptr;
      auto parser = std::unique_ptr<ConfigurationParser>(
          console::InitConsConfig(path.c_str(), M_ERROR));
      console::my_config = parser.get();
      return parser;
    }
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case Component::kTrayMonitor: {
      ::my_config = nullptr;
      auto parser
          = std::unique_ptr<ConfigurationParser>(InitTmonConfig(path.c_str(),
                                                                M_ERROR));
      ::my_config = parser.get();
      return parser;
    }
#endif
  }

  return nullptr;
}

std::optional<BareosResource::SourceLocation> CopySource(
    const BareosResource::SourceLocation* source)
{
  if (!source) { return std::nullopt; }
  return *source;
}

void AppendRelation(std::vector<RelationEntry>& relations,
                    const ConfigurationParser& config,
                    const ResourceItem& item,
                    BareosResource* target,
                    const BareosResource& resource)
{
  if (!target) { return; }

  relations.emplace_back(RelationEntry{
      item.name,
      config.ResToStr(target->rcode_),
      GetResourceName(target),
      CopySource(resource.GetMemberSource(item.name)),
  });
}

}  // namespace

std::optional<Component> ParseComponent(std::string_view component_name)
{
  if (component_name == "director") { return Component::kDirector; }
  if (component_name == "storage") { return Component::kStorage; }
  if (component_name == "client") { return Component::kClient; }
#ifdef BCONFIG_HAVE_CONSOLE
  if (component_name == "console") { return Component::kConsole; }
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
  if (component_name == "traymonitor") { return Component::kTrayMonitor; }
#endif
  return std::nullopt;
}

const char* ComponentToString(Component component)
{
  switch (component) {
    case Component::kDirector:
      return "director";
    case Component::kStorage:
      return "storage";
    case Component::kClient:
      return "client";
#ifdef BCONFIG_HAVE_CONSOLE
    case Component::kConsole:
      return "console";
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case Component::kTrayMonitor:
      return "traymonitor";
#endif
  }

  return "unknown";
}

LoadedConfig LoadConfig(Component component, const std::string& path, bool parse)
{
  InitializeRuntime();

  LoadedConfig result{component};
  result.parser = CreateParser(component, path);

  if (!result.parser) {
    result.messages.errors.emplace_back("failed to initialize parser");
    return result;
  }

  if (parse) {
    active_parse_messages = &result.messages;
    result.parser->scan_error_ = CaptureConfigError;
    result.parser->scan_warning_ = CaptureConfigWarning;
    result.parse_ok = result.parser->ParseConfig();
    active_parse_messages = nullptr;

    for (const auto& warning : result.parser->GetWarnings()) {
      result.messages.warnings.emplace_back(warning.c_str());
    }
  } else {
    result.parse_ok = true;
  }

  return result;
}

std::vector<ResourceSchemaEntry> CollectSchema(const ConfigurationParser& config)
{
  std::vector<ResourceSchemaEntry> resources;

  for (const ResourceTable* table = config.resource_definitions_; table->name;
       ++table) {
    ResourceSchemaEntry resource_entry;
    resource_entry.name = table->name;

    for (const ResourceItem* item = table->items; item && item->name; ++item) {
      DirectiveSchemaEntry directive_entry;
      directive_entry.name = item->name;
      directive_entry.datatype = DatatypeToString(item->type);
      if (item->default_value) {
        directive_entry.default_value = item->default_value;
      }
      directive_entry.required = item->is_required;
      directive_entry.deprecated = item->is_deprecated;
      directive_entry.aliases = item->aliases;
      if (item->description) { directive_entry.description = item->description; }
      resource_entry.directives.emplace_back(std::move(directive_entry));
    }

    resources.emplace_back(std::move(resource_entry));
  }

  return resources;
}

std::vector<ResourceInspectionEntry> CollectResources(ConfigurationParser& config)
{
  std::vector<ResourceInspectionEntry> resources;

  for (int resource_code = 0; resource_code < config.r_num_; ++resource_code) {
    for (BareosResource* resource = nullptr;
         (resource = config.GetNextRes(resource_code, resource)) != nullptr;) {
      ResourceInspectionEntry entry;
      entry.type = config.ResToStr(resource->rcode_);
      entry.name = GetResourceName(resource);
      entry.source = resource->GetDefinitionSource();
      entry.internal = resource->internal_;

      const ResourceItem* items = config.resource_definitions_[resource->rcode_].items;
      if (!items) {
        resources.emplace_back(std::move(entry));
        continue;
      }

      *config.resource_definitions_[resource->rcode_].allocated_resource_
          = resource;

      for (const ResourceItem* item = items; item->name; ++item) {
        if (!resource->IsMemberPresent(item->name)) { continue; }

        entry.directives.emplace_back(
            DirectiveUseEntry{item->name,
                              CopySource(resource->GetMemberSource(item->name))});

        switch (item->type) {
          case CFG_TYPE_RES: {
            auto target = *GetItemVariablePointer<BareosResource**>(*item);
            AppendRelation(entry.relations, config, *item, target, *resource);
            break;
          }
          case CFG_TYPE_ALIST_RES: {
            auto list
                = *GetItemVariablePointer<alist<BareosResource*>**>(*item);
            if (!list) { break; }
            for (auto target : *list) {
              AppendRelation(entry.relations, config, *item, target, *resource);
            }
            break;
          }
          default:
            break;
        }
      }

      resources.emplace_back(std::move(entry));
    }
  }

  return resources;
}

}  // namespace bconfig
