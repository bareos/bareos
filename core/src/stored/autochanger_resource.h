/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_AUTOCHANGER_RESOURCE_H_
#define BAREOS_STORED_AUTOCHANGER_RESOURCE_H_

#include "lib/bareos_resource.h"

class alist;

namespace storagedaemon {

class AutochangerResource : public BareosResource {
 public:
  AutochangerResource();
  virtual ~AutochangerResource() = default;
  AutochangerResource& operator=(const AutochangerResource& rhs);
  bool PrintConfigToBuffer(PoolMem& buf);

  alist* device;          /**< List of DeviceResource device pointers */
  char* changer_name;     /**< Changer device name */
  char* changer_command;  /**< Changer command  -- external program */
  brwlock_t changer_lock; /**< One changer operation at a time */
};
} /* namespace storagedaemon */

#endif /* BAREOS_STORED_AUTOCHANGER_RESOURCE_H_ */
