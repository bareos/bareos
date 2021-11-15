/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2021 Bareos GmbH & Co. KG

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
 * Gluster Filesystem API device abstraction.
 *
 * Marco van Wieringen, February 2014
 */

#ifndef BAREOS_STORED_BACKENDS_GFAPI_DEVICE_H_
#define BAREOS_STORED_BACKENDS_GFAPI_DEVICE_H_

#include <glusterfs/api/glfs.h>

#if defined GLFS_FTRUNCATE_HAS_FOUR_ARGS
#  define glfs_ftruncate(fd, offset) glfs_ftruncate(fd, offset, NULL, NULL)
#endif

namespace storagedaemon {

class gfapi_device : public Device {
 private:
  char* gfapi_configstring_;
  char* gfapi_uri_;
  char* gfapi_logfile_{nullptr};
  int gfapi_loglevel_{0};
  char* transport_;
  char* servername_;
  char* volumename_;
  char* basedir_;
  int serverport_;
  glfs_t* glfs_;
  glfs_fd_t* gfd_;
  POOLMEM* virtual_filename_;

 public:
  gfapi_device();
  ~gfapi_device();

  // Interface from Device
  int d_close(int) override;
  int d_open(const char* pathname, int flags, int mode) override;
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  boffset_t d_lseek(DeviceControlRecord* dcr,
                    boffset_t offset,
                    int whence) override;
  ssize_t d_read(int fd, void* buffer, size_t count) override;
  ssize_t d_write(int fd, const void* buffer, size_t count) override;
  bool d_truncate(DeviceControlRecord* dcr) override;
} override;
} /* namespace storagedaemon */
#endif  // BAREOS_STORED_BACKENDS_GFAPI_DEVICE_H_
