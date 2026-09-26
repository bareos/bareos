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

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dird/restore_plugin_hints.h"

struct tree_node;

namespace directordaemon::tree_browser_internal {

inline constexpr std::string_view kFrameColor = "\033[1;94m";
inline constexpr std::string_view kFrameHighlightColor = "\033[97;44m";
inline constexpr std::string_view kFrameHelpColor = "\033[2m";
inline constexpr std::string_view kFrameMarkedFileColor = "\033[32m";
inline constexpr std::string_view kFrameMarkedDirectoryColor = "\033[1;33m";
inline constexpr std::string_view kFrameDefaultColor = "\033[39m";
inline constexpr std::string_view kFrameResetColor = "\033[0m";
inline constexpr std::string_view kFrameVerticalBorder = "│";

inline constexpr std::string_view kKeyEnter = "key:enter";
inline constexpr std::string_view kKeyCancel = "key:cancel";
inline constexpr std::string_view kKeyBackspace = "key:backspace";
inline constexpr std::string_view kKeyTab = "key:tab";
inline constexpr std::string_view kKeyUp = "key:up";
inline constexpr std::string_view kKeyDown = "key:down";
inline constexpr std::string_view kKeyLeft = "key:left";
inline constexpr std::string_view kKeyRight = "key:right";
inline constexpr std::string_view kKeyHome = "key:home";
inline constexpr std::string_view kKeyEnd = "key:end";
inline constexpr std::string_view kKeyPageUp = "key:pageup";
inline constexpr std::string_view kKeyPageDown = "key:pagedown";
inline constexpr std::string_view kKeySpace = "key:space";
inline constexpr std::string_view kKeyInsert = "key:insert";
inline constexpr std::string_view kKeyDelete = "key:delete";
inline constexpr std::string_view kKeyTextPrefix = "key:text:";
inline constexpr std::string_view kResizePrefix = "resize:";

inline constexpr const char* kRunDialogFooter
    = "Up/Down Move  Enter/e/Space Edit  Left/Right Adjust  h/? Help  Esc/. "
      "Cancel";
inline constexpr const char* kRestoreDialogFooter
    = "Up/Down Move  Enter/e/Space Edit  Left/Right Adjust  h/? Help  Esc/. "
      "Cancel";
inline constexpr const char* kRelocationDialogFooter
    = "Up/Down Move  Enter/e/Space Edit  Left/Right Adjust  h/? Help  Esc/. "
      "Cancel";
inline constexpr const char* kListDialogFooter
    = "Up/Down Move  Enter Select  Home/End First/Last  h/? Help  Esc/. Cancel";
inline constexpr const char* kFieldEditorFooter
    = "Left/Right Field  Up/Down Adjust  Enter Accept  h/? Help  Esc/. Cancel";
inline constexpr const char* kBrowserFooter
    = "Arrows Move/Scroll  Ins/Del/Space Mark  +/- Glob  h/? Help  Esc Done";
inline constexpr const char* kSearchResultsFooter
    = "Arrows Move/Scroll  Space Mark  Enter Go  h/? Help  Esc Return";
inline constexpr const char* kSelectedFilesFooter
    = "Arrows Move/Scroll  Space Unmark  Enter Go  h/? Help  l/Esc Return";
inline constexpr const char* kEstimateBusyFooter
    = "Calculating estimate... Please wait; input is disabled";
inline constexpr const char* kPluginHintsFooter
    = "Up/Down Scroll  a All/Detected  o/Tab Options  h/? Help  p/Esc Return";
inline constexpr const char* kPluginOptionsFooter
    = "Up/Down Row  Left/Right Plugin  Enter Edit  h/? Help  Tab Files";
inline constexpr const char* kTextInputHelp
    = "Type text  Backspace Delete  Enter Accept  Esc Cancel";
inline constexpr const char* kGlobInputHelp
    = "Type wildcard pattern (e.g. *.txt)  Backspace Delete  Enter Accept  "
      "Esc Cancel";
inline constexpr const char* kDialogTextInputHelp
    = "Type text  Backspace Delete  Enter Accept  Esc/. Cancel";
inline constexpr const char* kDestinationBrowserFooter
    = "Arrows Move/Scroll  Enter Open/Use  / Search  h/? Help  Esc/. Cancel";
inline constexpr const char* kDestinationSearchHelpLine1
    = "Type substring/Space  Backspace Delete";
inline constexpr const char* kDestinationSearchHelpLine2
    = "Up/Down/Tab Move  PgUp/PgDn Page  Enter Accept  Esc/. Clear";
inline constexpr const char* kSelectionFooter
    = "Arrows Move  Enter Select  Type Filter  h/? Help  Esc/. Cancel";

inline constexpr std::array<std::string_view, 5> kRunDialogHelp = {
    "Down/Tab: next row; Up/Backspace: previous row",
    "Home/End: first/last row",
    "Enter or e/E/Space: run, edit, toggle, or expand the selected row",
    "Left/Right: adjust cycle values or scroll long values",
    "Esc/.: cancel; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 6> kRestoreDialogHelp = {
    "Down/Tab: next row; Up/Backspace: previous row",
    "Home/End: first/last row",
    "Enter or e/E/Space: run, edit, toggle, or expand the selected row",
    "Left/Right: adjust priority/Advanced Options or scroll long values",
    "b/B: browse destination while Where is selected; Esc/.: cancel",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 5> kRelocationDialogHelp = {
    "Down/Tab: next row; Up/Backspace: previous row",
    "Home/End: first/last row",
    "Enter or e/E/Space: apply, edit, or cycle the selected row",
    "Left/Right: cycle relocation mode or scroll long values",
    "Esc/.: cancel; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 4> kListDialogHelp = {
    "Down/Tab: next row; Up/Backspace: previous row",
    "Home/End: first/last row; Enter: select",
    "Esc/.: cancel",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 5> kFieldEditorHelp = {
    "Left/Right/Tab: next field; Backspace: previous field",
    "Home/End: first/last field; Up/Down: increase/decrease",
    "n/N: set the start time to now",
    "Enter: accept; Esc/.: cancel",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 5> kDestinationBrowserHelp = {
    "Up/Down/Tab: move; Backspace: parent directory",
    "Left/Right: scroll names; Home/End: first/last row",
    "PgUp/PgDn: move one page; Enter: open or use directory",
    "/: search; Esc/.: clear search or cancel",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 11> kBrowserHelp = {
    "Up/Down/Tab: move; Left/Right: scroll names",
    "Home/End: first/last column; Enter: open directory or '..'",
    "Backspace: parent directory",
    "Space/m: mark/unmark; Insert: mark; Delete: unmark",
    "+: mark by glob pattern; -: unmark by glob pattern",
    "a: mark all; u: unmark all",
    "l: list selections; e: estimate size; i: toggle details",
    "/: search; :: one classic command; c: classic mode",
    "p: plugin hints; o: focus Plugin Options; Tab: switch panes",
    "d/r/q/Esc: finish file selection",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 5> kSearchResultsHelp = {
    "Up/Down/Tab: move; Left/Right: scroll paths",
    "Home/End: first/last column; Space: mark/unmark",
    "e: estimate size; Enter: go to matching file",
    "Esc: return to files",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 5> kSelectedFilesHelp = {
    "Up/Down/Tab: move; Left/Right: scroll paths",
    "Home/End: first/last column; Space: unmark selection",
    "e: estimate size; Enter: go to selected file",
    "l/Esc: return to files",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 4> kPluginHintsHelp = {
    "Up/Down/Tab: scroll plugin hints",
    "a: toggle all known hints or only detected plugins",
    "o/Tab: focus Plugin Options; p/Esc: return to files",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 5> kPluginOptionsHelp = {
    "Up/Down: move row; Left/Right: switch plugin tab",
    "Enter: edit row; d: delete row/tab; n: add plugin tab",
    "Tab: save and focus files; Esc: discard edits and focus files",
    "r: finish file selection",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};
inline constexpr std::array<std::string_view, 3> kPluginBooleanHelp = {
    "Up/Down/Left/Right: toggle yes/no; Enter: confirm",
    "Esc: return to option choices; Tab: save and focus files",
    "h/?: open help; h/?/Enter/Esc/.: return from help",
};

inline bool IsHelpKey(std::string_view key)
{
  return key == "key:text:h" || key == "key:text:?";
}

inline bool IsHelpReturnKey(std::string_view key)
{
  return IsHelpKey(key) || key == kKeyEnter || key == kKeyCancel || key == "."
         || key == "key:text:.";
}

inline bool IsEnterKey(std::string_view key) { return key == kKeyEnter; }
inline bool IsCancelKey(std::string_view key)
{
  return key == kKeyCancel || key == "." || key == "key:text:.";
}
inline bool IsBackKey(std::string_view key)
{
  return key == kKeyCancel || key == kKeyBackspace || key == "."
         || key == "key:text:.";
}
inline bool IsEditKey(std::string_view key)
{
  return key == kKeySpace || key == "key:text:e" || key == "key:text:E";
}
inline bool IsNextRowKey(std::string_view key)
{
  return key == kKeyTab || key == kKeyDown;
}
inline bool IsPreviousRowKey(std::string_view key) { return key == kKeyUp; }
inline bool IsScrollLeftKey(std::string_view key) { return key == kKeyLeft; }
inline bool IsScrollRightKey(std::string_view key) { return key == kKeyRight; }
inline bool IsHomeKey(std::string_view key) { return key == kKeyHome; }
inline bool IsEndKey(std::string_view key) { return key == kKeyEnd; }
inline bool IsTextKey(std::string_view key)
{
  return key.starts_with(kKeyTextPrefix);
}
inline std::string_view TextKeyValue(std::string_view key)
{
  return key.substr(kKeyTextPrefix.size());
}

inline void TrimVisualInput(std::string_view* input)
{
  while (!input->empty()
         && (input->back() == '\r' || input->back() == '\n'
             || input->back() == ' ' || input->back() == '\t')) {
    input->remove_suffix(1);
  }
}

struct TerminalResizeInput {
  bool is_resize = false;
  int height = 0;
  int width = 0;
};

inline int ParsePositiveInteger(std::string_view text)
{
  int value = 0;
  auto [ptr, ec]
      = std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc() || ptr != text.data() + text.size() || value <= 0) {
    return 0;
  }
  return value;
}

inline TerminalResizeInput ParseTerminalResizeInput(std::string_view input)
{
  TerminalResizeInput result;
  if (!input.starts_with(kResizePrefix)) { return result; }

  result.is_resize = true;
  std::string_view size_view = input.substr(kResizePrefix.size());
  size_t separator = size_view.find(':');
  result.height = ParsePositiveInteger(size_view.substr(0, separator));
  if (separator != std::string_view::npos) {
    result.width = ParsePositiveInteger(size_view.substr(separator + 1));
  }
  return result;
}

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

std::string EstimateStatus(bool calculated,
                           bool stale,
                           uint64_t bytes,
                           bool running = false,
                           char spinner = '|');

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
// is detected. Returns {tree_rows, plugin_rows}, using roughly 25% for
// the tree and 75% for the options editor.
std::pair<size_t, size_t> SplitTreeAndPluginRows(size_t total_rows);

// The tree browser shows a synthetic ".." row before the real child
// rows whenever the current directory has a parent, letting the user
// jump up a level without needing the Backspace shortcut. These
// functions translate between a cursor position in the *displayed* list
// (which may include that synthetic row at index 0) and a real index
// into the list of real child rows.

// Number of displayed rows taken up by the synthetic ".." row: 1 when
// the current directory has a parent, 0 (root directory) otherwise.
size_t RowOffsetForParent(bool has_parent);

// Where the cursor should land by default when (re-)entering a
// directory: on the first real child row if there is one, otherwise on
// the synthetic ".." row (or 0 if there is no parent and no children).
size_t DefaultCursorFor(size_t row_offset, size_t child_row_count);

// True when the given displayed cursor position refers to the synthetic
// ".." row rather than a real child row.
bool IsParentRowCursor(size_t cursor, size_t row_offset);

// Maps a displayed cursor position to the corresponding index into the
// real child rows. Only meaningful when !IsParentRowCursor(cursor,
// row_offset); the caller is still responsible for bounds-checking the
// result against the real row count.
size_t DisplayedCursorToChildIndex(size_t cursor, size_t row_offset);

// Decides what plain text (no ANSI styling) the Plugin Options input
// line should display: the current input value, or -- when it's empty
// -- a placeholder hinting that the field is editable and giving an
// example, so the field's purpose is obvious even before the user
// types anything.
// Builds the plain-text label for each row of the structured Plugin
// Options editor for one plugin block, in row order: row 0 is
// "Plugin: <name>" (or a placeholder when empty), rows [1,
// options.size()] are "  key = value" (value omitted when empty, e.g.
// flag-style options), and the final row is the "+ add option"
// affordance. Pure/testable independently of ANSI styling and cursor
// highlighting (applied by the caller).
std::vector<std::string> BuildPluginOptionsRowLabels(
    const directordaemon::restore_plugin_hints::PluginOptionsBlock& block);

// Builds a one-line tab bar summarizing all plugin blocks (e.g.
// "[1:bpipe] >[2:barri]<"), with the active block wrapped in ">...<".
// Returns an empty string when there's only one block (nothing to
// switch between).
std::string BuildPluginOptionsTabBar(
    const std::vector<directordaemon::restore_plugin_hints::PluginOptionsBlock>&
        blocks,
    size_t active_block);

// Builds a single-line hint summarizing the known options (from the
// curated hint database) for the given plugin options block, resolving
// the block's hint the same module_name-aware way FileSet-level
// resolution does. Groups options into "already in FileSet" (with
// their current FileSet value, sourced from the matching detected
// definition, if any), "required" (marked "*"), and a count of
// remaining "optional" options. Returns an empty string when the
// block's plugin is unknown or has no documented options.
std::string BuildKnownOptionsHintLine(
    const directordaemon::restore_plugin_hints::PluginOptionsBlock& block,
    const std::vector<
        directordaemon::restore_plugin_hints::FileSetPluginDefinition>&
        definitions);

struct PluginOptionChoice {
  std::string key;
  std::string group;
  std::string description;
  directordaemon::restore_plugin_hints::PluginOptionType type;
};

// Builds the selectable known-option list for "+ add option", ordered by
// options already present in the FileSet, required options, then optional
// options. Keys already present in the editor block are omitted.
std::vector<PluginOptionChoice> BuildPluginOptionChoices(
    const directordaemon::restore_plugin_hints::PluginOptionsBlock& block,
    const std::vector<
        directordaemon::restore_plugin_hints::FileSetPluginDefinition>&
        definitions);

// Decides how many of a plugin block's editor rows can be shown given
// the pane's row budget, and whether there's still room for one extra
// "known options" hint line below them.
struct PluginOptionsRowWindow {
  size_t window;
  bool show_hint;
};

// reserved_top accounts for the tab bar row (1 when there's more than
// one plugin block being edited, else 0); row_count is the total
// number of editor rows (as returned by BuildPluginOptionsRowLabels()'s
// size, i.e. name row + option rows + the "+ add option" row).
PluginOptionsRowWindow ComputePluginOptionsRowWindow(size_t plugin_rows,
                                                     size_t reserved_top,
                                                     size_t row_count);

constexpr size_t MaxHorizontalOffset(size_t text_width, size_t viewport_width)
{
  return text_width > viewport_width ? text_width - viewport_width : 0;
}

}  // namespace directordaemon::tree_browser_internal

#endif  // BAREOS_DIRD_UA_TREE_BROWSER_INTERNAL_H_
