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

#ifndef BAREOS_LIB_BSTRINGLIST_H_
#define BAREOS_LIB_BSTRINGLIST_H_

#include "include/bareos.h"

class BStringList : public std::list<std::string>
{
public:
  BStringList();
  BStringList(const std::string &string_to_convert, char separator);
  BStringList &operator = (const BStringList &rhs);
  BStringList(const BStringList &other);
  BStringList &operator << (const std::string &rhs);
  BStringList &operator << (const int &rhs);
  BStringList &operator << (const char *rhs);
  std::string Join(char separator) const;
  std::string Join() const;
  void Append(std::vector<std::string> vec);
  void Append(char character);
  void Append(const char *str);

private:
  std::string Join(const char *separator) const;
};

#endif /* BAREOS_LIB_BSTRINGLIST_H_ */
