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


#include <jansson.h>
#include <cassert>
#include <format>
#include <string>

#include "lib/cli.h"

static constexpr std::size_t spaces_per_indent = 2;

void add_indent(std::string& result, size_t indent)
{
  for (size_t i = 0; i < indent * spaces_per_indent; ++i) { result += " "; }
}

std::string format_json(json_t* value, size_t indent)
{
  if (json_is_boolean(value)) {
    return json_boolean_value(value) ? "Yes" : "No";
  } else if (json_is_integer(value)) {
    return std::format("{}", json_integer_value(value));
  } else if (json_is_string(value)) {
    return std::format("\"{}\"", json_string_value(value));
  } else if (json_is_real(value)) {
    return std::format("{}", json_real_value(value));
  } else if (json_is_object(value)) {
    const char* key = {};
    json_t* inner = {};

    std::string result = "{";

    json_object_foreach(value, key, inner)
    {
      result += "\n";
      add_indent(result, indent);
      result += key;
      result += " = ";
      result += format_json(inner, indent + 1);
    }

    if (result.size() > 1) { result += "\n"; }

    result += "}";

    return result;
  } else if (json_is_array(value)) {
    std::string result;

    std::size_t index;
    json_t* x;

    json_array_foreach(value, index, x)
    {
      (void)index;
      result += format_json(x, indent);
      result += ",";
    }

    if (!result.empty()) {
      // remove last comma
      result.pop_back();
    } else {
      assert(0);
    }

    return result;
  } else {
    assert(0);
  }
}


int main(int argc, const char* argv[])
{
  CLI::App app;
  InitCLIApp(app, "The Bareos Config Reparser.", 2026);


  std::string path;
  app.add_option("path", path, "the json file to load");

  CLI11_PARSE(app, argc, argv);

  json_error_t error = {};
  json_t* toplevel = json_load_file(path.c_str(), 0, &error);

  if (!toplevel) {
    fprintf(stderr, "error: %s => %s\n", error.source, error.text);
    exit(1);
  }

  if (!json_is_object(toplevel)) {
    fprintf(stderr, "error: expected toplevel object\n");
    exit(1);
  }

  const char* resource_type = {};
  json_t* resource_map = {};

  json_object_foreach(toplevel, resource_type, resource_map)
  {
    const char* resource_name = {};
    json_t* resource = {};

    if (!json_is_object(resource_map)) {
      fprintf(stderr,
              "error: %s should be a map { resource-name -> resource-def }\n",
              resource_type);
      exit(1);
    }

    json_object_foreach(resource_map, resource_name, resource)
    {
      if (!json_is_object(resource)) {
        fprintf(stderr,
                "error: %s->%s should be a map { resource-option -> "
                "option_value }\n",
                resource_type, resource_name);
        exit(1);
      }

      printf("%s {\n", resource_type);
      printf("  Name = \"%s\",\n", resource_name);

      const char* option = {};
      json_t* value = {};

      json_object_foreach(resource, option, value)
      {
        printf("  %s = %s,\n", option, format_json(value, 1).c_str());
      }

      printf("}\n");
    }
  }
}
