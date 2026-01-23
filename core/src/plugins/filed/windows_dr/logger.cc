/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2026 Bareos GmbH & Co. KG

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
#include <optional>
#include <iostream>
#include <algorithm>

#include "format.h"

namespace progressbar {
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

namespace terminal {
#ifdef HAVE_WIN32
using terminal_handle = HANDLE;

bool check_if_terminal_handle(terminal_handle hndl)
{
  return hndl != INVALID_HANDLE_VALUE && hndl != NULL
         && GetFileType(hndl) == FILE_TYPE_CHAR;
}

std::size_t terminal_width(terminal_handle handle)
{
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(handle, &csbi);
  return csbi.srWindow.Right - csbi.srWindow.Left + 1;
}

bool terminal_write(terminal_handle handle, std::span<const char> bytes)
{
  size_t bytes_written = 0;

  while (bytes_written < bytes.size()) {
    DWORD new_bytes_written = 0;
    DWORD bytes_to_write
        = std::min(bytes.size() - bytes_written,
                   static_cast<size_t>(std::numeric_limits<DWORD>::max()));

    if (!WriteFile(handle, bytes.data() + bytes_written, bytes_to_write,
                   &new_bytes_written, NULL)) {
      return false;
    }


    bytes_written += new_bytes_written;
  }

  return true;
}

void terminal_hide_cursor(terminal_handle handle)
{
  CONSOLE_CURSOR_INFO cursorInfo;

  GetConsoleCursorInfo(handle, &cursorInfo);
  cursorInfo.bVisible = true;
  SetConsoleCursorInfo(handle, &cursorInfo);
}

void terminal_show_cursor(terminal_handle handle)
{
  CONSOLE_CURSOR_INFO cursorInfo;

  GetConsoleCursorInfo(handle, &cursorInfo);
  cursorInfo.bVisible = true;
  SetConsoleCursorInfo(handle, &cursorInfo);
}

void terminal_flush(terminal_handle handle) { FlushFileBuffers(handle); }
#else
using terminal_handle = int;

#  include <asm/termbits.h>
#  include <sys/ioctl.h>
#  include <unistd.h>

bool check_if_terminal_handle(terminal_handle handle) { return isatty(handle); }
std::size_t terminal_width(terminal_handle handle)
{
  struct winsize size{};
  int ret = ioctl(handle, TIOCGWINSZ, &size);
  if (ret < 0) {
    throw std::runtime_error{libbareos::format(
        "could not determine terminal size: {}", strerror(errno))};
  }
  // if the terminal reports a size of 0, just treat it as 80 instead
  return size.ws_col ? size.ws_col : 80;
}

bool terminal_write(terminal_handle handle, std::span<const char> bytes)
{
  size_t bytes_written = 0;

  size_t zero_write_count = 0;

  while (bytes_written < bytes.size()) {
    ssize_t new_bytes_written = write(handle, bytes.data() + bytes_written,
                                      bytes.size() - bytes_written);

    if (new_bytes_written < 0) { return false; }

    if (new_bytes_written == 0) {
      zero_write_count += 1;

      if (zero_write_count > 5) { return false; }
    }

    bytes_written += new_bytes_written;
  }
  return true;
}

void terminal_hide_cursor(terminal_handle handle)
{
  terminal_write(handle, "\033[?25l");
}

void terminal_show_cursor(terminal_handle handle)
{
  terminal_write(handle, "\033[?25h");
}

void terminal_flush(terminal_handle handle)
{
  static_cast<void>(fdatasync(handle));
}
#endif

struct terminal {
  void flush() { terminal_flush(m_handle); }

  bool write(std::span<const char> bytes)
  {
    return terminal_write(m_handle, bytes);
  }

  size_t width() { return terminal_width(m_handle); }

  void hide_cursor() { terminal_hide_cursor(m_handle); }

  void show_cursor() { terminal_show_cursor(m_handle); }

  static std::optional<terminal> wrap(terminal_handle hndl)
  {
    if (!check_if_terminal_handle(hndl)) { return std::nullopt; }

    return terminal{hndl};
  }


