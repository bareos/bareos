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

#include "dird/ua_tree_browser_internal.h"

#include <algorithm>
#include <clocale>

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/tree.h"

namespace {

using directordaemon::restore_plugin_hints::PluginOptionsBlock;
using directordaemon::tree_browser_internal::AlignTextColumns;
using directordaemon::tree_browser_internal::BuildKnownOptionsHintLine;
using directordaemon::tree_browser_internal::BuildPluginOptionChoices;
using directordaemon::tree_browser_internal::BuildPluginOptionsRowLabels;
using directordaemon::tree_browser_internal::BuildPluginOptionsTabBar;
using directordaemon::tree_browser_internal::CaseFoldForSearch;
using directordaemon::tree_browser_internal::ComputePluginOptionsRowWindow;
using directordaemon::tree_browser_internal::EstimateStatus;
using directordaemon::tree_browser_internal::FitText;
using directordaemon::tree_browser_internal::FormatDetailColumns;
using directordaemon::tree_browser_internal::FrameBorderStyle;
using directordaemon::tree_browser_internal::IsTopLevelSelection;
using directordaemon::tree_browser_internal::kBrowserFooter;
using directordaemon::tree_browser_internal::kBrowserHelp;
using directordaemon::tree_browser_internal::kDestinationBrowserFooter;
using directordaemon::tree_browser_internal::kDestinationBrowserHelp;
using directordaemon::tree_browser_internal::kDestinationSearchHelpLine1;
using directordaemon::tree_browser_internal::kDestinationSearchHelpLine2;
using directordaemon::tree_browser_internal::kDialogTextInputHelp;
using directordaemon::tree_browser_internal::kFieldEditorFooter;
using directordaemon::tree_browser_internal::kFieldEditorHelp;
using directordaemon::tree_browser_internal::kListDialogFooter;
using directordaemon::tree_browser_internal::kListDialogHelp;
using directordaemon::tree_browser_internal::kPluginBooleanHelp;
using directordaemon::tree_browser_internal::kPluginHintsFooter;
using directordaemon::tree_browser_internal::kPluginHintsHelp;
using directordaemon::tree_browser_internal::kPluginOptionsFooter;
using directordaemon::tree_browser_internal::kPluginOptionsHelp;
using directordaemon::tree_browser_internal::kRelocationDialogFooter;
using directordaemon::tree_browser_internal::kRelocationDialogHelp;
using directordaemon::tree_browser_internal::kRestoreDialogFooter;
using directordaemon::tree_browser_internal::kRestoreDialogHelp;
using directordaemon::tree_browser_internal::kRunDialogFooter;
using directordaemon::tree_browser_internal::kRunDialogHelp;
using directordaemon::tree_browser_internal::kSearchResultsFooter;
using directordaemon::tree_browser_internal::kSearchResultsHelp;
using directordaemon::tree_browser_internal::kSelectedFilesFooter;
using directordaemon::tree_browser_internal::kSelectedFilesHelp;
using directordaemon::tree_browser_internal::kSelectionFooter;
using directordaemon::tree_browser_internal::MaxHorizontalOffset;
using directordaemon::tree_browser_internal::RemoveLastUtf8Character;
using directordaemon::tree_browser_internal::RenderFrameBorder;
using directordaemon::tree_browser_internal::SortDirectoriesFirst;
using directordaemon::tree_browser_internal::StyleFrameContent;
using directordaemon::tree_browser_internal::TextCellWidth;

template <size_t N>
std::string JoinHelp(const std::array<std::string_view, N>& lines)
{
  std::string result;
  for (std::string_view line : lines) { result += line; }
  return result;
}

TEST(TreeBrowserRendering, FitsAndPadsAscii)
{
  EXPECT_EQ(FitText("abcdef", 5), "ab...");
  EXPECT_EQ(FitText("abc", 5), "abc  ");
}

TEST(TreeBrowserRendering, DocumentsRestoreBrowserKeyAliases)
{
  std::string help = JoinHelp(kBrowserHelp);
  for (std::string_view key :
       {"Up",    "Down", "Tab",       "Left",  "Right", "Home", "End",
        "Enter", "..",   "Backspace", "Space", "m",     "a",    "u",
        "e",     "i",    "l",         "/",     ":",     "h",    "?",
        "p",     "o",    "c",         "d",     "r",     "q",    "Esc"}) {
    EXPECT_NE(help.find(key), std::string::npos) << key;
  }
}

TEST(TreeBrowserRendering, DocumentsDialogNavigationAliases)
{
  std::string run_help = JoinHelp(kRunDialogHelp);
  std::string relocation_help = JoinHelp(kRelocationDialogHelp);
  for (const auto* help : {&run_help, &relocation_help}) {
    for (std::string_view key :
         {"Enter", "e/E", "Space", "Tab", "Down", "Up", "Backspace", "Left",
          "Right", "Home", "End", "Esc", "."}) {
      EXPECT_NE(help->find(key), std::string::npos) << key;
    }
  }

  std::string restore_help = JoinHelp(kRestoreDialogHelp);
  for (std::string_view key :
       {"Enter", "e/E", "Space", "Tab", "Down", "Up", "Backspace", "Left",
        "Right", "Home", "End", "b/B", "h/?"}) {
    EXPECT_NE(restore_help.find(key), std::string::npos) << key;
  }

  std::string list_help = JoinHelp(kListDialogHelp);
  for (std::string_view key :
       {"Enter", "Tab", "Down", "Up", "Backspace", "Home", "End", "Esc", "."}) {
    EXPECT_NE(list_help.find(key), std::string::npos) << key;
  }

  std::string field_help = JoinHelp(kFieldEditorHelp);
  for (std::string_view key :
       {"Left", "Right", "Tab", "Backspace", "Home", "End", "Up", "Down", "n/N",
        "Enter", "Esc", "."}) {
    EXPECT_NE(field_help.find(key), std::string::npos) << key;
  }
}

TEST(TreeBrowserRendering, DocumentsDestinationBrowserNavigation)
{
  std::string help = JoinHelp(kDestinationBrowserHelp);
  for (std::string_view key :
       {"Enter", "Tab", "Down", "Up", "Backspace", "Left", "Right", "Home",
        "End", "PgUp", "PgDn", "/", "Esc", "."}) {
    EXPECT_NE(help.find(key), std::string::npos) << key;
  }

  std::string search_help
      = std::string(kDestinationSearchHelpLine1) + kDestinationSearchHelpLine2;
  for (std::string_view key : {"Space", "Backspace", "Up", "Down", "Tab",
                               "PgUp", "PgDn", "Enter", "Esc", "."}) {
    EXPECT_NE(search_help.find(key), std::string::npos) << key;
  }

  for (std::string_view key : {"Type", "Backspace", "Enter", "Esc", "."}) {
    EXPECT_NE(std::string_view(kDialogTextInputHelp).find(key),
              std::string_view::npos)
        << key;
  }
}

TEST(TreeBrowserRendering, KeyHelpLinesFitDefaultTerminalWidth)
{
  for (std::string_view line :
       {kRunDialogFooter, kRestoreDialogFooter, kRelocationDialogFooter,
        kListDialogFooter, kFieldEditorFooter, kBrowserFooter,
        kDestinationBrowserFooter, kSearchResultsFooter, kSelectedFilesFooter,
        kPluginHintsFooter, kPluginOptionsFooter, kSelectionFooter,
        kDestinationSearchHelpLine1, kDestinationSearchHelpLine2,
        kDialogTextInputHelp}) {
    EXPECT_LE(TextCellWidth(line), 80) << line;
  }
  for (const auto* help : {&kRunDialogHelp, &kRelocationDialogHelp}) {
    for (std::string_view line : *help) {
      EXPECT_LE(TextCellWidth(line), 80) << line;
    }
  }
  for (std::string_view line : kRestoreDialogHelp) {
    EXPECT_LE(TextCellWidth(line), 80) << line;
  }
  for (std::string_view line : kListDialogHelp) {
    EXPECT_LE(TextCellWidth(line), 80) << line;
  }
  for (std::string_view line : kFieldEditorHelp) {
    EXPECT_LE(TextCellWidth(line), 80) << line;
  }
  for (std::string_view line : kBrowserHelp) {
    EXPECT_LE(TextCellWidth(line), 80) << line;
  }
  for (std::string_view line : kDestinationBrowserHelp) {
    EXPECT_LE(TextCellWidth(line), 80) << line;
  }
  for (const auto* help :
       {&kSearchResultsHelp, &kSelectedFilesHelp, &kPluginOptionsHelp}) {
    for (std::string_view line : *help) {
      EXPECT_LE(TextCellWidth(line), 80) << line;
    }
  }
  for (std::string_view line : kPluginHintsHelp) {
    EXPECT_LE(TextCellWidth(line), 80) << line;
  }
  for (std::string_view line : kPluginBooleanHelp) {
    EXPECT_LE(TextCellWidth(line), 80) << line;
  }
}

TEST(TreeBrowserRendering, CompactFootersAdvertiseContextualHelp)
{
  for (std::string_view footer :
       {kRunDialogFooter, kRestoreDialogFooter, kRelocationDialogFooter,
        kListDialogFooter, kFieldEditorFooter, kBrowserFooter,
        kDestinationBrowserFooter, kSearchResultsFooter, kSelectedFilesFooter,
        kPluginHintsFooter, kPluginOptionsFooter, kSelectionFooter}) {
    EXPECT_NE(footer.find("h/? Help"), std::string_view::npos) << footer;
  }

  EXPECT_EQ(std::string_view(kDialogTextInputHelp).find("h/?"),
            std::string_view::npos);
}

TEST(TreeBrowserRendering, AlignsDetailsAtRightEdge)
{
  EXPECT_EQ(AlignTextColumns(">  /directory", "4.0 kB  2026-09-15 22:30", 48),
            ">  /directory           4.0 kB  2026-09-15 22:30");
  EXPECT_EQ(AlignTextColumns("   filename", "12 B  2026-09-15 22:30", 48),
            "   filename               12 B  2026-09-15 22:30");
  EXPECT_EQ(TextCellWidth(AlignTextColumns("  /界", "1 kB  date", 20)), 20);
}

TEST(TreeBrowserRendering, FormatsStableSizeAndTimeColumns)
{
  EXPECT_EQ(FormatDetailColumns("12 B", "2026-09-15 22:30:00"),
            "    12 B  2026-09-15 22:30:00");
  EXPECT_EQ(FormatDetailColumns("Size", "Modified"),
            "    Size  Modified           ");
}

TEST(TreeBrowserRendering, RendersMcStyleFrameBorders)
{
  EXPECT_EQ(RenderFrameBorder(24, FrameBorderStyle::kTop, "Restore"),
            "┌─ Restore ────────────┐");
  EXPECT_EQ(RenderFrameBorder(24, FrameBorderStyle::kMiddle),
            "├──────────────────────┤");
  EXPECT_EQ(RenderFrameBorder(24, FrameBorderStyle::kBottom),
            "└──────────────────────┘");
  EXPECT_EQ(
      TextCellWidth(RenderFrameBorder(24, FrameBorderStyle::kTop, "Restore")),
      24);
}

TEST(TreeBrowserRendering, EmphasizesMarkedEntriesWithoutFixedForeground)
{
  EXPECT_EQ(StyleFrameContent("  * filename", '*', false, true),
            "\033[1m  \033[32m*\033[39m filename\033[0m");
  EXPECT_EQ(StyleFrameContent("> * filename", '*', true, true),
            "\033[97;44m> * filename\033[0m");
  EXPECT_EQ(StyleFrameContent("  * filename", '*', false, false),
            "  * filename");
}

TEST(TreeBrowserRendering, CollapsesSelectionsBelowMarkedDirectories)
{
  tree_node root;
  tree_node directory;
  tree_node file;
  directory.type = tree_node_type::Dir;
  directory.parent = &root;
  directory.extract = true;
  file.type = tree_node_type::File;
  file.parent = &directory;
  file.extract = true;

  EXPECT_TRUE(IsTopLevelSelection(&directory));
  EXPECT_FALSE(IsTopLevelSelection(&file));
  directory.extract = false;
  EXPECT_TRUE(IsTopLevelSelection(&file));
  root.extract = true;
  EXPECT_FALSE(IsTopLevelSelection(&root));

  tree_node synthetic_directory;
  synthetic_directory.type = tree_node_type::NewDir;
  synthetic_directory.parent = &root;
  synthetic_directory.extract = true;
  EXPECT_TRUE(IsTopLevelSelection(&synthetic_directory));

  tree_node synthetic_file;
  synthetic_file.type = tree_node_type::File;
  synthetic_file.parent = &synthetic_directory;
  synthetic_file.extract = true;
  EXPECT_FALSE(IsTopLevelSelection(&synthetic_file));
}

TEST(TreeBrowserRendering, SortsDirectoriesBeforeFiles)
{
  tree_node file_a;
  tree_node directory_a;
  tree_node file_b;
  tree_node directory_b;
  file_a.type = tree_node_type::File;
  directory_a.type = tree_node_type::Dir;
  file_b.type = tree_node_type::File;
  directory_b.type = tree_node_type::DirWin;

  std::vector<tree_node*> nodes
      = {&file_a, &directory_a, &file_b, &directory_b};
  SortDirectoriesFirst(&nodes);

  EXPECT_EQ(nodes, (std::vector<tree_node*>{&directory_a, &directory_b, &file_a,
                                            &file_b}));
}

TEST(TreeBrowserRendering, FormatsEstimateStatus)
{
  EXPECT_EQ(EstimateStatus(false, false, 0), "not calculated");
  EXPECT_EQ(EstimateStatus(true, true, 1024), "stale");
  EXPECT_FALSE(EstimateStatus(true, false, 0).empty());
  EXPECT_NE(EstimateStatus(true, false, 1024), "stale");
}

TEST(TreeBrowserRendering, OmitsDetailsWhenThePanelIsTooNarrow)
{
  EXPECT_EQ(AlignTextColumns("long filename", "1 kB  date", 10), "long fi...");
}

TEST(TreeBrowserRendering, PreservesUtf8Boundaries)
{
  EXPECT_EQ(TextCellWidth("Grüße"), 5);
  EXPECT_EQ(FitText("Grüße", 4), "G...");
  EXPECT_EQ(FitText("Grüße", 4, 2, false), "üße ");
}

TEST(TreeBrowserRendering, AccountsForWideAndCombiningCharacters)
{
  EXPECT_EQ(TextCellWidth("a界b"), 4);
  EXPECT_EQ(FitText("a界b", 4), "a界b");
  EXPECT_EQ(FitText("a界b", 3, 1, false), "界b");
  EXPECT_EQ(FitText("a界b", 3, 2, false), "b  ");

  constexpr std::string_view combined = "e\u0301x";
  EXPECT_EQ(TextCellWidth(combined), 2);
  EXPECT_EQ(FitText(combined, 2), combined);
}

TEST(TreeBrowserRendering, SanitizesTerminalControlsAndInvalidUtf8)
{
  EXPECT_EQ(FitText("a\x1b[31m", 6, 0, false), "a?[31m");
  EXPECT_EQ(FitText("a\xc2\x9b"
                    "31m",
                    5, 0, false),
            "a?31m");
  EXPECT_EQ(FitText("a\xff"
                    "b",
                    3, 0, false),
            "a?b");
}

TEST(TreeBrowserRendering, CaseFoldsUtf8SearchText)
{
  ASSERT_NE(setlocale(LC_ALL, ""), nullptr);
  EXPECT_EQ(CaseFoldForSearch("ÜBERGANG"), "übergang");
  EXPECT_EQ(CaseFoldForSearch("GrÜße"), "grüße");
  EXPECT_EQ(CaseFoldForSearch("界"), "界");
}

TEST(TreeBrowserRendering, RemovesOneCompleteUtf8Character)
{
  std::string text = "prüf界";
  RemoveLastUtf8Character(&text);
  EXPECT_EQ(text, "prüf");
  RemoveLastUtf8Character(&text);
  EXPECT_EQ(text, "prü");

  text = "e\u0301";
  RemoveLastUtf8Character(&text);
  EXPECT_EQ(text, "e");
}

TEST(TreeBrowserRendering, ComputesGlobalHorizontalLimitInCells)
{
  EXPECT_EQ(MaxHorizontalOffset(TextCellWidth("a界bc"), 3), 2);
  EXPECT_EQ(MaxHorizontalOffset(TextCellWidth("short"), 10), 0);
}

TEST(TreeBrowserRendering, BuildsAllKnownPluginHintLinesNonEmpty)
{
  std::vector<std::string> lines
      = directordaemon::tree_browser_internal::BuildAllKnownPluginHintLines();
  EXPECT_FALSE(lines.empty());
  // Every known hint contributes at least one non-blank display-name line.
  bool has_non_blank = false;
  for (const auto& line : lines) {
    if (!line.empty()) {
      has_non_blank = true;
      break;
    }
  }
  EXPECT_TRUE(has_non_blank);
}

TEST(TreeBrowserRendering, BuildsDetectedPluginHintLinesForEmptyInput)
{
  std::vector<directordaemon::restore_plugin_hints::FileSetPluginDefinition>
      definitions;
  std::vector<const directordaemon::restore_plugin_hints::PluginRestoreHint*>
      resolved;

  std::vector<std::string> lines
      = directordaemon::tree_browser_internal::BuildDetectedPluginHintLines(
          definitions, resolved);

  ASSERT_EQ(lines.size(), 1u);
  EXPECT_NE(lines[0].find("No restore plugins"), std::string::npos);
}

TEST(TreeBrowserRendering,
     BuildsDetectedPluginHintLinesForKnownAndUnknownPlugins)
{
  directordaemon::restore_plugin_hints::FileSetPluginDefinition known;
  known.raw = "python:module_name=bareos-fd-vmware:...";
  known.plugin_name = "python";
  known.option_keys = {"module_name"};

  directordaemon::restore_plugin_hints::FileSetPluginDefinition unknown;
  unknown.raw = "does-not-exist:foo=bar";
  unknown.plugin_name = "does-not-exist";

  std::vector<directordaemon::restore_plugin_hints::FileSetPluginDefinition>
      definitions = {known, unknown};
  std::vector<const directordaemon::restore_plugin_hints::PluginRestoreHint*>
      resolved
      = {directordaemon::restore_plugin_hints::ResolvePluginRestoreHint(known),
         directordaemon::restore_plugin_hints::ResolvePluginRestoreHint(
             unknown)};

  std::vector<std::string> lines
      = directordaemon::tree_browser_internal::BuildDetectedPluginHintLines(
          definitions, resolved);

  ASSERT_FALSE(resolved[0] == nullptr);
  EXPECT_EQ(resolved[1], nullptr);

  bool saw_known_plugin_line = false;
  bool saw_unknown_hint_note = false;
  for (const auto& line : lines) {
    if (line.find("Plugin: python") != std::string::npos) {
      saw_known_plugin_line = true;
    }
    if (line.find("no known hints for this plugin") != std::string::npos) {
      saw_unknown_hint_note = true;
    }
  }
  EXPECT_TRUE(saw_known_plugin_line);
  EXPECT_TRUE(saw_unknown_hint_note);
}

TEST(TreeBrowserRendering, SummarizesPluginNamesEmptyWhenNoDefinitions)
{
  std::vector<directordaemon::restore_plugin_hints::FileSetPluginDefinition>
      definitions;
  EXPECT_EQ(
      directordaemon::tree_browser_internal::SummarizePluginNames(definitions),
      "");
}

TEST(TreeBrowserRendering, SummarizesPluginNamesDeduplicatedAndJoined)
{
  directordaemon::restore_plugin_hints::FileSetPluginDefinition bpipe1;
  bpipe1.plugin_name = "bpipe";
  directordaemon::restore_plugin_hints::FileSetPluginDefinition bpipe2;
  bpipe2.plugin_name = "bpipe";
  directordaemon::restore_plugin_hints::FileSetPluginDefinition python;
  python.plugin_name = "python";

  std::vector<directordaemon::restore_plugin_hints::FileSetPluginDefinition>
      definitions = {bpipe1, python, bpipe2};

  EXPECT_EQ(
      directordaemon::tree_browser_internal::SummarizePluginNames(definitions),
      "bpipe, python");
}

TEST(TreeBrowserRendering, BuildsEmptyPluginOptionsAdvertisementWithoutPlugins)
{
  EXPECT_EQ(
      directordaemon::tree_browser_internal::BuildPluginOptionsAdvertisement(
          ""),
      "");
}

TEST(TreeBrowserRendering, BuildsPluginOptionsAdvertisementWithPlugins)
{
  std::string advertisement
      = directordaemon::tree_browser_internal::BuildPluginOptionsAdvertisement(
          "bpipe");
  EXPECT_NE(advertisement.find("bpipe"), std::string::npos);
  EXPECT_NE(advertisement.find('p'), std::string::npos);
  EXPECT_NE(advertisement.find('o'), std::string::npos);
}

TEST(TreeBrowserRendering, SplitTreeAndPluginRowsHandlesZero)
{
  EXPECT_EQ(directordaemon::tree_browser_internal::SplitTreeAndPluginRows(0),
            std::make_pair(size_t{0}, size_t{0}));
}

TEST(TreeBrowserRendering, SplitTreeAndPluginRowsUsesQuarterForTree)
{
  EXPECT_EQ(directordaemon::tree_browser_internal::SplitTreeAndPluginRows(10),
            std::make_pair(size_t{3}, size_t{7}));
}

TEST(TreeBrowserRendering, SplitTreeAndPluginRowsUsesRestForOptions)
{
  EXPECT_EQ(directordaemon::tree_browser_internal::SplitTreeAndPluginRows(11),
            std::make_pair(size_t{3}, size_t{8}));
}

TEST(TreeBrowserRendering, SplitTreeAndPluginRowsAddsUpToTheTotal)
{
  for (size_t total : {1, 2, 3, 7, 20, 99}) {
    auto [tree_rows, plugin_rows]
        = directordaemon::tree_browser_internal::SplitTreeAndPluginRows(total);
    EXPECT_EQ(tree_rows + plugin_rows, total) << "total=" << total;
  }
}

TEST(TreeBrowserRendering, RowOffsetForParentIsOneOnlyWithParent)
{
  using directordaemon::tree_browser_internal::RowOffsetForParent;
  EXPECT_EQ(RowOffsetForParent(false), size_t{0});
  EXPECT_EQ(RowOffsetForParent(true), size_t{1});
}

TEST(TreeBrowserRendering, DefaultCursorSkipsSyntheticRowWhenChildrenExist)
{
  using directordaemon::tree_browser_internal::DefaultCursorFor;
  // At the root (row_offset == 0) the default cursor is always on the
  // first child, if any.
  EXPECT_EQ(DefaultCursorFor(/*row_offset=*/0, /*child_row_count=*/3),
            size_t{0});
  // With a parent (row_offset == 1) and existing children, land on the
  // first real child (index 1 in the displayed list), skipping "..".
  EXPECT_EQ(DefaultCursorFor(/*row_offset=*/1, /*child_row_count=*/3),
            size_t{1});
}

TEST(TreeBrowserRendering, DefaultCursorLandsOnParentRowWhenEmpty)
{
  using directordaemon::tree_browser_internal::DefaultCursorFor;
  // An empty directory with a parent: index 0 is the synthetic ".." row
  // (there are no children to offset past), so the cursor lands there.
  EXPECT_EQ(DefaultCursorFor(/*row_offset=*/1, /*child_row_count=*/0),
            size_t{0});
  // An empty root directory: nothing to land on but index 0.
  EXPECT_EQ(DefaultCursorFor(/*row_offset=*/0, /*child_row_count=*/0),
            size_t{0});
}

TEST(TreeBrowserRendering, IsParentRowCursorOnlyTrueAtIndexZeroWithOffset)
{
  using directordaemon::tree_browser_internal::IsParentRowCursor;
  EXPECT_TRUE(IsParentRowCursor(/*cursor=*/0, /*row_offset=*/1));
  EXPECT_FALSE(IsParentRowCursor(/*cursor=*/1, /*row_offset=*/1));
  // No parent row exists at the root, so index 0 is a real child there.
  EXPECT_FALSE(IsParentRowCursor(/*cursor=*/0, /*row_offset=*/0));
}

TEST(TreeBrowserRendering, DisplayedCursorToChildIndexSubtractsOffset)
{
  using directordaemon::tree_browser_internal::DisplayedCursorToChildIndex;
  EXPECT_EQ(DisplayedCursorToChildIndex(/*cursor=*/1, /*row_offset=*/1),
            size_t{0});
  EXPECT_EQ(DisplayedCursorToChildIndex(/*cursor=*/3, /*row_offset=*/1),
            size_t{2});
  EXPECT_EQ(DisplayedCursorToChildIndex(/*cursor=*/0, /*row_offset=*/0),
            size_t{0});
}

TEST(PluginOptionsEditorRendering, RowLabelsShowNamePlaceholderWhenEmpty)
{
  PluginOptionsBlock block;
  auto rows = BuildPluginOptionsRowLabels(block);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0], "Plugin: (type plugin name)");
  EXPECT_EQ(rows[1], "  + add option");
}

