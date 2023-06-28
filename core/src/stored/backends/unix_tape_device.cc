/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2013 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Marco van Wieringen, December 2013
 *
 * UNIX Tape API device abstraction.
 *
 * Stacking is the following:
 *
 *   unix_tape_device::
 *         |
 *         v
 * generic_tape_device::
 *         |
 *         v
 *      Device::
 *
 */
/**
 * @file
 * UNIX Tape API device abstraction.
 */
#include <algorithm>
#include <vector>

#include <sys/ioctl.h>
#include <unistd.h>

#include "include/bareos.h"
#include "stored/stored.h"
#include "stored/sd_backends.h"
#include "generic_tape_device.h"
#include "unix_tape_device.h"

namespace {
static constexpr size_t size_increment = 1024 * 1024;
static constexpr size_t max_size_increment = 16 * size_increment;
static constexpr size_t next_size_increment(size_t count)
{
  if (count < size_increment) {
    return size_increment;
  } else {
    return (count / size_increment) * 2 * size_increment;
  }
}
}  // namespace

namespace storagedaemon {

int unix_tape_device::d_ioctl(int fd, ioctl_req_t request, char* op)
{
  return ::ioctl(fd, request, op);
}

unix_tape_device::unix_tape_device()
{
  SetCap(CAP_ADJWRITESIZE); /* Adjust write size to min/max */
}

ssize_t unix_tape_device::d_read(int fd, void* buffer, size_t count)
{
  ssize_t ret = ::read(fd, buffer, count);
  /* when the driver fails to `read()` with `ENOMEM`, then the provided buffer
   * was too small by re-reading with a temporary buffer that is enlarged
   * step-by-step, we can read the block and returns its first `count` bytes.
   * This allows the calling code to read the block header and resize its buffer
   * according to the size recorded in that header.
   */
  for (size_t bufsize = next_size_increment(count);
       ret == -1 && errno == ENOMEM && bufsize <= max_size_increment;
       bufsize = next_size_increment(bufsize)) {
    std::vector<char> tmpbuf(bufsize);
    bsr(1);       // go back one block so we can re-read
    block_num++;  // re-increment the block counter that bsr() just incremented
    if (auto tmpret = ::read(fd, tmpbuf.data(), tmpbuf.size()); tmpret != -1) {
      memcpy(buffer, tmpbuf.data(), count);
      ret = std::min(tmpret, static_cast<ssize_t>(count));
    }
  }
  return ret;
}

REGISTER_SD_BACKEND(tape, unix_tape_device)

} /* namespace storagedaemon  */