 private:
  terminal(terminal_handle handle) : m_handle{std::move(handle)} {}

  terminal_handle m_handle{};
};
};  // namespace terminal

namespace progressbar {

std::string format_h_m_s(std::chrono::nanoseconds dur)
{
  auto hours = duration_cast<std::chrono::hours>(dur);
  auto minutes = duration_cast<std::chrono::minutes>(dur - hours);
  auto seconds = duration_cast<std::chrono::seconds>(dur - hours - minutes);

  return libbareos::format("{:02}:{:02}:{:02}", hours.count(), minutes.count(),
                           seconds.count());
}


std::string format_time(std::chrono::nanoseconds elapsed,
                        std::size_t done,
                        std::size_t total)
{
  if (done != 0) {
    double ratio = static_cast<double>(total) / done - 1.0;
    auto leftover
        = std::chrono::duration_cast<std::chrono::nanoseconds>(ratio * elapsed);

    return libbareos::format("[{}/{}]", format_h_m_s(elapsed),
                             format_h_m_s(leftover));
  } else {
    return libbareos::format("[{}/--:--:--]", format_h_m_s(elapsed));
  }
}

void format(std::string& buffer,
            std::chrono::nanoseconds nanos,
            std::size_t current,
            std::size_t max,
            std::string_view suffix) noexcept
{
  // we assume here that prefix/suffix is simple ascii text taking up
  // one glyph per character

  size_t total_size = buffer.size();
  buffer.clear();

  auto output = [&](std::string_view view) {
    if (buffer.size() + view.size() > total_size) {
      throw std::runtime_error{libbareos::format(
          "{} + {} > {}\n", buffer.size(), view.size(), total_size)};
    }
    buffer.append(view.data(), view.size());
  };

  auto actual_suffix = format_time(nanos, current, max);

  actual_suffix.append(" ");
  actual_suffix.append(suffix);

  using namespace std::literals::string_view_literals;

  auto pct_done = max == 0
                      ? 0.0
                      : static_cast<double>(current) / static_cast<double>(max);

  std::string prefix;

  if (max > 0) {
    prefix = libbareos::format("{:3}%", static_cast<size_t>(100 * pct_done));
  } else {
    prefix = libbareos::format("---%");
  }
  size_t non_bar_size = prefix.size() + actual_suffix.size();

  if (total_size < non_bar_size) {
    output(prefix);
    output(" ");
    output(actual_suffix);
    return;
  }

  size_t bar_size = total_size - non_bar_size;

  auto decorator_size = " [] "sv.size();
  if (bar_size < decorator_size) {
    output(prefix);
    output(" ");
    output(actual_suffix);
    return;
  }

  size_t step_count = bar_size - decorator_size;

  std::size_t finished_steps = pct_done * step_count;

  output(prefix);
  output(" [");
  size_t current_step = 0;
  for (; current_step < finished_steps; ++current_step) { output("="); }
  if (current_step != step_count) {
    output(">");
    current_step += 1;
  }
  for (; current_step < step_count; ++current_step) { output(" "); }

  output("] ");
  output(actual_suffix);
}
};  // namespace progressbar

struct logger : public GenericLogger {
  enum Destination
  {
    StdOut,
    StdErr,
  };
  static constexpr Destination Current = Destination::StdErr;

  static terminal::terminal_handle handle()
  {
#if defined(HAVE_WIN32)
    if constexpr (Current == Destination::StdOut) {
      return GetStdHandle(STD_OUTPUT_HANDLE);
    } else if constexpr (Current == Destination::StdErr) {
      return GetStdHandle(STD_ERROR_HANDLE);
    }
#else
    if constexpr (Current == Destination::StdOut) {
      return STDOUT_FILENO;
    } else if constexpr (Current == Destination::StdErr) {
      return STDERR_FILENO;
    }
#endif
  }

  void Begin(std::size_t FileSize) override
  {
    if (auto term = terminal::terminal::wrap(handle())) {
      progress_bar.emplace(FileSize, *term);
    }
  }

