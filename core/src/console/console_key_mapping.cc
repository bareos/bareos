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

#include "console_key_mapping.h"

namespace console {

namespace {
// Well-known, stable Win32 virtual-key-code values (see winuser.h). Defined
// here as plain constants instead of including <windows.h>, so this
// translation unit (and its unit tests) has no Windows Console API
// dependency.
constexpr int kVkBack = 0x08;
constexpr int kVkReturn = 0x0D;
constexpr int kVkEscape = 0x1B;
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;
}  // namespace

std::string MapConsoleKeyEventToSelectionEvent(int virtual_key_code,
                                               wchar_t unicode_char,
                                               bool ctrl_pressed)
{
  // Ctrl-C: depending on the console mode this can arrive either as a
  // virtual key event with the ctrl modifier set, or as a plain 0x03 (ETX)
  // character -- handle both the same way the POSIX raw-mode reader
  // treats byte value 3.
  if ((ctrl_pressed && (unicode_char == L'c' || unicode_char == L'C'))
      || unicode_char == 3) {
    return "key:cancel";
  }

  if (ctrl_pressed) {
    switch (unicode_char) {
      case L'j':
      case L'J':
      case L'n':
      case L'N':
      case 10:
      case 14:
        return "key:down";
      case L'k':
      case L'K':
      case L'p':
      case L'P':
      case 11:
      case 16:
        return "key:up";
      case L'h':
      case L'H':
      case L'b':
      case L'B':
      case 2:
      case 8:
        return "key:left";
      case L'l':
      case L'L':
      case L'f':
      case L'F':
      case 12:
      case 6:
        return "key:right";
      default:
        break;
    }
  }

  switch (virtual_key_code) {
    case kVkUp:
      return "key:up";
    case kVkDown:
      return "key:down";
    case kVkLeft:
      return "key:left";
    case kVkRight:
      return "key:right";
    case kVkReturn:
      return "key:enter";
    case kVkBack:
      return "key:backspace";
    case kVkEscape:
      return "key:cancel";
    default:
      break;
  }

  if (unicode_char == L' ') { return "key:space"; }

  if (unicode_char >= 0x20 && unicode_char <= 0x7e) {
    std::string event = "key:text:";
    event.push_back(static_cast<char>(unicode_char));
    return event;
  }

  return "";
}

}  // namespace console
