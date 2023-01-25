/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_STORED_BACKENDS_AUTOCHANGER_TEST_DEVICE_H_
#define BAREOS_STORED_BACKENDS_AUTOCHANGER_TEST_DEVICE_H_

#include "stored/dev.h"
#include "unix_file_device.h"

namespace storagedaemon {

class autochanger_test_device : public unix_file_device {
 public:
  virtual bool IsTape() const override { return true; }
};

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BACKENDS_AUTOCHANGER_TEST_DEVICE_H_
