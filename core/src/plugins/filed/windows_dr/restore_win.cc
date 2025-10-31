/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include "file_format.h"
#include "error.h"
#include "common.h"

#include <iostream>
#include <string>
#include <stdexcept>
#include <span>
#include <initguid.h>  // ask virtdisk.h to include guid definitions
#include <virtdisk.h>
#include <zlib.h>
#include <optional>
#include <cstdint>

#include "parser.h"
#include "partitioning.h"
#include "format.h"

#include "util.h"

void WriteOverlapped(HANDLE hndl,
                     std::size_t offset,
                     const char* buffer,
                     DWORD size,
                     DWORD* bytes_written)
{
  OVERLAPPED overlapped = {};
  overlapped.Offset = (DWORD)offset;
  overlapped.OffsetHigh = (DWORD)(offset >> 32);

  if (WriteFile(hndl, buffer, size, bytes_written, &overlapped)) { return; }

  auto err = GetLastError();

  if (err == ERROR_IO_PENDING) {
    if (!GetOverlappedResult(hndl, &overlapped, bytes_written, TRUE)) {
      err = GetLastError();
      fprintf(stderr, "WriteFile: could not write (%llu, %llu): %d\n",
              static_cast<long long unsigned>(offset),
              static_cast<long long unsigned>(size), err);

      throw win_error("WriteFileW", err);
    }
    return;
  }

  fprintf(stderr, "WriteFile: could not write (%llu, %llu): %d\n",
          static_cast<long long unsigned>(offset),
          static_cast<long long unsigned>(size), err);
  throw win_error("WriteFileW", err);
}

void write_buffer(HANDLE output,
                  std::size_t start,
                  std::span<const char> buffer)
{
  std::size_t offset = 0;

#if 0
  fprintf(
      stderr,
      "%llu: %02X %02X %02X %02X %02X %02X ... %02X %02X %02X %02X %02X %02X\n",
      start, (unsigned char)buffer[0], (unsigned char)buffer[1],
      (unsigned char)buffer[2], (unsigned char)buffer[3],
      (unsigned char)buffer[4], (unsigned char)buffer[5],
      (unsigned char)buffer[buffer.size() - 6],
      (unsigned char)buffer[buffer.size() - 5],
      (unsigned char)buffer[buffer.size() - 4],
      (unsigned char)buffer[buffer.size() - 3],
      (unsigned char)buffer[buffer.size() - 2],
      (unsigned char)buffer[buffer.size() - 1]);
#endif

  while (offset != buffer.size()) {
    DWORD bytes_written = 0;

    WriteOverlapped(output, start + offset, buffer.data() + offset,
                    (DWORD)(buffer.size() - offset), &bytes_written);

    offset += bytes_written;
  }
}

class HandleOutput : public Output {
  static constexpr std::size_t buffer_cap = 4 << 20;

 public:
  HandleOutput(HANDLE hndl, std::size_t size) : hndl_{hndl}, size_{size}
  {
    buffer.reserve(buffer_cap);
  }

  void flush_buffer()
  {
    write_buffer(hndl_, current_offset_, buffer);
    current_offset_ += buffer.size();
    buffer.clear();
  }

  void append(std::span<const char> bytes) override
  {
    if (current_offset_ + bytes.size() > size_) {
      throw std::logic_error{"can not write past the end of the file"};
    }

    while (!bytes.empty()) {
      std::size_t buffer_free = buffer_cap - buffer.size();

      if (bytes.size() < buffer_free) {
        buffer.insert(buffer.end(), bytes.begin(), bytes.end());
        break;
      } else {
        buffer.insert(buffer.end(), bytes.data(), bytes.data() + buffer_free);
        flush_buffer();
        bytes = bytes.subspan(buffer_free);
      }
    }
  }

  void skip_forwards(std::size_t offset) override
  {
    auto actual_offset = current_offset_ + buffer.size();

    if (offset < actual_offset) {
      throw std::logic_error{libbareos::format(
          "Trying to skip to offset {}, when already at offset {} ({} + {})",
          offset, actual_offset, current_offset_, buffer.size())};
    }

    if (offset > size_) {
      throw std::logic_error{"can not seek past the end of the file"};
    }

    if (offset < current_offset_ + buffer_cap) {
      auto diff = offset - actual_offset;
      buffer.insert(buffer.end(), diff, 0);
    } else if (!buffer.empty()) {
      // zero out the rest of the buffer
      buffer.insert(buffer.end(), buffer_cap - buffer.size(), 0);
      flush_buffer();

      current_offset_ = offset;
    }
  }

