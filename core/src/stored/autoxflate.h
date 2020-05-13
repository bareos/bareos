/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_STORED_AUTOXFLATE_H_
#define BAREOS_SRC_STORED_AUTOXFLATE_H_

namespace storagedaemon {

enum class AutoXflateMode : uint16_t
{
  IO_DIRECTION_NONE = 0,
  IO_DIRECTION_IN,
  IO_DIRECTION_OUT,
  IO_DIRECTION_INOUT
};

}  // namespace storagedaemon

#endif  // BAREOS_SRC_STORED_AUTOXFLATE_H_
