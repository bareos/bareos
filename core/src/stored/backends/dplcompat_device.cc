/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#include "include/bareos.h"

#include "stored/stored.h"
#include "stored/sd_backends.h"
#include "chunked_device.h"
#include "lib/edit.h"
#include "dplcompat_device.h"

#include <string>
#include <optional>
#include <fmt/format.h>
#include <gsl/gsl>

namespace {
std::string get_chunk_name(storagedaemon::chunk_io_request* request)
{
  return fmt::format(FMT_STRING("{:04d}"), request->chunk);
}
bool is_chunk_name(const std::string& name)
{
  if (name.length() != 4) { return false; }
  for (char c : name) {
    if (c < '0' || c > '9') { return false; }
  }
  return true;
}
}  // namespace

namespace storagedaemon {

bool DropletCompatibleDevice::CheckRemoteConnection()
{
  return m_storage.test_connection();
}

bool DropletCompatibleDevice::FlushRemoteChunk(chunk_io_request* request)
{
  auto inflight_lease = getInflightLease(request);
  if (!inflight_lease) { return false; }

  const std::string obj_name = request->volname;
  const std::string obj_chunk = get_chunk_name(request);
  auto retries = 3;
  do {
    Dmsg1(100, "Flushing chunk %s/%s\n", obj_name.c_str(), obj_chunk.c_str());

    /* Check on the remote backing store if the chunk already exists.
     * We only upload this chunk if it is bigger then the chunk that exists
     * on the remote backing store. When using io-threads it could happen
     * that there are multiple flush requests for the same chunk when a
     * chunk is reused in a next backup job. We only want the chunk with
     * the biggest amount of valid data to persist as we only append to
     * chunks. */
    auto obj_stat = m_storage.stat(obj_name, obj_chunk);

    if (obj_stat && obj_stat->size > request->wbuflen) {
      Dmsg1(100,
            "Not uploading chunk %s with size %d, as chunk with size %d is "
            "already present\n",
            obj_name.c_str(), obj_stat->size, request->wbuflen);
      return true;
    }
    // FIXME more error handling here!
    auto obj_data = gsl::span{request->buffer, request->wbuflen};
    if (m_storage.upload(obj_name, obj_chunk, obj_data)) { return true; }
  } while (retries-- > 0);
  return false;
}

// Internal method for reading a chunk from the remote backing store.
bool DropletCompatibleDevice::ReadRemoteChunk(chunk_io_request* request)
{
  const std::string obj_name = request->volname;
  const std::string obj_chunk = get_chunk_name(request);
  Dmsg1(100, "Reading chunk %s\n", obj_name.c_str());

  // check object metadata
  auto obj_stat = m_storage.stat(obj_name, obj_chunk);
  if (!obj_stat) {
    Mmsg1(errmsg, T_("Failed to open %s/%s doesn't exist\n"), obj_name.c_str(), obj_chunk.c_str());
    Dmsg1(100, "%s", errmsg);
    dev_errno = EIO;
    return false;
  } else if (obj_stat->size > request->wbuflen) {
    Mmsg3(errmsg,
          T_("Failed to read %s (%ld) to big to fit in chunksize of %ld "
             "bytes\n"),
          obj_name.c_str(), obj_stat->size, request->wbuflen);
    Dmsg1(100, "%s", errmsg);
    dev_errno = EINVAL;
    return false;
  }

  // get remote object
  // char* buffer = static_cast<char*>(malloc(obj_stat->size));

  if (auto obj_data = m_storage.download(obj_name, obj_chunk, {request->buffer, obj_stat->size})) {
    *request->rbuflen = obj_data->size_bytes();
    //request->buffer = obj_data->data();
    return true;
  } else {
    Mmsg1(errmsg, T_("Failed to read %s/%s\n"), obj_name.c_str(), obj_chunk.c_str());
    Dmsg1(100, "%s", errmsg);
    dev_errno = EIO;
    return false;
  }
}

bool DropletCompatibleDevice::TruncateRemoteVolume(DeviceControlRecord*)
{
  const char* vol_name = getVolCatName();
  const auto list = m_storage.list(vol_name);
  for (const auto& [chunk_name, stat] : list) {
    if (is_chunk_name(chunk_name)) {
      if (!m_storage.remove(vol_name, chunk_name)) { return false; }
    }
  }
  return true;
}

ssize_t DropletCompatibleDevice::RemoteVolumeSize()
{
  const auto list = m_storage.list(getVolCatName());
  if (list.empty()) { return -1; }
  ssize_t total_size{0};
  for (const auto& [name, stat] : list) {
    if (is_chunk_name(name)) { total_size += stat.size; }
  }
  return total_size;
}


bool DropletCompatibleDevice::d_flush(DeviceControlRecord*)
{
  return WaitUntilChunksWritten();
};

int DropletCompatibleDevice::d_open(const char* pathname, int flags, int mode)
{
  return SetupChunk(pathname, flags, mode);
}

ssize_t DropletCompatibleDevice::d_read(int t_fd, void* buffer, size_t count)
{
  return ReadChunked(t_fd, buffer, count);
}

ssize_t DropletCompatibleDevice::d_write(int t_fd,
                                         const void* buffer,
                                         size_t count)
{
  return WriteChunked(t_fd, buffer, count);
}

int DropletCompatibleDevice::d_close(int) { return CloseChunk(); }

int DropletCompatibleDevice::d_ioctl(int, ioctl_req_t, char*) { return -1; }

boffset_t DropletCompatibleDevice::d_lseek(DeviceControlRecord*,
                                           boffset_t offset,
                                           int whence)
{
  switch (whence) {
    case SEEK_SET:
      offset_ = offset;
      break;
    case SEEK_CUR:
      offset_ += offset;
      break;
    case SEEK_END: {
      ssize_t volumesize;

      volumesize = ChunkedVolumeSize();

      Dmsg1(100, "Current volumesize: %lld\n", volumesize);

      if (volumesize >= 0) {
        offset_ = volumesize + offset;
      } else {
        return -1;
      }
      break;
    }
    default:
      return -1;
  }

  if (!LoadChunk()) { return -1; }

  return offset_;
}

bool DropletCompatibleDevice::d_truncate(DeviceControlRecord* dcr)
{
  return TruncateChunkedVolume(dcr);
}

REGISTER_SD_BACKEND(dplcompat, DropletCompatibleDevice)

} /* namespace storagedaemon */