  std::size_t current_offset() const override { return current_offset_; }

  ~HandleOutput() { flush_buffer(); }

 private:
  HANDLE hndl_;
  std::size_t current_offset_ = 0;
  std::size_t size_ = 0;

  std::vector<char> buffer{};
};

class OutputHandleGenerator {
 public:
  virtual HANDLE Create(disk_info info, disk_geometry geo) = 0;
  virtual void Close(HANDLE hndl) = 0;

  virtual ~OutputHandleGenerator() {}
};

static inline LARGE_INTEGER AsLargeInteger(std::uint64_t x)
{
  return LARGE_INTEGER{.QuadPart = static_cast<LONGLONG>(x)};
}

class RawFileGenerator : public OutputHandleGenerator {
 public:
  RawFileGenerator(std::wstring dir_path, GenericLogger* logger)
      : dir{std::move(dir_path)}, log{logger}
  {
    while (dir.back() == L'\\') { dir.pop_back(); }
  }


  HANDLE Create(disk_info info, disk_geometry) override
  {
    // todo: use path here
    auto wdisk_path = libbareos::format(L"{}\\disk-{}.raw", dir, ++disk_idx_);
    auto disk_path = FromUtf16(wdisk_path);

    // some ioctls need GENERIC_READ to work ...
    HANDLE output = CreateFileW(wdisk_path.c_str(),
                                GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE,
                                NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    if (output == INVALID_HANDLE_VALUE) {
      throw std::runtime_error{libbareos::format("could not open {}: Err={}",
                                                 disk_path, GetLastError())};
    }

    DWORD bytes_written;
    if (DeviceIoControl(output, FSCTL_SET_SPARSE, NULL, 0, NULL, 0,
                        &bytes_written, NULL)
        != 0) {
      if (!SetFilePointerEx(output, AsLargeInteger(info.disk_size), NULL,
                            FILE_BEGIN)
          || !SetEndOfFile(output)) {
        log->Info("could not enlarge {} to required size", disk_path);
      }

      if (!SetFilePointerEx(output, {}, NULL, FILE_BEGIN)) {
        throw std::runtime_error{libbareos::format(
            "could not reset file pointer after enlarging the file")};
      }
    } else {
      log->Info(
          "could not set {} to sparse (Err={}); will not try to set it to "
          "correct size",
          disk_path, GetLastError());
    }

    return output;
  }
  void Close(HANDLE hndl) override { CloseHandle(hndl); }

 private:
  std::size_t disk_idx_ = 0;
  std::wstring dir;
  GenericLogger* log;
};

class VhdxGenerator : public OutputHandleGenerator {
 public:
  VhdxGenerator(std::wstring dir_path, GenericLogger*)
      : dir{std::move(dir_path)}
  {
    while (dir.back() == L'\\') { dir.pop_back(); }
  }

  HANDLE Create(disk_info info, disk_geometry geo) override
  {
    auto disk_path = libbareos::format(L"{}\\disk-{}.vhdx", dir, ++disk_idx_);

    VIRTUAL_STORAGE_TYPE vst
        = {.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
           .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};

    CREATE_VIRTUAL_DISK_PARAMETERS params = {
      .Version = CREATE_VIRTUAL_DISK_VERSION_2,
      .Version2 = {
        .UniqueId = {},
        .MaximumSize = info.disk_size,
        .BlockSizeInBytes = {},
        .SectorSizeInBytes = gsl::narrow<ULONG>(geo.bytes_per_sector),
        .PhysicalSectorSizeInBytes = {},
        .ParentPath = nullptr,
        .SourcePath = nullptr,
      },
    };

    // logger_->Trace("creating disk");
    HANDLE output = INVALID_HANDLE_VALUE;

    DWORD hres = CreateVirtualDisk(
        &vst, disk_path.c_str(), VIRTUAL_DISK_ACCESS_NONE, NULL,
        CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &params, NULL, &output);

    if (hres != ERROR_SUCCESS) {
      throw std::runtime_error{libbareos::format(
          "CreateVirtualDisk({}) returned {}", FromUtf16(disk_path), hres)};
    }

    ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
        = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
    auto attach_res = AttachVirtualDisk(output, NULL,
                                        ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST,
                                        0, &attach_params, 0);

    if (attach_res != ERROR_SUCCESS) {
      CloseHandle(output);
      throw std::runtime_error(
          libbareos::format("AttachVirtualDisk({}) returned {}\n",
                            FromUtf16(disk_path), attach_res));
    }

    return output;
  }
  void Close(HANDLE hndl) override
  {
    DetachVirtualDisk(hndl, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
    CloseHandle(hndl);
  }

 private:
  std::size_t disk_idx_ = 0;
  std::wstring dir;
};

struct open_volume {
  std::wstring guid{};
  HANDLE hndl{INVALID_HANDLE_VALUE};


