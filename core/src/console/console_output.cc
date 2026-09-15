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

#include <string>

static FILE* output_file_ = stdout;
static bool teeout_enabled_ = false;
#if defined(HAVE_WIN32)
static bool ansi_passthrough_ = false;
#else
static bool ansi_passthrough_ = true;
#endif
static bool color_enabled_ = false;

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
void ConsoleSetAnsiPassthrough(bool enable) { ansi_passthrough_ = enable; }
void ConsoleSetColorEnabled(bool enable) { color_enabled_ = enable; }
bool ConsoleColorEnabled() { return color_enabled_; }

namespace console {

bool ShouldUseColor(bool output_is_tty,
                    bool ansi_supported,
                    const char* term,
                    const char* no_color)
{
  return output_is_tty && ansi_supported && no_color == nullptr
         && (!term || !Bstrcasecmp(term, "dumb"));
}

std::string StripAnsiEscapeSequences(std::string_view text)
{
  std::string out;
  for (size_t i = 0; i < text.size(); ++i) {
    unsigned char value = static_cast<unsigned char>(text[i]);
    if (value == 0x1b) {
      if (++i >= text.size()) { break; }
      char introducer = text[i];
      if (introducer == '[') {
        while (++i < text.size()) {
          value = static_cast<unsigned char>(text[i]);
          if (value >= 0x40 && value <= 0x7e) { break; }
        }
      } else if (introducer == ']' || introducer == 'P' || introducer == 'X'
                 || introducer == '^' || introducer == '_') {
        while (++i < text.size()) {
          value = static_cast<unsigned char>(text[i]);
          if (value == 0x07) { break; }
          if (value == 0x1b && i + 1 < text.size() && text[i + 1] == '\\') {
            ++i;
            break;
          }
        }
      }
      continue;
    }
    if (value == 0xc2 && i + 1 < text.size()) {
      unsigned char next = static_cast<unsigned char>(text[i + 1]);
      if (next >= 0x80 && next <= 0x9f) {
        ++i;
        continue;
      }
    }
    if ((value < 0x20 && value != '\n' && value != '\t') || value == 0x7f
        || (value >= 0x80 && value <= 0x9f)) {
      continue;
    }
    out.push_back(text[i]);
  }
  return out;
}

std::string ApplyStyle(std::string_view text,
                       ConsoleOutputStyle style,
                       bool color_enabled)
{
  if (!color_enabled || style == ConsoleOutputStyle::kDefault) {
    return std::string(text);
  }

  const char* prefix = "";
  switch (style) {
    case ConsoleOutputStyle::kInfo:
      prefix = "\033[36m";
      break;
    case ConsoleOutputStyle::kWarning:
      prefix = "\033[1;33m";
      break;
    case ConsoleOutputStyle::kError:
      prefix = "\033[1;31m";
      break;
    case ConsoleOutputStyle::kDefault:
      break;
  }
  return std::string(prefix) + std::string(text) + "\033[0m";
}

std::string ReadlinePrompt(std::string_view prompt, bool color_enabled)
{
  if (!color_enabled || prompt.empty()) { return std::string(prompt); }

  // Readline excludes bytes enclosed by \001 and \002 from cursor-width
  // calculations.
  return "\001\033[1;36m\002" + std::string(prompt) + "\001\033[0m\002";
}

}  // namespace console

void ConsoleOutput(const char* buf)
{
  std::string plain = console::StripAnsiEscapeSequences(buf);
  const char* file_buf
      = output_file_ == stdout && ansi_passthrough_ ? buf : plain.c_str();
  fputs(file_buf, output_file_);
  fflush(output_file_);
  if (teeout_enabled_) {
    const char* terminal_buf = ansi_passthrough_ ? buf : plain.c_str();
    fputs(terminal_buf, stdout);
    fflush(stdout);
  }
}

void ConsoleOutputStyled(const char* buf, ConsoleOutputStyle style)
{
  std::string plain = console::StripAnsiEscapeSequences(buf);
  std::string styled = console::ApplyStyle(plain, style, color_enabled_);
  ConsoleOutput(styled.c_str());
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
