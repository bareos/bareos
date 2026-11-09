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

#include "include/bareos.h"
#include "dird/ua_run_helpers.h"

#include <algorithm>

namespace directordaemon {

std::string FitRestoreOptionText(std::string_view text, size_t width)
{
  if (text.size() <= width) { return std::string(text); }
  if (width <= 3) { return std::string(text.substr(0, width)); }
  std::string result(text.substr(0, width - 3));
  result += "...";
  return result;
}

std::string ScrollRestoreOptionText(std::string_view text,
                                    size_t width,
                                    size_t offset)
{
  if (text.size() <= width) { return std::string(text); }
  if (width <= 2) { return std::string(text.substr(0, width)); }

  size_t marker_width = 0;
  if (offset > 0) { marker_width++; }
  if (offset + width - marker_width < text.size()) { marker_width++; }

  if (marker_width >= width) { return std::string(width, '.'); }

  size_t text_width = width - marker_width;
  size_t max_offset = text.size() > text_width ? text.size() - text_width : 0;
  offset = std::min(offset, max_offset);

  std::string result;
  if (offset > 0) { result += '<'; }
  result += text.substr(offset, text_width);
  if (offset + text_width < text.size()) { result += '>'; }
  return result;
}

std::string FormatRunScheduleRelativeTime(time_t when, time_t now)
{
  struct Unit {
    time_t seconds;
    const char* singular;
  };
  static const Unit units[] = {
      {31536000, NT_("year")}, {2592000, NT_("month")}, {604800, NT_("week")},
      {86400, NT_("day")},     {3600, NT_("hour")},     {60, NT_("minute")},
  };

  time_t difference = when >= now ? when - now : now - when;
  if (difference < 60) { return T_("now"); }

  for (const auto& unit : units) {
    if (difference >= unit.seconds) {
      time_t count = difference / unit.seconds;
      std::string text = std::to_string(count);
      text += " ";
      text += T_(unit.singular);
      if (count != 1) { text += "s"; }
      return when >= now ? T_("in ") + text : text + T_(" ago");
    }
  }

  return T_("now");
}

bool IsListingTimestamp(std::string_view text, size_t pos)
{
  if (pos + 19 > text.size()) { return false; }
  const auto digit = [](char ch) { return ch >= '0' && ch <= '9'; };
  return digit(text[pos]) && digit(text[pos + 1]) && digit(text[pos + 2])
         && digit(text[pos + 3]) && text[pos + 4] == '-' && digit(text[pos + 5])
         && digit(text[pos + 6]) && text[pos + 7] == '-' && digit(text[pos + 8])
         && digit(text[pos + 9]) && text[pos + 10] == ' '
         && digit(text[pos + 11]) && digit(text[pos + 12])
         && text[pos + 13] == ':' && digit(text[pos + 14])
         && digit(text[pos + 15]) && text[pos + 16] == ':'
         && digit(text[pos + 17]) && digit(text[pos + 18]);
}

bool ParseEstimateListingLine(std::string_view line,
                              std::string* path,
                              bool* directory)
{
  if (line.empty()) { return false; }
  switch (line.front()) {
    case 'd':
      *directory = true;
      break;
    case '-':
    case 'l':
    case 'b':
    case 'c':
    case 'f':
    case 's':
      *directory = false;
      break;
    default:
      return false;
  }

  size_t timestamp = std::string_view::npos;
  for (size_t i = 0; i + 19 <= line.size(); ++i) {
    if (IsListingTimestamp(line, i)) {
      timestamp = i;
      break;
    }
  }
  if (timestamp == std::string_view::npos) { return false; }

  size_t filename = timestamp + 19;
  while (filename < line.size() && line[filename] == ' ') { ++filename; }
  if (filename >= line.size()) { return false; }

  std::string parsed(line.substr(filename));
  size_t link_separator = parsed.find(" -> ");
  if (link_separator != std::string::npos) { parsed.resize(link_separator); }
  *path = std::move(parsed);
  return !path->empty();
}

std::string NormalizeBrowsePath(std::string_view path)
{
  std::string normalized(path);
  if (normalized.empty()) { return "/"; }
  while (normalized.size() > 1 && IsPathSeparator(normalized.back())) {
    normalized.pop_back();
  }
  return normalized.empty() ? "/" : normalized;
}

std::string ParentBrowsePath(std::string_view path)
{
  std::string normalized = NormalizeBrowsePath(path);
  if (normalized == "/") { return normalized; }
  size_t separator = normalized.find_last_of("/\\");
  if (separator == std::string::npos || separator == 0) { return "/"; }
  return normalized.substr(0, separator);
}

std::string BrowsePathBasename(std::string_view path)
{
  std::string normalized = NormalizeBrowsePath(path);
  if (normalized == "/") { return normalized; }
  size_t separator = normalized.find_last_of("/\\");
  if (separator == std::string::npos) { return normalized; }
  return normalized.substr(separator + 1);
}

bool IsImmediateBrowseChild(std::string_view parent, std::string_view child)
{
  std::string normalized_parent = NormalizeBrowsePath(parent);
  if (child == normalized_parent) { return false; }
  if (normalized_parent == "/") {
    if (child.empty() || child.front() != '/') { return false; }
    std::string_view remainder = child.substr(1);
    return !remainder.empty()
           && remainder.find_first_of("/\\") == std::string_view::npos;
  }

  if (child.size() <= normalized_parent.size()
      || child.substr(0, normalized_parent.size()) != normalized_parent
      || !IsPathSeparator(child[normalized_parent.size()])) {
    return false;
  }
  std::string_view remainder = child.substr(normalized_parent.size() + 1);
  return !remainder.empty()
         && remainder.find_first_of("/\\") == std::string_view::npos;
}

void RemoveLastBrowseSearchCharacter(std::string* text)
{
  if (text->empty()) { return; }
  size_t pos = text->size() - 1;
  while (pos > 0 && (static_cast<unsigned char>((*text)[pos]) & 0xc0) == 0x80) {
    --pos;
  }
  text->resize(pos);
}

bool ContainsBrowseSearchMatch(std::string_view text, std::string_view search)
{
  return search.empty() || text.find(search) != std::string_view::npos;
}

}  // namespace directordaemon
