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

#include "console_output.h"

#include <stdarg.h>

#include "include/bareos.h"

#if defined(HAVE_WIN32)
#  include <string>
#endif

static FILE* output_file_ = stdout;
static bool teeout_enabled_ = false;

void ConsoleOutputFormat(const char* fmt, ...)
{
  PoolMem buf;
  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  auto res = buf.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  ASSERT(res >= 0);
  ConsoleOutput(buf.c_str());
}

#if defined(HAVE_WIN32)
/**
 * The Windows console does not interpret ANSI/VT100 escape sequences (used
 * e.g. for reverse-video highlighting in interactive selection menus, see
 * InteractiveSelection::Format() in dird/ua_select.cc, and for the "clear
 * screen" sequence below) unless ENABLE_VIRTUAL_TERMINAL_PROCESSING has
 * been enabled on the console and confirmed to stick (see console.cc's
 * main(), which calls ConsoleSetAnsiPassthrough(true) when that succeeds).
 * Until/unless that happens, strip any ANSI CSI sequence (ESC '[' ...
 * final byte) before writing to the console, instead of printing them as
 * visible garbage control characters. This loses the highlighting/clear-
 * screen effect, but keeps the output readable; the plain-text "> "
 * selection marker that is emitted alongside the escape codes remains, so
 * the currently selected menu entry can still be identified.
 */
static bool strip_ansi_escapes_ = true;

void ConsoleSetAnsiPassthrough(bool enable) { strip_ansi_escapes_ = !enable; }

static std::string StripAnsiEscapeSequences(const char* buf)
{
  std::string out;
  for (const char* p = buf; *p != '\0'; ++p) {
    if (*p == '\x1b' && *(p + 1) == '[') {
      const char* q = p + 2;
      while (*q != '\0' && (*q < 0x40 || *q > 0x7e)) { ++q; }
      if (*q != '\0') { ++q; /* skip the final byte too */ }
      p = q - 1; /* -1 because the for loop will ++p */
      continue;
    }
    out.push_back(*p);
  }
  return out;
}
#endif  // HAVE_WIN32

void ConsoleOutput(const char* buf)
{
#if defined(HAVE_WIN32)
  std::string stripped;
  if (strip_ansi_escapes_) {
    stripped = StripAnsiEscapeSequences(buf);
    buf = stripped.c_str();
  }
#endif
  fputs(buf, output_file_);
  fflush(output_file_);
  if (teeout_enabled_) {
    fputs(buf, stdout);
    fflush(stdout);
  }
}

void EnableTeeOut() { teeout_enabled_ = true; }

void DisableTeeOut() { teeout_enabled_ = false; }

void SetTeeFile(FILE* f) { output_file_ = f; }

void CloseTeeFile()
{
  if (output_file_ != stdout) {
    fclose(output_file_);
    SetTeeFile(stdout);
    DisableTeeOut();
  }
}
