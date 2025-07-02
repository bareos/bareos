/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include "logger.h"
#include <cstdint>
#include "fmt/base.h"
#include "indicators/setting.hpp"

#include <indicators/progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <optional>

namespace progressbar {
using namespace indicators;

struct logger : public GenericLogger {
  using Output = typename indicators::TerminalHandle;
  static constexpr Output Current = Output::StdErr;

  static constexpr std::ostream& stream()
  {
    if constexpr (Current == Output::StdOut) {
      return std::cout;
    } else if constexpr (Current == Output::StdErr) {
      return std::cerr;
    }
  }

  static FILE* handle()
  {
    if constexpr (Current == Output::StdOut) {
      return stdout;
    } else if constexpr (Current == Output::StdErr) {
      return stderr;
    }
  }

  static void erase_line() { indicators::erase_line(Current); }

  static void show_cursor(bool show)
  {
    indicators::show_console_cursor(show, Current);
  }

  static bool is_a_tty()
  {
#if defined(HAVE_WIN32)
    HANDLE hndl = NULL;
    switch (Current) {
        // GetStdHandle return NULL if no such handle exists
      case TerminalHandle::StdOut: {
        hndl = GetStdHandle(STD_OUTPUT_HANDLE);
      }
      case TerminalHandle::StdErr: {
        hndl = GetStdHandle(STD_ERROR_HANDLE);
      }
    }
    return hndl != INVALID_HANDLE_VALUE && hndl != NULL
           && GetFileTYPE(hndl) == FILE_TYPE_CHAR;
#else
    switch (Current) {
      case TerminalHandle::StdOut:
        return isatty(STDOUT_FILENO);
      case TerminalHandle::StdErr:
        return isatty(STDERR_FILENO);
    }
    return false;
#endif
  }

  void Begin(std::size_t FileSize) override
  {
    if (is_a_tty()) { progress_bar.emplace(FileSize); }
  }
  void Progressed(std::size_t Amount) override
  {
    if (progress_bar) { progress_bar->progress(Amount); }
  }
  void End() override
  {
    if (!progress_bar) { return; }

    progress_bar.reset();
  }

  void SetStatus(std::string_view status) override
  {
    if (!progress_bar) { return; }
    progress_bar->bar.set_option(option::PostfixText(status));
  }

  void Info(std::string_view Message) override
  {
    if (progress_bar) {
      stream() << termcolor::reset;
      erase_line();
      stream().flush();
    }
    fmt::println(stderr, "{}", Message);
    if (progress_bar) { progress_bar->print(); }
  }

  struct progress_bar {
    std::size_t goal{0};
    std::size_t current{0};

    ProgressBar bar;

    progress_bar() = default;
    progress_bar(std::size_t goal_)
        : goal{goal_}
        , bar{
              option::BarWidth{50},
              option::Start{"["},
              option::Fill{"="},
              option::Lead{">"},
              option::Remainder{" "},
              option::End{"]"},
              option::ForegroundColor{Color::green},
              option::ShowPercentage{true},
              option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
              option::MaxProgress(goal_),
              option::ShowPercentage(true),

              // using anything but cout is not really supported ...
              option::Stream(Current),

              option::ShowElapsedTime(true),
              option::ShowRemainingTime(true),
          }
    {
      show_cursor(false);
      cursor_hidden = true;
    }
    progress_bar(progress_bar const&) = delete;
    progress_bar(progress_bar&&) = delete;
    progress_bar& operator=(progress_bar const&) = delete;
    progress_bar& operator=(progress_bar&&) = delete;

    void progress(std::size_t amount)
    {
      if (!bar.is_completed()) {
        current += amount;
        bar.set_progress(current);
      }
    }

    void print()
    {
      if (!bar.is_completed()) { bar.print_progress(); }
    }

    ~progress_bar()
    {
      if (!bar.is_completed()) { bar.mark_as_completed(); }
      if (cursor_hidden) { show_cursor(true); }
    }

    bool cursor_hidden{};
  };


  logger()
  {
    // indicators::show_console_cursor(false);
  }
  ~logger()
  {
    // indicators::show_console_cursor(true);
  }
  std::optional<progress_bar> progress_bar;
};

GenericLogger* get()
{
  static logger Instance;
  return std::addressof(Instance);
}
};  // namespace progressbar
