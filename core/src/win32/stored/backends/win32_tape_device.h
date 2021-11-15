/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2021 Bareos GmbH & Co. KG
   Copyright (C) 2013-2014 Planets Communications B.V.

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
 * Windows Tape API device abstraction.
 *
 * Marco van Wieringen, December 2013
 */

#ifndef BAREOS_WIN32_STORED_BACKENDS_WIN32_TAPE_DEVICE_H_
#define BAREOS_WIN32_STORED_BACKENDS_WIN32_TAPE_DEVICE_H_

namespace storagedaemon {

class win32_tape_device : public generic_tape_device {
 public:
  win32_tape_device();
  ~win32_tape_device() { close(nullptr); }

  int d_close(int) override;
  int d_open(const char* pathname, int flags, int mode) override;
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  ssize_t d_read(int fd, void* buffer, size_t count) override;
  ssize_t d_write(int fd, const void* buffer, size_t count) override;
  int TapeOp(struct mtop* mt_com);
  int TapeGet(struct mtget* mt_com);
  int TapePos(struct mtpos* mt_com);
};

} /* namespace storagedaemon */
#endif  // BAREOS_WIN32_STORED_BACKENDS_WIN32_TAPE_DEVICE_H_
