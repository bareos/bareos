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

#include "console/console_key_mapping.h"

#include "gtest/gtest.h"

using console::MapAnsiEscapeSequenceToSelectionEvent;
using console::MapConsoleKeyEventToSelectionEvent;
using console::MapUtf16InputToSelectionEvent;
using console::MapUtf8InputToSelectionEvent;

// Virtual-key-code constants mirrored from winuser.h (see the comment in
// console_key_mapping.cc for why this file avoids depending on
// <windows.h>).
namespace {
constexpr int kVkBack = 0x08;
constexpr int kVkReturn = 0x0D;
constexpr int kVkEscape = 0x1B;
constexpr int kVkPageUp = 0x21;
constexpr int kVkPageDown = 0x22;
constexpr int kVkEnd = 0x23;
constexpr int kVkHome = 0x24;
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;
constexpr int kVkInsert = 0x2D;
constexpr int kVkDelete = 0x2E;
constexpr int kVkNone = 0; /* no virtual key, e.g. a plain character event */
}  // namespace

TEST(ConsoleKeyMapping, ArrowKeysMapToNavigation)
{
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkUp, 0, false), "key:up");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkDown, 0, false), "key:down");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkLeft, 0, false), "key:left");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkRight, 0, false),
            "key:right");
}

TEST(ConsoleKeyMapping, HomeAndEndMapToHorizontalExtremes)
{
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkHome, 0, false), "key:home");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkEnd, 0, false), "key:end");
}

TEST(ConsoleKeyMapping, PageUpAndPageDownMapToPageNavigation)
{
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkPageUp, 0, false),
            "key:pageup");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkPageDown, 0, false),
            "key:pagedown");
}

TEST(ConsoleKeyMapping, InsertAndDeleteMapToMarkAndUnmark)
{
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkInsert, 0, false),
            "key:insert");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkDelete, 0, false),
            "key:delete");
}

TEST(ConsoleKeyMapping, AnsiPageKeysMapToPageNavigation)
{
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[5~"), "key:pageup");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[6~"), "key:pagedown");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[5;2~"), "key:pageup");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[6;5~"), "key:pagedown");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[5^"), "key:pageup");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[6$"), "key:pagedown");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[5@"), "key:pageup");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[5u"), "key:pageup");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[6;1u"), "key:pagedown");
}

TEST(ConsoleKeyMapping, AnsiCursorKeysMapToNavigation)
{
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[A"), "key:up");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[B"), "key:down");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[C"), "key:right");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[D"), "key:left");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[H"), "key:home");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[F"), "key:end");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[1~"), "key:home");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[4~"), "key:end");
}

TEST(ConsoleKeyMapping, AnsiInsertAndDeleteKeysMapToMarkAndUnmark)
{
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[2~"), "key:insert");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[3~"), "key:delete");
  EXPECT_EQ(MapAnsiEscapeSequenceToSelectionEvent("\x1b[3;5~"), "key:delete");
}

TEST(ConsoleKeyMapping, EnterBackspaceEscape)
{
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkReturn, L'\r', false),
            "key:enter");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkBack, L'\b', false),
            "key:backspace");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkEscape, 0x1B, false),
            "key:cancel");
}

TEST(ConsoleKeyMapping, TabSwitchesPanel)
{
  constexpr int kVkTab = 0x09;
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkTab, L'\t', false),
            "key:tab");
  // Some consoles only report the character, without a distinct virtual
  // key code.
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'\t', false),
            "key:tab");
}

TEST(ConsoleKeyMapping, CtrlCCancelsEitherWay)
{
  // Some consoles report Ctrl-C as a virtual key + ctrl modifier ...
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'c', true),
            "key:cancel");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'C', true),
            "key:cancel");
  // ... others as a plain 0x03 (ETX) character.
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 3, false),
            "key:cancel");
}

TEST(ConsoleKeyMapping, VimControlKeyBindings)
{
  // Ctrl-J / Ctrl-N map to down
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'j', true),
            "key:down");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'n', true),
            "key:down");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 10, true), "key:down");

  // Ctrl-K / Ctrl-P map to up
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'k', true), "key:up");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'p', true), "key:up");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 11, true), "key:up");

  // Ctrl-H / Ctrl-B map to left
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'h', true),
            "key:left");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'b', true),
            "key:left");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 8, true), "key:left");

  // Ctrl-L / Ctrl-F map to right
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'l', true),
            "key:right");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'f', true),
            "key:right");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 12, true), "key:right");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 6, true), "key:right");
}

TEST(ConsoleKeyMapping, ControlPageNavigation)
{
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'u', true),
            "key:pageup");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'U', true),
            "key:pageup");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 21, true),
            "key:pageup");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'd', true),
            "key:pagedown");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'D', true),
            "key:pagedown");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 4, true),
            "key:pagedown");
}

TEST(ConsoleKeyMapping, SpaceAndPrintableText)
{
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L' ', false),
            "key:space");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'a', false),
            "key:text:a");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'9', false),
            "key:text:9");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'~', false),
            "key:text:~");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, L'ü', false),
            "key:text:ü");
}

TEST(ConsoleKeyMapping, Utf8InputMapsAsOneCharacter)
{
  EXPECT_EQ(MapUtf8InputToSelectionEvent("ü"), "key:text:ü");
  EXPECT_EQ(MapUtf8InputToSelectionEvent("界"), "key:text:界");
  EXPECT_EQ(MapUtf8InputToSelectionEvent("\xc2\x9b"), "");
  EXPECT_EQ(MapUtf8InputToSelectionEvent("\xc0\xaf"), "");
  EXPECT_EQ(MapUtf8InputToSelectionEvent("\xff"), "");
}

TEST(ConsoleKeyMapping, Utf16SurrogatePairMapsAsOneCharacter)
{
  EXPECT_EQ(MapUtf16InputToSelectionEvent(0xd83d, 0xde00),
            "key:text:\xf0\x9f\x98\x80");
  EXPECT_EQ(MapUtf16InputToSelectionEvent(0xd83d), "");
  EXPECT_EQ(MapUtf16InputToSelectionEvent(0xde00), "");
  EXPECT_EQ(MapUtf16InputToSelectionEvent(L'ü'), "key:text:ü");
}

TEST(ConsoleKeyMapping, UnmappedKeyReturnsEmpty)
{
  // Function keys, modifier-only presses, non-printable control
  // characters, etc. don't map to any selection-menu event; the caller is
  // expected to keep waiting for the next key in that case.
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(0x70 /* VK_F1 */, 0, false), "");
  EXPECT_EQ(MapConsoleKeyEventToSelectionEvent(kVkNone, 0x01, false), "");
}
