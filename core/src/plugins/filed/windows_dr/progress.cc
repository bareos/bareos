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

#include "indicators/setting.hpp"

#include <cstdint>
#include <indicators/progress_bar.hpp>
// #include <indicators/dynamic_progress.hpp>
#include <indicators/cursor_control.hpp>
#include <sstream>

#include <thread>

void input()
{
  std::this_thread::sleep_for(std::chrono::seconds(1));
  //(void)fgetc(stdin);
}

int main()
{
  using namespace indicators;
  input();

  ProgressBar bar{option::BarWidth{50},
                  option::Start{"["},
                  option::Fill{"="},
                  option::Lead{">"},
                  option::Remainder{" "},
                  option::End{"]"},
                  option::PostfixText{"Doing stuff"},
                  option::ForegroundColor{Color::green},
                  option::ShowPercentage{true},
                  option::Stream(TerminalHandle::StdErr),
                  option::FontStyles{std::vector<FontStyle>{FontStyle::bold}},
                  option::MaxProgress(100)};

  input();
  // std::cout << termcolor::reset;
  bar.set_progress(20);
  input();
  // std::cout << termcolor::reset;
  // erase_line();
  // std::cout << "Weird!" << std::endl;
  bar.set_progress(40);
  input();
  // std::cout << termcolor::reset;
  // erase_line();
  // std::cout << "Weird!" << std::endl;
  bar.set_progress(80);
  input();
  // std::cout << termcolor::reset;
  bar.set_progress(100);
  std::cout << "Weird!" << std::endl;
  input();
  // std::cout << termcolor::reset;
  if (!bar.is_completed()) bar.mark_as_completed();
  std::cout << "Weird!" << std::endl;

  return 0;
}
