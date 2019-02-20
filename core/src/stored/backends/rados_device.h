/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2016 Planets Communications B.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * CEPH (librados) API device abstraction.
 *
 * Marco van Wieringen, February 2014
 */

#ifndef BAREOS_STORED_BACKENDS_RADOS_DEVICE_H_
#define BAREOS_STORED_BACKENDS_RADOS_DEVICE_H_

#include <rados/librados.h>

#ifdef HAVE_RADOS_STRIPER
#include <radosstriper/libradosstriper.h>
#endif

namespace storagedaemon {

/*
 * Use for versions lower then 0.68.0 of the API the old format and otherwise
 * the new one.
 */
#if LIBRADOS_VERSION_CODE < 17408
#define DEFAULT_CLIENTID "admin"
#else
#define DEFAULT_CLUSTERNAME "ceph"
#define DEFAULT_USERNAME "client.admin"
#endif

class rados_device : public Device {
 private:
  /*
   * Private Members
   */
  char* rados_configstring_;
  char* rados_conffile_;
  char* rados_poolname_;
#if LIBRADOS_VERSION_CODE < 17408
  char* rados_clientid_;
#else
  char* rados_clustername_;
  char* rados_username_;
#endif
  bool cluster_initialized_;
#ifdef HAVE_RADOS_STRIPER
  bool stripe_volume_;
  uint64_t stripe_unit_;
  uint32_t stripe_count_;
  uint64_t object_size_;
#endif
  rados_t cluster_;
  rados_ioctx_t ctx_;
#ifdef HAVE_RADOS_STRIPER
  rados_striper_t striper_;
#endif
  boffset_t offset_;
  POOLMEM* virtual_filename_;

  /*
   * Private Methods
   */
  ssize_t ReadObjectData(boffset_t offset, char* buffer, size_t count);
  ssize_t WriteObjectData(boffset_t offset, char* buffer, size_t count);
#ifdef HAVE_RADOS_STRIPER
  ssize_t StriperVolumeSize();
#endif
  ssize_t VolumeSize();
#ifdef HAVE_RADOS_STRIPER
  bool TruncateStriperVolume(DeviceControlRecord* dcr);
#endif
  bool TruncateVolume(DeviceControlRecord* dcr);

 public:
  /*
   * Public Methods
   */
  rados_device();
  ~rados_device();

  /*
   * Interface from Device
   */
  int d_close(int);
  int d_open(const char* pathname, int flags, int mode);
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL);
  boffset_t d_lseek(DeviceControlRecord* dcr, boffset_t offset, int whence);
  ssize_t d_read(int fd, void* buffer, size_t count);
  ssize_t d_write(int fd, const void* buffer, size_t count);
  bool d_truncate(DeviceControlRecord* dcr);
};
} /* namespace storagedaemon */
#endif /* BAREOS_STORED_BACKENDS_RADOS_DEVICE_H_ */
