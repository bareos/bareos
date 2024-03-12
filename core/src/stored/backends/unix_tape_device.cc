/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2013 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

namespace storagedaemon {

static std::vector<std::size_t> bufsizes(std::size_t count)
{
  constexpr std::size_t mb = 1024 * 1024;
  std::vector<std::size_t> sizes{1 * mb, 2 * mb, 4 * mb, 8 * mb, 16 * mb};
  sizes.erase(sizes.begin(),
              std::upper_bound(sizes.begin(), sizes.end(), count));
  return sizes;
}

int unix_tape_device::d_ioctl(int t_fd, ioctl_req_t request, char* op)
{
  return ::ioctl(t_fd, request, op);
}

unix_tape_device::unix_tape_device()
{
  SetCap(CAP_ADJWRITESIZE); /* Adjust write size to min/max */
}

ssize_t unix_tape_device::d_read(int t_fd, void* buffer, size_t count)
{
  ssize_t ret = ::read(t_fd, buffer, count);
  /* If the driver fails to `read()` with `ENOMEM`, then the provided buffer
   * was too small. By re-reading with a temporary buffer that is enlarged
   * step-by-step, we can read the block and return the first `count` bytes.
   * This allows the calling code to read the block header and resize its buffer
   * according to the size recorded in that header.
   */
  if (ret == -1 && errno == ENOMEM && HasCap(CAP_BSR)) {
    for (auto bufsize : bufsizes(count)) {
      // first go back one block so we can re-read
      if (!bsr(1)) {
        /* when backward-spacing fails for some reason, there's not much we
         * can do, so we just return the original ENOMEM and hope that the
         * caller knows the device is in a non well-defined state.
         */
        errno = ENOMEM;
        return -1;
      }
      block_num++;  // re-increment the block counter bsr() just decremented
      std::vector<char> tmpbuf(bufsize);
      if (auto tmpret = ::read(t_fd, tmpbuf.data(), tmpbuf.size());
          tmpret != -1) {
        memcpy(buffer, tmpbuf.data(), count);
        ret = std::min(tmpret, static_cast<ssize_t>(count));
        break;  // successful read
      } else if (errno != ENOMEM) {
        break;  // some other error occured
      }
    }
  }
  return ret;
}

REGISTER_SD_BACKEND(tape, unix_tape_device)

} /* namespace storagedaemon  */
