/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
 * Full-screen, Midnight-Commander-style interactive browser for the
 * restore file-selection tree, plus a whole-tree fulltext search. This
 * reuses the exact same raw single-keystroke / full-screen redraw
 * protocol (BNET_START_SELECT / BNET_END_SELECT / BNET_SELECT_INPUT) that
 * ua_select.cc's InteractiveSelection/DoPrompt() already use -- no changes
 * to the client (console.cc) or wire protocol are needed.
 *
 * All mark/unmark actions call directly into SetExtract() (declared in
 * ua_tree_internal.h, implemented in ua_tree.cc) so marking semantics stay
 * byte-identical to the classic "mark"/"unmark" commands. The ':' key
 * escapes for exactly one classic command (RunOneClassicTreeCommand()),
 * and the 'c' key permanently switches back to the classic "$ " prompt --
 * both preserve the current directory and all marks, and the classic
 * prompt can always switch back into the browser with "browse".
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/restore_plugin_hints.h"
#include "dird/ua_tree_browser.h"
#include "dird/ua_tree_browser_internal.h"
#include "dird/ua_tree_internal.h"
#include "lib/attribs.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/tree.h"
#include "lib/util.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cwctype>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace directordaemon {

namespace {

constexpr size_t kChromeLines = 7;
// Extra chrome lines needed when the split-screen Plugin Options pane is
// shown below the file tree: one additional frame border separating the
// two panes, plus the fixed Plugin Options input line.
constexpr size_t kPluginPaneChromeLines = 2;
constexpr size_t kMinVisibleRows = 3;
constexpr size_t kDefaultVisibleRows = 20;
constexpr size_t kDefaultTerminalWidth = 80;
constexpr size_t kMinTerminalWidth = 2;
constexpr size_t kMaxSearchMatches = 2000;
constexpr size_t kHorizontalScrollColumns = 8;
constexpr size_t kDetailSizeWidth = 8;
constexpr size_t kDetailTimeWidth = 19;

struct Utf8Character {
  char32_t codepoint;
  std::string_view bytes;
  bool valid;
};

Utf8Character NextUtf8Character(std::string_view text, size_t* offset)
{
  size_t start = *offset;
  unsigned char first = text[start];
  size_t length = 1;
  char32_t codepoint = first;

  if ((first & 0xe0) == 0xc0) {
    length = 2;
    codepoint = first & 0x1f;
  } else if ((first & 0xf0) == 0xe0) {
    length = 3;
    codepoint = first & 0x0f;
  } else if ((first & 0xf8) == 0xf0) {
    length = 4;
    codepoint = first & 0x07;
  } else if (first >= 0x80) {
    (*offset)++;
    return {0xfffd, text.substr(start, 1), false};
  }

  if (start + length > text.size()) {
    (*offset)++;
    return {0xfffd, text.substr(start, 1), false};
  }

  for (size_t i = 1; i < length; ++i) {
    unsigned char byte = text[start + i];
    if ((byte & 0xc0) != 0x80) {
      (*offset)++;
      return {0xfffd, text.substr(start, 1), false};
    }
    codepoint = (codepoint << 6) | (byte & 0x3f);
  }

  bool overlong = (length == 2 && codepoint < 0x80)
                  || (length == 3 && codepoint < 0x800)
                  || (length == 4 && codepoint < 0x10000);
  if (overlong || (codepoint >= 0xd800 && codepoint <= 0xdfff)
      || codepoint > 0x10ffff) {
    (*offset)++;
    return {0xfffd, text.substr(start, 1), false};
  }

  *offset += length;
  return {codepoint, text.substr(start, length), true};
}

bool IsTerminalControl(char32_t codepoint)
{
  return codepoint < 0x20 || (codepoint >= 0x7f && codepoint <= 0x9f);
}

bool IsCombiningCharacter(char32_t codepoint)
{
  return (codepoint >= 0x0300 && codepoint <= 0x036f)
         || (codepoint >= 0x1ab0 && codepoint <= 0x1aff)
         || (codepoint >= 0x1dc0 && codepoint <= 0x1dff)
         || (codepoint >= 0x20d0 && codepoint <= 0x20ff)
         || (codepoint >= 0xfe00 && codepoint <= 0xfe0f)
         || (codepoint >= 0xfe20 && codepoint <= 0xfe2f)
         || (codepoint >= 0xe0100 && codepoint <= 0xe01ef);
}

bool IsWideCharacter(char32_t codepoint)
{
  return codepoint >= 0x1100
         && (codepoint <= 0x115f || codepoint == 0x2329 || codepoint == 0x232a
             || (codepoint >= 0x2e80 && codepoint <= 0xa4cf
                 && codepoint != 0x303f)
             || (codepoint >= 0xac00 && codepoint <= 0xd7a3)
             || (codepoint >= 0xf900 && codepoint <= 0xfaff)
             || (codepoint >= 0xfe10 && codepoint <= 0xfe19)
             || (codepoint >= 0xfe30 && codepoint <= 0xfe6f)
             || (codepoint >= 0xff00 && codepoint <= 0xff60)
             || (codepoint >= 0xffe0 && codepoint <= 0xffe6)
             || (codepoint >= 0x1f300 && codepoint <= 0x1faff)
             || (codepoint >= 0x20000 && codepoint <= 0x3fffd));
}

size_t CharacterCellWidth(const Utf8Character& character)
{
  if (!character.valid || IsTerminalControl(character.codepoint)) { return 1; }
  if (IsCombiningCharacter(character.codepoint)) { return 0; }
  return IsWideCharacter(character.codepoint) ? 2 : 1;
}

struct RenderedCharacter {
  Utf8Character character;
  size_t width;
};

std::vector<RenderedCharacter> DecodeForDisplay(std::string_view text)
{
  std::vector<RenderedCharacter> characters;
  characters.reserve(text.size());
  for (size_t offset = 0; offset < text.size();) {
    Utf8Character character = NextUtf8Character(text, &offset);
    characters.push_back({character, CharacterCellWidth(character)});
  }
  return characters;
}

void AppendUtf8(std::string* output, char32_t codepoint)
{
  if (codepoint <= 0x7f) {
    output->push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7ff) {
    output->push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
    output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else if (codepoint <= 0xffff) {
    output->push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
    output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else {
    output->push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
    output->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
}

}  // namespace

namespace tree_browser_internal {

size_t TextCellWidth(std::string_view text)
{
  size_t width = 0;
  for (const RenderedCharacter& character : DecodeForDisplay(text)) {
    width += character.width;
  }
  return width;
}

std::string CaseFoldForSearch(std::string_view text)
{
  std::string folded;
  folded.reserve(text.size());
  for (size_t offset = 0; offset < text.size();) {
    Utf8Character character = NextUtf8Character(text, &offset);
    char32_t codepoint = character.valid ? character.codepoint : 0xfffd;
    if ((codepoint >= U'A' && codepoint <= U'Z')
        || (codepoint >= 0x00c0 && codepoint <= 0x00d6)
        || (codepoint >= 0x00d8 && codepoint <= 0x00de)) {
      codepoint += 0x20;
    } else if (codepoint == 0x0178) {
      codepoint = 0x00ff;
    } else {
#if WCHAR_MAX >= 0x10ffff
      codepoint = static_cast<char32_t>(
          std::towlower(static_cast<wint_t>(codepoint)));
#else
      if (codepoint <= WCHAR_MAX) {
        codepoint = static_cast<char32_t>(
            std::towlower(static_cast<wint_t>(codepoint)));
      }
#endif
    }
    AppendUtf8(&folded, codepoint);
  }
  return folded;
}

void RemoveLastUtf8Character(std::string* text)
{
  size_t offset = 0;
  size_t last = 0;
  while (offset < text->size()) {
    last = offset;
    NextUtf8Character(*text, &offset);
  }
  text->resize(last);
}

std::string FitText(std::string_view text,
                    size_t width,
                    size_t horizontal_offset,
                    bool ellipsis)
{
  std::vector<RenderedCharacter> characters = DecodeForDisplay(text);

  size_t first = 0;
  size_t skipped_width = 0;
  while (first < characters.size() && skipped_width < horizontal_offset) {
    skipped_width += characters[first].width;
    first++;
  }
  while (first < characters.size() && characters[first].width == 0) { first++; }

  size_t remaining_width = 0;
  for (size_t i = first; i < characters.size(); ++i) {
    remaining_width += characters[i].width;
  }
  bool truncated = remaining_width > width;
  size_t content_width
      = truncated && ellipsis && width >= 3 ? width - 3 : width;

  std::string fitted;
  fitted.reserve(width);
  size_t rendered_width = 0;
  for (size_t i = first; i < characters.size(); ++i) {
    const RenderedCharacter& character = characters[i];
    if (rendered_width + character.width > content_width) { break; }
    if (!character.character.valid
        || IsTerminalControl(character.character.codepoint)) {
      fitted.push_back('?');
    } else {
      fitted.append(character.character.bytes);
    }
    rendered_width += character.width;
  }

  if (truncated && ellipsis && width >= 3) {
    fitted.append("...");
    rendered_width += 3;
  }
  fitted.append(width - std::min(width, rendered_width), ' ');
  return fitted;
}

std::string AlignTextColumns(std::string_view left,
                             std::string_view right,
                             size_t width)
{
  constexpr size_t kColumnGap = 2;
  size_t right_width = TextCellWidth(right);
  if (right.empty() || right_width + kColumnGap >= width) {
    return FitText(left, width);
  }

  size_t left_width = width - right_width - kColumnGap;
  return FitText(left, left_width) + std::string(kColumnGap, ' ')
         + FitText(right, right_width, 0, false);
}

std::string FormatDetailColumns(std::string_view size,
                                std::string_view modified)
{
  size_t size_width = TextCellWidth(size);
  std::string size_column;
  if (size_width < kDetailSizeWidth) {
    size_column.append(kDetailSizeWidth - size_width, ' ');
    size_column.append(size);
  } else {
    size_column = FitText(size, kDetailSizeWidth);
  }
  return size_column + "  " + FitText(modified, kDetailTimeWidth, 0, false);
}

std::string RenderFrameBorder(size_t width,
                              FrameBorderStyle style,
                              std::string_view title)
{
  std::string_view left;
  std::string_view right;
  switch (style) {
    case FrameBorderStyle::kTop:
      left = "┌";
      right = "┐";
      break;
    case FrameBorderStyle::kMiddle:
      left = "├";
      right = "┤";
      break;
    case FrameBorderStyle::kBottom:
      left = "└";
      right = "┘";
      break;
  }

  if (width == 0) { return {}; }
  if (width == 1) { return std::string(left); }

  size_t inner_width = width - 2;
  std::string content;
  if (!title.empty() && TextCellWidth(title) + 3 <= inner_width) {
    content = "─ ";
    content += title;
    content += " ";
  }
  while (TextCellWidth(content) < inner_width) { content += "─"; }
  return std::string(left) + FitText(content, inner_width, 0, false)
         + std::string(right);
}

std::string StyleFrameContent(std::string content,
                              char mark,
                              bool highlighted,
                              bool color)
{
  if (!color) { return content; }
  if (highlighted) { return "\033[97;44m" + content + "\033[0m"; }

  if (mark == '*' && content.size() > 2) {
    content.insert(3, "\033[39m");
    content.insert(2, "\033[32m");
    return "\033[1m" + content + "\033[0m";
  }
  if (mark == '+' && content.size() > 2) {
    content.insert(3, "\033[0m");
    content.insert(2, "\033[1;33m");
  }
  return content;
}

bool IsTopLevelSelection(const tree_node* node)
{
  if (!node || !node->extract || node->type == tree_node_type::Root
      || node->type == tree_node_type::NewDir) {
    return false;
  }
  for (const tree_node* parent = node->parent; parent;
       parent = parent->parent) {
    if (parent->extract) { return false; }
  }
  return true;
}

void SortDirectoriesFirst(std::vector<tree_node*>* nodes)
{
  std::stable_partition(nodes->begin(), nodes->end(),
                        [](const tree_node* node) {
                          return node->type == tree_node_type::Dir
                                 || node->type == tree_node_type::DirWin;
                        });
}

std::string EstimateStatus(bool calculated, bool stale, uint64_t bytes)
{
  if (!calculated) { return "not calculated"; }
  if (stale) { return "stale"; }
  return SizeAsSiPrefixFormat(bytes);
}

std::vector<std::string> BuildDetectedPluginHintLines(
    const std::vector<
        directordaemon::restore_plugin_hints::FileSetPluginDefinition>&
        definitions,
    const std::vector<
        const directordaemon::restore_plugin_hints::PluginRestoreHint*>&
        resolved_hints)
{
  std::vector<std::string> lines;
  if (definitions.empty()) {
    lines.emplace_back(
        "  No restore plugins found in the FileSet(s) used by this restore.");
    return lines;
  }

  for (size_t i = 0; i < definitions.size(); ++i) {
    const auto& definition = definitions[i];
    const auto* hint = resolved_hints[i];

    lines.push_back("  Plugin: " + definition.plugin_name);
    if (!hint) {
      lines.push_back("    (no known hints for this plugin)");
      lines.emplace_back("");
      continue;
    }

    lines.push_back("    " + std::string(hint->display_name)
                    + (hint->support_level == "bareos"
                           ? ""
                           : " [" + std::string(hint->support_level) + "]"));
    if (!hint->note.empty()) {
      lines.push_back("    " + std::string(hint->note));
    }
    if (!hint->manual_url.empty()) {
      lines.push_back("    Manual: " + std::string(hint->manual_url));
    }
    std::string example
        = directordaemon::restore_plugin_hints::BuildPluginOptionExample(*hint);
    if (!example.empty()) { lines.push_back("    Example: " + example); }

    auto categorized
        = directordaemon::restore_plugin_hints::CategorizePluginOptions(
            *hint, definition.option_keys);

    if (!categorized.already_in_fileset.empty()) {
      lines.push_back("    Already in FileSet:");
      for (const auto* option : categorized.already_in_fileset) {
        lines.push_back("      " + std::string(option->name)
                        + " (from FileSet): "
                        + std::string(option->description));
      }
    }
    if (!categorized.required.empty()) {
      std::string heading = "    Required:";
      if (directordaemon::restore_plugin_hints::HintProvidesDefaultsElsewhere(
              *hint)) {
        heading += " (can also be preset via a config/defaults file option)";
      }
      lines.push_back(heading);
      for (const auto* option : categorized.required) {
        lines.push_back("      " + std::string(option->name) + ": "
                        + std::string(option->description));
      }
    }
    if (!categorized.optional.empty()) {
      lines.push_back("    Optional:");
      for (const auto* option : categorized.optional) {
        lines.push_back("      " + std::string(option->name) + ": "
                        + std::string(option->description));
      }
    }
    lines.emplace_back("");
  }
  return lines;
}

std::vector<std::string> BuildAllKnownPluginHintLines()
{
  std::vector<std::string> lines;
  for (const auto* hint :
       directordaemon::restore_plugin_hints::SortedPluginRestoreHints()) {
    lines.push_back("  " + std::string(hint->display_name)
                    + " (id: " + std::string(hint->id) + ")");
    if (!hint->note.empty()) {
      lines.push_back("    " + std::string(hint->note));
    }
    if (!hint->manual_url.empty()) {
      lines.push_back("    Manual: " + std::string(hint->manual_url));
    }
    for (const auto& option : hint->options) {
      lines.push_back("      " + std::string(option.name) + " ("
                      + std::string(option.status)
                      + "): " + std::string(option.description));
    }
    lines.emplace_back("");
  }
  return lines;
}

std::string SummarizePluginNames(
    const std::vector<
        directordaemon::restore_plugin_hints::FileSetPluginDefinition>&
        definitions)
{
  std::vector<std::string> names;
  for (const auto& definition : definitions) {
    if (definition.plugin_name.empty()) { continue; }
    if (std::find(names.begin(), names.end(), definition.plugin_name)
        == names.end()) {
      names.push_back(definition.plugin_name);
    }
  }

  std::string summary;
  for (size_t i = 0; i < names.size(); ++i) {
    if (i > 0) { summary += ", "; }
    summary += names[i];
  }
  return summary;
}

std::string BuildPluginOptionsAdvertisement(
    std::string_view plugin_names_summary)
{
  if (plugin_names_summary.empty()) { return ""; }
  std::string out = "Plugin backup detected (";
  out += plugin_names_summary;
  out += ") -- Tab Plugin Options pane  p Hints";
  return out;
}

std::pair<size_t, size_t> SplitTreeAndPluginRows(size_t total_rows)
{
  if (total_rows < 2) { return {total_rows, 0}; }
  size_t tree_rows = std::max(size_t{1}, (total_rows + 3) / 4);
  size_t plugin_rows = total_rows - tree_rows;
  return {tree_rows, plugin_rows};
}

std::vector<std::string> BuildPluginOptionsRowLabels(
    const directordaemon::restore_plugin_hints::PluginOptionsBlock& block)
{
  std::vector<std::string> rows;
  rows.push_back(
      "Plugin: "
      + (block.plugin_name.empty() ? "(type plugin name)" : block.plugin_name));
  for (const auto& [key, value] : block.options) {
    rows.push_back(value.empty() ? "  " + key : "  " + key + " = " + value);
  }
  rows.push_back("  + add option");
  return rows;
}

std::string BuildPluginOptionsTabBar(
    const std::vector<directordaemon::restore_plugin_hints::PluginOptionsBlock>&
        blocks,
    size_t active_block)
{
  if (blocks.size() <= 1) { return ""; }
  std::string out;
  for (size_t i = 0; i < blocks.size(); ++i) {
    if (i > 0) { out += " "; }
    std::string name
        = blocks[i].plugin_name.empty() ? "(new)" : blocks[i].plugin_name;
    std::string tab = "[" + std::to_string(i + 1) + ":" + name + "]";
    if (i == active_block) {
      out += ">" + tab + "<";
    } else {
      out += tab;
    }
  }
  return out;
}

std::string BuildKnownOptionsHintLine(
    const directordaemon::restore_plugin_hints::PluginOptionsBlock& block,
    const std::vector<
        directordaemon::restore_plugin_hints::FileSetPluginDefinition>&
        definitions)
{
  const auto* hint
      = directordaemon::restore_plugin_hints::ResolvePluginOptionsBlockHint(
          block);
  if (!hint || hint->options.empty()) { return ""; }

  const auto* matching_definition
      = directordaemon::restore_plugin_hints::FindMatchingPluginDefinition(
          *hint, definitions);
  std::vector<std::string> fileset_option_keys;
  if (matching_definition) {
    fileset_option_keys = matching_definition->option_keys;
  }

  auto categorized
      = directordaemon::restore_plugin_hints::CategorizePluginOptions(
          *hint, fileset_option_keys);

  std::string out = "Known:";
  if (!categorized.already_in_fileset.empty()) {
    out += " In FileSet:";
    for (size_t i = 0; i < categorized.already_in_fileset.size(); ++i) {
      out += (i == 0 ? " " : ", ");
      out += categorized.already_in_fileset[i]->name;
    }
    out += " |";
  }
  if (!categorized.required.empty()) {
    out += " Required:";
    for (size_t i = 0; i < categorized.required.size(); ++i) {
      out += (i == 0 ? " " : ", ");
      out += categorized.required[i]->name;
      out += "*";
    }
    if (directordaemon::restore_plugin_hints::HintProvidesDefaultsElsewhere(
            *hint)) {
      out += " (or via config file)";
    }
    out += " |";
  }
  if (!categorized.optional.empty()) {
    out += " Optional: " + std::to_string(categorized.optional.size())
           + " more (press p)";
  } else if (out.back() == '|') {
    out.pop_back();
  }
  if (!out.empty() && out.back() == ' ') { out.pop_back(); }
  return out;
}

std::vector<PluginOptionChoice> BuildPluginOptionChoices(
    const directordaemon::restore_plugin_hints::PluginOptionsBlock& block,
    const std::vector<
        directordaemon::restore_plugin_hints::FileSetPluginDefinition>&
        definitions)
{
  const auto* hint
      = directordaemon::restore_plugin_hints::ResolvePluginOptionsBlockHint(
          block);
  if (!hint) { return {}; }

  const auto* matching_definition
      = directordaemon::restore_plugin_hints::FindMatchingPluginDefinition(
          *hint, definitions);
  std::vector<std::string> fileset_option_keys;
  if (matching_definition) {
    fileset_option_keys = matching_definition->option_keys;
  }

  auto categorized
      = directordaemon::restore_plugin_hints::CategorizePluginOptions(
          *hint, fileset_option_keys);
  std::vector<PluginOptionChoice> choices;
  auto append =
      [&block, &choices](
          std::string_view group,
          const std::vector<
              const directordaemon::restore_plugin_hints::PluginOptionHint*>&
              options) {
        for (const auto* option : options) {
          bool already_added
              = std::any_of(block.options.begin(), block.options.end(),
                            [option](const auto& item) {
                              return item.first == option->name;
                            });
          if (!already_added) {
            choices.push_back({std::string(option->name), std::string(group),
                               std::string(option->description), option->type});
          }
        }
      };
  append("Already in FileSet", categorized.already_in_fileset);
  append("Required", categorized.required);
  append("Optional", categorized.optional);
  return choices;
}

PluginOptionsRowWindow ComputePluginOptionsRowWindow(size_t plugin_rows,
                                                     size_t reserved_top,
                                                     size_t row_count)
{
  size_t remaining
      = plugin_rows > reserved_top ? plugin_rows - reserved_top : 0;
  bool show_hint = remaining > row_count;
  size_t window = show_hint ? row_count : std::min(remaining, row_count);
  return {window, show_hint};
}

}  // namespace tree_browser_internal

namespace {

using tree_browser_internal::AlignTextColumns;
using tree_browser_internal::BuildAllKnownPluginHintLines;
using tree_browser_internal::BuildDetectedPluginHintLines;
using tree_browser_internal::BuildKnownOptionsHintLine;
using tree_browser_internal::BuildPluginOptionChoices;
using tree_browser_internal::BuildPluginOptionsAdvertisement;
using tree_browser_internal::BuildPluginOptionsRowLabels;
using tree_browser_internal::BuildPluginOptionsTabBar;
using tree_browser_internal::CaseFoldForSearch;
using tree_browser_internal::ComputePluginOptionsRowWindow;
using tree_browser_internal::EstimateStatus;
using tree_browser_internal::FitText;
using tree_browser_internal::FormatDetailColumns;
using tree_browser_internal::FrameBorderStyle;
using tree_browser_internal::IsTopLevelSelection;
using tree_browser_internal::MaxHorizontalOffset;
using tree_browser_internal::PluginOptionsRowWindow;
using tree_browser_internal::RemoveLastUtf8Character;
using tree_browser_internal::RenderFrameBorder;
using tree_browser_internal::SplitTreeAndPluginRows;
using tree_browser_internal::StyleFrameContent;
using tree_browser_internal::SummarizePluginNames;
using tree_browser_internal::TextCellWidth;

std::string FrameBorder(size_t width,
                        bool color,
                        FrameBorderStyle style,
                        std::string_view title = {},
                        bool focused = false)
{
  std::string line = RenderFrameBorder(width, style, title);
  if (!color) { return line + "\n"; }
  return (focused ? "\033[1;34m" : "\033[34m") + line + "\033[0m\n";
}

std::string FrameLine(size_t width,
                      std::string_view text,
                      bool color,
                      bool highlighted = false,
                      char mark = ' ')
{
  if (width < 2) { return FitText(text, width) + "\n"; }

  std::string content = FitText(text, width - 2);
  content = StyleFrameContent(std::move(content), mark, highlighted, color);

  std::string border = color ? "\033[34m│\033[0m" : "│";
  return border + content + border + "\n";
}

std::string PluginOptionsResultLine(size_t width,
                                    std::string_view value,
                                    bool color)
{
  constexpr std::string_view label = " Resulting Plugin Options: ";
  std::string text = std::string(label) + "[" + std::string(value) + "]";
  if (width < 2) { return FitText(text, width) + "\n"; }

  std::string content = FitText(text, width - 2);
  if (color && content.size() > label.size()) {
    content.insert(label.size(), "\033[1m");
    content += "\033[0m";
  }

  std::string border = color ? "\033[34m│\033[0m" : "│";
  return border + content + border + "\n";
}

// Renders one row of the structured Plugin Options editor. When
// `editing` is true (this row is currently being typed into), appends a
// reverse-video "cursor" glyph right after the text, and dims the
// placeholder-style text (e.g. "(type plugin name)") so it reads as a
// hint. When `selected` is true but not `editing`, the whole row is
// shown in reverse video instead (matches the tree pane's row
// highlighting), so "which row will Enter act on" is always visually
// obvious.
std::string PluginOptionsRowLine(size_t width,
                                 std::string_view text,
                                 bool is_placeholder,
                                 bool selected,
                                 bool editing,
                                 bool color)
{
  if (width < 2) { return FitText(text, width) + "\n"; }

  size_t inner_width = width - 2;
  size_t text_width = TextCellWidth(text);
  std::string content = FitText(text, inner_width);

  if (color) {
    if (editing) {
      if (is_placeholder) {
        content = "\033[2m" + content + "\033[0m";
      } else if (text_width < inner_width && text.size() < content.size()) {
        // See PluginOptionsRowLine's caller: text_width is a display-cell
        // count, not a byte offset, so it must not index into content
        // when text may contain multi-byte UTF-8 characters. Since
        // text_width < inner_width means FitText() rendered the whole
        // (untruncated) text byte-for-byte before appending plain ASCII
        // padding, text.size() -- the original string's byte length --
        // is the correct offset of the first padding space.
        content.replace(text.size(), 1, "\033[7m \033[0m");
      }
    } else if (selected) {
      content = "\033[97;44m" + content + "\033[0m";
    } else if (is_placeholder) {
      content = "\033[2m" + content + "\033[0m";
    }
  }

  std::string border = color ? "\033[34m│\033[0m" : "│";
  return border + content + border + "\n";
}

std::string StatusBar(size_t width, std::string_view text, bool color)
{
  std::string line = FitText(text, width);
  return color ? "\033[97;44m" + line + "\033[0m\n" : line + "\n";
}

std::string HelpLine(size_t width, std::string_view text, bool color)
{
  std::string line = FitText(text, width);
  return color ? "\033[2m" + line + "\033[0m\n" : line + "\n";
}

static_assert(MaxHorizontalOffset(20, 8) == 12);
static_assert(MaxHorizontalOffset(5, 8) == 0);

const char* MarkTag(const tree_node* node)
{
  if (node->extract) { return "*"; }
  if (node->extract_descendant) { return "+"; }
  return " ";
}

// Children of dir are alphabetical within each directory/file group.
std::vector<tree_node*> ChildRows(tree_node* dir)
{
  std::vector<tree_node*> rows;
  tree_node* node;
  foreach_child (node, dir) { rows.push_back(node); }
  tree_browser_internal::SortDirectoriesFirst(&rows);
  return rows;
}

// One-line "<size>  <date>" detail text for node, fetched from the
// catalog on demand. Only ever called for rows actually on screen -- never
// for a whole directory or the whole tree -- so it stays responsive.
std::string NodeDetail(UaContext* ua, tree_node* node)
{
  if (!ua->db) { return std::string(); }

  FileDbRecord fdbr;
  struct stat statp;
  memset(&statp, 0, sizeof(statp));

  POOLMEM* cwd = tree_getpath(node);
  POOLMEM* stripped = nullptr;
  char* pcwd = cwd;

  /* Strip the trailing '/' tree_getpath() adds for soft links to
   * directories -- get_file_attr... treats soft links as files, so they
   * don't have a trailing slash (same kludge DoDircmd() already uses). */
  if (node->type == tree_node_type::File && TreeNodeHasChild(node)) {
    stripped = GetPoolMemory(PM_FNAME);
    PmStrcpy(stripped, cwd);
    int len = strlen(stripped);
    if (len > 1) { stripped[len - 1] = '\0'; }
    pcwd = stripped;
  }

  fdbr.FileId = 0;
  fdbr.JobId = node->JobId;
  bool found = ua->db->GetFileAttributesRecord(ua->jcr, pcwd, NULL, &fdbr);
  if (found) {
    int32_t LinkFI;
    DecodeStat(fdbr.LStat, &statp, sizeof(statp), &LinkFI);
  }

  FreePoolMemory(cwd);
  if (stripped) { FreePoolMemory(stripped); }

  if (!found) { return std::string(); }

  time_t mtime
      = statp.st_ctime > statp.st_mtime ? statp.st_ctime : statp.st_mtime;
  char time_str[22];
  encode_time(mtime, time_str);

  return FormatDetailColumns(
      SizeAsSiPrefixFormat(static_cast<uint64_t>(statp.st_size)), time_str);
}

// Whole-tree, case-insensitive substring search against each node's own
// name (node->fname) -- the same field the classic "find" command already
// matches with fnmatch(). Cheap: FirstTreeNode()/NextTreeNode() is the
// exact same in-memory, no-DB-lookup traversal "find"/"count" already do
// on production-sized trees.
struct SearchResult {
  std::vector<tree_node*> matches;
  bool truncated = false;
};

SearchResult SearchWholeTree(TREE_ROOT* root, const std::string& term)
{
  SearchResult result;
  if (term.empty()) { return result; }

  std::string needle_lower = CaseFoldForSearch(term);

  for (tree_node* node = FirstTreeNode(root); node; node = NextTreeNode(node)) {
    if (node->type == tree_node_type::Root || !node->fname) { continue; }
    if (CaseFoldForSearch(node->fname).find(needle_lower)
        != std::string::npos) {
      result.matches.push_back(node);
      if (result.matches.size() >= kMaxSearchMatches) {
        result.truncated = true;
        break;
      }
    }
  }
  return result;
}

}  // namespace

bool TreeBrowserSupported(UaContext* ua)
{
  /* Mirrors the exact same "does this client support raw single-keystroke,
   * full-screen input" proxy DoPrompt() uses for supports_cursor_selection:
   * a client only ever reports a terminal size when it also implements the
   * raw-mode reader (see SendTerminalSize()/ReadSelectionInput() in
   * console.cc, both gated behind "#if !defined(HAVE_WIN32)"). API and
   * batch clients (WebUI passthrough, scripted/.bconsole input, etc.) must
   * always keep getting the unmodified classic prompt. */
  return ua != nullptr && !ua->api && !ua->batch && ua->terminal_height > 0;
}

namespace {

class TreeBrowser {
 public:
  TreeBrowser(UaContext* ua, TreeContext* tree) : ua_(ua), tree_(tree)
  {
    RebuildRows();
    // Gathered eagerly (rather than lazily on first 'p' press) so the
    // main browser view can immediately show the split-screen Plugin
    // Options pane when this restore involves plugin(s).
    GatherPluginHints();
    // If this restore involves a plugin, put keyboard focus on the
    // Plugin Options pane right away (it's rendered below the file tree
    // regardless) so the user is prompted for it instead of having to
    // notice/press Tab first.
    if (HasDetectedPlugins() && tree_->plugin_options_out) {
      FocusPluginOptionsPane();
    }
  }

  TreeBrowserExit Run();

 private:
  void RebuildRows();
  void SyncAfterClassicCommand();
  void EnterDirectory(tree_node* node);
  void GoToParent();
  void ToggleMarkCurrent();
  void MarkAllInDirectory(bool extract);
  void JumpToSearchMatch(tree_node* node);
  void RunSearch();
  void ClampSearchHorizontalOffset();
  void OpenSelectedFiles();
  void RebuildSelectedFiles();
  void ClampSelectedHorizontalOffset();
  void ClampPluginHintsOffset();
  void CalculateEstimate();
  void InvalidateEstimate();
  void FocusPluginOptionsPane();
  void TogglePluginPaneFocus();
  bool CommitPluginOptions();
  restore_plugin_hints::PluginOptionsBlock& ActivePluginBlock()
  {
    return plugin_options_blocks_[plugin_options_active_block_];
  }
  const restore_plugin_hints::PluginOptionsBlock& ActivePluginBlock() const
  {
    return plugin_options_blocks_[plugin_options_active_block_];
  }
  // Number of rows in the active block's editor (name row + option rows
  // + the trailing "+ add option" row).
  size_t PluginOptionsRowCount() const
  {
    return ActivePluginBlock().options.size() + 2;
  }
  std::vector<tree_browser_internal::PluginOptionChoice>
  AvailablePluginOptionChoices() const
  {
    return BuildPluginOptionChoices(ActivePluginBlock(),
                                    plugin_hint_definitions_);
  }
  void ClampPluginOptionsCursor();

  size_t ScreenWidth() const;
  size_t SearchPathWidth() const;
  size_t SearchHorizontalLimit() const;
  size_t SelectedHorizontalLimit() const;
  size_t MaxVisibleRows(bool detail_header = false,
                        bool plugin_split = false) const;
  // Row budget for the tree pane (top) and Plugin Options pane (bottom)
  // of the split-screen layout, valid only while HasDetectedPlugins().
  std::pair<size_t, size_t> SplitPanelRows() const
  {
    return SplitTreeAndPluginRows(MaxVisibleRows(detail_view_, true));
  }
  std::string RenderPanel() const;
  std::string RenderSearchInput() const;
  std::string RenderSearchResults() const;
  std::string RenderSelectedFiles() const;
  std::string RenderHelp() const;
  std::string RenderPluginHints() const;

  // Handles one "key:*" event. Sets *exit_reason and returns true when the
  // browser loop should stop (the user left the browser entirely).
  bool HandleKey(std::string_view key, TreeBrowserExit* exit_reason);
  bool HandleSearchInputKey(std::string_view key);
  void HandleSearchResultsKey(std::string_view key);
  void HandleSelectedFilesKey(std::string_view key);
  void HandleHelpKey(std::string_view key);
  void HandlePluginHintsKey(std::string_view key);
  void HandlePluginOptionsPaneKey(std::string_view key);
  void GatherPluginHints();
  bool HasDetectedPlugins() const { return !plugin_hint_definitions_.empty(); }
  std::vector<std::string> DetectedPluginHintLines() const
  {
    return BuildDetectedPluginHintLines(plugin_hint_definitions_,
                                        plugin_hint_resolved_);
  }

  UaContext* ua_;
  TreeContext* tree_;

  std::vector<tree_node*> rows_;
  tree_node* rows_dir_ = nullptr;
  size_t cursor_ = 0;
  bool detail_view_ = false;

  bool entering_search_term_ = false;
  bool showing_search_results_ = false;
  std::string search_term_;
  std::vector<tree_node*> search_matches_;
  std::vector<std::string> search_paths_;
  size_t search_max_path_length_ = 0;
  bool search_truncated_ = false;
  size_t search_cursor_ = 0;
  size_t search_horizontal_offset_ = 0;

  bool showing_selected_files_ = false;
  std::vector<tree_node*> selected_nodes_;
  std::vector<std::string> selected_paths_;
  size_t selected_max_path_length_ = 0;
  size_t selected_cursor_ = 0;
  size_t selected_horizontal_offset_ = 0;

  bool showing_help_ = false;
  bool showing_plugin_hints_ = false;
  bool plugin_hints_show_all_ = false;
  size_t plugin_hints_offset_ = 0;
  std::vector<restore_plugin_hints::FileSetPluginDefinition>
      plugin_hint_definitions_;
  std::vector<const restore_plugin_hints::PluginRestoreHint*>
      plugin_hint_resolved_;
  bool plugin_hints_gathered_ = false;

  // When a plugin is detected, the Plugin Options pane is always shown
  // (split-screen, below the tree); this only tracks which pane
  // currently has keyboard focus.
  bool plugin_pane_focused_ = false;

  // Structured Plugin Options editor state: one PluginOptionsBlock per
  // detected/added plugin ("tab"), navigated with Left/Right; within a
  // block, row 0 is the plugin name, rows [1, options.size()] are
  // option rows, and the last row is the "+ add option" affordance.
  enum class PluginOptionsEditMode
  {
    kBrowsing,  // Up/Down/Left/Right move the cursor; Enter opens a row.
    kChoosingNewRowKey,
    kChoosingBooleanValue,
    kEditingBlockName,
    kEditingNewRowKey,  // Typing the key for a brand-new option row.
    kEditingRowValue,   // Typing a value, either for a new or existing row.
  };
  PluginOptionsEditMode plugin_options_mode_ = PluginOptionsEditMode::kBrowsing;
  std::vector<restore_plugin_hints::PluginOptionsBlock> plugin_options_blocks_;
  size_t plugin_options_active_block_ = 0;
  size_t plugin_options_cursor_ = 0;
  size_t plugin_options_row_offset_ = 0;
  size_t plugin_options_choice_cursor_ = 0;
  bool plugin_options_boolean_value_ = true;
  std::string plugin_options_edit_buffer_;
  std::string plugin_options_pending_key_;
  restore_plugin_hints::PluginOptionType plugin_options_pending_type_
      = restore_plugin_hints::PluginOptionType::kString;
  // True while kEditingRowValue is for a brand-new row (cursor_ already
  // points past the last existing option, at the "+ add option" row);
  // false when editing an existing row's value in place.
  bool plugin_options_adding_new_row_ = false;

  bool estimate_calculated_ = false;
  bool estimate_stale_ = false;
  uint64_t estimated_bytes_ = 0;
  std::string status_line_;
};

void TreeBrowser::RebuildRows()
{
  rows_ = ChildRows(tree_->node);
  rows_dir_ = tree_->node;
}

void TreeBrowser::SyncAfterClassicCommand()
{
  // The ':' one-shot classic command may have changed directory (e.g.
  // "cd"), added/removed marks, or done nothing -- resync rows_ either
  // way, but only reset the cursor if the directory itself changed.
  if (tree_->node != rows_dir_) {
    RebuildRows();
    cursor_ = 0;
  } else {
    rows_ = ChildRows(tree_->node);
    if (cursor_ >= rows_.size()) {
      cursor_ = rows_.empty() ? 0 : rows_.size() - 1;
    }
  }
  InvalidateEstimate();
}

void TreeBrowser::EnterDirectory(tree_node* node)
{
  if (!TreeNodeHasChild(node)) { return; }
  tree_->node = node;
  RebuildRows();
  cursor_ = 0;
}

void TreeBrowser::GoToParent()
{
  if (!tree_->node->parent) { return; }
  tree_node* previous = tree_->node;
  tree_->node = tree_->node->parent;
  RebuildRows();
  auto it = std::find(rows_.begin(), rows_.end(), previous);
  cursor_ = it != rows_.end() ? static_cast<size_t>(it - rows_.begin()) : 0;
}

void TreeBrowser::ToggleMarkCurrent()
{
  if (cursor_ >= rows_.size()) { return; }
  tree_node* node = rows_[cursor_];
  bool extract = !node->extract;
  int changed = SetExtract(ua_, node, tree_, extract);
  status_line_ = extract ? "Marked " : "Unmarked ";
  status_line_ += std::to_string(changed);
  status_line_ += changed == 1 ? " entry" : " entries";
  if (changed > 0) { InvalidateEstimate(); }
}

void TreeBrowser::MarkAllInDirectory(bool extract)
{
  int changed = 0;
  for (tree_node* node : rows_) {
    changed += SetExtract(ua_, node, tree_, extract);
  }
  status_line_ = extract ? "Marked " : "Unmarked ";
  status_line_ += std::to_string(changed);
  status_line_ += changed == 1 ? " entry" : " entries";
  if (changed > 0) { InvalidateEstimate(); }
}

void TreeBrowser::JumpToSearchMatch(tree_node* node)
{
  tree_->node = node->parent ? node->parent : tree_->root;
  RebuildRows();
  auto it = std::find(rows_.begin(), rows_.end(), node);
  cursor_ = it != rows_.end() ? static_cast<size_t>(it - rows_.begin()) : 0;
  showing_search_results_ = false;
}

void TreeBrowser::RunSearch()
{
  SearchResult result = SearchWholeTree(tree_->root, search_term_);
  search_matches_ = std::move(result.matches);
  search_paths_.clear();
  search_paths_.reserve(search_matches_.size());
  search_max_path_length_ = 0;
  for (tree_node* node : search_matches_) {
    POOLMEM* path = tree_getpath(node);
    search_paths_.emplace_back(path ? path : (node->fname ? node->fname : ""));
    search_max_path_length_ = std::max(search_max_path_length_,
                                       TextCellWidth(search_paths_.back()));
    if (path) { FreePoolMemory(path); }
  }
  search_truncated_ = result.truncated;
  search_cursor_ = 0;
  search_horizontal_offset_ = 0;
  entering_search_term_ = false;
  showing_search_results_ = true;
}

void TreeBrowser::ClampSearchHorizontalOffset()
{
  search_horizontal_offset_
      = std::min(search_horizontal_offset_, SearchHorizontalLimit());
}

void TreeBrowser::OpenSelectedFiles()
{
  RebuildSelectedFiles();
  showing_selected_files_ = true;
}

void TreeBrowser::RebuildSelectedFiles()
{
  tree_node* previous = selected_cursor_ < selected_nodes_.size()
                            ? selected_nodes_[selected_cursor_]
                            : nullptr;
  selected_nodes_.clear();
  selected_paths_.clear();
  selected_max_path_length_ = 0;

  for (tree_node* node = FirstTreeNode(tree_->root); node;
       node = NextTreeNode(node)) {
    if (!IsTopLevelSelection(node)) { continue; }

    POOLMEM* path = tree_getpath(node);
    selected_nodes_.push_back(node);
    selected_paths_.emplace_back(path ? path
                                      : (node->fname ? node->fname : ""));
    selected_max_path_length_ = std::max(selected_max_path_length_,
                                         TextCellWidth(selected_paths_.back()));
    if (path) { FreePoolMemory(path); }
  }

  auto previous_it
      = std::find(selected_nodes_.begin(), selected_nodes_.end(), previous);
  if (previous_it != selected_nodes_.end()) {
    selected_cursor_
        = static_cast<size_t>(previous_it - selected_nodes_.begin());
  } else if (selected_cursor_ >= selected_nodes_.size()) {
    selected_cursor_ = selected_nodes_.empty() ? 0 : selected_nodes_.size() - 1;
  }
  ClampSelectedHorizontalOffset();
}

void TreeBrowser::ClampSelectedHorizontalOffset()
{
  selected_horizontal_offset_
      = std::min(selected_horizontal_offset_, SelectedHorizontalLimit());
}

void TreeBrowser::CalculateEstimate()
{
  estimated_bytes_ = 0;
  if (!ua_->db) {
    status_line_ = "Cannot calculate estimate without a catalog";
    return;
  }

  FileDbRecord fdbr;
  struct stat statp;
  for (tree_node* node = FirstTreeNode(tree_->root); node;
       node = NextTreeNode(node)) {
    if (!node->extract || node->type != tree_node_type::File) { continue; }

    POOLMEM* path = tree_getpath(node);
    fdbr.FileId = 0;
    fdbr.JobId = node->JobId;
    if (ua_->db->GetFileAttributesRecord(ua_->jcr, path, NULL, &fdbr)) {
      int32_t LinkFI;
      DecodeStat(fdbr.LStat, &statp, sizeof(statp), &LinkFI);
      if (S_ISREG(statp.st_mode) && statp.st_size > 0) {
        estimated_bytes_ += static_cast<uint64_t>(statp.st_size);
      }
    }
    FreePoolMemory(path);
  }
  estimate_calculated_ = true;
  estimate_stale_ = false;
  status_line_ = "Estimated selected data";
}

void TreeBrowser::InvalidateEstimate()
{
  if (estimate_calculated_) { estimate_stale_ = true; }
}

void TreeBrowser::FocusPluginOptionsPane()
{
  plugin_pane_focused_ = true;
  plugin_options_mode_ = PluginOptionsEditMode::kBrowsing;
  plugin_options_row_offset_ = 0;

  std::string existing
      = tree_->plugin_options_out ? *tree_->plugin_options_out : "";
  plugin_options_blocks_
      = restore_plugin_hints::ParsePluginOptionsDocument(existing);
  if (plugin_options_blocks_.empty()) {
    restore_plugin_hints::PluginOptionsBlock block;
    if (!plugin_hint_definitions_.empty()) {
      block = restore_plugin_hints::BuildInitialPluginOptionsBlock(
          plugin_hint_definitions_.front());
    }
    plugin_options_blocks_.push_back(std::move(block));
  }
  plugin_options_active_block_ = 0;
  plugin_options_cursor_ = 0;
  plugin_options_choice_cursor_ = 0;
}

void TreeBrowser::TogglePluginPaneFocus()
{
  if (plugin_pane_focused_) {
    // Discard any field that's still mid-edit (not yet committed as a
    // row) rather than half-applying it, then save the committed rows.
    plugin_options_mode_ = PluginOptionsEditMode::kBrowsing;
    // Only leave the pane once the value was actually committed; on ACL
    // rejection, stay focused so the user's edit isn't silently
    // discarded and they can adjust it (status_line_ explains why).
    if (CommitPluginOptions()) { plugin_pane_focused_ = false; }
  } else {
    FocusPluginOptionsPane();
  }
}

bool TreeBrowser::CommitPluginOptions()
{
  if (!tree_->plugin_options_out) {
    status_line_ = "Plugin Options are not supported in this context.";
    return false;
  }
  std::string document = restore_plugin_hints::BuildPluginOptionsDocument(
      plugin_options_blocks_);
  // Authorize every "pluginname:key=value:..." block individually --
  // SendPluginOptions() (dird/fd_cmds.cc) sends one "pluginoptions"
  // protocol command per block, so checking only the combined
  // multi-line document as one string would let an allowed block
  // smuggle an otherwise-denied block past a single-block ACL pattern.
  if (!restore_plugin_hints::AllPluginOptionsBlocksAuthorized(
          document, [this](const std::string& block) {
            return ua_->AclAccessOk(PluginOptions_ACL, block.c_str(), true);
          })) {
    status_line_ = "No authorization for \"PluginOptions\" specification.";
    return false;
  }
  *tree_->plugin_options_out = document;
  status_line_
      = document.empty() ? "Plugin Options cleared." : "Plugin Options saved.";
  return true;
}

void TreeBrowser::ClampPluginOptionsCursor()
{
  if (plugin_options_active_block_ >= plugin_options_blocks_.size()) {
    plugin_options_active_block_ = plugin_options_blocks_.empty()
                                       ? 0
                                       : plugin_options_blocks_.size() - 1;
  }
  size_t row_count = PluginOptionsRowCount();
  size_t max_row = row_count > 0 ? row_count - 1 : 0;
  plugin_options_cursor_ = std::min(plugin_options_cursor_, max_row);

  size_t reserved_top = plugin_options_blocks_.size() > 1 ? 1 : 0;
  reserved_top += 1;  // current plugin-options string preview
  PluginOptionsRowWindow layout = ComputePluginOptionsRowWindow(
      SplitPanelRows().second, reserved_top, row_count);
  size_t max_offset = row_count > layout.window ? row_count - layout.window : 0;
  if (plugin_options_cursor_ < plugin_options_row_offset_) {
    plugin_options_row_offset_ = plugin_options_cursor_;
  }
  if (layout.window > 0
      && plugin_options_cursor_ >= plugin_options_row_offset_ + layout.window) {
    plugin_options_row_offset_ = plugin_options_cursor_ - layout.window + 1;
  }
  plugin_options_row_offset_ = std::min(plugin_options_row_offset_, max_offset);
}

size_t TreeBrowser::MaxVisibleRows(bool detail_header, bool plugin_split) const
{
  if (ua_->terminal_height <= 0) { return kDefaultVisibleRows; }
  size_t chrome_lines = kChromeLines + (detail_header ? 1 : 0)
                        + (plugin_split ? kPluginPaneChromeLines : 0);
  size_t available
      = static_cast<size_t>(ua_->terminal_height) > chrome_lines
            ? static_cast<size_t>(ua_->terminal_height) - chrome_lines
            : 0;
  return std::max(kMinVisibleRows, available);
}

size_t TreeBrowser::ScreenWidth() const
{
  if (ua_->terminal_width <= 0) { return kDefaultTerminalWidth; }
  return std::max(kMinTerminalWidth, static_cast<size_t>(ua_->terminal_width));
}

size_t TreeBrowser::SearchPathWidth() const
{
  // Two frame borders plus the cursor and mark/type prefix.
  constexpr size_t kSearchRowChrome = 5;
  size_t width = ScreenWidth();
  return width > kSearchRowChrome ? width - kSearchRowChrome : 0;
}

size_t TreeBrowser::SearchHorizontalLimit() const
{
  size_t path_width = SearchPathWidth();
  if (path_width == 0 || search_paths_.empty()) { return 0; }
  return MaxHorizontalOffset(search_max_path_length_, path_width);
}

size_t TreeBrowser::SelectedHorizontalLimit() const
{
  size_t path_width = SearchPathWidth();
  if (path_width == 0 || selected_paths_.empty()) { return 0; }
  return MaxHorizontalOffset(selected_max_path_length_, path_width);
}

std::string TreeBrowser::RenderPanel() const
{
  size_t width = ScreenWidth();
  bool color = ua_->supports_color;

  // Once a plugin backup is detected, the screen is split: the file tree
  // keeps the top half and a Plugin Options pane (input line + scrollable
  // hint reference) occupies the bottom half, so the user can set/see
  // plugin options without leaving the file browser.
  bool split = HasDetectedPlugins();
  size_t max_visible = MaxVisibleRows(detail_view_, split);
  size_t plugin_rows = 0;
  if (split) {
    auto [tree_rows, bottom_rows] = SplitTreeAndPluginRows(max_visible);
    max_visible = tree_rows;
    plugin_rows = bottom_rows;
  }

  std::string tree_title = "Restore selection";
  if (split) { tree_title = (plugin_pane_focused_ ? "  " : "> ") + tree_title; }
  bool tree_focused = split && !plugin_pane_focused_;
  std::string out = FrameBorder(width, color, FrameBorderStyle::kTop,
                                tree_title, tree_focused);

  POOLMEM* cwd = tree_getpath(tree_->node);
  std::string path = " Path: ";
  path += cwd ? cwd : "/";
  out += FrameLine(width, path, color);
  if (cwd) { FreePoolMemory(cwd); }
  if (detail_view_) {
    std::string headings = AlignTextColumns(
        "    Name", FormatDetailColumns("Size", "Modified"), width - 2);
    out += FrameLine(width, headings, color);
  }
  out += FrameBorder(width, color, FrameBorderStyle::kMiddle, {}, tree_focused);

  size_t marked = 0;
  for (const tree_node* node : rows_) {
    if (node->extract || node->extract_descendant) { marked++; }
  }

  size_t first = cursor_ > max_visible / 2 ? cursor_ - max_visible / 2 : 0;
  if (!rows_.empty()) {
    first = std::min(first, rows_.size() - std::min(rows_.size(), max_visible));
  }
  size_t last = std::min(rows_.size(), first + max_visible);

  for (size_t row = 0; row < max_visible; ++row) {
    size_t i = first + row;
    if (i < last) {
      tree_node* node = rows_[i];
      bool highlighted = (i == cursor_);
      std::string entry = highlighted ? "> " : "  ";
      entry += MarkTag(node);
      entry += TreeNodeHasChild(node) ? "/" : " ";
      entry += node->fname ? node->fname : "";
      if (detail_view_) {
        entry = AlignTextColumns(entry, NodeDetail(ua_, node), width - 2);
      }
      out += FrameLine(width, entry, color, highlighted, MarkTag(node)[0]);
    } else if (rows_.empty() && row == 0) {
      out += FrameLine(width, "  (empty directory)", color);
    } else {
      out += FrameLine(width, "", color);
    }
  }

  if (!split) {
    out += FrameBorder(width, color, FrameBorderStyle::kBottom);
  } else {
    std::string plugin_title = "Plugin Options";
    plugin_title = (plugin_pane_focused_ ? "> " : "  ") + plugin_title;
    out += FrameBorder(width, color, FrameBorderStyle::kMiddle, plugin_title,
                       plugin_pane_focused_);

    bool choosing_option
        = plugin_options_mode_ == PluginOptionsEditMode::kChoosingNewRowKey;
    bool choosing_boolean
        = plugin_options_mode_ == PluginOptionsEditMode::kChoosingBooleanValue;
    std::vector<std::string> display_rows;
    size_t visual_cursor = plugin_options_cursor_;
    if (choosing_option) {
      display_rows.emplace_back(
          " Choose option (Enter select, type for custom):");
      for (const auto& choice : AvailablePluginOptionChoices()) {
        display_rows.push_back(
            "  [" + choice.group + "] " + choice.key + " ("
            + std::string(
                restore_plugin_hints::PluginOptionTypeName(choice.type))
            + ") - " + choice.description);
      }
      visual_cursor = plugin_options_choice_cursor_ + 1;
    } else if (choosing_boolean) {
      display_rows.push_back(" Choose value for " + plugin_options_pending_key_
                             + ":");
      display_rows.emplace_back("  yes");
      display_rows.emplace_back("  no");
      visual_cursor = plugin_options_boolean_value_ ? 1 : 2;
    } else {
      display_rows = BuildPluginOptionsRowLabels(ActivePluginBlock());
    }
    size_t row_count = display_rows.size();
    bool editing = plugin_pane_focused_
                   && plugin_options_mode_ != PluginOptionsEditMode::kBrowsing
                   && !choosing_option && !choosing_boolean;
    bool editing_is_placeholder = false;
    if (editing) {
      // plugin_options_cursor_ always points at the row currently being
      // edited: unchanged from the browsing selection for
      // kEditingBlockName/kEditingRowValue, and already sitting on the
      // "+ add option" row (the only row Enter can open
      // kEditingNewRowKey from) for kEditingNewRowKey.
      size_t idx = plugin_options_cursor_;
      switch (plugin_options_mode_) {
        case PluginOptionsEditMode::kEditingBlockName:
          display_rows[idx] = "Plugin: "
                              + (plugin_options_edit_buffer_.empty()
                                     ? std::string("(type plugin name)")
                                     : plugin_options_edit_buffer_);
          editing_is_placeholder = plugin_options_edit_buffer_.empty();
          break;
        case PluginOptionsEditMode::kEditingNewRowKey:
          display_rows[idx] = "  "
                              + (plugin_options_edit_buffer_.empty()
                                     ? std::string("(type option key)")
                                     : plugin_options_edit_buffer_);
          editing_is_placeholder = plugin_options_edit_buffer_.empty();
          break;
        case PluginOptionsEditMode::kEditingRowValue: {
          const std::string& key
              = plugin_options_adding_new_row_
                    ? plugin_options_pending_key_
                    : ActivePluginBlock().options[idx - 1].first;
          std::string type;
          if (plugin_options_adding_new_row_
              && plugin_options_pending_type_
                     != restore_plugin_hints::PluginOptionType::kString) {
            type = " ("
                   + std::string(restore_plugin_hints::PluginOptionTypeName(
                       plugin_options_pending_type_))
                   + ")";
          }
          display_rows[idx]
              = "  " + key + type + " = " + plugin_options_edit_buffer_;
          break;
        }
        default:
          break;
      }
    }

    restore_plugin_hints::PluginOptionsBlock preview_block
        = ActivePluginBlock();
    if (editing) {
      switch (plugin_options_mode_) {
        case PluginOptionsEditMode::kEditingBlockName:
          preview_block.plugin_name = plugin_options_edit_buffer_;
          break;
        case PluginOptionsEditMode::kEditingRowValue:
          if (plugin_options_adding_new_row_) {
            preview_block.options.emplace_back(plugin_options_pending_key_,
                                               plugin_options_edit_buffer_);
          } else if (plugin_options_cursor_ > 0
                     && plugin_options_cursor_
                            <= preview_block.options.size()) {
            preview_block.options[plugin_options_cursor_ - 1].second
                = plugin_options_edit_buffer_;
          }
          break;
        default:
          break;
      }
    }
    std::string current_string
        = restore_plugin_hints::BuildPluginOptionsBlock(preview_block);
    if (current_string.empty()) { current_string = "(empty)"; }

    std::string tab_bar = BuildPluginOptionsTabBar(
        plugin_options_blocks_, plugin_options_active_block_);
    size_t reserved_top = (tab_bar.empty() ? 0 : 1) + 1;
    PluginOptionsRowWindow layout
        = ComputePluginOptionsRowWindow(plugin_rows, reserved_top, row_count);
    if (choosing_option || choosing_boolean) { layout.show_hint = false; }
    size_t max_offset
        = row_count > layout.window ? row_count - layout.window : 0;
    size_t offset = std::min(plugin_options_row_offset_, max_offset);
    if (choosing_option || choosing_boolean) {
      offset = visual_cursor > layout.window / 2
                   ? visual_cursor - layout.window / 2
                   : 0;
      offset = std::min(offset, max_offset);
    }

    if (!tab_bar.empty()) { out += FrameLine(width, tab_bar, color); }
    out += PluginOptionsResultLine(width, current_string, color);
    for (size_t row = 0; row < layout.window; ++row) {
      size_t i = offset + row;
      bool selected = plugin_pane_focused_ && i == visual_cursor;
      bool editing_this_row = editing && i == plugin_options_cursor_;
      out += PluginOptionsRowLine(
          width, i < display_rows.size() ? display_rows[i] : "",
          editing_this_row && editing_is_placeholder, selected,
          editing_this_row, color);
    }
    size_t used_rows = reserved_top + layout.window;
    if (layout.show_hint) {
      out += FrameLine(width,
                       "  "
                           + BuildKnownOptionsHintLine(
                               ActivePluginBlock(), plugin_hint_definitions_),
                       color);
      used_rows++;
    }
    for (; used_rows < plugin_rows; ++used_rows) {
      out += FrameLine(width, "", color);
    }
    out += FrameBorder(width, color, FrameBorderStyle::kBottom);
  }

  std::string status;
  if (!status_line_.empty()) { status = " " + status_line_ + " | "; }
  status += " Entries: " + std::to_string(rows_.size());
  status += " | Marked: " + std::to_string(marked);
  status += detail_view_ ? " | Detail: on" : " | Detail: off";
  status += " | Estimate: "
            + EstimateStatus(estimate_calculated_, estimate_stale_,
                             estimated_bytes_);
  if (!rows_.empty()) {
    status += " | Showing " + std::to_string(first + 1) + "-"
              + std::to_string(last) + " of " + std::to_string(rows_.size());
  }
  if (split) {
    status += plugin_pane_focused_ ? " | Focus: Plugin Options"
                                   : " | Focus: Files";
  }
  out += StatusBar(width, status, color);

  std::string first_help_line;
  std::string second_help_line;
  if (split && plugin_pane_focused_) {
    if (plugin_options_mode_ == PluginOptionsEditMode::kChoosingNewRowKey) {
      first_help_line = " Up/Down Option  Enter Select";
      second_help_line = " Type name for custom option  Esc Back";
    } else if (plugin_options_mode_
               == PluginOptionsEditMode::kChoosingBooleanValue) {
      first_help_line = " Up/Down Choose yes/no  Enter Confirm";
      second_help_line = " Esc Back to option list";
    } else if (plugin_options_mode_ == PluginOptionsEditMode::kBrowsing) {
      first_help_line = " Up/Down Row  Left/Right Tab  Enter Edit  d Delete";
      second_help_line
          = " n New tab  Tab Save & switch to Files  Esc Discard edits";
    } else {
      first_help_line = " Type value  Backspace Delete";
      second_help_line = " Enter Confirm row  Esc Cancel this edit";
    }
  } else {
    first_help_line = " Enter Open  Space/m Mark  a All  u None  e Estimate";
    if (split) {
      first_help_line += "  |  "
                         + BuildPluginOptionsAdvertisement(
                             SummarizePluginNames(plugin_hint_definitions_));
    }
    second_help_line = " i Info  l List  / Search  h Help  c Classic  q Done";
  }
  out += HelpLine(width, first_help_line, color);
  out += HelpLine(width, second_help_line, color);
  return out;
}

std::string TreeBrowser::RenderSearchInput() const
{
  size_t width = ScreenWidth();
  bool color = ua_->supports_color;
  std::string out = FrameBorder(width, color, FrameBorderStyle::kTop, "Search");
  out += FrameLine(width, " Fulltext substring search of the restore tree",
                   color);
  out += FrameBorder(width, color, FrameBorderStyle::kMiddle);
  out += FrameLine(width, " Search: " + search_term_, color);
  for (size_t row = 1; row < MaxVisibleRows(); ++row) {
    out += FrameLine(width, "", color);
  }
  out += FrameBorder(width, color, FrameBorderStyle::kBottom);
  out += StatusBar(width, " Enter starts search | Esc returns to files", color);
  out += HelpLine(width, " Type search text  Backspace Delete  Enter Search",
                  color);
  out += HelpLine(width, " Esc Cancel", color);
  return out;
}

std::string TreeBrowser::RenderSearchResults() const
{
  size_t width = ScreenWidth();
  bool color = ua_->supports_color;
  std::string out
      = FrameBorder(width, color, FrameBorderStyle::kTop, "Search results");

  std::string query = " Search: \"" + search_term_ + "\"";
  if (search_truncated_) {
    query += " (first " + std::to_string(kMaxSearchMatches) + " matches)";
  }
  out += FrameLine(width, query, color);
  out += FrameBorder(width, color, FrameBorderStyle::kMiddle);

  size_t max_visible = MaxVisibleRows();
  size_t first
      = search_cursor_ > max_visible / 2 ? search_cursor_ - max_visible / 2 : 0;
  if (!search_matches_.empty()) {
    first
        = std::min(first, search_matches_.size()
                              - std::min(search_matches_.size(), max_visible));
  }
  size_t last = std::min(search_matches_.size(), first + max_visible);

  for (size_t row = 0; row < max_visible; ++row) {
    size_t i = first + row;
    if (i < last) {
      tree_node* node = search_matches_[i];
      bool highlighted = (i == search_cursor_);
      std::string entry = highlighted ? "> " : "  ";
      entry += MarkTag(node);
      entry += FitText(search_paths_[i], SearchPathWidth(),
                       search_horizontal_offset_, false);
      out += FrameLine(width, entry, color, highlighted, MarkTag(node)[0]);
    } else if (search_matches_.empty() && row == 0) {
      out += FrameLine(width, "  (no matches)", color);
    } else {
      out += FrameLine(width, "", color);
    }
  }
  out += FrameBorder(width, color, FrameBorderStyle::kBottom);

  std::string status = " Matches: " + std::to_string(search_matches_.size());
  status += " | Estimate: "
            + EstimateStatus(estimate_calculated_, estimate_stale_,
                             estimated_bytes_);
  status += " | Column: " + std::to_string(search_horizontal_offset_ + 1);
  if (!search_matches_.empty()) {
    status += " | Showing " + std::to_string(first + 1) + "-"
              + std::to_string(last) + " of "
              + std::to_string(search_matches_.size());
  }
  out += StatusBar(width, status, color);
  out += HelpLine(width, " Up/Down Move  Left/Right Scroll  Home/End Edges",
                  color);
  out += HelpLine(width, " Space Mark  Enter Go to file  Esc Return", color);
  return out;
}

std::string TreeBrowser::RenderSelectedFiles() const
{
  size_t width = ScreenWidth();
  bool color = ua_->supports_color;
  std::string out
      = FrameBorder(width, color, FrameBorderStyle::kTop, "Selected files");
  out += FrameLine(
      width,
      " Top-level selections; marked directories include their descendants",
      color);
  out += FrameBorder(width, color, FrameBorderStyle::kMiddle);

  size_t max_visible = MaxVisibleRows();
  size_t first = selected_cursor_ > max_visible / 2
                     ? selected_cursor_ - max_visible / 2
                     : 0;
  if (!selected_nodes_.empty()) {
    first
        = std::min(first, selected_nodes_.size()
                              - std::min(selected_nodes_.size(), max_visible));
  }
  size_t last = std::min(selected_nodes_.size(), first + max_visible);

  for (size_t row = 0; row < max_visible; ++row) {
    size_t i = first + row;
    if (i < last) {
      bool highlighted = i == selected_cursor_;
      std::string entry = highlighted ? "> *" : "  *";
      entry += FitText(selected_paths_[i], SearchPathWidth(),
                       selected_horizontal_offset_, false);
      out += FrameLine(width, entry, color, highlighted, '*');
    } else if (selected_nodes_.empty() && row == 0) {
      out += FrameLine(width, "  (no files selected)", color);
    } else {
      out += FrameLine(width, "", color);
    }
  }
  out += FrameBorder(width, color, FrameBorderStyle::kBottom);

  std::string status
      = " Top-level selections: " + std::to_string(selected_nodes_.size());
  status += " | Estimate: "
            + EstimateStatus(estimate_calculated_, estimate_stale_,
                             estimated_bytes_);
  status += " | Column: " + std::to_string(selected_horizontal_offset_ + 1);
  if (!selected_nodes_.empty()) {
    status += " | Showing " + std::to_string(first + 1) + "-"
              + std::to_string(last) + " of "
              + std::to_string(selected_nodes_.size());
  }
  out += StatusBar(width, status, color);
  out += HelpLine(width, " Up/Down Move  Left/Right Scroll  Home/End Edges",
                  color);
  out += HelpLine(width, " Space Unmark  Enter Go to file  l/Esc Return",
                  color);
  return out;
}

std::string TreeBrowser::RenderHelp() const
{
  size_t width = ScreenWidth();
  bool color = ua_->supports_color;
  std::string out = FrameBorder(width, color, FrameBorderStyle::kTop, "Help");

  constexpr std::string_view lines[] = {
      " Navigation",
      "   Up/Down          Move selection",
      "   Enter/Right      Open directory or jump to selected path",
      "   Left/Backspace   Go to parent directory",
      "",
      " Selection",
      "   Space or m       Mark/unmark current file or directory",
      "   a                Mark all entries in current directory",
      "   u                Unmark all entries in current directory",
      "   l                List top-level selected paths",
      "   e                Calculate selected data size",
      "",
      " Views and commands",
      "   i                Toggle Size and Modified columns",
      "   /                Search the entire restore tree",
      "   :                Run one classic selection command",
      "   c                Switch to classic selection mode",
      "   p                Show restore plugin hints",
      "   o                Focus the Plugin Options pane (if shown)",
      "   Tab              Switch focus: file tree <-> Plugin Options",
      "",
      " Plugin Options pane (once focused)",
      "   Up/Down          Move between plugin name / options / add-row",
      "   Left/Right       Switch plugin (when more than one is edited)",
      "   Enter            Edit the selected row",
      "   d                Delete the selected option (or whole plugin)",
      "   n                Add another plugin's options as a new tab",
      "",
      " Exit",
      "   q                Finish file selection",
      "   h, ?, or Esc     Close this help panel",
  };

  size_t visible_rows = MaxVisibleRows();
  for (size_t row = 0; row < visible_rows; ++row) {
    out += FrameLine(width, row < std::size(lines) ? lines[row] : "", color);
  }
  out += FrameBorder(width, color, FrameBorderStyle::kBottom);
  out += StatusBar(width, " Restore browser help", color);
  out += HelpLine(width, " h/?/Esc Return", color);
  out += HelpLine(width, "", color);
  return out;
}

void TreeBrowser::GatherPluginHints()
{
  plugin_hint_definitions_.clear();
  plugin_hint_resolved_.clear();
  plugin_hints_gathered_ = true;

  if (!ua_->db) {
    status_line_ = "Cannot look up plugin hints without a catalog";
    return;
  }

  // Every distinct JobId contributing to the restore tree may use its own
  // FileSet -- resolve each FileSet's plugin definitions at most once.
  std::vector<JobId_t> seen_job_ids;
  std::vector<DBId_t> seen_fileset_ids;
  for (tree_node* node = FirstTreeNode(tree_->root); node;
       node = NextTreeNode(node)) {
    if (node->JobId == 0
        || std::find(seen_job_ids.begin(), seen_job_ids.end(), node->JobId)
               != seen_job_ids.end()) {
      continue;
    }
    seen_job_ids.push_back(node->JobId);

    JobDbRecord jr;
    jr.JobId = node->JobId;
    if (DbLocker _{ua_->db}; !ua_->db->GetJobRecord(ua_->jcr, &jr)) {
      continue;
    }

    if (std::find(seen_fileset_ids.begin(), seen_fileset_ids.end(),
                  jr.FileSetId)
        != seen_fileset_ids.end()) {
      continue;
    }
    seen_fileset_ids.push_back(jr.FileSetId);

    FileSetDbRecord fsr;
    fsr.FileSetId = jr.FileSetId;
    if (!ua_->db->GetFilesetRecord(ua_->jcr, &fsr)) { continue; }
    if (!ua_->AclAccessOk(FileSet_ACL, fsr.FileSet)) { continue; }

    std::string fileset_text;
    if (!restore_plugin_hints::GetFileSetTextByFileSetId(ua_->db, jr.FileSetId,
                                                         &fileset_text)) {
      continue;
    }

    for (auto& definition :
         restore_plugin_hints::ExtractFileSetPluginDefinitions(fileset_text)) {
      plugin_hint_resolved_.push_back(
          restore_plugin_hints::ResolvePluginRestoreHint(definition));
      plugin_hint_definitions_.push_back(std::move(definition));
    }
  }
}

std::string TreeBrowser::RenderPluginHints() const
{
  size_t width = ScreenWidth();
  bool color = ua_->supports_color;
  std::string out
      = FrameBorder(width, color, FrameBorderStyle::kTop,
                    plugin_hints_show_all_ ? "All known plugin hints"
                                           : "Plugin hints for this restore");

  std::vector<std::string> lines = plugin_hints_show_all_
                                       ? BuildAllKnownPluginHintLines()
                                       : DetectedPluginHintLines();

  size_t visible_rows = MaxVisibleRows();
  for (size_t row = 0; row < visible_rows; ++row) {
    size_t i = plugin_hints_offset_ + row;
    out += FrameLine(width, i < lines.size() ? lines[i] : "", color);
  }
  out += FrameBorder(width, color, FrameBorderStyle::kBottom);
  out += StatusBar(width, " Restore plugin hints", color);
  out += HelpLine(width,
                  " Up/Down Scroll  a Toggle all/detected  o Options"
                  "  p/Esc Return",
                  color);
  out += HelpLine(width, "", color);
  return out;
}

bool TreeBrowser::HandleSearchInputKey(std::string_view key)
{
  if (key == "key:enter") {
    RunSearch();
  } else if (key == "key:cancel") {
    entering_search_term_ = false;
    search_term_.clear();
  } else if (key == "key:backspace") {
    if (!search_term_.empty()) { RemoveLastUtf8Character(&search_term_); }
  } else if (key == "key:space") {
    search_term_.push_back(' ');
  } else if (key.starts_with("key:text:")) {
    search_term_.append(key.substr(strlen("key:text:")));
  }
  return false;
}

void TreeBrowser::HandlePluginOptionsPaneKey(std::string_view key)
{
  if (plugin_options_mode_ == PluginOptionsEditMode::kChoosingNewRowKey) {
    std::vector<tree_browser_internal::PluginOptionChoice> choices
        = AvailablePluginOptionChoices();
    if (key == "key:enter" && !choices.empty()) {
      plugin_options_choice_cursor_
          = std::min(plugin_options_choice_cursor_, choices.size() - 1);
      const auto& choice = choices[plugin_options_choice_cursor_];
      plugin_options_pending_key_ = choice.key;
      plugin_options_pending_type_ = choice.type;
      plugin_options_edit_buffer_.clear();
      plugin_options_adding_new_row_ = true;
      if (choice.type == restore_plugin_hints::PluginOptionType::kBoolean) {
        plugin_options_boolean_value_ = true;
        plugin_options_mode_ = PluginOptionsEditMode::kChoosingBooleanValue;
      } else {
        plugin_options_mode_ = PluginOptionsEditMode::kEditingRowValue;
      }
    } else if (key == "key:cancel") {
      plugin_options_mode_ = PluginOptionsEditMode::kBrowsing;
    } else if (key == "key:up") {
      if (plugin_options_choice_cursor_ > 0) {
        plugin_options_choice_cursor_--;
      }
    } else if (key == "key:down") {
      if (plugin_options_choice_cursor_ + 1 < choices.size()) {
        plugin_options_choice_cursor_++;
      }
    } else if (key == "key:space") {
      plugin_options_mode_ = PluginOptionsEditMode::kEditingNewRowKey;
      plugin_options_edit_buffer_ = " ";
    } else if (key.starts_with("key:text:")) {
      plugin_options_mode_ = PluginOptionsEditMode::kEditingNewRowKey;
      plugin_options_edit_buffer_
          = std::string(key.substr(strlen("key:text:")));
    }
    return;
  }

  if (plugin_options_mode_ == PluginOptionsEditMode::kChoosingBooleanValue) {
    if (key == "key:enter") {
      ActivePluginBlock().options.emplace_back(
          plugin_options_pending_key_,
          plugin_options_boolean_value_ ? "yes" : "no");
      plugin_options_cursor_ = ActivePluginBlock().options.size();
      plugin_options_mode_ = PluginOptionsEditMode::kBrowsing;
    } else if (key == "key:cancel") {
      plugin_options_mode_ = PluginOptionsEditMode::kChoosingNewRowKey;
    } else if (key == "key:up" || key == "key:down" || key == "key:left"
               || key == "key:right") {
      plugin_options_boolean_value_ = !plugin_options_boolean_value_;
    }
    return;
  }

  if (plugin_options_mode_ != PluginOptionsEditMode::kBrowsing) {
    if (key == "key:enter") {
      switch (plugin_options_mode_) {
        case PluginOptionsEditMode::kEditingBlockName:
          ActivePluginBlock().plugin_name = plugin_options_edit_buffer_;
          break;
        case PluginOptionsEditMode::kEditingNewRowKey:
          if (plugin_options_edit_buffer_.empty()) {
            // Nothing typed: quietly abandon adding a new row.
            break;
          }
          plugin_options_pending_key_ = plugin_options_edit_buffer_;
          plugin_options_pending_type_
              = restore_plugin_hints::PluginOptionType::kString;
          plugin_options_edit_buffer_.clear();
          plugin_options_mode_ = PluginOptionsEditMode::kEditingRowValue;
          return;
        case PluginOptionsEditMode::kEditingRowValue:
          if (plugin_options_adding_new_row_) {
            ActivePluginBlock().options.emplace_back(
                plugin_options_pending_key_, plugin_options_edit_buffer_);
            plugin_options_cursor_ = ActivePluginBlock().options.size();
          } else {
            ActivePluginBlock().options[plugin_options_cursor_ - 1].second
                = plugin_options_edit_buffer_;
          }
          break;
        default:
          break;
      }
      plugin_options_mode_ = PluginOptionsEditMode::kBrowsing;
    } else if (key == "key:cancel") {
      plugin_options_mode_ = PluginOptionsEditMode::kBrowsing;
    } else if (key == "key:backspace") {
      if (!plugin_options_edit_buffer_.empty()) {
        RemoveLastUtf8Character(&plugin_options_edit_buffer_);
      }
    } else if (key == "key:space") {
      plugin_options_edit_buffer_.push_back(' ');
    } else if (key.starts_with("key:text:")) {
      plugin_options_edit_buffer_.append(key.substr(strlen("key:text:")));
    }
    return;
  }

  // kBrowsing: navigate rows/blocks and open the row editor.
  if (key == "key:enter") {
    if (plugin_options_cursor_ == 0) {
      plugin_options_mode_ = PluginOptionsEditMode::kEditingBlockName;
      plugin_options_edit_buffer_ = ActivePluginBlock().plugin_name;
    } else if (plugin_options_cursor_ == PluginOptionsRowCount() - 1) {
      std::vector<tree_browser_internal::PluginOptionChoice> choices
          = AvailablePluginOptionChoices();
      plugin_options_mode_ = choices.empty()
                                 ? PluginOptionsEditMode::kEditingNewRowKey
                                 : PluginOptionsEditMode::kChoosingNewRowKey;
      plugin_options_choice_cursor_ = 0;
      plugin_options_edit_buffer_.clear();
      plugin_options_pending_key_.clear();
      plugin_options_adding_new_row_ = true;
    } else {
      plugin_options_mode_ = PluginOptionsEditMode::kEditingRowValue;
      plugin_options_edit_buffer_
          = ActivePluginBlock().options[plugin_options_cursor_ - 1].second;
      plugin_options_adding_new_row_ = false;
    }
  } else if (key == "key:cancel") {
    plugin_pane_focused_ = false;
    plugin_options_blocks_ = restore_plugin_hints::ParsePluginOptionsDocument(
        tree_->plugin_options_out ? *tree_->plugin_options_out : "");
    if (plugin_options_blocks_.empty()) {
      // Keep the pane rendering a single (empty) block even when the
      // saved document is empty -- ActivePluginBlock() always assumes
      // at least one block exists.
      plugin_options_blocks_.emplace_back();
    }
    plugin_options_active_block_ = 0;
    plugin_options_cursor_ = 0;
  } else if (key == "key:up") {
    if (plugin_options_cursor_ > 0) { plugin_options_cursor_--; }
  } else if (key == "key:down") {
    if (plugin_options_cursor_ + 1 < PluginOptionsRowCount()) {
      plugin_options_cursor_++;
    }
  } else if (key == "key:left") {
    if (plugin_options_blocks_.size() > 1) {
      plugin_options_active_block_
          = (plugin_options_active_block_ + plugin_options_blocks_.size() - 1)
            % plugin_options_blocks_.size();
      plugin_options_cursor_ = 0;
    }
  } else if (key == "key:right") {
    if (plugin_options_blocks_.size() > 1) {
      plugin_options_active_block_
          = (plugin_options_active_block_ + 1) % plugin_options_blocks_.size();
      plugin_options_cursor_ = 0;
    }
  } else if (key == "key:text:n") {
    plugin_options_blocks_.emplace_back();
    plugin_options_active_block_ = plugin_options_blocks_.size() - 1;
    plugin_options_cursor_ = 0;
  } else if (key == "key:text:d") {
    auto& options = ActivePluginBlock().options;
    if (plugin_options_cursor_ >= 1
        && plugin_options_cursor_ <= options.size()) {
      options.erase(options.begin()
                    + static_cast<ptrdiff_t>(plugin_options_cursor_ - 1));
    } else if (plugin_options_cursor_ == 0
               && plugin_options_blocks_.size() > 1) {
      plugin_options_blocks_.erase(
          plugin_options_blocks_.begin()
          + static_cast<ptrdiff_t>(plugin_options_active_block_));
      if (plugin_options_active_block_ >= plugin_options_blocks_.size()) {
        plugin_options_active_block_ = plugin_options_blocks_.size() - 1;
      }
    }
    plugin_options_cursor_ = 0;
  }
  ClampPluginOptionsCursor();
}

void TreeBrowser::HandleSearchResultsKey(std::string_view key)
{
  if (key == "key:up") {
    if (search_cursor_ > 0) {
      search_cursor_--;
      ClampSearchHorizontalOffset();
    }
  } else if (key == "key:down") {
    if (search_cursor_ + 1 < search_matches_.size()) {
      search_cursor_++;
      ClampSearchHorizontalOffset();
    }
  } else if (key == "key:left") {
    search_horizontal_offset_
        = search_horizontal_offset_ > kHorizontalScrollColumns
              ? search_horizontal_offset_ - kHorizontalScrollColumns
              : 0;
  } else if (key == "key:right") {
    search_horizontal_offset_
        = std::min(SearchHorizontalLimit(),
                   search_horizontal_offset_ + kHorizontalScrollColumns);
  } else if (key == "key:home") {
    search_horizontal_offset_ = 0;
  } else if (key == "key:end") {
    search_horizontal_offset_ = SearchHorizontalLimit();
  } else if (key == "key:space") {
    if (search_cursor_ < search_matches_.size()) {
      tree_node* node = search_matches_[search_cursor_];
      if (SetExtract(ua_, node, tree_, !node->extract) > 0) {
        InvalidateEstimate();
      }
    }
  } else if (key == "key:text:e") {
    CalculateEstimate();
  } else if (key == "key:enter") {
    if (search_cursor_ < search_matches_.size()) {
      JumpToSearchMatch(search_matches_[search_cursor_]);
    }
  } else if (key == "key:cancel") {
    showing_search_results_ = false;
  }
}

void TreeBrowser::HandleSelectedFilesKey(std::string_view key)
{
  if (key == "key:up") {
    if (selected_cursor_ > 0) { selected_cursor_--; }
  } else if (key == "key:down") {
    if (selected_cursor_ + 1 < selected_nodes_.size()) { selected_cursor_++; }
  } else if (key == "key:left") {
    selected_horizontal_offset_
        = selected_horizontal_offset_ > kHorizontalScrollColumns
              ? selected_horizontal_offset_ - kHorizontalScrollColumns
              : 0;
  } else if (key == "key:right") {
    selected_horizontal_offset_
        = std::min(SelectedHorizontalLimit(),
                   selected_horizontal_offset_ + kHorizontalScrollColumns);
  } else if (key == "key:home") {
    selected_horizontal_offset_ = 0;
  } else if (key == "key:end") {
    selected_horizontal_offset_ = SelectedHorizontalLimit();
  } else if (key == "key:space") {
    if (selected_cursor_ < selected_nodes_.size()) {
      SetExtract(ua_, selected_nodes_[selected_cursor_], tree_, false);
      InvalidateEstimate();
      RebuildSelectedFiles();
    }
  } else if (key == "key:text:e") {
    CalculateEstimate();
  } else if (key == "key:enter") {
    if (selected_cursor_ < selected_nodes_.size()) {
      JumpToSearchMatch(selected_nodes_[selected_cursor_]);
      showing_selected_files_ = false;
    }
  } else if (key == "key:text:l" || key == "key:cancel") {
    showing_selected_files_ = false;
  }
}

void TreeBrowser::HandleHelpKey(std::string_view key)
{
  if (key == "key:text:h" || key == "key:text:?" || key == "key:cancel") {
    showing_help_ = false;
  }
}

void TreeBrowser::HandlePluginHintsKey(std::string_view key)
{
  std::vector<std::string> lines = plugin_hints_show_all_
                                       ? BuildAllKnownPluginHintLines()
                                       : DetectedPluginHintLines();
  size_t visible_rows = MaxVisibleRows();
  size_t max_offset
      = lines.size() > visible_rows ? lines.size() - visible_rows : 0;

  if (key == "key:up") {
    if (plugin_hints_offset_ > 0) { plugin_hints_offset_--; }
  } else if (key == "key:down") {
    if (plugin_hints_offset_ < max_offset) { plugin_hints_offset_++; }
  } else if (key == "key:text:a") {
    plugin_hints_show_all_ = !plugin_hints_show_all_;
    plugin_hints_offset_ = 0;
  } else if (key == "key:text:o" || key == "key:tab") {
    showing_plugin_hints_ = false;
    FocusPluginOptionsPane();
  } else if (key == "key:text:p" || key == "key:cancel") {
    showing_plugin_hints_ = false;
  }
}

void TreeBrowser::ClampPluginHintsOffset()
{
  // A terminal resize can shrink the visible row count (or the list of
  // rendered lines itself changes when toggling all/detected), so the
  // current scroll offset may end up past the end of the content --
  // clamp it back into range instead of leaving the panel blank.
  std::vector<std::string> lines = plugin_hints_show_all_
                                       ? BuildAllKnownPluginHintLines()
                                       : DetectedPluginHintLines();
  size_t visible_rows = MaxVisibleRows();
  size_t max_offset
      = lines.size() > visible_rows ? lines.size() - visible_rows : 0;
  plugin_hints_offset_ = std::min(plugin_hints_offset_, max_offset);
}

bool TreeBrowser::HandleKey(std::string_view key, TreeBrowserExit* exit_reason)
{
  status_line_.clear();

  if (showing_help_) {
    HandleHelpKey(key);
    return false;
  }
  if (showing_plugin_hints_) {
    HandlePluginHintsKey(key);
    return false;
  }
  if (showing_selected_files_) {
    HandleSelectedFilesKey(key);
    return false;
  }
  if (showing_search_results_) {
    HandleSearchResultsKey(key);
    return false;
  }
  if (entering_search_term_) { return HandleSearchInputKey(key); }

  // Tab always toggles which half of the split screen has keyboard focus
  // (only meaningful once a plugin backup was detected, since that's the
  // only time the Plugin Options pane is shown at all).
  if (key == "key:tab" && HasDetectedPlugins()) {
    TogglePluginPaneFocus();
    return false;
  }
  if (plugin_pane_focused_) {
    HandlePluginOptionsPaneKey(key);
    return false;
  }

  if (key == "key:up") {
    if (cursor_ > 0) { cursor_--; }
  } else if (key == "key:down") {
    if (cursor_ + 1 < rows_.size()) { cursor_++; }
  } else if (key == "key:right" || key == "key:enter") {
    if (cursor_ < rows_.size()) { EnterDirectory(rows_[cursor_]); }
  } else if (key == "key:left" || key == "key:backspace") {
    GoToParent();
  } else if (key == "key:space" || key == "key:text:m") {
    ToggleMarkCurrent();
  } else if (key == "key:text:a") {
    MarkAllInDirectory(true);
  } else if (key == "key:text:u") {
    MarkAllInDirectory(false);
  } else if (key == "key:text:i") {
    detail_view_ = !detail_view_;
  } else if (key == "key:text:e") {
    CalculateEstimate();
  } else if (key == "key:text:l") {
    OpenSelectedFiles();
  } else if (key == "key:text:h" || key == "key:text:?") {
    showing_help_ = true;
  } else if (key == "key:text:p") {
    if (!plugin_hints_gathered_) { GatherPluginHints(); }
    plugin_hints_offset_ = 0;
    showing_plugin_hints_ = true;
  } else if (key == "key:text:o") {
    if (HasDetectedPlugins()) {
      FocusPluginOptionsPane();
    } else {
      status_line_ = "No plugin detected for this restore.";
    }
  } else if (key == "key:text:/") {
    entering_search_term_ = true;
    search_term_.clear();
  } else if (key == "key:text::") {
    ClassicCommandOutcome outcome = RunOneClassicTreeCommand(ua_, tree_);
    SyncAfterClassicCommand();
    if (outcome == ClassicCommandOutcome::kLeaveSelection) {
      *exit_reason
          = ua_->quit ? TreeBrowserExit::kQuit : TreeBrowserExit::kDone;
      return true;
    }
    // kSwitchToBrowser (redundant, already browsing) and kContinue both
    // just fall through to redrawing the browser.
  } else if (key == "key:text:c") {
    *exit_reason = TreeBrowserExit::kSwitchToClassic;
    return true;
  } else if (key == "key:text:q" || key == "key:cancel") {
    *exit_reason = TreeBrowserExit::kDone;
    return true;
  }
  // Any other key (e.g. "key:noop", stray text) is silently ignored: this
  // is a single-purpose panel, not a filterable list, so unknown keys just
  // trigger a harmless redraw.
  return false;
}

TreeBrowserExit TreeBrowser::Run()
{
  BareosSocket* user = ua_->UA_sock;
  TreeBrowserExit exit_reason = TreeBrowserExit::kDone;

  for (;;) {
    std::string screen = showing_help_             ? RenderHelp()
                         : showing_plugin_hints_   ? RenderPluginHints()
                         : showing_selected_files_ ? RenderSelectedFiles()
                         : showing_search_results_ ? RenderSearchResults()
                         : entering_search_term_   ? RenderSearchInput()
                                                   : RenderPanel();

    user->signal(BNET_START_SELECT);
    ua_->SendMsg("%s", screen.c_str());
    user->signal(BNET_END_SELECT);
    user->signal(BNET_SELECT_INPUT);

    int status = user->recv();
    if (status == BNET_SIGNAL || IsBnetStop(user)) {
      return TreeBrowserExit::kQuit;
    }

    std::string_view input(user->msg, user->message_length);
    if (input.starts_with("resize:")) {
      std::string_view size_view = input.substr(strlen("resize:"));
      size_t separator = size_view.find(':');
      int new_height
          = atoi(std::string(size_view.substr(0, separator)).c_str());
      if (new_height > 0) { ua_->terminal_height = new_height; }
      if (separator != std::string_view::npos) {
        int new_width
            = atoi(std::string(size_view.substr(separator + 1)).c_str());
        if (new_width > 0) { ua_->terminal_width = new_width; }
      }
      ClampSearchHorizontalOffset();
      ClampPluginHintsOffset();
      continue;
    }

    if (HandleKey(input, &exit_reason)) { return exit_reason; }
  }
}

}  // namespace

TreeBrowserExit RunTreeBrowser(UaContext* ua, TreeContext* tree)
{
  TreeBrowser browser(ua, tree);
  return browser.Run();
}

} /* namespace directordaemon */