TEST(PluginOptionsEditorRendering, RowLabelsShowNameAndKeyValueOptions)
{
  PluginOptionsBlock block;
  block.plugin_name = "bpipe";
  block.options.emplace_back("file", "/tmp/x");
  block.options.emplace_back("verbose", "");

  auto rows = BuildPluginOptionsRowLabels(block);
  ASSERT_EQ(rows.size(), 4u);
  EXPECT_EQ(rows[0], "Plugin: bpipe");
  EXPECT_EQ(rows[1], "  file = /tmp/x");
  EXPECT_EQ(rows[2], "  verbose");
  EXPECT_EQ(rows[3], "  + add option");
}

TEST(PluginOptionsEditorRendering, TabBarEmptyForSingleBlock)
{
  std::vector<PluginOptionsBlock> blocks(1);
  EXPECT_EQ(BuildPluginOptionsTabBar(blocks, 0), "");
}

TEST(PluginOptionsEditorRendering, TabBarMarksActiveBlock)
{
  std::vector<PluginOptionsBlock> blocks(2);
  blocks[0].plugin_name = "bpipe";
  blocks[1].plugin_name = "barri";

  EXPECT_EQ(BuildPluginOptionsTabBar(blocks, 0), ">[1:bpipe]< [2:barri]");
  EXPECT_EQ(BuildPluginOptionsTabBar(blocks, 1), "[1:bpipe] >[2:barri]<");
}

