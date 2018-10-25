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

BStringList::BStringList() : std::list<std::string>() { return; }

BStringList::BStringList(const std::string &string_to_split, char separator)
  : std::list<std::string>()
{
  std::stringstream ss(string_to_split);
  std::string token;
  while (std::getline(ss, token, separator)) {
    push_back(token);
  }
}

BStringList::BStringList(const BStringList &other)
  : std::list<std::string>()
{
  *this = other;
}

BStringList& BStringList::operator=(const BStringList &rhs)
{
  std::list<std::string>::const_iterator it = rhs.cbegin();
  while (it != rhs.cend()) {
    push_back(*it++);
  }
  return *this;
}

BStringList& BStringList::operator << (const std::string &rhs)
{
  push_back(rhs);
  return *this;
}

BStringList& BStringList::operator << (const int &rhs)
{
  push_back(std::to_string(rhs));
  return *this;
}

BStringList& BStringList::operator << (const std::list<std::string> &rhs)
{
  Append(rhs);
  return *this;
}

BStringList& BStringList::operator << (const char *rhs)
{
  emplace_back(rhs);
  return *this;
}

void BStringList::Append(const std::list<std::string> &vec)
{
  for (auto str : vec) {
    push_back(str);
  }
}

void BStringList::Append(char character)
{
  push_back(std::string(1,character));
}

void BStringList::Append(const char *str)
{
  emplace_back(str);
}

std::string BStringList::Join(char separator) const
{
  return Join(&separator);
}

std::string BStringList::Join() const
{
  return Join(nullptr);
}

std::string BStringList::Join(const char *separator) const
{
  std::list<std::string>::const_iterator it = cbegin();
  std::string output;

  while (it != cend()) {
    output += *it++;
    if (separator) {
      if (it != cend()) {
        output += *separator;
      }
    }
  }
  return output;
}
