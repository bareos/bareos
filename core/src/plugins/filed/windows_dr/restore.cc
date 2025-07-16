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

#include "parser.h"
#include "partitioning.h"
#include "format.h"

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

class RawFileGenerator : public OutputHandleGenerator {
 public:
  HANDLE Create(disk_info info, disk_geometry) override
  {
    auto disk_path = libbareos::format(L"disk-{}.raw", ++disk_idx_);

    HANDLE output
        = CreateFileW(disk_path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                      CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    if (output == INVALID_HANDLE_VALUE) {
      throw std::runtime_error{libbareos::format(
          "could not open disk-{}.raw: Err={}", disk_idx_, GetLastError())};
    }

    // TODO: set file size
    (void)info;

    return output;
  }
  void Close(HANDLE hndl) override { CloseHandle(hndl); }

 private:
  std::size_t disk_idx_ = 0;
};

class VhdxGenerator : public OutputHandleGenerator {
 public:
  HANDLE Create(disk_info info, disk_geometry geo) override
  {
    auto disk_path = libbareos::format(L"disk-{}.vhdx", ++disk_idx_);

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

    fprintf(stderr, "creating disk\n");
    HANDLE output = INVALID_HANDLE_VALUE;

    DWORD hres = CreateVirtualDisk(
        &vst, disk_path.c_str(), VIRTUAL_DISK_ACCESS_NONE, NULL,
        CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &params, NULL, &output);

    if (hres != ERROR_SUCCESS) {
      throw std::runtime_error{libbareos::format(
          "CreateVirtualDisk(disk-{}.vhdx) returned {}", disk_idx_, hres)};
    }

    ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
        = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
    auto attach_res = AttachVirtualDisk(output, NULL,
                                        ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST,
                                        0, &attach_params, 0);

    if (attach_res != ERROR_SUCCESS) {
      CloseHandle(output);
      throw std::runtime_error(
          libbareos::format("AttachVirtualDisk(disk-{}.vhdx) returned {}\n",
                            disk_idx_, attach_res));
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
};

class RestoreToHandles : public GenericHandler {
 public:
  RestoreToHandles(OutputHandleGenerator* Generator, GenericLogger* logger)
      : Generator_{Generator}, logger_{logger}
  {
  }

  void BeginRestore(std::size_t num_disks [[maybe_unused]]) override {}
  void EndRestore() override {}
  void BeginDisk(disk_info info) override
  {
    if (disk_) {
      throw std::logic_error{"cannot begin disk after one was created"};
    }

    logger_->Info("begin disk {{ size {}, count {} }}", info.disk_size,
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
    logger_->Info("begin extent {{ size: {}, offset: {} }}", header.length,
                  header.offset);
    disk().BeginExtent(header);
  }
  void ExtentData(std::span<const char> data) override
  {
    // libbareos::println(stderr, "extent data {{ size: {} }}", data.size());
    disk().ExtentData(data);
  }
  void EndExtent() override
  {
    logger_->Info("extent end");
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

  OutputHandleGenerator* Generator_ = nullptr;
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

void do_restore(std::istream& stream, GenericLogger* logger, bool raw_file)
{
  auto output_generator = [&]() -> std::unique_ptr<OutputHandleGenerator> {
    if (raw_file) {
      return std::make_unique<RawFileGenerator>();
    } else {
      return std::make_unique<VhdxGenerator>();
    }
  }();
  RestoreToHandles alg{output_generator.get(), logger};

  parse_file_format(logger, stream, &alg);
}