  open_volume() = default;
  open_volume(std::wstring guid_, HANDLE hndl_)
      : guid{std::move(guid_)}, hndl{hndl_}
  {
  }

  open_volume(const open_volume&) = delete;
  open_volume& operator=(const open_volume&) = delete;

  open_volume(open_volume&& other) { *this = std::move(other); }
  open_volume& operator=(open_volume&& other)
  {
    std::swap(guid, other.guid);
    std::swap(hndl, other.hndl);
    return *this;
  }

  ~open_volume()
  {
    if (hndl != INVALID_HANDLE_VALUE) { CloseHandle(hndl); }
  }
};

class DiskHandles : public OutputHandleGenerator {
 public:
  DiskHandles(std::vector<std::size_t> ids_, GenericLogger* logger_)
      : ids{std::move(ids_)}, logger{logger_}
  {
    // we need to keep the volume handles open, to prevent windows to
    // remount them.  Once they are mounted, we can no longer overwrite
    // the disk directly.
    volumes = GetAllVolumes(ids);

    if (volumes.size() > 0) {
      // throw std::runtime_error{"some disk contains volumes"};
      logger->Info("found {} volumes on the selected disks", volumes.size());
      for (auto& vol : volumes) { DismountVolume(vol); }
    }

    handles = OpenAll(ids);
  }

  HANDLE Create(disk_info info, disk_geometry) override
  {
    HANDLE handle = handles[disk_idx_];
    {
      GET_LENGTH_INFORMATION length_info = {0};
      DWORD bytes_returned = 0;

      if (DeviceIoControl(handle, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
                          &length_info, sizeof(length_info), &bytes_returned,
                          NULL)) {
        auto disk_size = length_info.Length.QuadPart;

        if (disk_size < 0) {
          throw std::logic_error{libbareos::format(
              "disk size of \\\\.\\PhysicalDrive{} is negative! ({})",
              ids[disk_idx_], disk_size)};
        }
        logger->Trace("disk size = {}", disk_size);
        if (static_cast<std::uint64_t>(disk_size) < info.disk_size) {
          logger->Info(
              "disk \\\\.\\PhysicalDrive{} is too small: actual size = {}, "
              "required size = {}; continuing anyways",
              ids[disk_idx_], disk_size, info.disk_size);
        } else if (static_cast<std::uint64_t>(disk_size) > info.disk_size) {
          logger->Trace(
              "disk \\\\.\\PhysicalDrive{} is too large: actual size = {}, "
              "required size = {}; continuing anyways",
              ids[disk_idx_], disk_size, info.disk_size);
        }
      } else {
        logger->Info(
            "could not determine disk size for disk \\\\.\\PhysicalDrive{}: "
            "Err={}; continuing anyways",
            ids[disk_idx_], GetLastError());
      }
    }
    return handle;
  }

  void Close(HANDLE hndl) override
  {
    FlushFileBuffers(hndl);
    CloseHandle(hndl);

    handles[disk_idx_] = INVALID_HANDLE_VALUE;
    disk_idx_ += 1;
  }

  ~DiskHandles()
  {
    for (auto hndl : handles) {
      if (hndl != INVALID_HANDLE_VALUE) { CloseHandle(hndl); }
    }
  }

