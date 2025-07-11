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
#include <chrono>
#include "indicators/setting.hpp"

#include <indicators/progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <optional>

#include "format.h"

namespace progressbar {
using namespace indicators;


struct counter {
  using Clock = std::chrono::steady_clock;
  using Duration = Clock::duration;
  using Timepoint = Clock::time_point;
  using Value = std::size_t;
  static constexpr std::size_t BlockCount = 128;


  struct timed {
    Timepoint t;
    Value v;
  };

  struct block {
    block* Prev{nullptr};

    std::size_t Count{0};
    timed Values[BlockCount];
  };

  counter(Duration lookback_) : lookback{lookback_} {}

  block* NextBlock()
  {
    auto* Next = FreeList;
    if (!Next) {
      Next = new block;
    } else {
      FreeList = Next->Prev;
      Next->Count = 0;
    }
    assert(Next->Count == 0);

    return Next;
  }

  void add_data_point(Timepoint point, Value value)
  {
    cached.reset();

    if (!Last) {
      First = Last = NextBlock();

      Last->Values[Last->Count++] = {point, value};
      return;
    }

    assert(Last->Count > 0);

    auto& LastValue = Last->Values[Last->Count - 1];

    if (point - LastValue.t < std::chrono::milliseconds(30)) {
      // just treat them as the same event
      LastValue.v += value;
      return;
    }

    if (Last->Count == std::size(Last->Values)) {
      auto* Next = NextBlock();
      Next->Prev = Last;
      Last = Next;
    }

    Last->Values[Last->Count++] = {point, value};
  }

  double compute_average_per_sec()
  {
    if (!cached) {
      auto* Current = Last;

      assert(Current->Count > 0);

      auto Start = Current->Values[Current->Count - 1].t;
      auto LastPossible = Start - lookback;
      auto End = Start;


      std::size_t TotalValue = Current->Values[Current->Count - 1].v;

      for (size_t i = 1; i <= Current->Count - 1; ++i) {
        auto& Timed = Current->Values[Current->Count - 1 - i];
        if (Timed.t >= LastPossible) {
          TotalValue += Timed.v;
          assert(End >= Timed.t);
          End = Timed.t;
        }
      }

      auto* Next = Current;
      for (auto* Block = Current->Prev; Block;
           Next = Block, Block = Next->Prev) {
        assert(Block->Count == std::size(Block->Values));

        auto& LastEvent = Block->Values[std::size(Block->Values) - 1];

        if (LastEvent.t < LastPossible) {
          Next->Prev = nullptr;
          AddToFreeList(Block);
          First = Next;
          break;
        }

        for (size_t i = 1; i <= std::size(Block->Values); ++i) {
          auto& Timed = Block->Values[std::size(Block->Values) - i];
          if (Timed.t >= LastPossible) {
            TotalValue += Timed.v;
            assert(End >= Timed.t);
            End = Timed.t;
          }
        }
      }

      auto Diff = duration_cast<std::chrono::seconds>(Start - End);
      auto SecondCount = Diff.count();
      if (SecondCount == 0) { SecondCount = 1; }

      cached = static_cast<double>(TotalValue) / SecondCount;
    }

    return cached.value();
  }

  std::pair<std::size_t, std::size_t> block_count() const
  {
    std::size_t UsedCount{0}, FreeCount{0};
    for (auto* Current = Last; Current; Current = Current->Prev) {
      UsedCount += 1;
    }
    for (auto* Current = FreeList; Current; Current = Current->Prev) {
      FreeCount += 1;
    }

    return {UsedCount, FreeCount};
  }

  ~counter()
  {
    auto* Current = Last;
    while (Current) {
      auto* Prev = Current->Prev;
      delete Current;
      Current = Prev;

      if (!Current) {
        Current = FreeList;
        FreeList = nullptr;
      }
    }
  }

  counter(counter&&) = delete;
  counter(const counter&) = delete;
  counter& operator=(counter&&) = delete;
  counter& operator=(const counter&) = delete;

 private:
  void AddToFreeList(block* Block)
  {
    assert(Block);
    First->Prev = FreeList;
    FreeList = Block;
  }

  // how long should we store data
  Duration lookback;

  std::optional<double> cached{0.0};
  block *First{nullptr}, *Last{nullptr};
  block* FreeList{nullptr};
};


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
           && GetFileType(hndl) == FILE_TYPE_CHAR;
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
    (void)status;
    // if (!progress_bar) { return; }
    // progress_bar->bar.set_option(option::PostfixText(status));
  }

  void Info(std::string_view Message) override
  {
    if (progress_bar) {
      stream() << termcolor::reset;
      erase_line();
      stream().flush();
    }
    libbareos::println(stderr, "{}", Message);
    if (progress_bar) { progress_bar->print(); }
  }

  struct progress_bar {
    std::size_t goal{0};
    std::size_t pct{0};
    std::size_t current{0};

    ProgressBar bar;

    progress_bar() = default;
    progress_bar(std::size_t goal_)
        : goal{goal_}
        , pct{goal_ / 100}
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
      last_update = std::chrono::steady_clock::now();
      show_cursor(false);
      cursor_hidden = true;
    }
    progress_bar(progress_bar const&) = delete;
    progress_bar(progress_bar&&) = delete;
    progress_bar& operator=(progress_bar const&) = delete;
    progress_bar& operator=(progress_bar&&) = delete;

    std::string speed_to_text(double bytes_per_sec)
    {
      static constexpr std::pair<double, std::string_view> breakpoints[] = {
          {1 << 30, "GiB"},
          {1 << 20, "MiB"},
          {1 << 10, "KiB"},
      };

      std::string_view unit = "B";
      double speed = bytes_per_sec;

      for (auto& b : breakpoints) {
        if (bytes_per_sec > b.first) {
          speed = bytes_per_sec / b.first;
          unit = b.second;
          break;
        }
      }

      return libbareos::format("{:.2f} {}/s", speed, unit);
    }

    void progress(std::size_t amount)
    {
      auto this_update = std::chrono::steady_clock::now();
      current += amount;

      throughput.add_data_point(this_update, amount);

      if (bar.is_completed()) { return; }

      if (current != goal) {
        if (this_update - last_update < std::chrono::seconds(1)) { return; }
        last_update = this_update;
        bar.set_option(option::PostfixText(
            speed_to_text(throughput.compute_average_per_sec())));
      }

      bar.set_progress(current);
    }

    void print()
    {
      if (!bar.is_completed()) { bar.print_progress(); }
    }

    ~progress_bar()
    {
      bar.set_option(option::PostfixText("Done"));
      progress(goal - current);

      if (!bar.is_completed()) { bar.mark_as_completed(); }
      if (cursor_hidden) { show_cursor(true); }
    }

    bool cursor_hidden{};
    std::chrono::steady_clock::time_point last_update;
    counter throughput{std::chrono::minutes(1)};
  };


  logger(bool trace) : GenericLogger{trace}
  {
    // indicators::show_console_cursor(false);
  }
  ~logger()
  {
    // indicators::show_console_cursor(true);
  }
  std::optional<progress_bar> progress_bar;
};

GenericLogger* get(bool trace)
{
  static logger Instance{trace};
  return std::addressof(Instance);
}
};  // namespace progressbar
