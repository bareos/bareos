/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2019 Bareos GmbH & Co. KG

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
 * CEPHFS API device abstraction.
 *
 * Marco van Wieringen, May 2014
 */

#ifndef BAREOS_STORED_BACKENDS_CEPHFS_DEVICE_H_
#define BAREOS_STORED_BACKENDS_CEPHFS_DEVICE_H_

#include <cephfs/libcephfs.h>

namespace storagedaemon {

class cephfs_device : public Device {
 private:
  char* cephfs_configstring_;
  char* cephfs_conffile_;
  char* basedir_;
  struct ceph_mount_info* cmount_;
  POOLMEM* virtual_filename_;

 public:
  cephfs_device();
  ~cephfs_device();

  /*
   * Interface from Device
   */
  int d_close(int) override;
  int d_open(const char* pathname, int flags, int mode) override;
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  boffset_t d_lseek(DeviceControlRecord* dcr,
                    boffset_t offset,
                    int whence) override;
  ssize_t d_read(int fd, void* buffer, size_t count) override;
  ssize_t d_write(int fd, const void* buffer, size_t count) override;
  bool d_truncate(DeviceControlRecord* dcr) override;
};

} /* namespace storagedaemon */

#endif /* BAREOS_STORED_BACKENDS_CEPHFS_DEVICE_H_ */