 private:
  std::vector<open_volume> GetAllVolumes(std::span<std::size_t> disk_ids)
  {
    /* we want to find all volumes on all disks.
     * a volume consists of multiple extents, each of which is on exacly one
     * disk. So what we basically need to do is return a list of all volumes
     * whose extents are all on the disks with the given disk ids.
     * If a volume has no extent on _any_ of the given disk ids, then we can
     * safely ignore it.
     * _BUT_ if a volume has extents both on a given disk id, and on a disk
     * _not_ in the list, then we need to error out. There is no way to handle
     * this correctly, as we will completely destroy the volume.
     * Maybe in the future we can give the user the option to ignore this
     * and just continue on, but I am not sure if Windows even allows us to
     * write to the disk in that case. */

    std::vector<open_volume> volumes;

    wchar_t volume_guid[MAX_PATH];
    HANDLE iter = FindFirstVolumeW(volume_guid, std::size(volume_guid));

    if (iter == INVALID_HANDLE_VALUE) {
      throw std::runtime_error{libbareos::format(
          "could not find first volume: Err={}", GetLastError())};
    }

    std::vector<char> extent_buffer;
    extent_buffer.resize(1024 * 1024);

    for (;;) {
      std::wstring guid{volume_guid};
      if (guid.size() == 0) {
        throw std::runtime_error{libbareos::format(
            "received volume with empty guid; cannot continue")};
      }

      while (guid.back() == L'\\') { guid.pop_back(); }

      logger->Trace("found volume {}", FromUtf16(guid));

      HANDLE hndl = CreateFileW(guid.c_str(), GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                OPEN_EXISTING, 0, NULL);

      if (hndl == INVALID_HANDLE_VALUE) {
        auto err = GetLastError();
        throw std::runtime_error{libbareos::format(
            "could not open volume {}: Err={}", FromUtf16(guid), err)};
      }

      bool check_extents = false;
      // first check that this is an actual disk volume
      {
        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;
        std::vector<char> buffer;
        buffer.resize(1024);
        DWORD bytes_returned;
        for (;;) {
          if (!DeviceIoControl(hndl, IOCTL_STORAGE_QUERY_PROPERTY, &query,
                               sizeof(query), buffer.data(), buffer.size(),
                               &bytes_returned, NULL)) {
            auto err = GetLastError();
            if (err == ERROR_MORE_DATA) {
              buffer.resize(buffer.size() * 2);
              continue;
            }

            throw std::runtime_error{libbareos::format(
                "could not query storage device for volume {}: Err={}",
                FromUtf16(guid), err)};
          }

          break;
        }

        auto* desc
            = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer.data());
        switch (desc->DeviceType) {
            // see
            // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/scsi/ns-scsi-_inquirydata
            //
            // DIRECT_ACCESS_DEVICE 0x00 	Disk
            // SEQUENTIAL_ACCESS_DEVICE 0x01 	Tape device
            // PRINTER_DEVICE 0x02 	Printer
            // PROCESSOR_DEVICE 0x03 	Scanner, printer, etc
            // WRITE_ONCE_READ_MULTIPLE_DEVICE 0x04 	WORM
            // READ_ONLY_DIRECT_ACCESS_DEVICE 0x05 	CD-ROM
            // SCANNER_DEVICE 0x06 	Scanner
            // OPTICAL_DEVICE 0x07 	Optical disk
            // MEDIUM_CHANGER 0x08 	Jukebox
            // COMMUNICATION_DEVICE 0x09 	Network device
            // ARRAY_CONTROLLER_DEVICE 0x0C 	Array controller
            // SCSI_ENCLOSURE_DEVICE 0x0D 	SCSI enclosure device
            // REDUCED_BLOCK_DEVICE 0x0E 	For example, 1394 disk
            // OPTICAL_CARD_READER_WRITER_DEVICE 0x0F 	Optical card
            // reader/writer BRIDGE_CONTROLLER_DEVICE 0x10 	Bridge
            // controller OBJECT_BASED_STORAGE_DEVICE 0x11 	OSD device
            // HOST_MANAGED_ZONED_BLOCK_DEVICE 0x14 	Host managed zoned block
            // device UNKNOWN_OR_NO_DEVICE 0x1F 	Unknown or no device
            // type LOGICAL_UNIT_NOT_PRESENT_DEVICE 0x7F 	A logical unit
            // is not present

          case 0x00:
          case 0x14:
          case 0x0E: {
            logger->Trace("look at extents of volume {} of type {:x}",
                          FromUtf16(guid), desc->DeviceType);
            check_extents = true;
          } break;
          default: {
            logger->Trace("volume {} is not a disk (type = {:x})",
                          FromUtf16(guid), desc->DeviceType);
            CloseHandle(hndl);
          }
        }
      }

      if (check_extents) {
        std::optional<std::size_t> requested_disk_id;
        std::optional<std::size_t> not_requested_disk_id;

        DWORD bytes_written = 0;
        for (;;) {
          if (!DeviceIoControl(hndl, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                               nullptr, 0, extent_buffer.data(),
                               extent_buffer.size(), &bytes_written, nullptr)) {
            auto err = GetLastError();

            if (err != ERROR_MORE_DATA) {
              throw std::runtime_error{libbareos::format(
                  "could not enumerate all extents for volume {}: Err={}",
                  FromUtf16(guid), err)};
            }

            extent_buffer.resize(extent_buffer.size() * 2);
          }

          break;
        }

        if (bytes_written == 0) {
          // no extents, so we can safely ignore this volume.
          logger->Trace("volume {} has no extents -> ignored", FromUtf16(guid));
          CloseHandle(hndl);
        } else {
          auto extents
              = reinterpret_cast<VOLUME_DISK_EXTENTS*>(extent_buffer.data());

          for (size_t i = 0; i < extents->NumberOfDiskExtents; ++i) {
            auto& extent = extents->Extents[i];

            if (std::find(std::begin(disk_ids), std::end(disk_ids),
                          extent.DiskNumber)
                == std::end(disk_ids)) {
              not_requested_disk_id = extent.DiskNumber;
            } else {
              requested_disk_id = extent.DiskNumber;
            }
          }

          /* we now have 4 cases to consider.
           * | extent on requested | extent on non requested | result |
           * |---------------------+-------------------------+--------|
           * |        true         |           true          | error! |
           * |        false        |           true          | ignore |
           * |        true         |           false         | keep   |
           * |        false        |           false         |  (1)   |
           * |--------------------------------------------------------|
           * (1): this case is not possible, as this can only happen if
           *      there are no extents, which we checked above.
           *      As such we will just ignore it */


          if (requested_disk_id && not_requested_disk_id) {
            throw std::runtime_error{libbareos::format(
                "volume {} is both on disk {} and disk {}: cannot continue",
                FromUtf16(guid), requested_disk_id.value(),
                not_requested_disk_id.value())};
          } else if (!requested_disk_id && not_requested_disk_id) {
            // ignore (see above)
            logger->Trace("volume {} => ignored", FromUtf16(guid));
            CloseHandle(hndl);
          } else if (requested_disk_id && !not_requested_disk_id) {
            logger->Trace("volume {} => kept", FromUtf16(guid));
            volumes.emplace_back(std::move(guid), hndl);
          } else {
            // ignore (see above)
            logger->Trace("volume {} => ignored (no extents)", FromUtf16(guid));
            CloseHandle(hndl);
          }
        }
      }

      if (!FindNextVolumeW(iter, volume_guid, sizeof(volume_guid))) {
        auto err = GetLastError();
        if (err == ERROR_NO_MORE_FILES) { break; }
        throw std::runtime_error{libbareos::format(
            "could not find next volume: Err={}", GetLastError())};
      }
    }

