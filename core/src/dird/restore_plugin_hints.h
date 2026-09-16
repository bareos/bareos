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
/**
 * @file
 * Curated, static knowledge base of restore plugin option hints -- the
 * single source of truth for the ".pluginhints" dot command (consumed by
 * both bconsole and, via the webui-proxy, the webui) and for the
 * interactive restore tree browser's plugin hints panel
 * (ua_tree_browser.cc). Mirrors what used to be a webui-only static
 * dataset (webui-vue/src/data/restorePluginHints.js) and FileSet plugin
 * definition parsing (webui-vue/src/utils/restore.js), now available to
 * every console.
 */

#ifndef BAREOS_DIRD_RESTORE_PLUGIN_HINTS_H_
#define BAREOS_DIRD_RESTORE_PLUGIN_HINTS_H_

#include "include/bc_types.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

class BareosDb;

namespace directordaemon::restore_plugin_hints {

// One documented (or reverse-engineered) restore plugin option.
struct PluginOptionHint {
  std::string_view name;
  std::string_view status;  // "required" | "optional" | "known"
  std::string_view description;
  std::string_view source;  // "plugin-doc" | "plugin-readme" |
                            // "plugin-source" | "plugin-example"
};

// All known hints for one restore plugin.
struct PluginRestoreHint {
  std::string_view id;  // Stable key, e.g. "vmware".
  std::string_view display_name;
  std::string_view manual_url;        // May be empty.
  std::string_view option_separator;  // Usually ":".
  std::string_view note;              // May be empty.
  std::string_view support_level;     // "bareos" (default) | "third-party" |
                                      // "contrib"
  std::span<const std::string_view> aliases;
  std::span<const PluginOptionHint> options;
};

// All curated plugin hints, in declaration order (callers sort for
// display as needed -- see SortedPluginRestoreHints()).
std::span<const PluginRestoreHint> AllPluginRestoreHints();

// Same as AllPluginRestoreHints(), but sorted by display_name -- matches
// the webui's "all known plugin hints" listing order.
std::vector<const PluginRestoreHint*> SortedPluginRestoreHints();

// Looks a hint up by its id or by any of its aliases (case-insensitive).
// Returns nullptr if unknown.
const PluginRestoreHint* FindPluginRestoreHint(std::string_view id_or_alias);

// One "Plugin = ..." definition extracted from a FileSet's raw text,
// plus the option keys ("key=") seen inside it.
struct FileSetPluginDefinition {
  std::string raw;
  std::string plugin_name;  // Text before the first ':'.
  std::vector<std::string> option_keys;
};

// Extracts every "Plugin = "..."" (including continuation lines) from a
// FileSet resource's raw text (FileSetDbRecord::FileSetText), the same
// way the webui parses FileSet::filesettext.
std::vector<FileSetPluginDefinition> ExtractFileSetPluginDefinitions(
    std::string_view fileset_text);

// Resolves the hint (if any) that matches a FileSet plugin definition,
// preferring an alias match on its "module_name=" option over the raw
// plugin loader name (e.g. "python-fd", "grpc").
const PluginRestoreHint* ResolvePluginRestoreHint(
    const FileSetPluginDefinition& definition);

// Builds a short "key=... :key2=..." usage example from a hint's
// options, preferring required options.
std::string BuildPluginOptionExample(const PluginRestoreHint& hint);

// Fetches FileSet.FileSetText for a given FileSetId. Needed because
// BareosDb::GetFilesetRecord() only fetches the id/name/MD5/createtime
// columns, not the (potentially large) FileSetText column. Returns false
// if the record can't be found. Shared by the ".pluginhints" dot command
// and the interactive restore tree browser's plugin hints panel.
bool GetFileSetTextByFileSetId(BareosDb* db,
                               DBId_t fileset_id,
                               std::string* text);

}  // namespace directordaemon::restore_plugin_hints

#endif  // BAREOS_DIRD_RESTORE_PLUGIN_HINTS_H_
