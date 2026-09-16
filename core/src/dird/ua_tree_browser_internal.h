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

#ifndef BAREOS_DIRD_UA_TREE_BROWSER_INTERNAL_H_
#define BAREOS_DIRD_UA_TREE_BROWSER_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dird/restore_plugin_hints.h"

struct tree_node;

namespace directordaemon::tree_browser_internal {

enum class FrameBorderStyle
{
  kTop,
  kMiddle,
  kBottom,
};

size_t TextCellWidth(std::string_view text);

std::string CaseFoldForSearch(std::string_view text);

void RemoveLastUtf8Character(std::string* text);

std::string FitText(std::string_view text,
                    size_t width,
                    size_t horizontal_offset = 0,
                    bool ellipsis = true);

std::string AlignTextColumns(std::string_view left,
                             std::string_view right,
                             size_t width);

std::string FormatDetailColumns(std::string_view size,
                                std::string_view modified);

std::string RenderFrameBorder(size_t width,
                              FrameBorderStyle style,
                              std::string_view title = {});

std::string StyleFrameContent(std::string content,
                              char mark,
                              bool highlighted,
                              bool color);

bool IsTopLevelSelection(const tree_node* node);

void SortDirectoriesFirst(std::vector<tree_node*>* nodes);

std::string EstimateStatus(bool calculated, bool stale, uint64_t bytes);

// Renders detected "Plugin = ..." FileSet definitions (and their resolved
// hints, if any) as plain text lines for the tree browser's plugin hints
// panel. definitions and resolved_hints must be the same size (parallel
// vectors, as produced by GatherPluginHints()).
std::vector<std::string> BuildDetectedPluginHintLines(
    const std::vector<
        directordaemon::restore_plugin_hints::FileSetPluginDefinition>&
        definitions,
    const std::vector<
        const directordaemon::restore_plugin_hints::PluginRestoreHint*>&
        resolved_hints);

// Renders every known plugin restore hint (sorted by display name) as
// plain text lines -- the "show all known plugin hints" view.
std::vector<std::string> BuildAllKnownPluginHintLines();

// Builds a short, deduplicated, comma-joined summary of the plugin names
// used by the FileSet(s) backing this restore (e.g. "bpipe" or
// "bpipe, python"), for display in the browser's persistent status bar.
// Returns an empty string when definitions is empty.
std::string SummarizePluginNames(
    const std::vector<
        directordaemon::restore_plugin_hints::FileSetPluginDefinition>&
        definitions);

// Builds the advertisement shown in the main browser's help footer when
// at least one plugin was detected in the FileSet(s) backing this
// restore, prompting the user towards the plugin hints panel ('p') and
// the Plugin Options entry ('o'). Returns an empty string when no
// plugin was detected (plugin_names_summary empty).
std::string BuildPluginOptionsAdvertisement(
    std::string_view plugin_names_summary);

// Splits the body's line budget (as returned by the browser's
// MaxVisibleRows()) between the file tree (top) and the Plugin Options
// pane (bottom) for the split-screen layout shown once a plugin backup
// is detected. Returns {tree_rows, plugin_rows}; the tree gets the
// extra row on an odd split. plugin_rows is 0 when there's no room for
// a second pane (total_rows == 0).
std::pair<size_t, size_t> SplitTreeAndPluginRows(size_t total_rows);

// Decides what plain text (no ANSI styling) the Plugin Options input
// line should display: the current input value, or -- when it's empty
// -- a placeholder hinting that the field is editable and giving an
// example, so the field's purpose is obvious even before the user
// types anything.
std::string PluginOptionsInputDisplayText(std::string_view input);

constexpr size_t MaxHorizontalOffset(size_t text_width, size_t viewport_width)
{
  return text_width > viewport_width ? text_width - viewport_width : 0;
}

}  // namespace directordaemon::tree_browser_internal

#endif  // BAREOS_DIRD_UA_TREE_BROWSER_INTERNAL_H_
