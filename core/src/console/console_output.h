/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_CONSOLE_CONSOLE_OUTPUT_H_
#define BAREOS_CONSOLE_CONSOLE_OUTPUT_H_

#include <cstdio>
#include <string>
#include <string_view>
#include "include/compiler_macro.h"

enum class ConsoleOutputStyle
{
  kDefault,
  kInfo,
  kWarning,
  kError,
};

void ConsoleOutputFormat(const char* fmt, ...) PRINTF_LIKE(1, 2);
void ConsoleOutput(const char* buf);
void ConsoleOutputStyled(const char* buf, ConsoleOutputStyle style);
void ConsoleSetColorEnabled(bool enable);
bool ConsoleColorEnabled();
void EnableTeeOut();
void DisableTeeOut();
void SetTeeFile(FILE* f);
void CloseTeeFile();

/**
 * Controls whether ConsoleOutput() strips ANSI/VT100 escape sequences
 * before writing to the console (the default, since most Windows consoles
 * do not interpret them and would otherwise show raw control characters).
 * Call with enable=true once the console has been confirmed to support
 * ENABLE_VIRTUAL_TERMINAL_PROCESSING, so escape sequences (e.g. the
 * reverse-video highlighting used by the interactive restore selection
 * menu) are passed through and rendered instead of stripped.
 */
void ConsoleSetAnsiPassthrough(bool enable);

namespace console {

bool ShouldUseColor(bool output_is_tty,
                    bool ansi_supported,
                    const char* term,
                    const char* no_color);
std::string StripAnsiEscapeSequences(std::string_view text);
std::string ApplyStyle(std::string_view text,
                       ConsoleOutputStyle style,
                       bool color_enabled);
std::string ReadlinePrompt(std::string_view prompt, bool color_enabled);

}  // namespace console

#endif  // BAREOS_CONSOLE_CONSOLE_OUTPUT_H_
