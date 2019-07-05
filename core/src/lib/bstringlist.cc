/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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

#include "bstringlist.h"

#include <sstream>
#include <algorithm>
#include <iterator>

BStringList::BStringList() : std::vector<std::string>() { return; }

BStringList::BStringList(const std::string& string_to_split,
                         std::string string_separator)
    : std::vector<std::string>()
{
  std::size_t find_pos = 0;
  std::size_t start_pos = 0;

  do {
    find_pos = string_to_split.find(string_separator, start_pos);
    std::string temp;
    temp.assign(string_to_split, start_pos, find_pos - start_pos);
    push_back(temp);
    start_pos = find_pos + string_separator.size();
  } while (find_pos != std::string::npos);
}

BStringList::BStringList(const std::string& string_to_split, char separator)
    : std::vector<std::string>()
{
  std::stringstream ss(string_to_split);
  std::string token;
  while (std::getline(ss, token, separator)) { push_back(token); }
}

BStringList::BStringList(const BStringList& other) : std::vector<std::string>()
{
  *this = other;
}

BStringList& BStringList::operator=(const BStringList& rhs)
{
  std::vector<std::string>::const_iterator it = rhs.cbegin();
  while (it != rhs.cend()) { push_back(*it++); }
  return *this;
}

BStringList& BStringList::operator<<(const std::string& rhs)
{
  push_back(rhs);
  return *this;
}

BStringList& BStringList::operator<<(const int& rhs)
{
  push_back(std::to_string(rhs));
  return *this;
}

BStringList& BStringList::operator<<(const std::vector<std::string>& rhs)
{
  Append(rhs);
  return *this;
}

BStringList& BStringList::operator<<(const char* rhs)
{
  emplace_back(rhs);
  return *this;
}

void BStringList::Append(const std::vector<std::string>& vec)
{
  for (auto str : vec) { push_back(str); }
}

void BStringList::Append(char character)
{
  push_back(std::string(1, character));
}

void BStringList::Append(const char* str) { emplace_back(str); }

void BStringList::PopFront()
{
  if (size() >= 1) { erase(begin()); }
}

std::string BStringList::Join(char separator) const { return Join(&separator); }

std::string BStringList::Join() const { return Join(nullptr); }

std::string BStringList::JoinReadable() const { return Join(' '); }

std::string BStringList::Join(const char* separator) const
{
  std::vector<std::string>::const_iterator it = cbegin();
  std::string output;

  while (it != cend()) {
    output += *it++;
    if (separator) {
      if (it != cend()) { output += *separator; }
    }
  }
  return output;
}
