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

#include <functional>
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

// One "pluginname:key1=value1:key2=value2:..." block, as sent in a
// single "pluginoptions" protocol command / used by one FileSet
// "Plugin = ..." definition. option.first is empty for a flag-style
// option that has no "=value" part.
struct PluginOptionsBlock {
  std::string plugin_name;
  std::vector<std::pair<std::string, std::string>> options;
};

// Parses one "pluginname:key1=value1:key2=value2:..." block. Options
// without an "=" are kept as flag-style options with an empty value.
PluginOptionsBlock ParsePluginOptionsBlock(std::string_view block);

// Inverse of ParsePluginOptionsBlock(): joins the plugin name and
// options back into a single "pluginname:key=value:..." string, using
// separator (defaults to ":", matching PluginRestoreHint::
// option_separator for all curated plugins).
std::string BuildPluginOptionsBlock(const PluginOptionsBlock& block,
                                    std::string_view separator = ":");

// Parses a full interactive "pluginoptions" document: one or more
// blocks (see PluginOptionsBlock), separated by newlines -- the format
// used by the restore command's "pluginoptions=" argument once it
// covers more than one plugin (see SendPluginOptions() in
// dird/fd_cmds.cc, which sends one "pluginoptions" protocol command per
// block). Empty lines are skipped.
std::vector<PluginOptionsBlock> ParsePluginOptionsDocument(
    std::string_view document);

// Inverse of ParsePluginOptionsDocument(): joins blocks back into a
// single newline-separated document.
std::string BuildPluginOptionsDocument(
    const std::vector<PluginOptionsBlock>& blocks,
    std::string_view separator = ":");

// Checks whether every individual "pluginname:key=value:..." block in
// a (possibly multi-block, newline-separated) pluginoptions document
// is authorized on its own, via is_block_authorized (typically a
// PluginOptions_ACL check). Required because SendPluginOptions() (in
// dird/fd_cmds.cc) sends one "pluginoptions" protocol command per
// block -- authorizing only the combined multi-line document as one
// string would let an allowed block "smuggle" an otherwise-denied
// block past a single-block-oriented ACL pattern. An empty document
// (no blocks) is always authorized.
bool AllPluginOptionsBlocksAuthorized(
    std::string_view document,
    const std::function<bool(const std::string&)>& is_block_authorized);

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
