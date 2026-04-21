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
#include <string_view>

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
      if (parser) { parser->SetOptionValidation(false); }
      directordaemon::my_config = parser.get();
      return parser;
    }
    case Component::kStorage: {
      storagedaemon::my_config = nullptr;
      storagedaemon::me = nullptr;
      auto parser = std::unique_ptr<ConfigurationParser>(
          storagedaemon::InitSdConfig(path.c_str(), M_ERROR));
      storagedaemon::my_config = parser.get();
      if (parser) {
        parser->SetOptionValidation(false);
        parser->SetRunReadyCallback(false);
      }
      return parser;
    }
    case Component::kClient: {
      filedaemon::my_config = nullptr;
      filedaemon::me = nullptr;
      auto parser = std::unique_ptr<ConfigurationParser>(
          filedaemon::InitFdConfig(path.c_str(), M_ERROR));
      if (parser) { parser->SetOptionValidation(false); }
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
      if (parser) { parser->SetOptionValidation(false); }
      console::my_config = parser.get();
      return parser;
    }
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case Component::kTrayMonitor: {
      ::my_config = nullptr;
      auto parser = std::unique_ptr<ConfigurationParser>(
          InitTmonConfig(path.c_str(), M_ERROR));
      if (parser) { parser->SetOptionValidation(false); }
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

std::optional<BareosResource::SourceLocation> SourceOrDefinition(
    const BareosResource& resource,
    std::string_view member_name)
{
  auto source = CopySource(resource.GetMemberSource(member_name));
  if (source) { return source; }
  return resource.GetDefinitionSource();
}

const LoadedConfig* FindPeerConfig(
    const std::vector<const LoadedConfig*>& peers,
    Component component)
{
  auto it = std::find_if(
      peers.begin(), peers.end(),
      [component](const auto* p) { return p && p->component == component; });
  return it == peers.end() ? nullptr : *it;
}

template <typename ResourceT>
std::vector<ResourceT*> CollectTypedResources(const LoadedConfig& loaded)
{
  std::vector<ResourceT*> resources;

  for (int resource_code = 0; resource_code < loaded.parser->r_num_;
       ++resource_code) {
    for (BareosResource* resource = nullptr;
         (resource = loaded.parser->GetNextRes(resource_code, resource))
         != nullptr;) {
      if (auto* typed = dynamic_cast<ResourceT*>(resource)) {
        resources.emplace_back(typed);
      }
    }
  }

  return resources;
}

template <typename ResourceT, typename Predicate>
std::vector<ResourceT*> FindTypedResources(const LoadedConfig& loaded,
                                           Predicate predicate)
{
  std::vector<ResourceT*> matches;

  for (auto* resource : CollectTypedResources<ResourceT>(loaded)) {
    if (predicate(resource)) { matches.emplace_back(resource); }
  }

  return matches;
}

const char* SafeString(const char* value) { return value ? value : ""; }

bool PasswordsMatch(const s_password& lhs, const s_password& rhs)
{
  if (lhs.encoding != rhs.encoding) { return false; }
  if (!lhs.value || !rhs.value) { return lhs.value == rhs.value; }
  return std::string_view(lhs.value) == std::string_view(rhs.value);
}

void AppendExternalRelation(
    std::vector<ExternalRelationEntry>& relations,
    const char* relation_name,
    const LoadedConfig& target_config,
    BareosResource* target,
    std::optional<BareosResource::SourceLocation> source,
    std::optional<std::string> detail = std::nullopt)
{
  if (!target) { return; }

  relations.emplace_back(ExternalRelationEntry{
      relation_name,
      ComponentToString(target_config.component),
      target_config.parser->ResToStr(target->rcode_),
      GetResourceName(target),
      std::move(source),
      true,
      std::move(detail),
  });
}

void AppendMissingExternalRelation(
    std::vector<ExternalRelationEntry>& relations,
    const char* relation_name,
    Component target_component,
    const char* target_type,
    std::string target_name,
    std::optional<BareosResource::SourceLocation> source,
    std::string detail)
{
  relations.emplace_back(ExternalRelationEntry{
      relation_name,
      ComponentToString(target_component),
      target_type,
      std::move(target_name),
      std::move(source),
      false,
      std::move(detail),
  });
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

void AppendRunOverrideRelation(std::vector<RelationEntry>& relations,
                               const ConfigurationParser& config,
                               const char* directive_name,
                               BareosResource* target,
                               const directordaemon::RunResource& run)
{
  if (!target) { return; }

  relations.emplace_back(RelationEntry{
      directive_name,
      config.ResToStr(target->rcode_),
      GetResourceName(target),
      run.GetDefinitionSource(),
  });
}

void AppendDirectorScheduleRelations(std::vector<RelationEntry>& relations,
                                     const ConfigurationParser& config,
                                     const BareosResource& resource)
{
  auto schedule
      = dynamic_cast<const directordaemon::ScheduleResource*>(&resource);
  if (!schedule) { return; }

  for (auto* run = schedule->run; run != nullptr; run = run->next) {
    AppendRunOverrideRelation(relations, config, "Run.Pool", run->pool, *run);
    AppendRunOverrideRelation(relations, config, "Run.FullPool", run->full_pool,
                              *run);
    AppendRunOverrideRelation(relations, config, "Run.VirtualFullPool",
                              run->vfull_pool, *run);
    AppendRunOverrideRelation(relations, config, "Run.IncrementalPool",
                              run->inc_pool, *run);
    AppendRunOverrideRelation(relations, config, "Run.DifferentialPool",
                              run->diff_pool, *run);
    AppendRunOverrideRelation(relations, config, "Run.NextPool", run->next_pool,
                              *run);
    AppendRunOverrideRelation(relations, config, "Run.Storage", run->storage,
                              *run);
    AppendRunOverrideRelation(relations, config, "Run.Messages", run->msgs,
                              *run);
  }
}

void AppendDirectorPeerRelations(ResourceInspectionEntry& entry,
                                 const LoadedConfig& loaded,
                                 const std::vector<const LoadedConfig*>& peers,
                                 BareosResource& resource)
{
  if (loaded.component != Component::kDirector) { return; }

  const auto local_directors
      = CollectTypedResources<directordaemon::DirectorResource>(loaded);
  const auto local_director_name_source = SourceOrDefinition(resource, "Name");
  const auto local_password_source = SourceOrDefinition(resource, "Password");

  if (const auto* client_config = FindPeerConfig(peers, Component::kClient)) {
    if (auto* local_director
        = dynamic_cast<directordaemon::DirectorResource*>(&resource)) {
      auto peer_directors = FindTypedResources<filedaemon::DirectorResource>(
          *client_config, [local_director](auto* peer) {
            return std::string_view(SafeString(peer->resource_name_))
                   == SafeString(local_director->resource_name_);
          });

      if (peer_directors.empty()) {
        AppendMissingExternalRelation(
            entry.external_relations, "Peer.Director", Component::kClient,
            "Director", SafeString(local_director->resource_name_),
            local_director_name_source,
            "no loaded client config defines a Director with this name");
      } else {
        for (auto* peer : peer_directors) {
          AppendExternalRelation(entry.external_relations, "Peer.Director",
                                 *client_config, peer,
                                 local_director_name_source);
        }
      }
    } else if (auto* client
               = dynamic_cast<directordaemon::ClientResource*>(&resource)) {
      auto peer_clients = FindTypedResources<filedaemon::ClientResource>(
          *client_config, [client](auto* peer) {
            return std::string_view(SafeString(peer->resource_name_))
                   == SafeString(client->resource_name_);
          });

      if (peer_clients.empty()) {
        AppendMissingExternalRelation(
            entry.external_relations, "Peer.Client", Component::kClient,
            "Client", SafeString(client->resource_name_),
            SourceOrDefinition(*client, "Name"),
            "no loaded client config defines a Client with this name");
      } else {
        for (auto* peer : peer_clients) {
          AppendExternalRelation(entry.external_relations, "Peer.Client",
                                 *client_config, peer,
                                 SourceOrDefinition(*client, "Name"));
        }
      }

      bool matched_auth = false;
      for (auto* candidate_director : local_directors) {
        auto peer_directors = FindTypedResources<filedaemon::DirectorResource>(
            *client_config, [client, candidate_director](auto* peer) {
              return std::string_view(SafeString(peer->resource_name_))
                         == SafeString(candidate_director->resource_name_)
                     && PasswordsMatch(client->password_, peer->password_);
            });

        for (auto* peer : peer_directors) {
          matched_auth = true;
          AppendExternalRelation(
              entry.external_relations, "Peer.DirectorAuth", *client_config,
              peer, local_password_source,
              std::string("matched via Director \"")
                  + SafeString(candidate_director->resource_name_) + "\"");
        }
      }

      if (!matched_auth && !local_directors.empty()) {
        AppendMissingExternalRelation(
            entry.external_relations, "Peer.DirectorAuth", Component::kClient,
            "Director", SafeString(local_directors.front()->resource_name_),
            local_password_source,
            "no loaded client Director matches a local Director name and this "
            "Client password");
      }
    }
  }

  if (const auto* storage_config = FindPeerConfig(peers, Component::kStorage)) {
    if (auto* local_director
        = dynamic_cast<directordaemon::DirectorResource*>(&resource)) {
      auto peer_directors = FindTypedResources<storagedaemon::DirectorResource>(
          *storage_config, [local_director](auto* peer) {
            return std::string_view(SafeString(peer->resource_name_))
                   == SafeString(local_director->resource_name_);
          });

      if (peer_directors.empty()) {
        AppendMissingExternalRelation(
            entry.external_relations, "Peer.Director", Component::kStorage,
            "Director", SafeString(local_director->resource_name_),
            local_director_name_source,
            "no loaded storage config defines a Director with this name");
      } else {
        for (auto* peer : peer_directors) {
          AppendExternalRelation(entry.external_relations, "Peer.Director",
                                 *storage_config, peer,
                                 local_director_name_source);
        }
      }
    } else if (auto* storage
               = dynamic_cast<directordaemon::StorageResource*>(&resource)) {
      bool matched_auth = false;
      for (auto* candidate_director : local_directors) {
        auto peer_directors
            = FindTypedResources<storagedaemon::DirectorResource>(
                *storage_config, [storage, candidate_director](auto* peer) {
                  return std::string_view(SafeString(peer->resource_name_))
                             == SafeString(candidate_director->resource_name_)
                         && PasswordsMatch(storage->password_, peer->password_);
                });

        for (auto* peer : peer_directors) {
          matched_auth = true;
          AppendExternalRelation(
              entry.external_relations, "Peer.DirectorAuth", *storage_config,
              peer, local_password_source,
              std::string("matched via Director \"")
                  + SafeString(candidate_director->resource_name_) + "\"");
        }
      }

      if (!matched_auth && !local_directors.empty()) {
        AppendMissingExternalRelation(
            entry.external_relations, "Peer.DirectorAuth", Component::kStorage,
            "Director", SafeString(local_directors.front()->resource_name_),
            local_password_source,
            "no loaded storage Director matches a local Director name and this "
            "Storage password");
      }

      if (matched_auth) {
        for (const auto& device : storage->devices) {
          auto peer_devices = FindTypedResources<storagedaemon::DeviceResource>(
              *storage_config, [&device](auto* peer) {
                return std::string_view(SafeString(peer->resource_name_))
                       == device.name;
              });

          if (peer_devices.empty()) {
            AppendMissingExternalRelation(
                entry.external_relations, "Device", Component::kStorage,
                "Device", device.name, SourceOrDefinition(*storage, "Device"),
                "no loaded storage config defines this Device");
          } else {
            for (auto* peer : peer_devices) {
              AppendExternalRelation(entry.external_relations, "Device",
                                     *storage_config, peer,
                                     SourceOrDefinition(*storage, "Device"));
            }
          }
        }
      }
    }
  }

#ifdef BCONFIG_HAVE_CONSOLE
  if (const auto* console_config = FindPeerConfig(peers, Component::kConsole)) {
    if (auto* local_director
        = dynamic_cast<directordaemon::DirectorResource*>(&resource)) {
      auto peer_directors = FindTypedResources<console::DirectorResource>(
          *console_config, [local_director](auto* peer) {
            return std::string_view(SafeString(peer->resource_name_))
                       == SafeString(local_director->resource_name_)
                   && PasswordsMatch(local_director->password_,
                                     peer->password_);
          });

      if (peer_directors.empty()) {
        AppendMissingExternalRelation(
            entry.external_relations, "Peer.DefaultConsole",
            Component::kConsole, "Director",
            SafeString(local_director->resource_name_), local_password_source,
            "no loaded console Director matches this Director name and "
            "password");
      } else {
        for (auto* peer : peer_directors) {
          AppendExternalRelation(entry.external_relations,
                                 "Peer.DefaultConsole", *console_config, peer,
                                 local_password_source);
        }
      }
    } else if (auto* console_resource
               = dynamic_cast<directordaemon::ConsoleResource*>(&resource)) {
      auto peer_consoles = FindTypedResources<console::ConsoleResource>(
          *console_config, [console_resource](auto* peer) {
            return std::string_view(SafeString(peer->resource_name_))
                       == SafeString(console_resource->resource_name_)
                   && PasswordsMatch(console_resource->password_,
                                     peer->password_);
          });

      if (peer_consoles.empty()) {
        AppendMissingExternalRelation(
            entry.external_relations, "Peer.Console", Component::kConsole,
            "Console", SafeString(console_resource->resource_name_),
            local_password_source,
            "no loaded console config defines a Console with this name and "
            "password");
      } else {
        for (auto* peer : peer_consoles) {
          AppendExternalRelation(entry.external_relations, "Peer.Console",
                                 *console_config, peer, local_password_source);
        }
      }
    }
  }
#endif
}

std::vector<ResourceInspectionEntry> CollectResourcesImpl(
    ConfigurationParser& config,
    const LoadedConfig* loaded,
    const std::vector<const LoadedConfig*>& peers)
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

      const ResourceItem* items
          = config.resource_definitions_[resource->rcode_].items;
      if (!items) {
        resources.emplace_back(std::move(entry));
        continue;
      }

      *config.resource_definitions_[resource->rcode_].allocated_resource_
          = resource;

      for (const ResourceItem* item = items; item->name; ++item) {
        if (!resource->IsMemberPresent(item->name)) { continue; }

        entry.directives.emplace_back(DirectiveUseEntry{
            item->name, CopySource(resource->GetMemberSource(item->name))});

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

      AppendDirectorScheduleRelations(entry.relations, config, *resource);
      if (loaded) {
        AppendDirectorPeerRelations(entry, *loaded, peers, *resource);
      }

      resources.emplace_back(std::move(entry));
    }
  }

  return resources;
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

LoadedConfig LoadConfig(Component component,
                        const std::string& path,
                        bool parse)
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

    if (!result.messages.errors.empty()) { result.parse_ok = false; }
  } else {
    result.parse_ok = true;
  }

  return result;
}

std::vector<ResourceSchemaEntry> CollectSchema(
    const ConfigurationParser& config)
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
      if (item->description) {
        directive_entry.description = item->description;
      }
      resource_entry.directives.emplace_back(std::move(directive_entry));
    }

    resources.emplace_back(std::move(resource_entry));
  }

  return resources;
}

std::vector<ResourceInspectionEntry> CollectResources(
    ConfigurationParser& config)
{
  return CollectResourcesImpl(config, nullptr, {});
}

std::vector<ResourceInspectionEntry> CollectResources(
    const LoadedConfig& loaded,
    const std::vector<const LoadedConfig*>& peers)
{
  return CollectResourcesImpl(*loaded.parser, &loaded, peers);
}

}  // namespace bconfig
