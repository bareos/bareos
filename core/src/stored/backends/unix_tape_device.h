/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
/*
 * UNIX API device abstraction.
 *
 * Marco van Wieringen, December 2013
 */

#ifndef BAREOS_STORED_BACKENDS_UNIX_TAPE_DEVICE_H_
#define BAREOS_STORED_BACKENDS_UNIX_TAPE_DEVICE_H_

namespace storagedaemon {

class unix_tape_device : public generic_tape_device {
 public:
  unix_tape_device();
  ~unix_tape_device() { close(nullptr); };

  int d_ioctl(int fd, ioctl_req_t request, char* op) override;
};

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BACKENDS_UNIX_TAPE_DEVICE_H_
