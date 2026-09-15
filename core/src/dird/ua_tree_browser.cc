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
#include "dird/ua_tree_internal.h"
#include "lib/attribs.h"
#include "lib/bnet.h"
#include "lib/edit.h"
#include "lib/tree.h"
#include "lib/util.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace directordaemon {

namespace {

constexpr size_t kChromeLines = 5;
constexpr size_t kMinVisibleRows = 3;
constexpr size_t kDefaultVisibleRows = 20;
constexpr size_t kMaxSearchMatches = 2000;

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

// One-line "  <size>  <date>" detail suffix for node, fetched from the
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

  std::string detail = "  ";
  detail += SizeAsSiPrefixFormat(static_cast<uint64_t>(statp.st_size));
  detail += "  ";
  detail += time_str;
  return detail;
}

std::string ToLower(std::string_view v)
{
  std::string out(v);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return out;
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

  std::string needle_lower = ToLower(term);

  for (tree_node* node = FirstTreeNode(root); node; node = NextTreeNode(node)) {
    if (node->type == tree_node_type::Root || !node->fname) { continue; }
    if (ToLower(node->fname).find(needle_lower) != std::string::npos) {
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

  size_t MaxVisibleRows() const;
  std::string RenderPanel() const;
  std::string RenderSearchInput() const;
  std::string RenderSearchResults() const;

  // Handles one "key:*" event. Sets *exit_reason and returns true when the
  // browser loop should stop (the user left the browser entirely).
  bool HandleKey(std::string_view key, TreeBrowserExit* exit_reason);
  bool HandleSearchInputKey(std::string_view key);
  void HandleSearchResultsKey(std::string_view key);

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
  bool search_truncated_ = false;
  size_t search_cursor_ = 0;

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
    if (cursor_ >= rows_.size()) { cursor_ = rows_.empty() ? 0 : rows_.size() - 1; }
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
  SetExtract(ua_, node, tree_, !node->extract);
}

void TreeBrowser::MarkAllInDirectory(bool extract)
{
  for (tree_node* node : rows_) { SetExtract(ua_, node, tree_, extract); }
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
  search_truncated_ = result.truncated;
  search_cursor_ = 0;
  entering_search_term_ = false;
  showing_search_results_ = true;
}

size_t TreeBrowser::MaxVisibleRows() const
{
  if (ua_->terminal_height <= 0) { return kDefaultVisibleRows; }
  size_t available = static_cast<size_t>(ua_->terminal_height) > kChromeLines
                         ? static_cast<size_t>(ua_->terminal_height) - kChromeLines
                         : 0;
  return std::max(kMinVisibleRows, available);
}

std::string TreeBrowser::RenderPanel() const
{
  std::string out;
  POOLMEM* cwd = tree_getpath(tree_->node);
  out += "Path: ";
  out += cwd ? cwd : "/";
  out += "\n";
  if (cwd) { FreePoolMemory(cwd); }

  size_t marked = 0;
  for (const tree_node* node : rows_) {
    if (node->extract || node->extract_descendant) { marked++; }
  }
  out += std::to_string(rows_.size());
  out += " entries, ";
  out += std::to_string(marked);
  out += " marked";
  if (detail_view_) { out += "  [detail view on]"; }
  out += "\n";

  if (rows_.empty()) {
    out += "  (empty directory)\n";
  } else {
    size_t max_visible = MaxVisibleRows();
    size_t first = cursor_ > max_visible / 2 ? cursor_ - max_visible / 2 : 0;
    first = std::min(first, rows_.size() - std::min(rows_.size(), max_visible));
    size_t last = std::min(rows_.size(), first + max_visible);

    if (first > 0) { out += "  ...\n"; }
    for (size_t i = first; i < last; ++i) {
      tree_node* node = rows_[i];
      bool highlighted = (i == cursor_);
      out += highlighted ? "> " : "  ";
      if (highlighted) { out += "\033[7m"; }
      out += MarkTag(node);
      out += TreeNodeHasChild(node) ? "d " : "- ";
      out += node->fname ? node->fname : "";
      if (TreeNodeHasChild(node)) { out += "/"; }
      if (detail_view_) { out += NodeDetail(ua_, node); }
      if (highlighted) { out += "\033[0m"; }
      out += "\n";
    }
    if (last < rows_.size()) { out += "  ...\n"; }
  }

  if (!status_line_.empty()) {
    out += status_line_;
    out += "\n";
  }

  out += "[Up/Down] move  [Enter/->] open  [<-] parent  [Space] mark  "
        "[a] mark all  [u] unmark all  [i] detail  [/] search  "
        "[:] command  [c] classic mode  [q] done\n";
  return out;
}

std::string TreeBrowser::RenderSearchInput() const
{
  std::string out = "Search whole tree (fulltext, substring): ";
  out += search_term_;
  out += "\n[Enter] search  [Esc] cancel\n";
  return out;
}

std::string TreeBrowser::RenderSearchResults() const
{
  std::string out = "Search results for \"";
  out += search_term_;
  out += "\": ";
  out += std::to_string(search_matches_.size());
  out += " match(es)";
  if (search_truncated_) {
    out += " (showing first ";
    out += std::to_string(kMaxSearchMatches);
    out += ", refine your search for more)";
  }
  out += "\n";

  if (search_matches_.empty()) {
    out += "  (no matches)\n";
  } else {
    size_t max_visible = MaxVisibleRows();
    size_t first
        = search_cursor_ > max_visible / 2 ? search_cursor_ - max_visible / 2 : 0;
    first = std::min(
        first, search_matches_.size() - std::min(search_matches_.size(), max_visible));
    size_t last = std::min(search_matches_.size(), first + max_visible);

    if (first > 0) { out += "  ...\n"; }
    for (size_t i = first; i < last; ++i) {
      tree_node* node = search_matches_[i];
      bool highlighted = (i == search_cursor_);
      out += highlighted ? "> " : "  ";
      if (highlighted) { out += "\033[7m"; }
      out += MarkTag(node);
      POOLMEM* path = tree_getpath(node);
      out += path ? path : (node->fname ? node->fname : "");
      if (path) { FreePoolMemory(path); }
      if (highlighted) { out += "\033[0m"; }
      out += "\n";
    }
    if (last < search_matches_.size()) { out += "  ...\n"; }
  }

  out += "[Up/Down] move  [Space] mark  [Enter] go to  [Esc] back\n";
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
    if (!search_term_.empty()) { search_term_.pop_back(); }
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
    if (search_cursor_ > 0) { search_cursor_--; }
  } else if (key == "key:down") {
    if (search_cursor_ + 1 < search_matches_.size()) { search_cursor_++; }
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

bool TreeBrowser::HandleKey(std::string_view key, TreeBrowserExit* exit_reason)
{
  status_line_.clear();

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
  } else if (key == "key:text:/") {
    entering_search_term_ = true;
    search_term_.clear();
  } else if (key == "key:text::") {
    ClassicCommandOutcome outcome = RunOneClassicTreeCommand(ua_, tree_);
    SyncAfterClassicCommand();
    if (outcome == ClassicCommandOutcome::kLeaveSelection) {
      *exit_reason = ua_->quit ? TreeBrowserExit::kQuit : TreeBrowserExit::kDone;
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
    std::string screen = showing_search_results_ ? RenderSearchResults()
                        : entering_search_term_   ? RenderSearchInput()
                                                   : RenderPanel();

    user->signal(BNET_START_SELECT);
    ua_->SendMsg("%s", screen.c_str());
    user->signal(BNET_END_SELECT);
    user->signal(BNET_SELECT_INPUT);

    int status = user->recv();
    if (status == BNET_SIGNAL || IsBnetStop(user)) { return TreeBrowserExit::kQuit; }

    std::string_view input(user->msg, user->message_length);
    if (input.starts_with("resize:")) {
      std::string_view size_view = input.substr(strlen("resize:"));
      size_t separator = size_view.find(':');
      int new_height = atoi(std::string(size_view.substr(0, separator)).c_str());
      if (new_height > 0) { ua_->terminal_height = new_height; }
      if (separator != std::string_view::npos) {
        int new_width
            = atoi(std::string(size_view.substr(separator + 1)).c_str());
        if (new_width > 0) { ua_->terminal_width = new_width; }
      }
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
