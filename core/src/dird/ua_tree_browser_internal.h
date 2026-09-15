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

#ifndef BAREOS_DIRD_UA_TREE_BROWSER_INTERNAL_H_
#define BAREOS_DIRD_UA_TREE_BROWSER_INTERNAL_H_

#include <cstddef>
#include <string>
#include <string_view>

namespace directordaemon::tree_browser_internal {

size_t TextCellWidth(std::string_view text);

std::string CaseFoldForSearch(std::string_view text);

void RemoveLastUtf8Character(std::string* text);

std::string FitText(std::string_view text,
                    size_t width,
                    size_t horizontal_offset = 0,
                    bool ellipsis = true);

constexpr size_t MaxHorizontalOffset(size_t text_width, size_t viewport_width)
{
  return text_width > viewport_width ? text_width - viewport_width : 0;
}

}  // namespace directordaemon::tree_browser_internal

#endif  // BAREOS_DIRD_UA_TREE_BROWSER_INTERNAL_H_