    FindVolumeClose(iter);

    return volumes;
  }

  void DismountVolume(const open_volume& volume)
  {
    DWORD bytes_returned = 0;
    // lock ensures that we are the only ones that have an open handle to the
    // volume this lock is unlocked automatically as soon as the volume handle
    // is cleaned up
    if (DeviceIoControl(volume.hndl, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0,
                        &bytes_returned, NULL)
        == 0) {
      auto err = GetLastError();
      throw std::runtime_error{libbareos::format(
          "could not lock volume {}: Err={}", FromUtf16(volume.guid), err)};
    }
    logger->Trace("volume {} was locked", FromUtf16(volume.guid));
    if (DeviceIoControl(volume.hndl, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0,
                        &bytes_returned, NULL)
        == 0) {
      auto err = GetLastError();
      throw std::runtime_error{libbareos::format(
          "could not dismount volume {}: Err={}", FromUtf16(volume.guid), err)};
    }
    logger->Trace("volume {} was dismounted", FromUtf16(volume.guid));
  }

  std::vector<HANDLE> OpenAll(std::span<std::size_t> ids)
  {
    std::vector<HANDLE> result;
    result.reserve(ids.size());

    for (auto id : ids) {
      logger->Trace("looking at \\\\.\\PhysicalDrive{}", id);
      auto disk_path = libbareos::format(L"\\\\.\\PhysicalDrive{}", id);
      HANDLE output
          = CreateFileW(disk_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                        NULL, OPEN_EXISTING, 0, NULL);

      if (output == INVALID_HANDLE_VALUE) {
        logger->Trace("opening \\\\.\\PhysicalDrive{} failed", id);
        auto err = GetLastError();
        for (auto hndl : result) { CloseHandle(hndl); }

        throw std::runtime_error{
            libbareos::format("could not open disk {}: Err={}", id, err)};
      }

      result.push_back(output);
    }

    return result;
  }


