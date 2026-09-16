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
#include <vector>

namespace directordaemon {

namespace {

constexpr size_t kChromeLines = 8;
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
  if (highlighted) { return "\033[7;36m" + content + "\033[0m"; }

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
  if (!node || !node->extract || node->type == tree_node_type::Root) {
    return false;
  }
  for (const tree_node* parent = node->parent; parent;
       parent = parent->parent) {
    if (parent->extract) { return false; }
  }
  return true;
}

}  // namespace tree_browser_internal

namespace {

using tree_browser_internal::AlignTextColumns;
using tree_browser_internal::CaseFoldForSearch;
using tree_browser_internal::FitText;
using tree_browser_internal::FormatDetailColumns;
using tree_browser_internal::FrameBorderStyle;
using tree_browser_internal::IsTopLevelSelection;
using tree_browser_internal::MaxHorizontalOffset;
using tree_browser_internal::RemoveLastUtf8Character;
using tree_browser_internal::RenderFrameBorder;
using tree_browser_internal::StyleFrameContent;
using tree_browser_internal::TextCellWidth;

std::string MenuBar(size_t width, bool color)
{
  constexpr std::string_view menu
      = " Restore  Mark  View  Search  Command  Help";
  std::string line = FitText(menu, width);
  return color ? "\033[1;36m" + line + "\033[0m\n" : line + "\n";
}

std::string FrameBorder(size_t width,
                        bool color,
                        FrameBorderStyle style,
                        std::string_view title = {})
{
  std::string line = RenderFrameBorder(width, style, title);
  return color ? "\033[2;36m" + line + "\033[0m\n" : line + "\n";
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

  std::string border = color ? "\033[2;36m│\033[0m" : "│";
  return border + content + border + "\n";
}

std::string StatusBar(size_t width, std::string_view text, bool color)
{
  std::string line = FitText(text, width);
  return color ? "\033[7;36m" + line + "\033[0m\n" : line + "\n";
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

// Children of dir, in the exact same (alphabetical) order foreach_child
// already yields them (see NodeCompare()/insert_tree_node() in tree.cc).
std::vector<tree_node*> ChildRows(tree_node* dir)
{
  std::vector<tree_node*> rows;
  tree_node* node;
  foreach_child (node, dir) { rows.push_back(node); }
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

  size_t ScreenWidth() const;
  size_t SearchPathWidth() const;
  size_t SearchHorizontalLimit() const;
  size_t SelectedHorizontalLimit() const;
  size_t MaxVisibleRows(bool detail_header = false) const;
  std::string RenderPanel() const;
  std::string RenderSearchInput() const;
  std::string RenderSearchResults() const;
  std::string RenderSelectedFiles() const;

  // Handles one "key:*" event. Sets *exit_reason and returns true when the
  // browser loop should stop (the user left the browser entirely).
  bool HandleKey(std::string_view key, TreeBrowserExit* exit_reason);
  bool HandleSearchInputKey(std::string_view key);
  void HandleSearchResultsKey(std::string_view key);
  void HandleSelectedFilesKey(std::string_view key);

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

size_t TreeBrowser::MaxVisibleRows(bool detail_header) const
{
  if (ua_->terminal_height <= 0) { return kDefaultVisibleRows; }
  size_t chrome_lines = kChromeLines + (detail_header ? 1 : 0);
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
  std::string out = MenuBar(width, color);
  out += FrameBorder(width, color, FrameBorderStyle::kTop, "Restore selection");

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
  out += FrameBorder(width, color, FrameBorderStyle::kMiddle);

  size_t marked = 0;
  for (const tree_node* node : rows_) {
    if (node->extract || node->extract_descendant) { marked++; }
  }

  size_t max_visible = MaxVisibleRows(detail_view_);
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
  out += FrameBorder(width, color, FrameBorderStyle::kBottom);

  std::string status;
  if (!status_line_.empty()) { status = " " + status_line_ + " | "; }
  status += " Entries: " + std::to_string(rows_.size());
  status += " | Marked: " + std::to_string(marked);
  status += detail_view_ ? " | Detail: on" : " | Detail: off";
  if (!rows_.empty()) {
    status += " | Showing " + std::to_string(first + 1) + "-"
              + std::to_string(last) + " of " + std::to_string(rows_.size());
  }
  out += StatusBar(width, status, color);
  out += HelpLine(width,
                  " Enter Open  Space Mark  a All  u None  i Info  m Selected",
                  color);
  out += HelpLine(
      width, " Arrows Move  Left Parent  / Search  c Classic  q Done", color);
  return out;
}

std::string TreeBrowser::RenderSearchInput() const
{
  size_t width = ScreenWidth();
  bool color = ua_->supports_color;
  std::string out = MenuBar(width, color);
  out += FrameBorder(width, color, FrameBorderStyle::kTop, "Search");
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
  std::string out = MenuBar(width, color);
  out += FrameBorder(width, color, FrameBorderStyle::kTop, "Search results");

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
  std::string out = MenuBar(width, color);
  out += FrameBorder(width, color, FrameBorderStyle::kTop, "Selected files");
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
  status += " | Column: " + std::to_string(selected_horizontal_offset_ + 1);
  if (!selected_nodes_.empty()) {
    status += " | Showing " + std::to_string(first + 1) + "-"
              + std::to_string(last) + " of "
              + std::to_string(selected_nodes_.size());
  }
  out += StatusBar(width, status, color);
  out += HelpLine(width, " Up/Down Move  Left/Right Scroll  Home/End Edges",
                  color);
  out += HelpLine(width, " Space Unmark  Enter Go to file  m/Esc Return",
                  color);
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
      SetExtract(ua_, node, tree_, !node->extract);
    }
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
      RebuildSelectedFiles();
    }
  } else if (key == "key:enter") {
    if (selected_cursor_ < selected_nodes_.size()) {
      JumpToSearchMatch(selected_nodes_[selected_cursor_]);
      showing_selected_files_ = false;
    }
  } else if (key == "key:text:m" || key == "key:cancel") {
    showing_selected_files_ = false;
  }
}

bool TreeBrowser::HandleKey(std::string_view key, TreeBrowserExit* exit_reason)
{
  status_line_.clear();

  if (showing_selected_files_) {
    HandleSelectedFilesKey(key);
    return false;
  }
  if (showing_search_results_) {
    HandleSearchResultsKey(key);
    return false;
  }
  if (entering_search_term_) { return HandleSearchInputKey(key); }

  if (key == "key:up") {
    if (cursor_ > 0) { cursor_--; }
  } else if (key == "key:down") {
    if (cursor_ + 1 < rows_.size()) { cursor_++; }
  } else if (key == "key:right" || key == "key:enter") {
    if (cursor_ < rows_.size()) { EnterDirectory(rows_[cursor_]); }
  } else if (key == "key:left" || key == "key:backspace") {
    GoToParent();
  } else if (key == "key:space") {
    ToggleMarkCurrent();
  } else if (key == "key:text:a") {
    MarkAllInDirectory(true);
  } else if (key == "key:text:u") {
    MarkAllInDirectory(false);
  } else if (key == "key:text:i") {
    detail_view_ = !detail_view_;
  } else if (key == "key:text:m") {
    OpenSelectedFiles();
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
    std::string screen = showing_selected_files_   ? RenderSelectedFiles()
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
