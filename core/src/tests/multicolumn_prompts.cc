/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#include "gtest/gtest.h"
#include "include/bareos.h"

#include "dird/ua_select.h"
#include "include/jcr.h"

using namespace directordaemon;

class PromptsFormatting : public ::testing::Test {
 protected:
  void SetUp() override { ua = new UaContext(&jcr); }

  void TearDown() override { delete ua; }

  void PopulateUaWithPrompts(UaContext* t_ua, const char** list)
  {
    StartPrompt(t_ua, "start");
    for (int i = 0; list[i]; ++i) { AddPrompt(t_ua, list[i]); }
  }

  int window_width{80};
  int lines_threshold{20};
  JobControlRecord jcr{};
  UaContext* ua{};
};

TEST_F(PromptsFormatting, ReturnsNothingOnAnEmptyList)
{
  const char* list[] = {nullptr};
  PopulateUaWithPrompts(ua, list);

  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      "");
  /* clang-format on */
}

TEST_F(PromptsFormatting, ReturnsSingleElementWhenOnlyOnePromptIsAvailable)
{
  const char* list[] = {T_("bareos1"), nullptr};
  PopulateUaWithPrompts(ua, list);

  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      "1: bareos1\n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting10Elements_StandardWidthNoThreshold)
{
  const char* list[]
      = {T_("bareos1"), T_("bareos2"),  T_("bareos3"), T_("bareos4"),
         T_("bareos5"), T_("bareos6"),  T_("bareos7"), T_("bareos8"),
         T_("bareos9"), T_("bareos10"), nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   3: bareos3   5: bareos5   7: bareos7   9: bareos9  \n"
      " 2: bareos2   4: bareos4   6: bareos6   8: bareos8  10: bareos10 \n"
     );
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting15Elements_StandardWidthNoThreshold)
{
  const char* list[]
      = {T_("bareos1"),  T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"),  T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"),  T_("bareos10"), T_("bareos11"), T_("bareos12"),
         T_("bareos13"), T_("bareos14"), T_("bareos15"),

         nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);
  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   4: bareos4   7: bareos7  10: bareos10 13: bareos13 \n"
      " 2: bareos2   5: bareos5   8: bareos8  11: bareos11 14: bareos14 \n"
      " 3: bareos3   6: bareos6   9: bareos9  12: bareos12 15: bareos15 \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting16Elements_StandardWidthNoThreshold)
{
  const char* list[]
      = {T_("bareos1"),  T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"),  T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"),  T_("bareos10"), T_("bareos11"), T_("bareos12"),
         T_("bareos13"), T_("bareos14"), T_("bareos15"), T_("bareos16"),
         nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   4: bareos4   7: bareos7  10: bareos10 13: bareos13 16: bareos16 \n"
      " 2: bareos2   5: bareos5   8: bareos8  11: bareos11 14: bareos14 \n"
      " 3: bareos3   6: bareos6   9: bareos9  12: bareos12 15: bareos15 \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting21Elements_StandardWidthNoThreshold)
{
  const char* list[]
      = {T_("bareos1"),  T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"),  T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"),  T_("bareos10"), T_("bareos11"), T_("bareos12"),
         T_("bareos13"), T_("bareos14"), T_("bareos15"), T_("bareos16"),
         T_("bareos17"), T_("bareos18"), T_("bareos19"), T_("bareos20"),
         T_("bareos21"), nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: bareos1   5: bareos5   9: bareos9  13: bareos13 17: bareos17 21: bareos21 \n"
      " 2: bareos2   6: bareos6  10: bareos10 14: bareos14 18: bareos18 \n"
      " 3: bareos3   7: bareos7  11: bareos11 15: bareos15 19: bareos19 \n"
      " 4: bareos4   8: bareos8  12: bareos12 16: bareos16 20: bareos20 \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting,
       NoMulticolumnformattingWhenNumberOfElementsLessThanThreshold)
{
  const char* list[] = {T_("List last 20 Jobs run"), T_("Cancel"), nullptr};

  PopulateUaWithPrompts(ua, list);

  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      "1: List last 20 Jobs run\n"
      "2: Cancel\n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, FormatsForVeryLargeWidth)
{
  const char* list[] = {
      T_("List last 20 Jobs run"),
      T_("List Jobs where a given File is saved"),
      T_("Enter list of comma separated JobIds to select"),
      T_("Enter SQL list command"),
      T_("Select the most recent backup for a client"),
      T_("Select backup for a client before a specified time"),
      T_("Enter a list of files to restore"),
      T_("Enter a list of files to restore before a specified time"),
      T_("Find the JobIds of the most recent backup for a client"),
      T_("Find the JobIds for a backup for a client before a specified time"),
      T_("Enter a list of directories to restore for found JobIds"),
      T_("Select full restore to a specified Job date"),
      T_("Cancel"),
      nullptr};

  PopulateUaWithPrompts(ua, list);

  window_width = 5000;
  lines_threshold = 10;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(
      output.c_str(),
      " 1: List last 20 Jobs run                                             "
      " 2: List Jobs where a given File is saved                             "
      " 3: Enter list of comma separated JobIds to select                    "
      " 4: Enter SQL list command                                            "
      " 5: Select the most recent backup for a client                        "
      " 6: Select backup for a client before a specified time                "
      " 7: Enter a list of files to restore                                  "
      " 8: Enter a list of files to restore before a specified time          "
      " 9: Find the JobIds of the most recent backup for a client            "
      "10: Find the JobIds for a backup for a client before a specified time "
      "11: Enter a list of directories to restore for found JobIds           "
      "12: Select full restore to a specified Job date                       "
      "13: Cancel                                                            \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Format15Elements_SmallWidth10LineThreshold)
{
  const char* list[]
      = {T_("bareos1"),  T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"),  T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"),  T_("bareos10"), T_("bareos11"), T_("bareos12"),
         T_("bareos13"), T_("bareos14"), T_("bareos15"),

         nullptr};

  PopulateUaWithPrompts(ua, list);

  window_width = 60;
  lines_threshold = 10;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(output.c_str(),
               " 1: bareos1   5: bareos5   9: bareos9  13: bareos13 \n"
               " 2: bareos2   6: bareos6  10: bareos10 14: bareos14 \n"
               " 3: bareos3   7: bareos7  11: bareos11 15: bareos15 \n"
               " 4: bareos4   8: bareos8  12: bareos12 \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting, Formatting_NoWidth)
{
  const char* list[]
      = {T_("bareos1"), T_("bareos2"),  T_("bareos3"),  T_("bareos4"),
         T_("bareos5"), T_("bareos6"),  T_("bareos7"),  T_("bareos8"),
         T_("bareos9"), T_("bareos10"), T_("bareos11"), nullptr};

  PopulateUaWithPrompts(ua, list);

  window_width = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(output.c_str(),
               " 1: bareos1\n"
               " 2: bareos2\n"
               " 3: bareos3\n"
               " 4: bareos4\n"
               " 5: bareos5\n"
               " 6: bareos6\n"
               " 7: bareos7\n"
               " 8: bareos8\n"
               " 9: bareos9\n"
               "10: bareos10\n"
               "11: bareos11\n");
  /* clang-format on */
}

TEST_F(PromptsFormatting,
       FormatPromptsContainingSpacesAndRegularPrompts_StandartWidthNoThreshold)
{
  const char* list[] = {T_(""), T_("Listsaved"), T_("Cancel"), nullptr};

  PopulateUaWithPrompts(ua, list);

  lines_threshold = 0;
  std::string output = FormatPrompts(ua, window_width, lines_threshold);

  /* clang-format off */
  EXPECT_STREQ(output.c_str(),
               "1:           "
               "2: Listsaved "
               "3: Cancel    \n");
  /* clang-format on */
}

TEST_F(PromptsFormatting,
       FormatPromptsContainingOnlySpacesPrompts_StandartWidthNoThreshold)
{
  const char* list[] = {T_(""), T_(" "), T_("  "), nullptr};

  PopulateUaWithPrompts(ua, list);

  std::string output = FormatPrompts(ua, 80, 20);

  /* clang-format off */
  EXPECT_STREQ(output.c_str(),
               "1: \n"
               "2:  \n"
               "3:   \n");
  /* clang-format on */
}

TEST(InteractiveSelection, FiltersAndSelectsInDirector)
{
  std::vector<std::string> options{"Alpha", "Beta", "Gamma"};
  InteractiveSelection selection(options);

  EXPECT_EQ(selection.ApplyInput("key:text:b"),
            SelectionInputResult::kContinue);
  EXPECT_EQ(selection.Format("Choices:\n", "Select"),
            "Choices:\n"
            "Select (Up/Down/Left/Right, Enter, Esc, type a number or text to "
            "filter):\n"
            "Filter: b\n"
            "> \033[7m2: Beta\033[0m\n");
  EXPECT_EQ(selection.ApplyInput("key:enter"), SelectionInputResult::kSelected);
  EXPECT_EQ(selection.selected_index(), 1);
}

TEST(InteractiveSelection, MarksSelectedLineWithPlainTextIndicator)
{
  /* The selected line must carry a plain-text marker ("> ") in addition to
   * the ANSI reverse-video escape codes, since screen readers and braille
   * displays attached to a terminal read the character stream only and do
   * not surface ANSI attribute codes. Without this marker, a blind user
   * has no way to tell which item is currently selected. */
  std::vector<std::string> options{"Alpha", "Beta", "Gamma"};
  InteractiveSelection selection(options);

  EXPECT_EQ(selection.Format("", "Select"),
            "Select (Up/Down/Left/Right, Enter, Esc, type a number or text to "
            "filter):\n"
            "> \033[7m1: Alpha\033[0m\n"
            "  2: Beta\n"
            "  3: Gamma\n");

  EXPECT_EQ(selection.ApplyInput("key:down"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.Format("", "Select"),
            "Select (Up/Down/Left/Right, Enter, Esc, type a number or text to "
            "filter):\n"
            "  1: Alpha\n"
            "> \033[7m2: Beta\033[0m\n"
            "  3: Gamma\n");
}

TEST(InteractiveSelection, NavigatesVisibleOptions)
{
  std::vector<std::string> options{"first", "second", "third"};
  InteractiveSelection selection(options);

  EXPECT_EQ(selection.ApplyInput("key:down"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 1);
  EXPECT_EQ(selection.ApplyInput("key:up"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 0);
  EXPECT_EQ(selection.ApplyInput("key:cancel"),
            SelectionInputResult::kCanceled);
}

TEST(InteractiveSelection, AcceptsExplicitSelection)
{
  std::vector<std::string> options{"first", "second", "third"};
  InteractiveSelection selection(options);

  EXPECT_EQ(selection.ApplyInput("key:select:3"),
            SelectionInputResult::kSelected);
  EXPECT_EQ(selection.selected_index(), 2);
}

TEST(InteractiveSelection, AcceptsPlainNumericSelection)
{
  std::vector<std::string> options{"first", "second", "third"};
  InteractiveSelection selection(options);

  EXPECT_EQ(selection.ApplyInput("2"), SelectionInputResult::kSelected);
  EXPECT_EQ(selection.selected_index(), 1);
}

TEST(InteractiveSelection, AcceptsNumericSelectionWithWhitespace)
{
  std::vector<std::string> options{"first", "second", "third"};
  InteractiveSelection selection(options);

  EXPECT_EQ(selection.ApplyInput(" 2 \r\n"), SelectionInputResult::kSelected);
  EXPECT_EQ(selection.selected_index(), 1);
}

TEST(InteractiveSelection, AcceptsEmptyLineAsSelection)
{
  std::vector<std::string> options{"first", "second", "third"};
  InteractiveSelection selection(options);

  EXPECT_EQ(selection.ApplyInput(""), SelectionInputResult::kSelected);
  EXPECT_EQ(selection.selected_index(), 0);
}

TEST(InteractiveSelection, AcceptsPlainTextFiltersFromLineBasedClients)
{
  std::vector<std::string> options{"Alpha", "Makefile", "Gamma"};
  InteractiveSelection selection(options);

  EXPECT_EQ(selection.ApplyInput("Makefile"), SelectionInputResult::kSelected);
  EXPECT_EQ(selection.selected_index(), 1);
}

TEST(InteractiveSelection, KeepsSelectionInVisibleWindow)
{
  std::vector<std::string> options;
  for (int i = 1; i <= 30; ++i) {
    options.emplace_back("option " + std::to_string(i));
  }
  InteractiveSelection selection(options);
  for (int i = 0; i < 25; ++i) {
    EXPECT_EQ(selection.ApplyInput("key:down"),
              SelectionInputResult::kContinue);
  }

  const auto output = selection.Format("", "Select");
  EXPECT_NE(output.find("> \033[7m26: option 26\033[0m\n"), std::string::npos);
  EXPECT_NE(output.find("  ...\n"), std::string::npos);
  EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 22);
}

TEST(InteractiveSelection, HonorsSmallExplicitVisibleWindow)
{
  /* Directors compute a smaller max_visible_options for clients that
   * report a short terminal (see DoPrompt()'s use of
   * ua->terminal_height), so the whole menu fits without scrolling its
   * header/first options off-screen. */
  std::vector<std::string> options;
  for (int i = 1; i <= 30; ++i) {
    options.emplace_back("option " + std::to_string(i));
  }
  InteractiveSelection selection(options);
  for (int i = 0; i < 25; ++i) {
    EXPECT_EQ(selection.ApplyInput("key:down"),
              SelectionInputResult::kContinue);
  }

  const auto output = selection.Format("", "Select", /*max_visible_options=*/5);
  EXPECT_NE(output.find("> \033[7m26: option 26\033[0m\n"), std::string::npos);
  EXPECT_NE(output.find("  ...\n"), std::string::npos);
  // header line + leading "..." + 5 option lines + trailing "..." = 8 lines
  EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 8);
}

TEST(InteractiveSelection, RendersMultipleColumnsWhenWideEnough)
{
  /* Directors compute a column count from the client's reported terminal
   * width (see DoPrompt()'s use of ua->terminal_width) so a short-but-wide
   * terminal can show many options at once, spread across columns, instead
   * of only a handful in a single vertical list. */
  std::vector<std::string> options;
  for (int i = 1; i <= 12; ++i) {
    options.emplace_back("option " + std::to_string(i));
  }
  InteractiveSelection selection(options);
  selection.SetColumnLayout(/*rows_per_column=*/4, /*num_columns=*/3);

  const auto output = selection.Format("", "Select", /*max_visible_options=*/4);

  // All 12 options fit exactly into 3 columns of 4 rows, so nothing is
  // truncated.
  EXPECT_EQ(output.find("  ...\n"), std::string::npos);
  // Column-major layout: the first row holds options 1, 5, and 9.
  auto first_option = output.find("1: option 1");
  auto second_option = output.find("5: option 5");
  auto third_option = output.find("9: option 9");
  ASSERT_NE(first_option, std::string::npos);
  ASSERT_NE(second_option, std::string::npos);
  ASSERT_NE(third_option, std::string::npos);
  EXPECT_LT(first_option, second_option);
  EXPECT_LT(second_option, third_option);
  auto first_newline = output.find('\n', first_option);
  EXPECT_GT(first_newline, third_option);

  // header line + 4 option rows = 5 lines
  EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 5);
}

TEST(InteractiveSelection, LeftRightNavigateBetweenColumns)
{
  std::vector<std::string> options;
  for (int i = 1; i <= 12; ++i) {
    options.emplace_back("option " + std::to_string(i));
  }
  InteractiveSelection selection(options);
  selection.SetColumnLayout(/*rows_per_column=*/4, /*num_columns=*/3);

  EXPECT_EQ(selection.selected_index(), 0u);
  EXPECT_EQ(selection.ApplyInput("key:right"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 4u);
  EXPECT_EQ(selection.ApplyInput("key:right"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 8u);
  // Already in the last column: no adjacent column to the right.
  EXPECT_EQ(selection.ApplyInput("key:right"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 8u);

  EXPECT_EQ(selection.ApplyInput("key:left"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 4u);
  EXPECT_EQ(selection.ApplyInput("key:left"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 0u);
  // Already in the first column: no adjacent column to the left.
  EXPECT_EQ(selection.ApplyInput("key:left"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 0u);

  // Up/down still move within a single option at a time, unaffected by the
  // column layout.
  EXPECT_EQ(selection.ApplyInput("key:down"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 1u);
}

TEST(InteractiveSelection, LeftRightAlignsWithRenderedColumnsWhenNotFull)
{
  // Regression test: with 6 matches, 4 rows per column, and 2 columns, the
  // second column only has 2 entries (not a full 4), so a stale
  // recomputed-from-window-size row count would misalign rendering with
  // Left/Right navigation. Format() and SelectAdjacentColumn() must agree
  // on the same fixed rows_per_column_ (4) stride regardless of how many
  // entries actually land in the last column.
  std::vector<std::string> options;
  for (int i = 1; i <= 6; ++i) {
    options.emplace_back("option " + std::to_string(i));
  }
  InteractiveSelection selection(options);
  selection.SetColumnLayout(/*rows_per_column=*/4, /*num_columns=*/2);

  const auto output = selection.Format("", "Select", /*max_visible_options=*/4);
  // Row 0 must pair option 1 (column 0, row 0) with option 5 (column 1,
  // row 0 == global position 4), not option 4 (which a
  // recomputed-from-window-size rows-per-column of 3 would wrongly pick).
  auto row0_option1 = output.find("1: option 1");
  auto row0_option5 = output.find("5: option 5");
  ASSERT_NE(row0_option1, std::string::npos);
  ASSERT_NE(row0_option5, std::string::npos);
  auto first_newline = output.find('\n', row0_option1);
  EXPECT_LT(row0_option5, first_newline);

  EXPECT_EQ(selection.selected_index(), 0u);
  EXPECT_EQ(selection.ApplyInput("key:right"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 4u);  // option 5, not option 4
  EXPECT_EQ(selection.ApplyInput("key:right"), SelectionInputResult::kContinue);
  // Already in the last (2nd) column: no adjacent column to the right.
  EXPECT_EQ(selection.selected_index(), 4u);
  EXPECT_EQ(selection.ApplyInput("key:left"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 0u);
}

TEST(InteractiveSelection, LeftRightFallBackToUpDownInSingleColumn)
{
  // Without an explicit multi-column layout (the default), left/right keep
  // their historical behavior of being synonyms for up/down.
  std::vector<std::string> options{"a", "b", "c"};
  InteractiveSelection selection(options);
  EXPECT_EQ(selection.ApplyInput("key:right"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 1u);
  EXPECT_EQ(selection.ApplyInput("key:left"), SelectionInputResult::kContinue);
  EXPECT_EQ(selection.selected_index(), 0u);
}

TEST(InteractiveSelection, LoneDotCancelsBeforeAnyFilterTyped)
{
  // "." has always been the classic shortcut to cancel a selection. In
  // raw/arrow-key mode a typed "." arrives as "key:text:.", so this must be
  // special-cased to keep the shortcut working.
  std::vector<std::string> options{"a", "b", "c"};
  InteractiveSelection selection(options);
  EXPECT_EQ(selection.ApplyInput("key:text:."),
            SelectionInputResult::kCanceled);
}

TEST(InteractiveSelection, DotIsAnOrdinaryFilterCharacterOnceTyping)
{
  // Once the user has already started typing a filter, "." should behave
  // like any other filter character instead of canceling.
  std::vector<std::string> options{"alpha", "a.b", "gamma"};
  InteractiveSelection selection(options);
  EXPECT_EQ(selection.ApplyInput("key:text:a"),
            SelectionInputResult::kContinue);
  EXPECT_EQ(selection.ApplyInput("key:text:."),
            SelectionInputResult::kContinue);
  const auto output = selection.Format("", "Select");
  EXPECT_NE(output.find("2: a.b"), std::string::npos);
  EXPECT_EQ(output.find("1: alpha"), std::string::npos);
}