TEST(PluginOptionsEditorRendering, TabBarShowsPlaceholderForUnnamedBlock)
{
  std::vector<PluginOptionsBlock> blocks(2);
  blocks[0].plugin_name = "bpipe";
  EXPECT_EQ(BuildPluginOptionsTabBar(blocks, 1), "[1:bpipe] >[2:(new)]<");
}

TEST(PluginOptionsEditorRendering, KnownOptionsHintListsNamesWithRequiredMark)
{
  PluginOptionsBlock block;
  block.plugin_name = "vmware";
  std::vector<directordaemon::restore_plugin_hints::FileSetPluginDefinition>
      definitions;
  std::string line = BuildKnownOptionsHintLine(block, definitions);
  EXPECT_NE(line.find("Required:"), std::string::npos);
  EXPECT_NE(line.find("vcserver*"), std::string::npos);
}

TEST(PluginOptionsEditorRendering, KnownOptionsHintEmptyForUnknownPlugin)
{
  PluginOptionsBlock block;
  block.plugin_name = "does-not-exist";
  std::vector<directordaemon::restore_plugin_hints::FileSetPluginDefinition>
      definitions;
  EXPECT_EQ(BuildKnownOptionsHintLine(block, definitions), "");
}

TEST(PluginOptionsEditorRendering,
     OptionChoicesAreGroupedAndExcludeOptionsAlreadyAdded)
{
  PluginOptionsBlock block;
  block.plugin_name = "python";
  block.options.emplace_back("module_name", "bareos-fd-vmware");
  block.options.emplace_back("vcuser", "restore-user");

  directordaemon::restore_plugin_hints::FileSetPluginDefinition definition;
  definition.plugin_name = "python";
  definition.raw = "python:module_name=bareos-fd-vmware:vcserver=fileset-host";
  definition.option_keys = {"module_name", "vcserver"};

  auto choices = BuildPluginOptionChoices(block, {definition});
  ASSERT_FALSE(choices.empty());
  EXPECT_EQ(choices.front().key, "vcserver");
  EXPECT_EQ(choices.front().group, "Already in FileSet");
  EXPECT_EQ(
      std::find_if(choices.begin(), choices.end(),
                   [](const auto& choice) { return choice.key == "vcuser"; }),
      choices.end());

  auto required = std::find_if(
      choices.begin(), choices.end(),
      [](const auto& choice) { return choice.group == "Required"; });
  auto optional = std::find_if(
      choices.begin(), choices.end(),
      [](const auto& choice) { return choice.group == "Optional"; });
  ASSERT_NE(required, choices.end());
  ASSERT_NE(optional, choices.end());
  EXPECT_LT(required, optional);
  EXPECT_NE(std::find_if(choices.begin(), choices.end(),
                         [](const auto& choice) {
                           return choice.type
                                  == directordaemon::restore_plugin_hints::
                                      PluginOptionType::kBoolean;
                         }),
            choices.end());
}

