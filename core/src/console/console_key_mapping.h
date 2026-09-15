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

#ifndef BAREOS_CONSOLE_CONSOLE_KEY_MAPPING_H_
#define BAREOS_CONSOLE_CONSOLE_KEY_MAPPING_H_

#include <string>
#include <string_view>

namespace console {

/**
 * Maps a single Windows console key-press event to the "key:..." event
 * protocol string understood by InteractiveSelection::ApplyInput() (see
 * dird/ua_select.cc) -- the same protocol produced by the POSIX raw-mode
 * reader in console.cc's ReadSelectionInput().
 *
 * This takes plain integers instead of the Win32 KEY_EVENT_RECORD type so
 * the mapping logic itself has no dependency on <windows.h> and can be
 * unit-tested on any platform. On Windows, pass:
 *   - virtual_key_code: KEY_EVENT_RECORD::wVirtualKeyCode (e.g. VK_UP)
 *   - unicode_char:     KEY_EVENT_RECORD::uChar.UnicodeChar
 *   - ctrl_pressed:     whether LEFT_CTRL_PRESSED or RIGHT_CTRL_PRESSED is
 *                       set in KEY_EVENT_RECORD::dwControlKeyState
 *
 * Returns an empty string if the key press does not map to any protocol
 * event; the caller should then keep waiting for the next key/event.
 */
std::string MapConsoleKeyEventToSelectionEvent(int virtual_key_code,
                                               wchar_t unicode_char,
                                               bool ctrl_pressed);

/**
 * Maps exactly one UTF-8 encoded printable character to a key:text event.
 * Invalid encodings and C0/C1 control characters return an empty string.
 */
std::string MapUtf8InputToSelectionEvent(std::string_view input);

/**
 * Maps one UTF-16 code unit, or a high/low surrogate pair, to a key:text
 * event. Invalid scalar values and terminal controls return an empty string.
 */
std::string MapUtf16InputToSelectionEvent(wchar_t first, wchar_t second = 0);

}  // namespace console

#endif  // BAREOS_CONSOLE_CONSOLE_KEY_MAPPING_H_
