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
constexpr int kVkEnd = 0x23;
constexpr int kVkHome = 0x24;
constexpr int kVkLeft = 0x25;
constexpr int kVkUp = 0x26;
constexpr int kVkRight = 0x27;
constexpr int kVkDown = 0x28;
constexpr int kVkTab = 0x09;

std::string EncodeUtf8(char32_t codepoint)
{
  std::string result;
  if (codepoint <= 0x7f) {
    result.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7ff) {
    result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
    result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else if (codepoint <= 0xffff) {
    result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
    result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else {
    result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
    result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
    result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
  return result;
}
}  // namespace

std::string MapUtf8InputToSelectionEvent(std::string_view input)
{
  if (input.empty() || input.size() > 4) { return ""; }

  unsigned char first = input[0];
  size_t expected_length = 0;
  char32_t codepoint = 0;
  if (first <= 0x7f) {
    expected_length = 1;
    codepoint = first;
  } else if ((first & 0xe0) == 0xc0) {
    expected_length = 2;
    codepoint = first & 0x1f;
  } else if ((first & 0xf0) == 0xe0) {
    expected_length = 3;
    codepoint = first & 0x0f;
  } else if ((first & 0xf8) == 0xf0) {
    expected_length = 4;
    codepoint = first & 0x07;
  } else {
    return "";
  }
  if (input.size() != expected_length) { return ""; }

  for (size_t i = 1; i < input.size(); ++i) {
    unsigned char byte = input[i];
    if ((byte & 0xc0) != 0x80) { return ""; }
    codepoint = (codepoint << 6) | (byte & 0x3f);
  }

  bool overlong = (expected_length == 2 && codepoint < 0x80)
                  || (expected_length == 3 && codepoint < 0x800)
                  || (expected_length == 4 && codepoint < 0x10000);
  if (overlong || codepoint < 0x20 || (codepoint >= 0x7f && codepoint <= 0x9f)
      || (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) {
    return "";
  }

  return "key:text:" + std::string(input);
}

std::string MapUtf16InputToSelectionEvent(wchar_t first, wchar_t second)
{
  char32_t codepoint = static_cast<char32_t>(first);
  if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
    char32_t low = static_cast<char32_t>(second);
    if (low < 0xdc00 || low > 0xdfff) { return ""; }
    codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
  } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
    return "";
  } else if (second != 0) {
    return "";
  }

  if (codepoint < 0x20 || (codepoint >= 0x7f && codepoint <= 0x9f)
      || codepoint > 0x10ffff) {
    return "";
  }
  return "key:text:" + EncodeUtf8(codepoint);
}

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
    case kVkHome:
      return "key:home";
    case kVkEnd:
      return "key:end";
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
    case kVkTab:
      return "key:tab";
    default:
      break;
  }

  if (unicode_char == L' ') { return "key:space"; }

  if (unicode_char == L'\t') { return "key:tab"; }

  if (unicode_char >= 0x20 && unicode_char <= 0x7e) {
    std::string event = "key:text:";
    event.push_back(static_cast<char>(unicode_char));
    return event;
  }
  if (unicode_char > 0x9f
      && !(unicode_char >= 0xd800 && unicode_char <= 0xdfff)) {
    return "key:text:" + EncodeUtf8(static_cast<char32_t>(unicode_char));
  }

  return "";
}

}  // namespace console
