/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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

#ifndef BAREOS_INCLUDE_RELEASE_NUMBERS_H_
#define BAREOS_INCLUDE_RELEASE_NUMBERS_H_

#include "include/bc_types.h"

enum class BareosVersionNumber : uint32_t
{
  kRelease_18_2 = static_cast<uint32_t>(1802),
  kUndefined    = static_cast<uint32_t>(1)
};

class BareosVersionToMajorMinor
{
public:
  uint32_t major = 0;
  uint32_t minor = 0;
  BareosVersionToMajorMinor(BareosVersionNumber version) {
    uint32_t version_number = static_cast<uint32_t>(version);
    major = version_number / 100;
    minor = version_number % 100;
  }
};

#endif /* BAREOS_INCLUDE_RELEASE_NUMBERS_H_ */
