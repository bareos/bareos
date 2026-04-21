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

#include <iostream>

#include "include/exit_codes.h"
#include "lib/cli.h"

namespace {

void PrintSource(const std::optional<BareosResource::SourceLocation>& source)
{
  if (!source) { return; }
  std::cout << " @ " << source->file << ":" << source->line;
}

void PrintMessages(const bconfig::ParseMessages& messages)
{
  if (!messages.errors.empty()) {
    std::cout << "Errors:\n";
    for (const auto& error : messages.errors) {
      std::cout << "  - " << error << "\n";
    }
  }

  if (!messages.warnings.empty()) {
    std::cout << "Warnings:\n";
    for (const auto& warning : messages.warnings) {
      std::cout << "  - " << warning << "\n";
    }
  }
}

void PrintHeader(const bconfig::LoadedConfig& loaded)
{
  std::cout << "Component: " << bconfig::ComponentToString(loaded.component)
            << "\n"
            << "Config: " << loaded.parser->get_base_config_path() << "\n"
            << "Parse status: " << (loaded.parse_ok ? "ok" : "failed") << "\n";
  PrintMessages(loaded.messages);
}

void PrintRelationsOnly(ConfigurationParser& parser)
{
  bool found_relations = false;

  for (const auto& resource : bconfig::CollectResources(parser)) {
    if (resource.internal || resource.relations.empty()) { continue; }

    found_relations = true;
    std::cout << "\nResource: " << resource.type << " \"" << resource.name
              << "\"";
    PrintSource(resource.source);
    std::cout << "\n";

    for (const auto& relation : resource.relations) {
      std::cout << "  - " << relation.directive << " -> "
                << relation.target_type << " \"" << relation.target_name
                << "\"";
      PrintSource(relation.source);
      std::cout << "\n";
    }
  }

  if (!found_relations) { std::cout << "\nNo internal relations found.\n"; }
}

}  // namespace

int main(int argc, char** argv)
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  CLI::App app;
  InitCLIApp(app, "Inspect Bareos configuration schemas and parsed resources.");
  AddDebugOptions(app);

  std::string component_name{};
  std::string config_path{};

  auto* schema_command = app.add_subcommand(
      "schema", "Print resource and directive metadata for one component.");
  schema_command
      ->add_option("component", component_name,
                   "Component to inspect: director, storage, client"
#ifdef BCONFIG_HAVE_CONSOLE
                   ", console"
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
                   ", traymonitor"
#endif
                   )
      ->required();

  auto* inspect_command = app.add_subcommand(
      "inspect", "Parse one component config and print resources and links.");
  inspect_command
      ->add_option("component", component_name,
                   "Component to inspect: director, storage, client"
#ifdef BCONFIG_HAVE_CONSOLE
                   ", console"
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
                   ", traymonitor"
#endif
                   )
      ->required();
  inspect_command
      ->add_option("config", config_path,
                   "Config file or configuration directory root.")
      ->required();

  auto* relations_command = app.add_subcommand(
      "relations",
      "Parse one component config and print only internal relations.");
  relations_command
      ->add_option("component", component_name,
                   "Component to inspect: director, storage, client"
#ifdef BCONFIG_HAVE_CONSOLE
                   ", console"
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
                   ", traymonitor"
#endif
                   )
      ->required();
  relations_command
      ->add_option("config", config_path,
                   "Config file or configuration directory root.")
      ->required();

  ParseBareosApp(app, argc, argv);

  auto component = bconfig::ParseComponent(component_name);
  if (!component) {
    std::cerr << "Unknown component: " << component_name << "\n";
    return BEXIT_FAILURE;
  }

  if (*schema_command) {
    auto loaded = bconfig::LoadConfig(*component, "", false);
    if (!loaded.parser) {
      PrintMessages(loaded.messages);
      return BEXIT_FAILURE;
    }

    std::cout << "Component: " << bconfig::ComponentToString(*component)
              << "\n";
    for (const auto& resource : bconfig::CollectSchema(*loaded.parser)) {
      std::cout << "\nResource: " << resource.name << "\n";
      for (const auto& directive : resource.directives) {
        std::cout << "  - " << directive.name << " [" << directive.datatype
                  << "]";
        if (directive.required) { std::cout << " required"; }
        if (directive.deprecated) { std::cout << " deprecated"; }
        if (directive.default_value) {
          std::cout << " default=" << *directive.default_value;
        }
        if (!directive.aliases.empty()) {
          std::cout << " aliases=";
          for (size_t i = 0; i < directive.aliases.size(); ++i) {
            if (i > 0) { std::cout << ","; }
            std::cout << directive.aliases[i];
          }
        }
        std::cout << "\n";
        if (directive.description) {
          std::cout << "      " << *directive.description << "\n";
        }
      }
    }

    return BEXIT_SUCCESS;
  }

  auto loaded = bconfig::LoadConfig(*component, config_path);
  if (!loaded.parser) {
    PrintMessages(loaded.messages);
    return BEXIT_FAILURE;
  }

  PrintHeader(loaded);

  if (*relations_command) {
    PrintRelationsOnly(*loaded.parser);
    return loaded.parse_ok ? BEXIT_SUCCESS : BEXIT_FAILURE;
  }

  for (const auto& resource : bconfig::CollectResources(*loaded.parser)) {
    if (resource.internal) { continue; }

    std::cout << "\nResource: " << resource.type << " \"" << resource.name
              << "\"";
    PrintSource(resource.source);
    std::cout << "\n";

    if (!resource.directives.empty()) {
      std::cout << "  Directives:\n";
      for (const auto& directive : resource.directives) {
        std::cout << "    - " << directive.name;
        PrintSource(directive.source);
        std::cout << "\n";
      }
    }

    if (!resource.relations.empty()) {
      std::cout << "  Internal relations:\n";
      for (const auto& relation : resource.relations) {
        std::cout << "    - " << relation.directive << " -> "
                  << relation.target_type << " \"" << relation.target_name
                  << "\"";
        PrintSource(relation.source);
        std::cout << "\n";
      }
    }
  }

  return loaded.parse_ok ? BEXIT_SUCCESS : BEXIT_FAILURE;
}