  std::size_t disk_idx_ = 0;
  std::vector<std::size_t> ids;
  std::vector<HANDLE> handles;
  std::vector<open_volume> volumes;

  GenericLogger* logger;
};

class RestoreToHandles : public GenericHandler {
 public:
  RestoreToHandles(std::unique_ptr<OutputHandleGenerator> Generator,
                   GenericLogger* logger)
      : Generator_{std::move(Generator)}, logger_{logger}
  {
  }

  void BeginRestore(std::size_t num_disks [[maybe_unused]]) override {}
  void EndRestore() override {}
  void BeginDisk(disk_info info) override
  {
    if (disk_) {
      throw std::logic_error{"cannot begin disk after one was created"};
    }

    logger_->Info("begin disk {{ size: {}, extent count: {} }}", info.disk_size,
                  info.extent_count);
    auto geo = geometry_for_size(info.disk_size);
    HANDLE hndl = Generator_->Create(info, geo);
    disk_.emplace(hndl, geo, info.disk_size);
  }
  void EndDisk() override
  {
    logger_->Info("disk done");
    HANDLE hndl = disk_->hndl;
    disk_.reset();
    Generator_->Close(hndl);
  }

  void BeginMbrTable(const partition_info_mbr& mbr) override
  {
    disk().BeginMbrTable(mbr);
  }

  void BeginGptTable(const partition_info_gpt& gpt) override
  {
    disk().BeginGptTable(gpt);
  }

  void BeginRawTable(const partition_info_raw& raw) override
  {
    disk().BeginRawTable(raw);
  }

  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    disk().MbrEntry(entry, data);
  }

  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    disk().GptEntry(entry, data);
  }

  void EndPartTable() override { disk().EndPartTable(); }

  void BeginExtent(extent_header header) override
  {
    logger_->Trace("begin extent {{ size: {}, offset: {} }}", header.length,
                   header.offset);
    disk().BeginExtent(header);
  }
  void ExtentData(std::span<const char> data) override
  {
    // logger_->Trace("extent data {{ size: {} }}", data.size());
    disk().ExtentData(data);
  }
  void EndExtent() override
  {
    logger_->Trace("extent end");
    disk().EndExtent();
  }

  ~RestoreToHandles()
  {
    if (disk_) {
      HANDLE hndl = disk_->hndl;
      disk_.reset();
      Generator_->Close(hndl);
    }
  }

 private:
  disk::Parser& disk()
  {
    if (!disk_) {
      throw std::logic_error{"cannot access disk if no disk is started"};
    }
    return disk_->parser;
  }

  std::unique_ptr<OutputHandleGenerator> Generator_;
  GenericLogger* logger_ = nullptr;
  struct open_disk {
    HANDLE hndl;
    HandleOutput output;
    disk::Parser parser;

    open_disk(HANDLE hndl_, disk_geometry geo, std::size_t disk_size)
        : hndl{hndl_}, output{hndl_, disk_size}, parser{geo, disk_size, &output}
    {
    }
  };
  std::optional<open_disk> disk_;
};

#include "restore.h"
namespace barri::restore {
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           vhdx_directory dir)
{
  return std::make_unique<RestoreToHandles>(
      std::make_unique<VhdxGenerator>(dir.path, logger), logger);
}
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           raw_directory dir)
{
  return std::make_unique<RestoreToHandles>(
      std::make_unique<RawFileGenerator>(dir.path, logger), logger);
}
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger, disk_ids ids)
{
  return std::make_unique<RestoreToHandles>(
      std::make_unique<DiskHandles>(ids, logger), logger);
}
}  // namespace barri::restore