  void Progressed(std::size_t Amount) override
  {
    if (progress_bar) { progress_bar->progress(Amount); }
  }

  void End() override
  {
    if (!progress_bar) { return; }

    progress_bar->finish();
    progress_bar.reset();
  }

  void SetStatus(std::string_view status) override
  {
    Trace("switching status to '{}'", status);
  }

  void Output(Message message) override
  {
    if (progress_bar) { progress_bar->erase_bar(); }
    std::cerr << libbareos::format("{:{}}{}\n", "", indent, message.text);
    if (progress_bar) { progress_bar->print(); }
  }

  struct progress_bar {
    std::size_t goal{0};
    std::size_t pct{0};
    std::size_t current{0};
    terminal::terminal term;

    progress_bar(std::size_t goal_, terminal::terminal t)
        : goal{goal_}, pct{goal_ / 100}, term{std::move(t)}
    {
      start_time = last_update = std::chrono::steady_clock::now();
      t.hide_cursor();
      cursor_hidden = true;
    }
    progress_bar(progress_bar const&) = delete;
    progress_bar(progress_bar&&) = delete;
    progress_bar& operator=(progress_bar const&) = delete;
    progress_bar& operator=(progress_bar&&) = delete;

    static std::string format_size(double size)
    {
      static constexpr std::pair<double, std::string_view> breakpoints[] = {
          {0x1p60, "Ei"}, {0x1p50, "Pi"}, {0x1p40, "Ti"},
          {0x1p30, "Gi"}, {0x1p20, "Mi"}, {0x1p10, "Ki"},
      };

      auto found
          = std::find_if(std::begin(breakpoints), std::end(breakpoints),
                         [&size](auto pair) { return pair.first < size; });

      if (found != std::end(breakpoints)) {
        return libbareos::format("{:.2f}{}B", size / found->first,
                                 found->second);
      } else {
        return libbareos::format("{:.2f}B", size);
      }
    }

    static std::string format_speed(double bytes_per_sec)
    {
      auto result = format_size(bytes_per_sec);

      result += "/s";

      return result;
    }

    void progress(std::size_t amount)
    {
      if (current >= goal) { return; }

      auto this_update = std::chrono::steady_clock::now();
      current += amount;

      throughput.add_data_point(this_update, amount);

      if (current != goal) {
        if (this_update - last_update < std::chrono::seconds(1)) { return; }
        last_update = this_update;
        suffix = format_size(current) + " "
                 + format_speed(throughput.compute_average_per_sec());
        erase_bar();
        print();
      }
    }

    std::string suffix;
    std::string buffer;

    void print()
    {
      buffer.resize(term.width());
      auto time_elapsed = last_update - start_time;
      progressbar::format(buffer, time_elapsed, current, goal, suffix);
      term.write(buffer);
      term.flush();
    }

    void erase_bar()
    {
      buffer.resize(buffer.size() + 2);
      buffer[0] = '\r';
      for (size_t i = 1; i < buffer.size() - 1; ++i) { buffer[i] = ' '; }
      buffer[buffer.size() - 1] = '\r';
      term.write(buffer);
    }

    void finish()
    {
      erase_bar();
      suffix = "Done";
      auto time_elapsed = last_update - start_time;

      if (time_elapsed.count() == 0) {
        suffix = format_size(current) + " ---B/s";
      } else {
        double seconds = static_cast<double>(time_elapsed.count())
                         / std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::seconds(1))
                               .count();

        auto speed = current / seconds;
        suffix = format_size(current) + " " + format_speed(speed);
      }


      print();
      term.write("\n");
    }

    ~progress_bar()
    {
      if (cursor_hidden) { term.show_cursor(); }
    }

    bool cursor_hidden{};
    std::chrono::steady_clock::time_point last_update;
    std::chrono::steady_clock::time_point start_time;
    counter throughput{std::chrono::minutes(1)};
  };


  logger(bool trace) : GenericLogger{trace} {}
  std::optional<progress_bar> progress_bar;
};

GenericLogger* get(bool trace)
{
  static logger Instance{trace};
  return std::addressof(Instance);
}
};  // namespace progressbar