TEST(PluginOptionsEditorRendering, BarriOptionChoicesStartWithRestoreTarget)
{
  PluginOptionsBlock block;
  block.plugin_name = "barri";

  auto choices = BuildPluginOptionChoices(block, {});
  ASSERT_FALSE(choices.empty());
  EXPECT_EQ(choices.front().key, "files");
  EXPECT_EQ(choices.front().group, "Required");
  EXPECT_EQ(choices.front().type,
            directordaemon::restore_plugin_hints::PluginOptionType::kPath);
}

TEST(PluginOptionsEditorRendering, IncusOptionChoicesIncludeRestorePath)
{
  PluginOptionsBlock block;
  block.plugin_name = "python";
  block.options.emplace_back("module_name", "bareos-fd-incus");

  auto choices = BuildPluginOptionChoices(block, {});
  auto restore_path = std::find_if(
      choices.begin(), choices.end(),
      [](const auto& choice) { return choice.key == "restore_path"; });
  ASSERT_NE(restore_path, choices.end());
  EXPECT_EQ(restore_path->group, "Optional");
  EXPECT_EQ(restore_path->type,
            directordaemon::restore_plugin_hints::PluginOptionType::kPath);
}

TEST(PluginOptionsEditorRendering, RowWindowShowsAllRowsAndHintWhenRoomy)
{
  auto layout = ComputePluginOptionsRowWindow(/*plugin_rows=*/10,
                                              /*reserved_top=*/0,
                                              /*row_count=*/3);
  EXPECT_EQ(layout.window, 3u);
  EXPECT_TRUE(layout.show_hint);
}

TEST(PluginOptionsEditorRendering, RowWindowOmitsHintWhenExactFit)
{
  auto layout = ComputePluginOptionsRowWindow(/*plugin_rows=*/3,
                                              /*reserved_top=*/0,
                                              /*row_count=*/3);
  EXPECT_EQ(layout.window, 3u);
  EXPECT_FALSE(layout.show_hint);
}

TEST(PluginOptionsEditorRendering, RowWindowScrollsWhenTooSmall)
{
  auto layout = ComputePluginOptionsRowWindow(/*plugin_rows=*/2,
                                              /*reserved_top=*/0,
                                              /*row_count=*/5);
  EXPECT_EQ(layout.window, 2u);
  EXPECT_FALSE(layout.show_hint);
}

TEST(PluginOptionsEditorRendering, RowWindowAccountsForReservedTopRow)
{
  auto layout = ComputePluginOptionsRowWindow(/*plugin_rows=*/4,
                                              /*reserved_top=*/1,
                                              /*row_count=*/3);
  EXPECT_EQ(layout.window, 3u);
  EXPECT_FALSE(layout.show_hint);
}

}  // namespace
