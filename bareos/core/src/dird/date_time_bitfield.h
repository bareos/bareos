/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_DATE_TIME_BITFIELD_H_
#define BAREOS_DIRD_DATE_TIME_BITFIELD_H_

#include "lib/bits.h"

namespace directordaemon {

struct DateTimeBitfield {
  char hour[NbytesForBits(24 + 1)]{0};
  char mday[NbytesForBits(31 + 1)]{0};
  char month[NbytesForBits(12 + 1)]{0};
  char wday[NbytesForBits(7 + 1)]{0};
  char wom[NbytesForBits(5 + 1)]{0};
  char woy[NbytesForBits(54 + 1)]{0};
  bool last_week_of_month{false};
};

}  // namespace directordaemon

#endif  // BAREOS_DIRD_DATE_TIME_BITFIELD_H_
