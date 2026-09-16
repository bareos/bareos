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

#include <clocale>

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/tree.h"

namespace {

using directordaemon::tree_browser_internal::AlignTextColumns;
using directordaemon::tree_browser_internal::CaseFoldForSearch;
using directordaemon::tree_browser_internal::FitText;
using directordaemon::tree_browser_internal::FormatDetailColumns;
using directordaemon::tree_browser_internal::FrameBorderStyle;
using directordaemon::tree_browser_internal::IsTopLevelSelection;
using directordaemon::tree_browser_internal::MaxHorizontalOffset;
using directordaemon::tree_browser_internal::RemoveLastUtf8Character;
using directordaemon::tree_browser_internal::RenderFrameBorder;
using directordaemon::tree_browser_internal::StyleFrameContent;
using directordaemon::tree_browser_internal::TextCellWidth;

TEST(TreeBrowserRendering, FitsAndPadsAscii)
{
  EXPECT_EQ(FitText("abcdef", 5), "ab...");
  EXPECT_EQ(FitText("abc", 5), "abc  ");
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
            "\033[7;36m> * filename\033[0m");
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
  EXPECT_FALSE(IsTopLevelSelection(&root));
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

}  // namespace
