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

#include "dird/ua_visual_busy.h"

#include "include/bareos.h"
#include "dird.h"
#include "dird/ua_tree_browser_internal.h"
#include "lib/bnet.h"

#include <array>
#include <utility>

namespace directordaemon {

namespace visual_busy_internal {

using tree_browser_internal::FitText;
using tree_browser_internal::FrameBorderStyle;
using tree_browser_internal::kFrameColor;
using tree_browser_internal::kFrameHelpColor;
using tree_browser_internal::kFrameHighlightColor;
using tree_browser_internal::kFrameResetColor;
using tree_browser_internal::kFrameVerticalBorder;
using tree_browser_internal::RenderFrameBorder;
using tree_browser_internal::StyleFrameContent;

namespace {

constexpr std::array<char, 4> kSpinnerFrames = {'|', '/', '-', '\\'};

std::string FrameBorder(size_t width,
                        bool color,
                        FrameBorderStyle style,
                        std::string_view title = {})
{
  std::string line = RenderFrameBorder(width, style, title);
  if (!color) { return line + "\n"; }
  return std::string(kFrameColor) + line + std::string(kFrameResetColor) + "\n";
}

std::string FrameLine(size_t width, std::string_view text, bool color)
{
  if (width < 2) { return FitText(text, width) + "\n"; }

  std::string content = FitText(text, width - 2);
  content = StyleFrameContent(std::move(content), ' ', false, color);
  std::string border = color ? std::string(kFrameColor)
                                   + std::string(kFrameVerticalBorder)
                                   + std::string(kFrameResetColor)
                             : std::string(kFrameVerticalBorder);
  return border + content + border + "\n";
}

std::string StyledLine(size_t width,
                       std::string_view text,
                       std::string_view style,
                       bool color)
{
  std::string line = FitText(text, width);
  return color
             ? std::string(style) + line + std::string(kFrameResetColor) + "\n"
             : line + "\n";
}

}  // namespace

char BusySpinner::Start(Clock::time_point now)
{
  frame_index_ = 1;
  next_refresh_ = now + kRefreshInterval;
  return kSpinnerFrames.front();
}

std::optional<char> BusySpinner::Tick(Clock::time_point now)
{
  if (now < next_refresh_) { return std::nullopt; }

  char frame = kSpinnerFrames[frame_index_ % kSpinnerFrames.size()];
  frame_index_++;
  next_refresh_ = now + kRefreshInterval;
  return frame;
}

std::string RenderBusyScreen(int width,
                             int height,
                             bool color,
                             std::string_view title,
                             std::string_view message,
                             std::string_view detail,
                             char spinner)
{
  constexpr size_t kDefaultWidth = 80;
  constexpr size_t kDefaultContentRows = 20;
  constexpr size_t kChromeRows = 4;

  size_t screen_width = width > 0
                            ? std::max(size_t{2}, static_cast<size_t>(width))
                            : kDefaultWidth;
  size_t content_rows = height > static_cast<int>(kChromeRows)
                            ? static_cast<size_t>(height) - kChromeRows
                        : height > 0 ? 3
                                     : kDefaultContentRows;

  std::string screen
      = FrameBorder(screen_width, color, FrameBorderStyle::kTop, title);
  screen += FrameLine(screen_width, detail, color);
  for (size_t row = 1; row < content_rows; ++row) {
    screen += FrameLine(screen_width, "", color);
  }
  screen += FrameBorder(screen_width, color, FrameBorderStyle::kBottom);
  screen += StyledLine(screen_width, " " + std::string(message) + " " + spinner,
                       kFrameHighlightColor, color);
  screen += StyledLine(screen_width, T_("Please wait; input is disabled"),
                       kFrameHelpColor, color);
  if (!screen.empty() && screen.back() == '\n') { screen.pop_back(); }
  return screen;
}

}  // namespace visual_busy_internal

VisualBusyIndicator::VisualBusyIndicator(UaContext* ua, Renderer renderer)
    : ua_(ua), renderer_(std::move(renderer))
{
}

void VisualBusyIndicator::Start()
{
  char frame = spinner_.Start(visual_busy_internal::BusySpinner::Clock::now());
  Redraw(frame);
}

void VisualBusyIndicator::Tick()
{
  auto frame = spinner_.Tick(visual_busy_internal::BusySpinner::Clock::now());
  if (frame) { Redraw(*frame); }
}

void VisualBusyIndicator::Redraw(char spinner)
{
  if (!ua_ || !ua_->UA_sock) { return; }

  std::string screen = renderer_(spinner);
  ua_->UA_sock->signal(BNET_START_SELECT);
  ua_->SendMsg("%s", screen.c_str());
  ua_->UA_sock->signal(BNET_END_SELECT);
}

}  // namespace directordaemon
